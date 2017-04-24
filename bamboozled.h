#pragma once

#include <netinet/in.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdint.h>

#define VERSION "0.1"
#define OPC_SYSTEM_IDENTIFIER 0xB0B

#define JSMN_STRICT

// RGB tuple representing a pixel
typedef struct
{
    uint8_t r, g, b;
} rgbPixel;

// premultiplied RGBA tuplet representing a pixel
typedef struct
{
    uint8_t r, g, b, a;
} rgbaPixel;

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

typedef struct layer
{
    struct layer *prev;
    struct layer *next;
    uint16_t channelLengths[254];
    rgbaPixel *channels[254];
    int sock;
} layer;

layer *layer_init();
void layer_unlink(layer *l);
void layer_moveToFront(layer *l);
void layer_moveToBack(layer *l);
void layer_destroy(layer *l);

void layer_blit(layer *l, uint8_t channel, rgbaPixel *src, int length);
void layer_composite();
void layer_repr(uint8_t c);

bool dirty;
pthread_cond_t dirty_cv;
pthread_mutex_t dirty_mutex;