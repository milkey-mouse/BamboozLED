#pragma once

#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include "opc.h"

#define VERSION "0.1"
#define OPC_SYSTEM_IDENTIFIER 0xB0B

#define MAX_CLIENTS 64

#define JSMN_STRICT

// Maximum possible number of pixels per message packet
#define MAX_PIXELS_PER_LAYER ((1 << 16) / sizeof(rgbaPixel))

typedef struct bamboozled_address
{
    in_addr_t host;
    uint16_t port;
} bamboozled_address;

typedef struct bamboozled_config
{
    bamboozled_address listen;
    bamboozled_address destination;
    rgbPixel background;
} bamboozled_config;

bamboozled_config config;
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
void layer_repr(uint8_t c);