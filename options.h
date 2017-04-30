#pragma once

#include <netinet/in.h>
#include <stdint.h>
#include "layer.h"

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
