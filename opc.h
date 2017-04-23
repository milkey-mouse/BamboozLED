#pragma once

#include "bamboozled.h"

// OPC command codes
#define OPC_SET_PIXELS 0
#define OPC_SET_ARGB 2
#define OPC_SYSTEM_EXCLUSIVE 255

bool opc_listen(layer *l);

uint8_t opc_receive(layer *l, uint32_t timeout_ms);