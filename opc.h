#pragma once

#include <netinet/in.h>
#include <stdint.h>
#include "options.h"
#include "layer.h"

#define OPC_SYSTEM_IDENTIFIER 0xB0B

/* OPC command to set pixels to RGB values. */
#define OPC_SET_PIXELS 0

/* OPC command to set (virtual) pixels to RGBA values. */
#define OPC_SET_ARGB 2

/* BamboozLED SysEx commands */
#define COMMAND_MOVETOFRONT 0
#define COMMAND_MOVETOBACK 1
#define COMMAND_MOVEUP 2
#define COMMAND_MOVEDOWN 3

/* SysEx OPC command; implementation varies by device. BamboozLED uses this for
   moving layers up and down. */
#define OPC_SYSTEM_EXCLUSIVE 255

/* Listen on a specified interface and port, dispatching new threads to handle
   clients. */
void opc_serve(in_addr_t host, uint16_t port);

/* Called by opc_serve() when instantiating a new thread. Listens in the port
   specified in the layer and blits sent pixels to that layer's buffer until
   the client disconnects. */
void *opc_receive(layer *l);

/* Timeout in milliseconds for sending data to the destination server. If a
   frame cannot send it will automatically attempt to reconnect after this
   much time as well. */
#define OPC_SEND_TIMEOUT 1000

/* Resolve a hostname or IP address to send pixels to based on the information
   in info->dest. After resolution, a socket can be opened with opc_connect(). */
void opc_resolve(bamboozled_address *info);

/* Makes one attempt to open the connection for a sink if needed, timing out
   after timeout_ms.  Returns 1 if connected, 0 if the timeout expired. */
bool opc_connect(bamboozled_address *info, uint32_t timeout_ms);

/* Sends data to a sink, making at most one attempt to open the connection
   if needed and waiting at most timeout_ms for each I/O operation.  Returns
   1 if all the data was sent, 0 otherwise. */
bool opc_send(bamboozled_address *info, const uint8_t *data, ssize_t len, uint32_t timeout_ms);

/* Sends RGB data for 'count' pixels to channel 'channel'.  Makes one attempt
   to connect the sink if needed; if the connection could not be opened, then
   the data is not sent.  Returns true if the data was sent, false otherwise. */
bool opc_put_pixels(bamboozled_address *info, uint8_t channel, uint16_t count, rgbPixel *pixels);