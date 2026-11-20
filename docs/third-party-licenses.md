# Third-party notices

## SunCalc lunar phase formulas

The implementation in `components/weather_data/weather_time.c` adapts the
orbital approximation in SunCalc 1.9.0 by Vladimir Agafonkin.

Source: https://github.com/mourner/suncalc/tree/v1.9.0

Copyright (c) 2014, Vladimir Agafonkin
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

## Dependencies

- ESP-IDF 5.4.0: Apache-2.0 and bundled component notices, distributed with SDK.
- LVGL 8.4.0: MIT, license retained in fetched component.
- cJSON 1.7.18 on desktop and the ESP-IDF json component: MIT, license retained
  with the dependency source.
- SDL2: zlib license, supplied by the desktop installation.
- libcurl 8.7.1 on the verified Mac: curl license, dynamically supplied by the OS.
- ESP LittleFS 1.19.1 wrapper by Brian Pugh: MIT; upstream littlefs: BSD-3-Clause.
- JetBrains Mono 2.304: SIL OFL 1.1, retained in `assets/fonts/OFL.txt`.

The firmware release includes the fetched LVGL, ESP LittleFS, littlefs, cJSON,
ESP-IDF and font license texts under `dist/weather-station/licenses/`.
The original generated landscape art is documented in `assets/README.md`.

## Weather data

Weather data is provided by Open-Meteo, with attribution required by CC BY 4.0.
The application must display attribution in its read-only About Data view.
The free API is subject to its usage terms and non-commercial limits.

- https://open-meteo.com/en/docs
- https://open-meteo.com/en/terms
- https://creativecommons.org/licenses/by/4.0/

The deterministic `fixtures/demo.json` contains invented demonstration values.
It must not be presented as an observation or saved to the normal weather cache.
