#include <stdint.h>
#include <stdio.h>

#include "bamboozled.h"
#include "opc.h"

// 0x100007f = 127.0.0.1
bamboozled_config config = {
    {0x100007f, 7891}, // listen
    {0x100007f, 7890}, // destination
    {0, 0, 0}          // background
};

int main(int argc, char **argv)
{
    parse_args(argc, argv);
    printf("bamboozled v. %s\n", VERSION);
    layer *l;
    while (1)
    {
        l = layer_init(config.listen.port);
        if (l == NULL)
        {
            break;
        }
        while (l->sock >= 0)
        {
            opc_receive(l, 10000);
        }
    }
    return 0;
}