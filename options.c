#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>

#include "bamboozled.h"
#include "jsmn.h"

static void parse_address(const char *str, bamboozled_address *addr)
{
    char *colon = strchr(str, ':');
    if (colon == NULL)
    {
        fputs("address must be in host:port format\n", stderr);
        exit(1);
    }
    addr->host = malloc(colon - str);
    memcpy(addr->host, str, colon - str);
    if (!isdigit(colon[1]))
    {
        fputs("port must be a number\n", stderr);
        exit(1);
    }
    char *l;
    unsigned long p = strtoul(colon + 1, &l, 0);
    if (errno == ERANGE || p > 65535)
    {
        fputs("port number cannot be above 65535\n", stderr);
        exit(1);
    }
    else if (l != strchr(str, '\0'))
    {
        fputs("unexpected data after port number\n", stderr);
        exit(1);
    }
    else
    {
        addr->port = (uint16_t)p;
    }
}

static void parse_config_address(bamboozled_address *addr, jsmntok_t *tok, char *jsonStr, bool allowNullIP)
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
            fprintf(stderr, "host must be an IP address%s\n", allowNullIP ? " or null" : "");
            exit(1);
        }

        if (tok[3].type == JSMN_PRIMITIVE)
        {
            if (!isdigit(jsonStr[tok[3].start]))
            {
                fputs("port must be a number\n", stderr);
                exit(1);
            }
            unsigned long p = strtoul(jsonStr + tok[3].start, NULL, 0);
            if (errno == ERANGE || p > 65535)
            {
                fputs("Port number cannot be above 65535\n", stderr);
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

static void parse_config_color(rgbPixel *pix, jsmntok_t *tok, char *jsonStr)
{
    if (tok[1].type == JSMN_ARRAY)
    {
        for (int i = 0; i < 3; i++)
        {
            if (tok[2 + i].type == JSMN_PRIMITIVE && isdigit(jsonStr[tok[3].start]))
            {
                unsigned long p = strtoul(jsonStr + tok[3].start, NULL, 0);
                if (errno == ERANGE || p > 255)
                {
                    fputs("[r, g, b] must be between 0 and 255\n", stderr);
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
            fputs("r,g,b must be between 0 and 255\n", stderr);
            exit(1);
        }
        else
        {
            ((uint8_t *)pix)[i] = p;
            s = end+1;
        }
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
        fputs("Could not open config file \n", stderr);
        perror(filename);
        exit(1);
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
        fputs("Config file is not valid JSON\n", stderr);
        exit(1);
    case JSMN_ERROR_PART:
        fputs("Config file JSON is incomplete\n", stderr);
        exit(1);
    case JSMN_ERROR_NOMEM:
        fputs("Too many tokens in JSON file\n", stderr);
        exit(1);
    }

    if (tokcnt < 1 || tokens[0].type != JSMN_OBJECT)
    {
        fputs("Top-level JSON token is not an object\n", stderr);
        exit(1);
    }

    for (int i = 1; i < tokcnt; i++)
    {
        if (jsoneq(jsonStr, &tokens[i], "listen"))
        {
            parse_config_address(&(config.listen), &tokens[i], jsonStr, true);
            i += 3;
        }
        else if (jsoneq(jsonStr, &tokens[i], "destination"))
        {
            parse_config_address(&(config.destination), &tokens[i], jsonStr, false);
            i += 3;
        }
        else if (jsoneq(jsonStr, &tokens[i], "background"))
        {
            parse_config_color(&(config.background), &tokens[i], jsonStr);
            i += 4;
        }
    }
    free(jsonStr);
}

static void showHelp(char* arg0)
{
    printf("usage: %s [--listen=HOST:PORT] [--destination=HOST:PORT] [--background=R,G,B] [--config=PATH] [--help]", arg0);
    puts("    -l/--listen        the address to accept clients on (default 127.0.0.1:7891)");
    puts("    -d/--destination   the OPC server to send composited frames to (default 127.0.0.1:7890)");
    puts("    -b/--background    set the background color behind all dynamic layers");
    puts("    -c/--config        read configuration from a JSON file, overriding any previous flags");
    puts("    -h/--help          shows this help text");
    exit(0);
}

void parse_args(int argc, char **argv)
{

    if (argc <= 1)
    {
        showHelp(argv[0]);
    }
    else
    {
        static struct option longopts[] = {
            {"listen", required_argument, NULL, 'l'},
            {"destination", required_argument, NULL, 'd'},
            {"background", required_argument, NULL, 'b'},
            {"config", required_argument, NULL, 'c'},
            {"help", no_argument, NULL, 'h'}};
        int arg;
        while ((arg = getopt_long(argc, argv, "l:d:b:c:h:", longopts, NULL)) != -1)
        {
            switch (arg)
            {
            case 'l':
                parse_address(optarg, &(config.listen));
                break;
            case 'd':
                parse_address(optarg, &(config.destination));
                break;
            case 'b':
                parse_color(optarg, &(config.background));
                break;
            case 'c':
                parse_config(optarg);
                break;
            case 'h':
                showHelp(argv[0]);
            }
        }
    }
}