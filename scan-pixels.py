#!/usr/bin/python3

from itertools import chain
import time
import opc

client = opc.Client("localhost:7891", verbose=True)
if client.can_connect():
    for i in chain(range(5), range(3, -1, -1)):
        arr = [(0, 0, 0, 0)] * 5
        arr[i] = (255, 255, 255, 255)
        client.put_pixels(arr, channel=1)
        time.sleep(0.25)
else:
    print("Can't connect to server")
