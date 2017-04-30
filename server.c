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
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include "opc.h"

void opc_serve(in_addr_t host, uint16_t port)
{
    struct sockaddr_in server, client;
    int sockfd;
    int one = 1;

    sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    memcpy(&server.sin_addr, &host, sizeof(server.sin_addr));
    server.sin_port = htons(port);

    server.sin_family = AF_INET;
    memset(&server.sin_zero, 0, sizeof(server.sin_zero));
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    if (bind(sockfd, (struct sockaddr *)(&server), sizeof(server)) != 0)
    {
        fprintf(stderr, "could not bind to port %d: ", port);
        perror(NULL);
        exit(1);
    }
    if (listen(sockfd, 3) != 0)
    {
        fprintf(stderr, "could not listen on port %d: ", port);
        perror(NULL);
        exit(1);
    }
    fprintf(stderr, "listening on port %d\n", port);

    int client_sock;
    socklen_t sl = sizeof(struct sockaddr_in);
    while ((client_sock = accept(sockfd, (struct sockaddr *)&client, (socklen_t *)&sl)))
    {
        char buffer[16];
        inet_ntop(AF_INET, &(client.sin_addr), buffer, sizeof(buffer));
        fprintf(stderr, "client connected from %s\n", buffer);

        layer *l = layer_init();
        l->sock = client_sock;

        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, (void * (*)(void *))opc_receive, l) < 0)
        {
            perror("could not create thread to handle connection: ");
            exit(1);
        }
    }

    if (client_sock < 0)
    {
        perror("accept failed: ");
        exit(1);
    }
}

void *opc_receive(layer *l)
{
    uint16_t payload_expected;
    uint16_t header_length = 0;
    uint16_t payload_length = 0;
    uint8_t header[4];
    uint8_t payload[1 << 16];

    ssize_t received = 1;
    while (received > 0)
    {
        if (header_length < 4)
        {
            // need header
            received = recv(l->sock, header + header_length, 4 - header_length, 0);
            if (received > 0)
            {
                header_length += received;
            }
        }
        if (header_length == 4)
        {
            // header complete
            payload_expected = (header[2] << 8) | header[3];
            if (payload_length < payload_expected)
            {
                // need payload
                received = recv(l->sock, payload + payload_length, payload_expected - payload_length, 0);
                if (received > 0)
                {
                    payload_length += received;
                }
            }
            if (header_length == 4 && payload_length == payload_expected)
            {
                // payload complete
                switch (header[1])
                {
                case OPC_SET_ARGB:
                    layer_blit(l, header[0], (rgbaPixel *)&(payload), payload_length / 4);
                    break;
                case OPC_SYSTEM_EXCLUSIVE:
                    if (((payload[0] << 8) | payload[1]) == OPC_SYSTEM_IDENTIFIER)
                    {
                        // reorder layers or whatever
                        break;
                    }
                default:
                    // send to the destination server directly
                    break;
                }
                header_length = 0;
                payload_length = 0;
            }
        }
    }
    close(l->sock);
    l->sock = -1;
    fputs("client closed connection\n", stderr);
    pthread_mutex_lock(&dirty_mutex);
    dirty = true;
    pthread_cond_broadcast(&dirty_cv);
    pthread_mutex_unlock(&dirty_mutex);
    return 0;
}
