/* Copyright 2013 Ka-Ping Yee (Modified by Milkey Mouse)

Licensed under the Apache License, Version 2.0 (the "License"); you may not
use this file except in compliance with the License. You may obtain a copy
of the License at: http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied. See the License for the
specific language governing permissions and limitations under the License. */

#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>

#include "bamboozled.h"
#include "opc.h"

bool opc_listen(layer *l)
{
    struct sockaddr_in address;
    int sockfd;
    int one = 1;

    sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    address.sin_family = AF_INET;
    address.sin_port = htons(l->port);

    memcpy(&address.sin_addr, &config.listen.host, sizeof(address.sin_addr));
    memset(&address.sin_zero, 0, sizeof(address.sin_zero));
    if (bind(sockfd, (struct sockaddr *)&address, sizeof(address)) != 0)
    {
        fprintf(stderr, "Could not bind to port %d: ", l->port);
        perror(NULL);
        return false;
    }
    if (listen(sockfd, 0) != 0)
    {
        fprintf(stderr, "Could not listen on port %d: ", l->port);
        perror(NULL);
        return false;
    }
    l->listen_sock = sockfd;
    return true;
}

uint8_t opc_receive(layer *l, uint32_t timeout_ms)
{
    int nfds;
    fd_set readfds;
    struct timeval timeout;
    struct sockaddr_in address;
    socklen_t address_len = sizeof(address);
    uint16_t payload_expected;
    ssize_t received = 1; // nonzero, because we treat zero to mean "closed"
    char buffer[64];

    // Select for inbound data or connections.
    FD_ZERO(&readfds);
    if (l->listen_sock >= 0)
    {
        FD_SET(l->listen_sock, &readfds);
        nfds = l->listen_sock + 1;
    }
    else if (l->sock >= 0)
    {
        FD_SET(l->sock, &readfds);
        nfds = l->sock + 1;
    }
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    select(nfds, &readfds, NULL, NULL, &timeout);
    if (l->listen_sock >= 0 && FD_ISSET(l->listen_sock, &readfds))
    {
        // handle an inbound connection
        l->sock = accept(l->listen_sock, (struct sockaddr *)&(address), &address_len);
        inet_ntop(AF_INET, &(address.sin_addr), buffer, sizeof(buffer));
        fprintf(stderr, "Client connected from %s\n", buffer);
        close(l->listen_sock);
        l->listen_sock = -1;
        l->header_length = 0;
        l->payload_length = 0;
    }
    else if (l->sock >= 0 && FD_ISSET(l->sock, &readfds))
    {
        // handle data on an existing connection
        if (l->header_length < 4)
        {
            // need header
            received = recv(l->sock, l->header + l->header_length, 4 - l->header_length, 0);
            if (received > 0)
            {
                l->header_length += received;
            }
        }
        if (l->header_length == 4)
        {
            // header complete
            payload_expected = (l->header[2] << 8) | l->header[3];
            if (l->payload_length < payload_expected)
            {
                // need payload
                received = recv(l->sock, l->payload + l->payload_length, payload_expected - l->payload_length, 0);
                if (received > 0)
                {
                    l->payload_length += received;
                }
            }
            if (l->header_length == 4 && l->payload_length == payload_expected)
            {
                // payload complete
                switch (l->header[1])
                {
                case OPC_SET_ARGB:
                    layer_blit(l, l->header[0], (rgbaPixel *)&(l->payload), l->payload_length / 4);
                    break;
                case OPC_SYSTEM_EXCLUSIVE:
                    if (((l->payload[0] << 8) | l->payload[1]) == OPC_SYSTEM_IDENTIFIER)
                    {
                        // reorder layers or whatever
                        break;
                    }
                default:
                    // send to the destination server directly
                    break;
                }
                l->header_length = 0;
                l->payload_length = 0;
            }
        }
        if (received <= 0)
        {
            // connection was closed; wait for more connections
            fputs("Client closed connection\n", stderr);
            layer_destroy(l);
            return 1;
        }
    }
    else
    {
        // timeout_ms milliseconds passed with no incoming data or connections
        return 0;
    }
    return 1;
}
