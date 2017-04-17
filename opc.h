/* Copyright 2013 Ka-Ping Yee

Licensed under the Apache License, Version 2.0 (the "License"); you may not
use this file except in compliance with the License.  You may obtain a copy
of the License at: http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied.  See the License for the
specific language governing permissions and limitations under the License. */

// Open Pixel Control, a protocol for controlling arrays of RGB lights.
#pragma once

/* RGB tuple representing a pixel */
typedef struct
{
    uint8_t r, g, b;
} rgbPixel;

/* premultiplied RGBA tuplet representing a pixel */
typedef struct
{
    uint8_t r, g, b, a;
} rgbaPixel;

/* OPC broadcast channel */
#define OPC_BROADCAST 0

/* OPC command codes */
#define OPC_SET_PIXELS 0
#define OPC_SET_ARGB 2
#define OPC_SYSTEM_EXCLUSIVE 255

// OPC client functions ----------------------------------------------------

/* Handle for an OPC sink created by opc_new_sink. */
typedef int8_t opc_sink;

/* Creates a new OPC sink.  hostport should be in "host" or "host:port" form. */
/* No TCP connection is attempted yet; the connection will be automatically */
/* opened as necessary by opc_put_pixels, and reopened if it closes. */
opc_sink opc_new_sink(char *hostport);

/* Sends RGB data for 'count' pixels to channel 'channel'.  Makes one attempt */
/* to connect the sink if needed; if the connection could not be opened, the */
/* the data is not sent.  Returns 1 if the data was sent, 0 otherwise. */
uint8_t opc_put_pixels(opc_sink sink, uint8_t channel, uint16_t count, rgbPixel *pixels);

// OPC server functions ----------------------------------------------------

/* Handle for an OPC source created by opc_new_source. */
typedef int8_t opc_source;

/* Creates a new OPC source by listening on the specified TCP port.  At most */
/* one incoming connection is accepted at a time; if the connection closes, */
/* the next call to opc_receive will begin listening for another connection. */
opc_source opc_new_source(uint16_t port);

/* Handles the next I/O event for a given OPC source; if incoming data is */
/* received that completes a pixel data packet, calls the handler with the */
/* pixel data.  Returns 1 if there was any I/O, 0 if the timeout expired. */
uint8_t opc_receive(opc_source source, uint32_t timeout_ms);

/* Resets an OPC source to its initial state by closing the connection. */
void opc_reset_source(opc_source source);