#include "bamboozled.h"
#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include "options.h"
#include "layer.h"
#include "jsmn.h"
#include "opc.h"

static void parse_address(char *str, bamboozled_address *addr, bool multiple, bool resolveHostnames)
{
    char *colon = strchr(str, ':');
    if (colon == NULL)
    {
        fputs("address must be in host:port format\n", stderr);
        exit(1);
    }
    *colon = '\0';
    if (resolveHostnames)
    {
        addr->dest = malloc(sizeof(opc_sink) + strlen(str) + 1);
        strcpy((char *)&addr->dest->hostname, str);
        memset(addr->dest, 0, sizeof(opc_sink));
        addr->dest->sock = -1;
        opc_resolve(addr);
    }
    else if (inet_pton(AF_INET, str, &addr->host) == 0)
    {
        fputs("host must be a valid IP address\n", stderr);
        exit(1);
    }

    if (!isdigit(colon[1]))
    {
        fputs("port must be a number\n", stderr);
        exit(1);
    }
    char *l;
    unsigned long p = strtoul(colon + 1, &l, 0);
    if (errno == ERANGE || p > 65535 || p == 0)
    {
        fputs("port number must be 1-65535\n", stderr);
        exit(1);
    }
    else if (*l != '\0' && (!multiple || *l != ','))
    {
        fputs("unexpected data after port number\n", stderr);
        exit(1);
    }
    else
    {
        addr->port = (uint16_t)p;
    }
    if (multiple && *l == ',')
    {
        addr->next = malloc(sizeof(bamboozled_address));
        parse_address(l + 1, addr->next, multiple, resolveHostnames);
    }
    else
    {
        addr->next = NULL;
    }
}

static void parse_color(char *s, rgbPixel *pix)
{
    char *end;
    for (int i = 0; i < 3; i++)
    {
        unsigned long p = strtoul(s, &end, 0);
        if (end == s)
        {
            fputs("r,g,b must be numbers\n", stderr);
            exit(1);
        }
        else if (i != 2 && *end != ',')
        {
            fputs("r,g,b must be 3 numbers separated by commas\n", stderr);
            exit(1);
        }
        else if (errno == ERANGE || p > 255)
        {
            fputs("r,g,b must be 0-255\n", stderr);
            exit(1);
        }
        else
        {
            ((uint8_t *)pix)[i] = p;
            s = end + 1;
        }
    }
}

static void parse_config_address(bamboozled_address *addr, jsmntok_t *tok, char *jsonStr, bool resolveHostnames)
{
    // check for [host, port]
    if (tok[0].type == JSMN_ARRAY && tok[0].size == 2)
    {
        // parse host
        switch (tok[1].type)
        {
        case JSMN_STRING:
            jsonStr[tok[1].end] = '\0';
            if (resolveHostnames)
            {
                addr->dest = malloc(sizeof(opc_sink) + strlen(jsonStr + tok[1].start) + 1);
                strcpy((char *)&addr->dest->hostname, jsonStr + tok[1].start);
                memset(addr->dest, 0, sizeof(opc_sink));
                addr->dest->sock = -1;
                opc_resolve(addr);
            }
            else if (inet_pton(AF_INET, jsonStr + tok[1].start, &addr->host) == 0)
            {
                fputs("host must be a valid IP address\n", stderr);
                exit(1);
            }

            break;
        case JSMN_PRIMITIVE:
            if (!resolveHostnames && tok[1].end - tok[1].start >= 4 && jsonStr[tok[1].start] == 'n')
            {
                inet_pton(AF_INET, "0.0.0.0", &addr->host);
                break;
            }
        default:
            fprintf(stderr, "host must be a%s\n", resolveHostnames ? " string" : "n IP address or null");
            exit(1);
        }

        // parse port
        if (tok[2].type == JSMN_PRIMITIVE)
        {
            if (!isdigit(jsonStr[tok[2].start]))
            {
                fputs("port must be a number\n", stderr);
                exit(1);
            }
            unsigned long p = strtoul(jsonStr + tok[2].start, NULL, 0);
            if (errno == ERANGE || p > 65535 || p == 0)
            {
                fputs("port number must be 1-65535\n", stderr);
                exit(1);
            }
            addr->port = (unsigned short)p;
        }
        else
        {
            fputs("port must be an IP address\n", stderr);
            exit(1);
        }
    }
    else
    {
        fputs("address format must be [host, port]\n", stderr);
        exit(1);
    }
}

static void parse_config_address_list(bamboozled_address *addr, jsmntok_t *tok, char *jsonStr, bool resolveHostnames)
{
    // array should be [host,port] or [[host,port], [host,port], ...]
    if (tok[0].type == JSMN_ARRAY && tok[0].size > 0)
    {
        switch (tok[1].type)
        {
        case JSMN_ARRAY:
            // multiple addresses ([[host,port], [host,port], ...])
            for (int i = 0; i < tok[0].size * (tok[1].size + 1); i += tok[1].size + 1)
            {
                parse_config_address(addr, &tok[i + 1], jsonStr, resolveHostnames);
                if (i + tok[1].size + 1 != tok[0].size * (tok[1].size + 1))
                {
                    // we need to allocate another address
                    addr->next = malloc(sizeof(bamboozled_address));
                    addr = addr->next;
                    addr->next = NULL;
                }
            }
            return;
        case JSMN_STRING:
            // single address ([host,port])
            parse_config_address(addr, tok, jsonStr, resolveHostnames);
            return;
        default:
            break;
        }
    }
    fputs("address format must be [host, port]\n", stderr);
    exit(1);
}

static void parse_config_color(rgbPixel *pix, jsmntok_t *tok, char *jsonStr)
{
    // check for [r, g, b]
    if (tok[0].type == JSMN_ARRAY && tok[0].size == 3)
    {
        // parse each color
        for (int i = 0; i < 3; i++)
        {
            if (tok[i + 1].type == JSMN_PRIMITIVE && isdigit(jsonStr[tok[2].start]))
            {
                unsigned long p = strtoul(jsonStr + tok[2].start, NULL, 0);
                if (errno == ERANGE || p > 255)
                {
                    fputs("[r, g, b] must be 0-255\n", stderr);
                    exit(1);
                }
                ((uint8_t *)pix)[i] = (uint8_t)p;
            }
            else
            {
                fputs("[r, g, b] must be numbers\n", stderr);
                exit(1);
            }
        }
    }
    else
    {
        fputs("background format must be [r, g, b]\n", stderr);
        exit(1);
    }
}

static bool jsoneq(const char *json, jsmntok_t *tok, const char *s)
{
    return (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
            strncmp(json + tok->start, s, tok->end - tok->start) == 0);
}

static void parse_config(char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        fputs("could not open config file ", stderr);
        perror(filename);
        exit(1);
    }
    fseek(fp, 0, SEEK_END);
    int fsize = ftell(fp);
    rewind(fp);
    char *jsonStr = malloc(fsize);
    if (fread(jsonStr, 1, fsize, fp) != fsize)
    {
        fputs("could not open config file\n", stderr);
    }

    fclose(fp);

    jsmn_parser parser;
    jsmn_init(&parser);

    int tokcnt = jsmn_parse(&parser, jsonStr, fsize, NULL, 256);
    jsmntok_t *tokens = malloc(tokcnt * sizeof(jsmntok_t));
    jsmn_init(&parser);
    switch (jsmn_parse(&parser, jsonStr, fsize, tokens, tokcnt))
    {
    case JSMN_ERROR_INVAL:
        fputs("config file is not valid JSON\n", stderr);
        exit(1);
    case JSMN_ERROR_PART:
        fputs("config file JSON is incomplete\n", stderr);
        exit(1);
    case JSMN_ERROR_NOMEM:
        fputs("too many tokens in JSON file\n", stderr);
        exit(1);
    }

    if (tokcnt == 0 || tokens[0].type != JSMN_OBJECT)
    {
        fputs("top-level JSON token is not an object\n", stderr);
        exit(1);
    }

    for (int i = 1; i < tokcnt; i++)
    {
        if (jsoneq(jsonStr, &tokens[i], "listen"))
        {
            parse_config_address(&config.listen, &tokens[i + 1], jsonStr, false);
        }
        else if (jsoneq(jsonStr, &tokens[i], "destination"))
        {
            parse_config_address_list(&config.destination, &tokens[i + 1], jsonStr, true);
        }
        else if (jsoneq(jsonStr, &tokens[i], "background"))
        {
            parse_config_color(&config.background, &tokens[i + 1], jsonStr);
        }
    }
    free(tokens);
    free(jsonStr);
}

static void showHelp(char *arg0)
{
    printf("usage: %s [--listen=HOST:PORT] [--destination=HOST:PORT] [--background=R,G,B] [--config=PATH] [--help]\n", arg0);
    puts("    -l/--listen        the address to accept clients on (default 127.0.0.1:7891)");
    puts("    -d/--destination   the OPC server to send composited frames to (default 127.0.0.1:7890)");
    puts("    -b/--background    set the background color behind all dynamic layers");
    puts("    -c/--config        read configuration from a JSON file, overriding any previous flags");
    puts("    -h/--help          shows this help text");
}

void parse_args(int argc, char **argv)
{
    if (argc <= 1)
    {
        showHelp(argv[0]);
        exit(0);
    }
    else
    {
        static struct option longopts[] = {
            {"listen", required_argument, NULL, 'l'},
            {"destination", required_argument, NULL, 'd'},
            {"background", required_argument, NULL, 'b'},
            {"config", required_argument, NULL, 'c'},
            {"help", no_argument, NULL, 'h'},
            {NULL, 0, NULL, 0}};
        int arg;
        while ((arg = getopt_long(argc, argv, "l:d:b:c:h", longopts, NULL)) != -1)
        {
            switch (arg)
            {
            case 'l':
                parse_address(optarg, &config.listen, false, false);
                break;
            case 'd':
                parse_address(optarg, &config.destination, true, true);
                break;
            case 'b':
                parse_color(optarg, &config.background);
                break;
            case 'c':
                parse_config(optarg);
                break;
            case 'h':
                showHelp(argv[0]);
                exit(0);
                break;
            case '?':
                showHelp(argv[0]);
                exit(1);
                break;
            }
        }
    }
}
