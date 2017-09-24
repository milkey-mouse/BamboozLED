#include "bamboozled.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "options.h"
#include "opc.h"

// 0x100007f = 127.0.0.1
bamboozled_config config = {
    {0x100007f, 7891, NULL, NULL}, // listen
    {0x100007f, 7890, NULL, NULL}, // destination
    {0, 0, 0}                      // background
};

static void *opc_server_wrapper()
{
    opc_serve(config.listen.host, config.listen.port);
    return 0;
}

int main(int argc, char **argv)
{
    parse_args(argc, argv);
    // check if any destinations loop back to the listen port
    for (bamboozled_address *dest = &config.destination; dest != NULL; dest = dest->next)
    {
        if (config.listen.host == dest->host && config.listen.port == dest->port)
        {
            puts("listen and destination addresses must not be the same");
            return 1;
        }
    }
    printf("BamboozLED v. %s\n", VERSION);

    pthread_cond_init(&dirty_cv, NULL);
    pthread_mutex_init(&dirty_mutex, NULL);

    fflush(stdout);
    for (bamboozled_address *dest = &config.destination; dest != NULL; dest = dest->next)
    {
        if (!opc_connect(dest, OPC_SEND_TIMEOUT))
        {
            return 1;
        }
    }

    pthread_t server_thread;
    if (pthread_create(&server_thread, NULL, opc_server_wrapper, NULL) < 0)
    {
        perror("could not create server thread: ");
        return 1;
    }

    uint32_t dirty_main[8];
    memset(dirty_main, 0, sizeof(dirty_main));
    while (1)
    {
        pthread_mutex_lock(&dirty_mutex);
        while (memcmp(dirty, dirty_main, sizeof(dirty)) == 0)
        {
            pthread_cond_wait(&dirty_cv, &dirty_mutex);
        }
        memcpy(dirty_main, dirty, sizeof(dirty));
        memset(dirty, 0, sizeof(dirty));
        pthread_mutex_unlock(&dirty_mutex);
        uint8_t d = 0;
        for (int i = 0; i < sizeof(dirty_main) / sizeof(dirty_main[0]); i++)
        {
            for (int j = 0; j < (2 << sizeof(dirty_main[0])); j++)
            {
                if (d == 255)
                {
                    break;
                }
                d++;
                if (dirty_main[i] & (1 << j))
                {
                    layer_composite(d);
                }
            }
        }
        for (bamboozled_address *dest = &config.destination; dest != NULL; dest = dest->next)
        {
            d = 0;
            for (int i = 0; i < sizeof(dirty_main) / sizeof(dirty_main[0]); i++)
            {
                for (int j = 0; j < (2 << sizeof(dirty_main[0])); j++)
                {
                    if (d == 255)
                    {
                        break;
                    }
                    d++;
                    if (dirty_main[i] & (1 << j))
                    {
                        layer_send(dest, d);
                    }
                }
            }
        }
        memset(dirty_main, 0, sizeof(dirty_main));
    }
    return 0;
}
