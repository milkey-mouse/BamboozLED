# BamboozLED

BamboozLED is a compositor for [Open Pixel Control](http://openpixelcontrol.org/) servers. It proxies the commands to another OPC server while adding one additional command that accepts RGBA pixels instead of plain RGB. This allows multiple clients to connect and push pixels at the same time with BamboozLED blending each buffer together and sending them to the destination server.

# Installation

    $ git clone https://github.com/milkey-mouse/bamboozled
    $ cd bamboozled
    $ make
    # make install

## Running on startup

### Config file

Create a config file at `/etc/bamboozled.json`:

    {
        "listen": ["127.0.0.1", 7891],
        "destination": ["127.0.0.1", 7890],
        "background": [0, 0, 0]
    }

`listen` specifies which address & port to listen on. By default it only allows connections from `localhost`, but by changing `127.0.0.1` to `null` or `0.0.0.0` you can allow connections from any IP on your local network.

`destination` specifies the address & port of the "target" OPC server that BamboozLED sends the composited pixels to. By default it is `127.0.0.1:7890`. `destination` can also be a list of destinations, and BamboozLED will forward OPC commands to all of them.

`background` is the color "beneath" all the dyamic layers; that is, if no clients are connected or a pixel is transparent, this background color will show through. (If the lights are supposed to actually light up the room as well as looking cool, it is recommended to set it to `[255, 255, 255]` (white) so they provide light when no clients are connected.)

### `systemd` unit file

If your system uses `systemd`, `make install` automatically added a `systemd` unit file that can be used to automatically start BamboozLED. Enable it to run on startup with the following command:

    # systemctl enable bamboozled.service

# Layers

The first (bottommost) layer is the background. The initial layer order is determined by connection order: each client to connect to the server gets a new layer on top of all the others. Layers can be reordered, however: see [Moving layers](#moving-layers). BamboozLED will [composite](https://en.wikipedia.org/wiki/Alpha_compositing) layers on top of each other.

# OPC API

### Sending pixel data

In order to preserve compatibility with existing programs that expect to set pixels with RGB triplets, BamboozLED uses OPC command 2 ([0](http://openpixelcontrol.org) and [1](https://github.com/zestyping/openpixelcontrol/issues/40) are in use) for sending RGBA pixels. The command functions the exact same as the existing command 0 (send RGB pixels) but expects 4 bytes per pixel instead of 3:

| channel | command | length (n) |          | data                             |
|---------|---------|------------|----------|----------------------------------|
| 0-255   | 2       | high byte  | low byte | `n` bytes of message data (RGBA) |

### Moving layers

`SysEx` (command 255) is also captured and used for moving layers up and down:

| channel | command | length |   | system ID | BamboozLED command |
|---------|---------|--------|---|-----------|--------------------|
| any     | 255     | 0      | 3 | `0xB0B`   | 0-3                |

The channel number doesn't matter because a layer encompasses all channels sent by that client. The BamboozLED command can be any of the following:

| BamboozLED command | Action               |
| -------------------|----------------------|
| 0                  | move layer to front  |
| 1                  | move layer to back   |
| 2                  | move layer up by 1   |
| 3                  | move layer down by 1 |

Any other OPC commands will be passed through as-is to the destination OPC server.