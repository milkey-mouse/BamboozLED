#include <stdint.h>
#include <stdio.h>

#include "boblight.h"
#include "opc.h"

bob_config config = {
    {"127.0.0.1", 7891}, // listen
    {"127.0.0.1", 7890}, // destination
    {0, 0, 0},           // background
    true                 // opcCompat
};

int main(int argc, char **argv)
{
    parse_args(argc, argv);
    printf("boblight v. %s\n", VERSION);
    opc_source s = opc_new_source(config.listen.port);
    while (s >= 0)
    {
        opc_receive(s, 10000);
    }
    return 0;
}