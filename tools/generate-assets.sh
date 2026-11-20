#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p assets/fonts assets/generated .tools
font=assets/fonts/JetBrainsMono-Regular.ttf
if [ ! -f "$font" ]; then
    curl --fail --location --output "$font" \
        https://raw.githubusercontent.com/JetBrains/JetBrainsMono/v2.304/fonts/ttf/JetBrainsMono-Regular.ttf
fi
if [ ! -f assets/fonts/OFL.txt ]; then
    curl --fail --location --output assets/fonts/OFL.txt \
        https://raw.githubusercontent.com/JetBrains/JetBrainsMono/v2.304/OFL.txt
fi
for size in 14 16 20 28; do
    npx --yes lv_font_conv@1.5.3 --font "$font" --size "$size" --bpp 4 \
        --range 0x20-0x7e,0xa0-0xff,0x400-0x45f,0x2013-0x2014,0x2026,0x2212 \
        --format lvgl --no-compress --no-prefilter \
        --output "assets/generated/weather_font_${size}.c"
done
npx --yes lv_font_conv@1.5.3 --font "$font" --size 80 --bpp 4 \
    --range 0x20-0x39,0x43,0xb0,0x2013,0x2212 --format lvgl --no-compress --no-prefilter \
    --output assets/generated/weather_font_80.c
cc -std=c11 -O2 tools/generate-landscape.c -lm -o .tools/generate-landscape
.tools/generate-landscape assets/generated/landscape.c
