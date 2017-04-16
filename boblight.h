#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "opc.h"

#define VERSION "0.1"
#define OPC_SYSTEM_IDENTIFIER 0xB0B

#define MAX_CLIENTS 64

#define JSMN_STRICT

// Maximum possible number of pixels per message packet
#define MAX_PIXELS_PER_LAYER ((1 << 16) / sizeof(rgbaPixel))

typedef struct bob_address
{
    char *host;
    uint16_t port;
} bob_address;

typedef struct bob_config
{
    bob_address listen;
    bob_address destination;
    rgbPixel background;
    bool opcCompat;
} bob_config;

bob_config config;
void parse_args(int argc, char **argv);

typedef struct layer {
    bool active;
    uint16_t channelLengths[254];
    rgbaPixel *channels[254];
} layer;

typedef int layer_handle;
layer_handle layer_init();
void layer_destroy(layer_handle lh);

void layer_blit(layer_handle lh, uint8_t channel, rgbaPixel *src, int length);
void layer_composite();