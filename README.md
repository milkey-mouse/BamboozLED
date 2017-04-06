# `boblight`

`boblight` is a compositor for [Open Pixel Control](http://openpixelcontrol.org/) servers. It proxies the commands to another OPC server while adding one additional command that accepts RGBA pixels instead of plain RGB. This allows multiple clients to connect and push pixels at the same time with `boblight` blending each buffer together and sending them to the target server.

# Installation

    git clone --recursive https://github.com/milkey-mouse/boblight; cd boblight
    make && sudo make install

## Running on startup

### Config file

Create a config file for `boblight` at `/etc/boblight.json`:
    
    {
        "listen": ["127.0.0.1", 80],
        "destination": ["127.0.0.1", "7890"],
        "opcCompat": true,
        "pixelCount": 512
    }

`listen` specifies which address & port to listen on. By default it only allows connections from `localhost`, but by changing `127.0.0.1` to `null` or `0.0.0.0` you can allow connections from any IP on your local network.

`destination` specifies the address & port of the "target" OPC server that `boblight` sends the composited pixels to. By default it is assumed that you are running the server on the same machine as `boblight` on port `7890`.

`opcCompat` specifies if `boblight` should attempt to preserve compatibility with existing OPC clients (see [OPC compatibility](#opc-compatibility)). If set to `false`, `boblight` will function the same as a normal OPC server, except expecting 4 bytes (RGBA) per LED instead of the standard 3 (RGB).

### `systemd` unit file

If your distribution uses [`systemd`](https://en.wikipedia.org/wiki/Systemd)  (most of them at this point), `make install` automatically added a `systemd` unit file that can be used to automatically start `boblight`. Enable it to run on startup with the following command:

    sudo systemctl enable boblight.service

# Layers

The first (bottommost) layer is Layer 0; this layer can be set to either all white or all black. The layer order is determined by connection order: each client to connect to the server gets a new layer that is, by default, on top of all the others.

`boblight` will blend pixels with alpha values less than 255 with the colors of the corresponding pixels on the layers below them.

# OPC compatiblity

*This does not apply when `opcCompat` is set to `false`.*

In order to preserve compatibility with existing programs that expect to set pixels with RGB triplets, `boblight` uses OPC command 255 (SysEx) for sending RGBA pixels. `boblight` looks for the system ID `0xB0B` and processes data with the following format:

| channel | command | length (n) |          | system identifier (2 bytes) | data                      |
|---------|---------|------------|----------|-----------------------------|---------------------------|
| 0-255   | 255     | high byte  | low byte | `0xB0B` (big endian)        | `n` bytes of message data |

Any other OPC commands (and any SysEx commands with a system identifier other than `0xB0B`) will be passed through directly to the destination OPC server.