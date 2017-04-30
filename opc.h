#pragma once

#include <netinet/in.h>
#include <stdint.h>
#include "layer.h"

#define OPC_SYSTEM_IDENTIFIER 0xB0B

/* OPC command to set pixels to RGB values. */
#define OPC_SET_PIXELS 0

/* OPC command to set (virtual) pixels to RGBA values. */
#define OPC_SET_ARGB 2

/* SysEx OPC command; implementation varies by device.
   BamboozLED uses this for moving layers up and down. */
#define OPC_SYSTEM_EXCLUSIVE 255

/* Maximum number of RGBA pixels per message packet. */
#define MAX_PIXELS_PER_LAYER ((1 << 16) / sizeof(rgbaPixel))

/* Listen on a specified interface and port, dispatching
   new threads to handle clients. */
void opc_serve(in_addr_t host, uint16_t port);

/* Called by opc_serve() when instantiating a new thread.
   Listens in the port specified in the layer and blits
   sent pixels to that layer's buffer until the client
   disconnects. */
void *opc_receive(layer *l);
