#include "weather_config.h"
#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        fputs("Usage: weather-config-check config.json\n", stderr);
        return 2;
    }
    weather_config_t config;
    char field[80];
    if (!weather_config_load(argv[1], &config, field)) {
        fprintf(stderr, "Invalid configuration field: %s\n", field);
        return 1;
    }
    puts("Configuration valid; private values withheld.");
    return 0;
}
