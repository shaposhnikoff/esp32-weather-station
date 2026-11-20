#pragma once
#include <stdbool.h>
#include <stdint.h>

bool desktop_display_init(void);
bool desktop_display_step(uint32_t elapsed_ms);
bool desktop_display_save(const char *path);
void desktop_display_close(void);
