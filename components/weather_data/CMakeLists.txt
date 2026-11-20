idf_component_register(SRCS "weather_parse.c" "weather_time.c" "weather_cache.c" "weather_policy.c"
    INCLUDE_DIRS "include" REQUIRES json)
