#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "boblight.h"

//TODO: ARM NEON/x86 SSE for SIMD optimizations

rgbaPixel composited[MAX_PIXELS_PER_LAYER];
layer layers[MAX_CLIENTS];

/*static void layer_repr()
{
    printf("composited");
    for (int i = 0; i < MAX_PIXELS_PER_LAYER; i++)
    {
        if (i >= 5)
        {
            printf(", ...");
            break;
        }
        printf(", %02x %02x %02x %02x", composited[i].r, composited[i].g, composited[i].b, composited[i].a);
    }
    printf("\n");
}*/

layer_handle layer_init()
{
    layer_handle l;
    for (l = 0; layers[l] != NULL; l++)
        ;
    layers[l] = malloc(MAX_PIXELS_PER_LAYER * sizeof(rgbaPixel));
    memset(layers[l], 0, MAX_PIXELS_PER_LAYER * sizeof(rgbaPixel));
    return l;
}

void layer_destroy(layer_handle l)
{
    free(layers[l]);
    layers[l] = NULL;
}

void layer_blit(layer_handle lh, rgbaPixel *src, int length)
{
    if (layers[lh] == NULL)
    {
        return;
    }
    int i;
    for (i = 0; i < MAX_PIXELS_PER_LAYER; i++)
    {
        if (length >= 4)
        {
            layers[lh][i].r = src[i].r * src[i].a;
            layers[lh][i].g = src[i].g * src[i].a;
            layers[lh][i].b = src[i].b * src[i].a;
            layers[lh][i].a = src[i].a;
            length -= 4;
        }
        else
        {
            layers[lh][i].r = 0;
            layers[lh][i].g = 0;
            layers[lh][i].b = 0;
            layers[lh][i].a = 0;
        }
    }
    layer_composite();
    //layer_repr();
}

void layer_composite()
{
    for (int i = 0; i < MAX_PIXELS_PER_LAYER; i++)
    {
        composited[i].r = config.background.r;
        composited[i].g = config.background.g;
        composited[i].b = config.background.b;
        composited[i].a = 255;
    }
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (layers[i] == NULL)
        {
            continue;
        }
        rgbaPixel *src;
        rgbaPixel *dst;
        for (int p = 0; p < MAX_PIXELS_PER_LAYER; p++)
        {
            src = &layers[i][p];
            dst = &composited[p];
            dst->r = src->r + (dst->r * (255 - src->a) >> 8);
            dst->g = src->g + (dst->g * (255 - src->a) >> 8);
            dst->b = src->b + (dst->b * (255 - src->a) >> 8);
            dst->a = src->a + (dst->a * (255 - src->a) >> 8);
        }
    }
}