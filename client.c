/* Copyright 2013 Ka-Ping Yee (Modified by Milkey Mouse)

Licensed under the Apache License, Version 2.0 (the "License"); you may not
use this file except in compliance with the License. You may obtain a copy
of the License at: http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied. See the License for the
specific language governing permissions and limitations under the License. */

#include "bamboozled.h"
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <time.h>
#include "opc.h"

void opc_resolve(bamboozled_address *info)
{
    struct addrinfo *ai;
    struct addrinfo *addr;
    int err = getaddrinfo(info->dest->hostname, NULL, NULL, &addr);
    if (err == 0)
    {
        for (ai = addr; ai != NULL; ai = ai->ai_next)
        {
            if (ai->ai_family == AF_INET)
            {
                memcpy(&info->host, &((struct sockaddr_in *)(addr->ai_addr))->sin_addr, sizeof(in_addr_t));
                freeaddrinfo(addr);
                return;
            }
        }
        fprintf(stderr, "could not resolve address %s\n", info->dest->hostname);
    }
    else
    {
        fprintf(stderr, "could not resolve address %s: %s\n", info->dest->hostname, gai_strerror(err));
    }
    exit(1);
}

bool opc_connect(bamboozled_address *info, uint32_t timeout_ms)
{
    // check if we are waiting for a previous timeout
    struct timespec curtime;
    clock_gettime(CLOCK_MONOTONIC, &curtime);
    if (curtime.tv_sec < info->dest->timeout_end.tv_sec || (curtime.tv_sec == info->dest->timeout_end.tv_sec && curtime.tv_nsec < info->dest->timeout_end.tv_nsec))
    {
        return false;
    }

    // create the sockaddr we will connect() to
    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = info->host;
    dest.sin_port = htons(info->port);
    memset(&dest.sin_zero, 0, sizeof(dest.sin_zero));

    // do a non-blocking connect so we can control the timeout
    int sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    fcntl(sock, F_SETFL, O_NONBLOCK);
    if (connect(sock, (struct sockaddr *)&dest, sizeof(dest)) < 0 && errno != EINPROGRESS)
    {
        fprintf(stderr, "failed to connect to %s: ", info->dest->hostname);
        perror(NULL);
        close(sock);
        return false;
    }

    // create timeout for select()
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = timeout_ms % 1000;

    // wait for a result
    fd_set writefds;
    FD_ZERO(&writefds);
    FD_SET(sock, &writefds);
    select(sock + 1, NULL, &writefds, NULL, &timeout);
    if (FD_ISSET(sock, &writefds))
    {
        int opt_errno = 0;
        socklen_t opt_len;
        getsockopt(sock, SOL_SOCKET, SO_ERROR, &opt_errno, &opt_len);
        if (opt_errno == 0)
        {
            fprintf(stderr, "connected to destination %s:%hu\n", info->dest->hostname, info->port);
            info->dest->sock = sock;
            return true;
        }
        else
        {
            fprintf(stderr, "failed to connect to %s: %s\n", info->dest->hostname, strerror(opt_errno));
            close(sock);
            if (opt_errno == ECONNREFUSED)
            {
                // set a timer before another connection attempt
                info->dest->timeout_end.tv_sec = curtime.tv_sec + timeout.tv_sec;
                info->dest->timeout_end.tv_nsec = curtime.tv_nsec + timeout.tv_usec * 1000;
                if (info->dest->timeout_end.tv_nsec > 1000000000)
                {
                    info->dest->timeout_end.tv_nsec -= 1000000000;
                    info->dest->timeout_end.tv_sec++;
                }
            }
            return false;
        }
    }
    fprintf(stderr, "connection to %s:%hu timed out after %d ms\n", info->dest->hostname, info->port, timeout_ms);
    return false;
}

bool opc_send(bamboozled_address *info, const uint8_t *data, ssize_t len, uint32_t timeout_ms)
{
    ssize_t total_sent = 0;
    ssize_t sent;

    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = timeout_ms % 1000;
    setsockopt(info->dest->sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    while (total_sent < len)
    {
        void (*pipe_sig)(int) = signal(SIGPIPE, SIG_IGN);
        sent = send(info->dest->sock, data + total_sent, len - total_sent, 0);
        signal(SIGPIPE, pipe_sig);
        if (sent <= 0)
        {
            fputs("error sending data to ", stderr);
            perror(info->dest->hostname);
            return false;
        }
        total_sent += sent;
    }
    return true;
}

bool opc_put_pixels(bamboozled_address *info, uint8_t channel, uint16_t count, rgbPixel *pixels)
{
    if (info->dest->sock == -1 && !opc_connect(info, OPC_SEND_TIMEOUT))
    {
        return false;
    }

    uint8_t header[4];
    uint16_t len = count * 3;

    header[0] = channel;
    header[1] = OPC_SET_PIXELS;
    header[2] = len >> 8;
    header[3] = len & 0xff;
    return opc_send(info, header, 4, OPC_SEND_TIMEOUT) &&
           opc_send(info, (uint8_t *)pixels, len, OPC_SEND_TIMEOUT);
}