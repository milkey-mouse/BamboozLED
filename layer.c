#include "bamboozled.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "options.h"
#include "layer.h"
#include "opc.h"

rgbPixel composited[254][MAX_PIXELS];
uint16_t maxPixelsSent = 0;

layer *head;
layer *tail;
pthread_mutex_t layers_mutex;

uint64_t dirty[4];
pthread_cond_t dirty_cv;
pthread_mutex_t dirty_mutex;

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
        printf(", [%03hhu, %03hhu, %03hhu]", composited[c][i].r, composited[c][i].g, composited[c][i].b);
    }
    printf("\n");
}

layer *layer_init()
{
    layer *l = malloc(sizeof(layer));
    memset(l, 0, sizeof(layer));
    pthread_mutex_lock(&layers_mutex);
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
    pthread_mutex_unlock(&layers_mutex);
    return l;
}

void layer_unlink(layer *l)
{
    if (l == head && l == tail)
    {
        head = NULL;
        tail = NULL;
    }
    else if (l == head)
    {
        head = l->next;
        l->next->prev = NULL;
    }
    else if (l == tail)
    {
        tail = l->prev;
        l->prev->next = NULL;
    }
    else
    {
        l->prev->next = l->next;
        l->next->prev = l->prev;
    }
}

void layer_destroy(layer *l)
{
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

void layer_moveToFront(layer *l)
{
    pthread_mutex_lock(&layers_mutex);
    if (tail != l)
    {
        layer_unlink(l);
        l->prev = tail;
        l->next = NULL;
        tail = l;
    }
    pthread_mutex_unlock(&layers_mutex);
}

void layer_moveToBack(layer *l)
{
    pthread_mutex_lock(&layers_mutex);
    if (head != l)
    {
        layer_unlink(l);
        l->prev = NULL;
        l->next = head;
        head = l;
    }
    pthread_mutex_unlock(&layers_mutex);
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
        pthread_mutex_lock(&dirty_mutex);
        memset(dirty, 0xFF, sizeof(dirty));
        pthread_cond_broadcast(&dirty_cv);
        pthread_mutex_unlock(&dirty_mutex);
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
        pthread_mutex_lock(&dirty_mutex);
        dirty[channel >> 6] |= (1 << (channel & 127));
        pthread_cond_broadcast(&dirty_cv);
        pthread_mutex_unlock(&dirty_mutex);
    }
}

void layer_composite(uint8_t c)
{
    //TODO: ARM NEON/x86 SSE for SIMD optimizations
    if (c == 0)
    {
        pthread_mutex_lock(&layers_mutex);
        for (layer *l = head; l != NULL; l = l->next)
        {
            if (l->sock == -1)
            {
                layer *tmp = l->next;
                layer_destroy(l);
                if (tmp == NULL)
                {
                    break;
                }
                l = tmp;
            }

            for (int c = 0; c < 254; c++)
            {
                rgbPixel *comp = composited[c];
                for (int i = 0; i < maxPixelsSent; i++)
                {
                    comp[i].r = config.background.r;
                    comp[i].g = config.background.g;
                    comp[i].b = config.background.b;
                }

                rgbaPixel *chan = l->channels[c];
                for (int p = 0; p < l->channelLengths[c]; p++)
                {
                    comp[p].r = chan[p].r + (((comp[p].r * (255 - chan[p].a) + 1) * 257) >> 16);
                    comp[p].g = chan[p].g + (((comp[p].g * (255 - chan[p].a) + 1) * 257) >> 16);
                    comp[p].b = chan[p].b + (((comp[p].b * (255 - chan[p].a) + 1) * 257) >> 16);
                }
            }
        }
        pthread_mutex_unlock(&layers_mutex);
    }
    else
    {
        c--;

        rgbPixel *comp = composited[c];
        for (int i = 0; i < maxPixelsSent; i++)
        {
            comp[i].r = config.background.r;
            comp[i].g = config.background.g;
            comp[i].b = config.background.b;
        }

        pthread_mutex_lock(&layers_mutex);
        for (layer *l = head; l != NULL; l = l->next)
        {
            if (l->sock == -1)
            {
                layer *tmp = l->next;
                layer_destroy(l);
                if (tmp == NULL)
                {
                    break;
                }
                l = tmp;
            }

            rgbaPixel *chan = l->channels[c];
            for (int p = 0; p < l->channelLengths[c]; p++)
            {
                comp[p].r = chan[p].r + (((comp[p].r * (255 - chan[p].a) + 1) * 257) >> 16);
                comp[p].g = chan[p].g + (((comp[p].g * (255 - chan[p].a) + 1) * 257) >> 16);
                comp[p].b = chan[p].b + (((comp[p].b * (255 - chan[p].a) + 1) * 257) >> 16);
            }
        }
        pthread_mutex_unlock(&layers_mutex);
    }
}

void layer_send(bamboozled_address *dest, uint8_t c)
{
    if (c == 0)
    {
        for (int i = 0; i < 255; i++)
        {
            opc_put_pixels(dest, c, maxPixelsSent, composited[c]);
        }
    }
    else
    {
        opc_put_pixels(dest, c - 1, maxPixelsSent, composited[c]);
    }
}