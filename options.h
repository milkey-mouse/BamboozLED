#pragma once

#include <netinet/in.h>
#include <stdint.h>
#include <time.h>

#ifndef PIXELS
typedef struct rgbPixel
{
    uint8_t r, g, b;
} rgbPixel;

typedef struct rgbaPixel
{
    uint8_t r, g, b, a;
} rgbaPixel;
#define PIXELS
#endif

typedef struct opc_sink
{
    int sock;
    struct timespec timeout_end;
    char hostname[];
} opc_sink;

typedef struct bamboozled_address
{
    in_addr_t host;
    uint16_t port;
    opc_sink *dest;
    struct bamboozled_address *next;
} bamboozled_address;

typedef struct bamboozled_config
{
    bamboozled_address listen;
    bamboozled_address destination;
    rgbPixel background;
} bamboozled_config;

bamboozled_config config;
void parse_args(int argc, char **argv);
