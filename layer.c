#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include "bamboozled.h"
#include "opc.h"

rgbaPixel composited[254][MAX_PIXELS_PER_LAYER];
uint16_t maxPixelsSent = 0;

layer *head;
layer *tail;

void layer_repr(uint8_t c)
{
    printf("channel %hhu", c);
    for (int i = 0; i < maxPixelsSent; i++)
    {
        if (i >= 5)
        {
            printf(", ...");
            break;
        }
        printf(", [%03hhu, %03hhu, %03hhu, %03hhu]", composited[c][i].r, composited[c][i].g, composited[c][i].b, composited[c][i].a);
    }
    printf("\n");
}

layer *layer_init(uint16_t port)
{
    layer *l = malloc(sizeof(layer));
    memset(l, 0, sizeof(layer));
    if (head == NULL)
    {
        head = l;
        tail = l;
    }
    else
    {
        tail->next = l;
        l->prev = tail;
    }
    l->port = port;
    if (opc_listen(l))
    {
        fprintf(stderr, "Listening on port %d\n", l->port);
        return l;
    }
    else
    {
        return NULL;
    }
}

void layer_unlink(layer *l)
{
    if (l == head && l == tail) {
        head = NULL;
        tail = NULL;
    }
    else if (l == head)
    {
        head = l->next;
    }
    else if (l == tail)
    {
        tail = l->prev;
    }
    else
    {
        l->prev->next = l->next;
        l->next->prev = l->prev;
    }
}

void layer_moveToFront(layer *l)
{
    if (head != l)
    {
        layer_unlink(l);
        l->prev = NULL;
        l->next = head;
        head = l;
    }
}

void layer_moveToBack(layer *l)
{
    if (tail != l)
    {
        layer_unlink(l);
        l->prev = tail;
        l->next = NULL;
        tail = l;
    }
}

void layer_destroy(layer *l)
{
    if (l->sock >= 0)
    {
        close(l->sock);
    }
    l->sock = -1;

    layer_unlink(l);

    for (int c = 0; c < 254; c++)
    {
        if (l->channels[c] != NULL)
        {
            free(l->channels[c]);
        }
    }
    free(l);
}

void layer_blit(layer *l, uint8_t channel, rgbaPixel *src, int length)
{
    if (length == 0)
    {
        return;
    }
    if (channel == 0)
    {
        layer_blit(l, 1, src, length);
        for (int i = 1; i < 254; i++)
        {
            if (length > l->channelLengths[i])
            {
                l->channels[i] = realloc(l->channels[i], length * sizeof(rgbaPixel));
            }
            memcpy(l->channels[i], l->channels[0], length);
        }
    }
    else
    {
        channel--;
        if (length > l->channelLengths[channel])
        {
            l->channels[channel] = realloc(l->channels[channel], length * sizeof(rgbaPixel));
            l->channelLengths[channel] = length;
            if (maxPixelsSent < length)
            {
                maxPixelsSent = length;
            }
        }
        for (int i = 0; i < length; i++)
        {
            l->channels[channel][i].r = ((src[i].r * src[i].a + 1) * 257) >> 16;
            l->channels[channel][i].g = ((src[i].g * src[i].a + 1) * 257) >> 16;
            l->channels[channel][i].b = ((src[i].b * src[i].a + 1) * 257) >> 16;
            l->channels[channel][i].a = src[i].a;
        }
    }
    layer_composite();
    layer_repr(channel);
}

void layer_composite()
{
    //TODO: ARM NEON/x86 SSE for SIMD optimizations
    for (int c = 0; c < 254; c++)
    {
        for (int i = 0; i < maxPixelsSent; i++)
        {
            composited[c][i].r = config.background.r;
            composited[c][i].g = config.background.g;
            composited[c][i].b = config.background.b;
            composited[c][i].a = 255;
        }
        for (layer *l = head; l != NULL; l = l->next)
        {
            for (int p = 0; p < l->channelLengths[c]; p++)
            {
                composited[c][p].r = l->channels[c][p].r + (((composited[c][p].r * (255 - l->channels[c][p].a) + 1) * 257) >> 16);
                composited[c][p].g = l->channels[c][p].g + (((composited[c][p].g * (255 - l->channels[c][p].a) + 1) * 257) >> 16);
                composited[c][p].b = l->channels[c][p].b + (((composited[c][p].b * (255 - l->channels[c][p].a) + 1) * 257) >> 16);
                composited[c][p].a = l->channels[c][p].a + (((composited[c][p].a * (255 - l->channels[c][p].a) + 1) * 257) >> 16);
            }
        }
    }
}