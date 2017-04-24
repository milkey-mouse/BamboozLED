#pragma once

#include "bamboozled.h"

// OPC command codes
#define OPC_SET_PIXELS 0
#define OPC_SET_ARGB 2
#define OPC_SYSTEM_EXCLUSIVE 255

void opc_serve(in_addr_t host, uint16_t port);
void *opc_receive(layer *l);