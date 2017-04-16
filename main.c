#include <stdint.h>
#include <stdio.h>

#include "bamboozled.h"
#include "opc.h"

bamboozled_config config = {
    {"127.0.0.1", 7891}, // listen
    {"127.0.0.1", 7890}, // destination
    {0, 0, 0}            // background
};

int main(int argc, char **argv)
{
    parse_args(argc, argv);
    printf("bamboozled v. %s\n", VERSION);
    opc_source s = opc_new_source(config.listen.port);
    while (s >= 0)
    {
        opc_receive(s, 10000);
    }
    return 0;
}