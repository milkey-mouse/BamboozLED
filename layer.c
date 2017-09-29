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

rgbArray composited[255];
pthread_mutex_t composited_mutex;

layer *head;
layer *tail;
pthread_mutex_t layers_mutex;

uint32_t dirty[8];
pthread_cond_t dirty_cv;
pthread_mutex_t dirty_mutex;

void layer_repr(uint8_t c)
{
    printf("channel %hhu", c);
    for (int i = 0; i < composited[c].length; i++)
    {
        if (i >= 5)
        {
            printf(", ...");
            break;
        }
        printf(", [%03hhu, %03hhu, %03hhu]", composited[c].pixels[i].r, composited[c].pixels[i].g, composited[c].pixels[i].b);
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

    for (int c = 0; c < 255; c++)
    {
        if (l->channels[c].pixels != NULL)
        {
            free(l->channels[c].pixels);
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
        tail->next = l;
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
        head->prev = l;
        head = l;
    }
    pthread_mutex_unlock(&layers_mutex);
}

void layer_moveUp(layer *l)
{
    pthread_mutex_lock(&layers_mutex);
    if (tail != l)
    {
        l->prev = l->prev->prev;
        l->next = l->prev;
        l->next->prev = l;
        l->prev->next = l;
    }
    // move towards tail
    pthread_mutex_unlock(&layers_mutex);
}

void layer_moveDown(layer *l)
{
    pthread_mutex_lock(&layers_mutex);
    if (head != l)
    {
        l->next = l->next->next;
        l->prev = l->next;
        l->prev->next = l;
        l->next->prev = l;
    }
    // move towards head
    pthread_mutex_unlock(&layers_mutex);
}

void layer_blit(layer *l, uint8_t channel, pixArray src, int length, bool alpha)
{
    if (length == 0)
    {
        return;
    }
    if (channel == 0)
    {
        layer_blit(l, 1, src, length, alpha);
        for (int i = 1; i < 255; i++)
        {
            if (length > l->channels[i].length)
            {
                if (length > composited[channel].length)
                {
                    pthread_mutex_lock(&composited_mutex);
                    composited[channel].pixels = realloc(composited[channel].pixels, length * sizeof(rgbPixel));
                    composited[channel].length = length;
                    pthread_mutex_unlock(&composited_mutex);
                }
                l->channels[i].pixels = realloc(l->channels[i].pixels, length * sizeof(rgbaPixel));
                l->channels[i].length = length;
            }
            memcpy(l->channels[i].pixels, l->channels[0].pixels, length);
        }
        pthread_mutex_lock(&dirty_mutex);
        memset(dirty, 0xFF, sizeof(dirty));
        pthread_cond_broadcast(&dirty_cv);
        pthread_mutex_unlock(&dirty_mutex);
    }
    else
    {
        channel--;
        if (length > l->channels[channel].length)
        {
            if (length > composited[channel].length)
            {
                pthread_mutex_lock(&composited_mutex);
                composited[channel].pixels = realloc(composited[channel].pixels, length * sizeof(rgbPixel));
                composited[channel].length = length;
                pthread_mutex_unlock(&composited_mutex);
            }
            l->channels[channel].pixels = realloc(l->channels[channel].pixels, length * sizeof(rgbaPixel));
            l->channels[channel].length = length;
        }
        if (alpha)
        {
            for (int i = 0; i < length; i++)
            {
                l->channels[channel].pixels[i].r = ((src.rgba[i].r * src.rgba[i].a + 1) * 257) >> 16;
                l->channels[channel].pixels[i].g = ((src.rgba[i].g * src.rgba[i].a + 1) * 257) >> 16;
                l->channels[channel].pixels[i].b = ((src.rgba[i].b * src.rgba[i].a + 1) * 257) >> 16;
                l->channels[channel].pixels[i].a = src.rgba[i].a;
            }
        }
        else
        {
            for (int i = 0; i < length; i++)
            {
                l->channels[channel].pixels[i].r = src.rgb[i].r;
                l->channels[channel].pixels[i].g = src.rgb[i].g;
                l->channels[channel].pixels[i].b = src.rgb[i].b;
                l->channels[channel].pixels[i].a = 255;
            }
        }
        pthread_mutex_lock(&dirty_mutex);
        dirty[channel >> 5] |= (1 << (channel & 31));
        pthread_cond_broadcast(&dirty_cv);
        pthread_mutex_unlock(&dirty_mutex);
    }
}

void layer_composite(uint8_t c)
{
    //TODO: ARM NEON/x86 SSE for SIMD optimizations
    if (c == 0)
    {
        for (c = 1; c <= 255; c++)
        {
            layer_composite(c);
        }
    }
    else
    {
        c--;

        pthread_mutex_lock(&composited_mutex);
        rgbPixel *comp = composited[c].pixels;
        for (int i = 0; i < composited[c].length; i++)
        {
            composited[c].pixels[i].r = config.background.r;
            composited[c].pixels[i].g = config.background.g;
            composited[c].pixels[i].b = config.background.b;
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

            rgbaPixel *chan = l->channels[c].pixels;
            for (int p = 0; p < l->channels[c].length; p++)
            {
                comp[p].r = chan[p].r + (((comp[p].r * (255 - chan[p].a) + 1) * 257) >> 16);
                comp[p].g = chan[p].g + (((comp[p].g * (255 - chan[p].a) + 1) * 257) >> 16);
                comp[p].b = chan[p].b + (((comp[p].b * (255 - chan[p].a) + 1) * 257) >> 16);
            }
        }
        pthread_mutex_unlock(&layers_mutex);
        pthread_mutex_unlock(&composited_mutex);
    }
}

void layer_send(bamboozled_address *dest, uint8_t c)
{
    if (c == 0)
    {
        for (int i = 1; i <= 255; i++)
        {
            opc_put_pixels(dest, i, composited[i - 1].length, composited[i - 1].pixels);
        }
    }
    else
    {
        opc_put_pixels(dest, c, composited[c - 1].length, composited[c - 1].pixels);
    }
}