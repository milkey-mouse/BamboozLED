#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "bamboozled.h"

#include <stdio.h>

//TODO: ARM NEON/x86 SSE for SIMD optimizations

rgbaPixel composited[254][MAX_PIXELS_PER_LAYER];
uint16_t maxPixelsSent = 0;

layer layers[MAX_CLIENTS];

static void layer_repr(uint8_t c)
{
    printf("channel %hhu", c);
    for (int i = 0; i < MAX_PIXELS_PER_LAYER; i++)
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

layer_handle layer_init()
{
    layer_handle lh;
    for (lh = 0; layers[lh].active; lh++)
    {
        if (lh == MAX_CLIENTS)
        {
            return -1;
        }
    }
    layers[lh].active = true;
    return lh;
}

void layer_destroy(layer_handle lh)
{
    for (int c = 0; c < 254; c++)
    {
        if (layers[lh].channelLengths[c] > 0)
        {
            free(layers[lh].channels[c]);
            layers[lh].channelLengths[c] = 0;
            layers[lh].channels[c] = NULL;
        }
    }
    layers[lh].active = false;
}

void layer_blit(layer_handle lh, uint8_t channel, rgbaPixel *src, int length)
{
    if (channel == 0)
    {
        for (int i = 1; i < 254; i++)
        {
            //if (layers[lh].channels[i] != NULL)
            //{
            layer_blit(lh, i, src, length);
            //}
        }
    }
    else
    {
        channel--;
        if (length > layers[lh].channelLengths[channel])
        {
            layers[lh].channels[channel] = realloc(layers[lh].channels[channel], length * sizeof(rgbaPixel));
            layers[lh].channelLengths[channel] = length;
            if (maxPixelsSent < length)
            {
                length = maxPixelsSent;
            }
        }
        for (int i = 0; i < length; i++)
        {
            layers[lh].channels[channel][i].r = src[i].r * src[i].a >> 8;
            layers[lh].channels[channel][i].g = src[i].g * src[i].a >> 8;
            layers[lh].channels[channel][i].b = src[i].b * src[i].a >> 8;
            layers[lh].channels[channel][i].a = src[i].a;
        }
        layer_composite();
        layer_repr(channel);
    }
}

void layer_composite()
{
    for (int c = 0; c < 254; c++)
    {
        for (int i = 0; i < maxPixelsSent; i++)
        {
            composited[c][i].r = config.background.r;
            composited[c][i].g = config.background.g;
            composited[c][i].b = config.background.b;
            composited[c][i].a = 255;
        }
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (layers[i].active)
            {
                for (int p = 0; p < layers[i].channelLengths[c]; p++)
                {
                    printf("%hhu", layers[i].channelLengths[c]);
                    // TODO: add option for disabling fast blending math
                    // As of now it can only go up to 254
                    composited[c][p].r = layers[i].channels[c][p].r + (composited[c][p].r * (255 - layers[i].channels[c][p].a) >> 8);
                    composited[c][p].g = layers[i].channels[c][p].g + (composited[c][p].g * (255 - layers[i].channels[c][p].a) >> 8);
                    composited[c][p].b = layers[i].channels[c][p].b + (composited[c][p].b * (255 - layers[i].channels[c][p].a) >> 8);
                    composited[c][p].a = layers[i].channels[c][p].a + (composited[c][p].a * (255 - layers[i].channels[c][p].a) >> 8);
                }
            }
        }
    }
}