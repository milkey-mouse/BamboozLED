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

bool dirty[254];
pthread_cond_t dirty_cv;
pthread_mutex_t dirty_mutex;

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

    bool dirty_main[254];
    memset(dirty_main, 0, sizeof(dirty_main));
    while (1)
    {
        pthread_mutex_lock(&dirty_mutex);
        while (memcmp(dirty, dirty_main, sizeof(dirty)) == 0)
        {
            pthread_cond_wait(&dirty_cv, &dirty_mutex);
        }
        memcpy(dirty_main, dirty, sizeof(dirty_main));
        memset(dirty, 0, sizeof(dirty));
        pthread_mutex_unlock(&dirty_mutex);
        for (uint8_t d = 0; d < 254; d++)
        {
            if (dirty_main[d])
            {
                layer_composite(d);
            }
        }
        for (bamboozled_address *dest = &config.destination; dest != NULL; dest = dest->next)
        {
            for (int d = 0; d < 254; d++)
            {
                if (dirty_main[d])
                {
                    layer_send(dest, d + 1);
                }
            }
        }
        memset(dirty_main, 0, sizeof(dirty_main));
    }
    return 0;
}
