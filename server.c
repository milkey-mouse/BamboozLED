/* Copyright 2013 Ka-Ping Yee

Licensed under the Apache License, Version 2.0 (the "License"); you may not
use this file except in compliance with the License.  You may obtain a copy
of the License at: http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied.  See the License for the
specific language governing permissions and limitations under the License. */

#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>

#include "bamboozled.h"
#include "opc.h"

/* Internal structure for a source.  sock >= 0 iff the connection is open. */
typedef struct
{
  uint16_t port;
  int listen_sock;
  int sock;
  uint16_t header_length;
  uint8_t header[4];
  uint16_t payload_length;
  uint8_t payload[1 << 16];
  layer_handle layer;
} opc_source_info;

static opc_source_info opc_sources[MAX_CLIENTS];
static int opc_next_source = 0;

int opc_listen(uint16_t port)
{
  struct sockaddr_in address;
  int sockfd;
  int one = 1;

  sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

  address.sin_family = AF_INET;
  address.sin_port = htons(port);

  memset(&address.sin_addr, 0, sizeof(address.sin_addr));
  memset(&address.sin_zero, 0, sizeof(address.sin_zero));
  if (bind(sockfd, (struct sockaddr *)&address, sizeof(address)) != 0)
  {
    fprintf(stderr, "OPC: Could not bind to port %d: ", port);
    perror(NULL);
    return -1;
  }
  if (listen(sockfd, 0) != 0)
  {
    fprintf(stderr, "OPC: Could not listen on port %d: ", port);
    perror(NULL);
    return -1;
  }
  return sockfd;
}

opc_source opc_new_source(uint16_t port)
{
  opc_source_info *info;

  /* Allocate an opc_source_info entry. */
  if (opc_next_source >= MAX_CLIENTS)
  {
    fprintf(stderr, "OPC: No more sources available\n");
    return -1;
  }
  info = &opc_sources[opc_next_source];

  /* Listen on the specified port. */
  info->port = port;
  info->listen_sock = opc_listen(port);
  if (info->listen_sock < 0)
  {
    return -1;
  }

  /* Create a new pixel buffer for this client's layer. */
  info->layer = layer_init();

  /* Increment opc_next_source only if we were successful. */
  fprintf(stderr, "OPC: Listening on port %d\n", port);
  return opc_next_source++;
}

uint8_t opc_receive(opc_source source, uint32_t timeout_ms)
{
  int nfds;
  fd_set readfds;
  struct timeval timeout;
  opc_source_info *info = &opc_sources[source];
  struct sockaddr_in address;
  socklen_t address_len = sizeof(address);
  uint16_t payload_expected;
  ssize_t received = 1; /* nonzero, because we treat zero to mean "closed" */
  char buffer[64];

  if (source < 0 || source >= opc_next_source)
  {
    fprintf(stderr, "OPC: Source %d does not exist\n", source);
    return 0;
  }

  /* Select for inbound data or connections. */
  FD_ZERO(&readfds);
  if (info->listen_sock >= 0)
  {
    FD_SET(info->listen_sock, &readfds);
    nfds = info->listen_sock + 1;
  }
  else if (info->sock >= 0)
  {
    FD_SET(info->sock, &readfds);
    nfds = info->sock + 1;
  }
  timeout.tv_sec = timeout_ms / 1000;
  timeout.tv_usec = (timeout_ms % 1000) * 1000;
  select(nfds, &readfds, NULL, NULL, &timeout);
  if (info->listen_sock >= 0 && FD_ISSET(info->listen_sock, &readfds))
  {
    /* Handle an inbound connection. */
    info->sock = accept(
        info->listen_sock, (struct sockaddr *)&(address), &address_len);
    inet_ntop(AF_INET, &(address.sin_addr), buffer, 64);
    fprintf(stderr, "OPC: Client connected from %s\n", buffer);
    close(info->listen_sock);
    info->listen_sock = -1;
    info->header_length = 0;
    info->payload_length = 0;
  }
  else if (info->sock >= 0 && FD_ISSET(info->sock, &readfds))
  {
    /* Handle inbound data on an existing connection. */
    if (info->header_length < 4)
    { /* need header */
      received = recv(info->sock, info->header + info->header_length,
                      4 - info->header_length, 0);
      if (received > 0)
      {
        info->header_length += received;
      }
    }
    if (info->header_length == 4)
    { /* header complete */
      payload_expected = (info->header[2] << 8) | info->header[3];
      if (info->payload_length < payload_expected)
      { /* need payload */
        received = recv(info->sock, info->payload + info->payload_length,
                        payload_expected - info->payload_length, 0);
        if (received > 0)
        {
          info->payload_length += received;
        }
      }
      if (info->header_length == 4 &&
          info->payload_length == payload_expected)
      { /* payload complete */
        switch (info->header[1])
        {
        case OPC_SET_PIXELS:
          if (!config.opcCompat)
          {
            layer_blit(info->layer, info->header[0], (rgbaPixel *)&(info->payload), info->payload_length / 4);
            break;
          }
        case OPC_SYSTEM_EXCLUSIVE:
          if (config.opcCompat && ((info->payload[0] << 8) | info->payload[1]) == OPC_SYSTEM_IDENTIFIER)
          {
            layer_blit(info->layer, info->header[0], (rgbaPixel *)&(info->payload[2]), (info->payload_length - 2) / 4);
            break;
          }
        default:
          // send to the destination server directly
          break;
        }
        info->header_length = 0;
        info->payload_length = 0;
      }
    }
    if (received <= 0)
    {
      /* Connection was closed; wait for more connections. */
      fprintf(stderr, "OPC: Client closed connection\n");
      close(info->sock);
      info->sock = -1;
      layer_destroy(info->layer);
      info->listen_sock = opc_listen(info->port);
    }
  }
  else
  {
    /* timeout_ms milliseconds passed with no incoming data or connections. */
    return 0;
  }
  return 1;
}

void opc_reset_source(opc_source source)
{
  opc_source_info *info = &opc_sources[source];
  if (source < 0 || source >= opc_next_source)
  {
    fprintf(stderr, "OPC: Source %d does not exist\n", source);
    return;
  }

  if (info->sock >= 0)
  {
    fprintf(stderr, "OPC: Closed connection\n");
    close(info->sock);
    info->sock = -1;
    info->listen_sock = opc_listen(info->port);
  }
}
