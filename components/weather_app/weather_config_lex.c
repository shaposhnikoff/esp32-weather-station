#include "weather_config_internal.h"
#include <string.h>

static int digit(unsigned char c) { return c >= '0' && c <= '9'; }
static int hex(unsigned char c)
{
    if (digit(c)) return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool utf8(const unsigned char **cursor)
{
    const unsigned char *p = *cursor;
    unsigned c = *p++, value, minimum, count;
    if (c >= 0xc2 && c <= 0xdf) { value = c & 31; minimum = 0x80; count = 1; }
    else if (c >= 0xe0 && c <= 0xef) { value = c & 15; minimum = 0x800; count = 2; }
    else if (c >= 0xf0 && c <= 0xf4) { value = c & 7; minimum = 0x10000; count = 3; }
    else return false;
    while (count--) {
        c = *p++;
        if ((c & 0xc0) != 0x80) return false;
        value = (value << 6) | (c & 63);
    }
    if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
        return false;
    *cursor = p;
    return true;
}

bool weather_config_lexical_valid(const char *json)
{
    const unsigned char *p = (const unsigned char *)json;
    unsigned depth = 0;
    while (*p) {
        if (*p == '"') {
            ++p;
            while (*p != '"') {
                unsigned c = *p++;
                if (c < 32) return false;
                if (c == '\\') {
                    c = *p++;
                    if (c == 'u') {
                        unsigned value = 0;
                        for (unsigned i = 0; i < 4; ++i) {
                            const int h = hex(*p++);
                            if (h < 0) return false;
                            value = (value << 4) | (unsigned)h;
                        }
                        if (!value) return false;
                    } else if (!c || !strchr("\"\\/bfnrt", (int)c)) return false;
                } else if (c >= 128) {
                    --p;
                    if (!utf8(&p)) return false;
                }
            }
            ++p;
        } else if (*p == '-' || digit(*p)) {
            if (*p == '-') ++p;
            if (*p == '0') ++p;
            else {
                if (*p < '1' || *p > '9') return false;
                while (digit(*p)) ++p;
            }
            if (*p == '.') {
                ++p;
                if (!digit(*p)) return false;
                while (digit(*p)) ++p;
            }
            if (*p == 'e' || *p == 'E') {
                ++p;
                if (*p == '+' || *p == '-') ++p;
                if (!digit(*p)) return false;
                while (digit(*p)) ++p;
            }
            if (*p && !strchr(" \t\r\n,}]", *p)) return false;
        } else if (!strncmp((const char *)p, "true", 4)) p += 4;
        else if (!strncmp((const char *)p, "false", 5)) p += 5;
        else if (!strncmp((const char *)p, "null", 4)) p += 4;
        else if (*p == '{' || *p == '[') {
            if (++depth > 8) return false;
            ++p;
        } else if (*p == '}' || *p == ']') {
            if (!depth) return false;
            --depth;
            ++p;
        } else if (strchr(" \t\r\n,:", *p)) ++p;
        else return false;
    }
    return depth == 0;
}
