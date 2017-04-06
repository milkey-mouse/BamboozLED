#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <errno.h>

#include "boblight.h"
#include "jsmn.h"

Address listen = {"127.0.0.1", 7891};
Address destination = {"127.0.0.1", 7890};
bool opcCompat = true;

static int parse_address(const char *str, Address *addr)
{
    char *colon = strchr(str, ':');
    if (colon == NULL)
    {
        puts("Address must be in host:port format");
        return -1;
    }
    addr->host = malloc(colon - str);
    memcpy(addr->host, str, colon - str);
    char *l;
    unsigned long p = strtoul(colon + 1, &l, 10);
    if (errno == ERANGE || p > 65535)
    {
        puts("Port number cannot be above 65535");
        return -1;
    }
    else if (l != strchr(str, '\0'))
    {
        puts("Unexpected data after port number");
        return -1;
    }
    else
    {
        addr->port = (unsigned short)p;
    }
    return 0;
}

static int parse_config_address(Address *addr, jsmntok_t *tok, char *jsonStr, bool allowNullIP)
{
    if (tok[1].type == JSMN_ARRAY)
    {
        switch (tok[2].type)
        {
        case JSMN_STRING:
            addr->host = malloc(tok[2].end - tok[2].start + 1);
            memcpy(addr->host, jsonStr + tok[2].start, tok[2].end - tok[2].start);
            addr->host[tok[2].end - tok[2].start] = '\0';
            break;
        case JSMN_PRIMITIVE:
            if (allowNullIP && strncmp(jsonStr + tok[2].start, "null", 4) == 0)
            {
                addr->host = "0.0.0.0";
                break;
            }
        default:
            if (allowNullIP)
            {
                fputs("host must be an IP address or null", stderr);
                return -1;
            }
            else
            {
                fputs("host must be an IP address", stderr);
                return -1;
            }

            return -1;
        }

        if (tok[3].type == JSMN_PRIMITIVE)
        {
            unsigned long p = strtoul(jsonStr + tok[3].start, NULL, 10);
            if (errno == ERANGE || p > 65535)
            {
                puts("Port number cannot be above 65535");
                return -1;
            }
            addr->port = (unsigned short)p;
        }
        else
        {
            fputs("port must be an IP address", stderr);
            return -1;
        }
    }
    else
    {
        fputs("address format must be [host, port]", stderr);
        return -1;
    }
    return 0;
}

static int jsoneq(const char *json, jsmntok_t *tok, const char *s)
{
    if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
        strncmp(json + tok->start, s, tok->end - tok->start) == 0)
    {
        return 0;
    }
    return -1;
}

static int parse_config(char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        fputs("Could not open file ", stderr);
        perror(filename);
        return -1;
    }
    fseek(fp, 0, SEEK_END);
    int fsize = ftell(fp);
    rewind(fp);
    char *jsonStr = malloc(fsize);
    fread(jsonStr, 1, fsize, fp);
    fclose(fp);

    jsmn_parser parser;
    jsmn_init(&parser);

    int tokcnt = jsmn_parse(&parser, jsonStr, fsize, NULL, 256);
    jsmntok_t *tokens = malloc(tokcnt * sizeof(jsmntok_t));
    jsmn_init(&parser);
    switch (jsmn_parse(&parser, jsonStr, fsize, tokens, tokcnt))
    {
    case JSMN_ERROR_INVAL:
        fputs("Config file is not valid JSON", stderr);
        return -1;
    case JSMN_ERROR_PART:
        fputs("Config file JSON is incomplete", stderr);
        return -1;
    case JSMN_ERROR_NOMEM:
        fputs("Too many tokens in JSON file", stderr);
        return -1;
    }

    if (tokcnt < 1 || tokens[0].type != JSMN_OBJECT)
    {
        fputs("Top-level JSON token is not an object", stderr);
        return -1;
    }

    for (int i = 1; i < tokcnt; i++)
    {
        if (jsoneq(jsonStr, &tokens[i], "listen") == 0)
        {
            if (parse_config_address(&listen, &tokens[i], jsonStr, true) != 0)
            {
                return -1;
            }
            i += 3;
        }
        else if (jsoneq(jsonStr, &tokens[i], "destination") == 0)
        {
            if (parse_config_address(&destination, &tokens[i], jsonStr, false) != 0)
            {
                return -1;
            }
            i += 3;
        }
        else if (jsoneq(jsonStr, &tokens[i], "opcCompat") == 0)
        {
            if (tokens[i + 1].type == JSMN_PRIMITIVE)
            {
                if (strncmp(jsonStr + tokens[i + 1].start, "true", 4) == 0)
                {
                    opcCompat = true;
                }
                else if (strncmp(jsonStr + tokens[i + 1].start, "false", 4) == 0)
                {
                    opcCompat = false;
                }
                else
                {
                    fputs("opcCompat must be true or false", stderr);
                    return -1;
                }
            }
            else
            {
                fputs("opcCompat must be true or false", stderr);
                return -1;
            }
        }
    }
    free(jsonStr);

    return 0;
}

static void showHelp()
{
    puts("usage: boblight [--listen=HOST:PORT] [--destination=HOST:PORT] [--noOPCcompat] [--config=PATH] [--help]");
    puts("    -l/--listen        the address to accept clients on (default 127.0.0.1:7891)");
    puts("    -d/--destination   the OPC server to send composited frames to (default 127.0.0.1:7890)");
    puts("    -o/--noOPCcompat   break compatibility with existing OPC clients and interpret all pixels as RGBA");
    puts("    -c/--config        read configuration from a JSON file, overriding any previous flags");
    puts("    -h/--help          shows this help text");
}

int main(int argc, char **argv)
{

    if (argc <= 1)
    {
        showHelp();
        return 0;
    }

    static struct option longopts[] = {
        {"listen", optional_argument, NULL, 'l'},
        {"destination", optional_argument, NULL, 'd'},
        {"noOPCcompat", no_argument, NULL, 'o'},
        {"config", optional_argument, NULL, 'c'},
        {"help", no_argument, NULL, 'h'}};

    int arg;
    while ((arg = getopt_long(argc, argv, "l:d:o:c:h:", longopts, NULL)) != -1)
    {
        switch (arg)
        {
        case 'l':
            if (parse_address(optarg, &listen) != 0)
            {
                return 1;
            }
            break;
        case 'd':
            if (parse_address(optarg, &destination) != 0)
            {
                return 1;
            }
            break;
        case 'o':
            opcCompat = false;
            break;
        case 'c':
            if (parse_config(optarg) != 0)
            {
                return 1;
            }
            break;
        case 'h':
            showHelp();
            return 0;
        }
    }
    printf("boblight v. %s\n", VERSION);
    //printf("listen      host: %s port: %hu\n", listen.host, listen.port);
    //printf("destination host: %s port: %hu\n", destination.host, destination.port);
    //printf("opc compat: %s\n", opcCompat ? "true" : "false");
    return 0;
}