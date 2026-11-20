idf_component_register(SRCS "weather_config.c" "weather_config_lex.c"
    INCLUDE_DIRS "include" REQUIRES weather_data json)
