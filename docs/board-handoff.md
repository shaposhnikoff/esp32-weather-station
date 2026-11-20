# Хендоф сборщику: рабочая конфигурация ESP32-S3 и RGB-экрана

Этот документ можно перенести в другой проект и передать агенту перед
реализацией. Он фиксирует аппаратный контракт работающей погодной станции
из `/Users/max/git/oled/weather`, проверенный 20 ноября 2026 года.
Настройки приложения, Wi-Fi и пароль для переноса дисплея не нужны.

## Задание принимающему агенту

> Сначала подними дисплей и touch с параметрами этого документа. Сохрани
> разрешение, порядок RGB-пинов, тайминги, последовательность reset, размещение
> буферов и настройки LVGL. Покажи тестовый экран на физической плате, затем
> подключай интерфейс приложения. Не подбирай параметры по названию похожей
> платы и не меняй несколько параметров одновременно при диагностике.
> Сборка без ошибок ещё не подтверждает работу дисплея.

## Какая плата действительно проверена

| Параметр | Рабочее значение |
| --- | --- |
| MCU | ESP32-S3, ревизия чипа v0.2 |
| Flash | 16 MB |
| PSRAM | 8 MB, octal, 80 MHz |
| Физическое разрешение | **1024 × 600**, landscape |
| Профиль в исходном проекте | `waveshare-lcd5b` |
| Интерфейс LCD | Параллельный RGB, 16 линий данных, RGB565 |
| Touch | GT911, I2C address `0x5D` |
| Расширитель выводов | CH422G |
| Поворот и отражение | Не применяются |

**Не подставляй 800 × 480.** В исходном проекте такой профиль тоже существует,
но он не соответствует подключённому экрану.

Разрешение установлено чтением работающей заводской прошивки через USB JTAG:

```text
LCD_CAM register 0x6004101c = 0xce12b8ab
LCD_CAM register 0x60041020 = 0x4b33ff15
width  = ((0x4b33ff15 >> 8) & 0xfff) + 1 = 1024
height = ((0xce12b8ab >> 11) & 0x3ff) + 1 = 600
```

Это подтверждает активное разрешение. Тайминги ниже взяты из реально
используемого рабочего профиля; они не выданы за расшифрованные заводские
тайминги. Маркировка и ревизия самой PCB отдельно не подтверждены.
Название `waveshare-lcd5b` обозначает выбранный программный профиль.

## Закреплённый стек

| Компонент | Версия / настройка |
| --- | --- |
| PlatformIO platform | `espressif32@6.10.0` |
| Framework | ESP-IDF `5.4.0` |
| PlatformIO framework package | `framework-espidf@3.50400.0` |
| PlatformIO board | `esp32-s3-devkitc-1` |
| LVGL | **8.4.0**, одинаково на MCU и desktop |
| CPU | 240 MHz |
| Flash runtime mode | QIO, 80 MHz |
| Заголовок загрузчика при записи | **DIO**, 80 MHz, 16 MB |
| RTOS tick | 1000 Hz |
| Консоль | USB Serial/JTAG, 115200 baud |

Не заменяй LVGL 8 на LVGL 9 без отдельного портирования драйвера и интерфейса.
В исходной среде PlatformIO Core сообщил `6.2.0.post1`; это наблюдаемая
версия установленного инструмента, а не обещание наличия такой версии в PyPI.

## Пины и RGB-тайминги

Порядок `data_gpio_nums` критичен: это последовательность **D0 … D15**,
а не отсортированный список GPIO.

```c
#define BOARD_WIDTH 1024
#define BOARD_HEIGHT 600
#define BOARD_RGB_PINS {14, 38, 18, 17, 10, 39, 0, 45, \
                        48, 47, 21, 1, 2, 42, 41, 40}
#define BOARD_PCLK 7
#define BOARD_HSYNC 46
#define BOARD_VSYNC 3
#define BOARD_DE 5

#define BOARD_PIXEL_CLOCK_HZ 21000000
#define BOARD_HSYNC_PULSE 30
#define BOARD_HSYNC_BACK 145
#define BOARD_HSYNC_FRONT 170
#define BOARD_VSYNC_PULSE 2
#define BOARD_VSYNC_BACK 23
#define BOARD_VSYNC_FRONT 12
```

Рабочая конфигурация ESP-IDF:

```c
const esp_lcd_rgb_panel_config_t config = {
    .clk_src = LCD_CLK_SRC_DEFAULT,
    .timings = {
        .pclk_hz = BOARD_PIXEL_CLOCK_HZ,
        .h_res = BOARD_WIDTH,
        .v_res = BOARD_HEIGHT,
        .hsync_pulse_width = BOARD_HSYNC_PULSE,
        .hsync_back_porch = BOARD_HSYNC_BACK,
        .hsync_front_porch = BOARD_HSYNC_FRONT,
        .vsync_pulse_width = BOARD_VSYNC_PULSE,
        .vsync_back_porch = BOARD_VSYNC_BACK,
        .vsync_front_porch = BOARD_VSYNC_FRONT,
        .flags.pclk_active_neg = true,
    },
    .data_width = 16,
    .bits_per_pixel = 16,
    .num_fbs = 1,
    .bounce_buffer_size_px = BOARD_WIDTH * 10,
    .dma_burst_size = 64,
    .hsync_gpio_num = BOARD_HSYNC,
    .vsync_gpio_num = BOARD_VSYNC,
    .de_gpio_num = BOARD_DE,
    .pclk_gpio_num = BOARD_PCLK,
    .disp_gpio_num = -1,
    .data_gpio_nums = BOARD_RGB_PINS,
    .flags.fb_in_psram = true,
};
```

Затем последовательно вызываются `esp_lcd_new_rgb_panel`,
`esp_lcd_panel_reset`, `esp_lcd_panel_init` и включение подсветки через CH422G.
Остальные flags в этой структуре остаются нулевыми. Не добавляй swap/mirror
и не увеличивай pixel clock при первом переносе.
`21000000` — запрошенная частота PCLK; измерение частоты осциллографом
и работа при 60 FPS не заявляются.

## CH422G, reset и подсветка

| Сигнал | Подключение |
| --- | --- |
| I2C SDA / SCL | GPIO 8 / 9 |
| I2C | Master, 400 kHz, внутренние pull-up включены |
| CH422G mode command address | `0x24`, записать один байт `0x01` |
| CH422G output command address | `0x38`, записать один байт состояния выходов |
| GT911 reset | EXIO1, bit `1 << 1` |
| Подсветка | EXIO2, bit `1 << 2` |
| LCD reset | EXIO3, bit `1 << 3` |
| SD chip select | EXIO4, bit `1 << 4` |
| USB select | EXIO5, bit `1 << 5` |
| GT911 INT | GPIO 4 |

Сохраняй остальные биты при изменении одного выхода. Начальная маска
состояния в рабочем драйвере: EXIO1, EXIO3, EXIO4 и EXIO5 установлены,
EXIO2 сброшен — `0x3A`.

Последовательность:

1. Инициализировать I2C и записать `0x01` по адресу `0x24`.
2. Опустить LCD reset через EXIO3, ждать 10 ms, поднять EXIO3.
3. Настроить GPIO 4 как output и установить LOW.
4. Опустить touch reset через EXIO1, ждать 100 ms, поднять EXIO1.
5. Ждать 200 ms и выполнить `gpio_reset_pin(GPIO_NUM_4)`.
6. Инициализировать RGB-панель.
7. Поднять EXIO2 для включения подсветки.

Здесь `0x24` и `0x38` — адреса I2C-команд CH422G; не преобразовывай их
в обычные внутренние регистры другого I2C-расширителя.
Подсветка управляется цифровым выходом. Аппаратный PWM не реализован:
в погодном приложении промежуточная яркость создаётся затемняющим слоем LVGL,
а при нуле EXIO2 выключается.

## GT911

Рабочий адрес — `0x5D`; он согласован с последовательностью reset выше.
Не меняй его на `0x14` без изменения и проверки этой последовательности.

- Читать статус по 16-битному адресу `0x814E`.
- Если bit 7 не установлен, сохранять последнее состояние контакта,
  а не генерировать отпускание при каждом таком опросе.
- Количество контактов — младшие 4 бита; драйвер использует первый из 1–5.
- Данные первого контакта читать с `0x814F`, 8 байт.
- X: `point[1] | point[2] << 8`; Y: `point[3] | point[4] << 8`.
- После обработки сбрасывать статус записью `{0x81, 0x4e, 0}`.
- Координаты допустимы при `x < 1024`, `y < 600`; swap и mirror не применяются.

В `lv_indev_data_t` передаются физические координаты панели. Для текущего
масштабированного интерфейса преобразование делает LVGL; дополнительное
масштабирование координат в драйвере не требуется.

## Память и DMA

| Буфер | Размещение | Размер |
| --- | --- | --- |
| RGB frame buffer | PSRAM, выделяет RGB driver | `1024 * 600 * 2 = 1228800` байт |
| LVGL draw buffer | PSRAM, `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT` | `1024 * 40` пикселей, `81920` байт |
| Два bounce buffer | Внутренняя RAM, выделяет RGB driver | По `1024 * 10` пикселей, суммарно `40960` байт |

`bounce_buffer_size_px` задаётся **в пикселях**, не в байтах.
В `lv_disp_draw_buf_init` размер также передаётся в пикселях:

```c
lv_color_t *pixels = heap_caps_malloc(
    BOARD_WIDTH * 40 * sizeof(lv_color_t),
    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
assert(pixels);
static lv_disp_draw_buf_t buffer;
lv_disp_draw_buf_init(&buffer, pixels, NULL, BOARD_WIDTH * 40);
```

Не переноси полный framebuffer во внутреннюю RAM. Сохрани резерв внутренней
памяти для DMA и учитывай нагрузку Wi-Fi/TLS. Сетевые операции и разбор JSON
в погодной станции выполняются отдельной задачей; LVGL обслуживает главный
поток. Рабочий контракт содержит один framebuffer и копирующий flush —
добавление double buffering или zero-copy требует отдельной синхронизации.

## Обязательные настройки SDK

Создай `sdkconfig.defaults` с этими параметрами:

```ini
CONFIG_IDF_TARGET="esp32s3"
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_ESPTOOLPY_FLASHMODE_QIO=y
CONFIG_ESPTOOLPY_FLASHFREQ_80M=y
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=65536
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y
CONFIG_FREERTOS_HZ=1000
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y
CONFIG_COMPILER_OPTIMIZATION_PERF=y
CONFIG_ESP_MAIN_TASK_STACK_SIZE=12288
CONFIG_LV_CONF_SKIP=n
```

Разметка разделов и TLS/SNTP относятся к приложению и добавляются отдельно.
В исходном проекте используются `CONFIG_PARTITION_TABLE_CUSTOM=y`,
`CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"` и
`CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y`.

**Наличие параметра в defaults не доказывает его применение.** Старая
сгенерированная конфигурация может сохранять прежние значения. В новом проекте
используй отдельный `board_build.esp-idf.sdkconfig_path` и проверь итоговый
sdkconfig, в том числе отключённый `LV_CONF_SKIP`. Не переноси старые
`sdkconfig`, `.pio` и `managed_components` как исходные настройки.

## Обязательные настройки LVGL 8

Общий `include/lv_conf.h`:

```c
#ifndef LV_CONF_H
#define LV_CONF_H
#include <stdlib.h>
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_ALLOC malloc
#define LV_MEM_CUSTOM_FREE free
#define LV_MEM_CUSTOM_REALLOC realloc
#define LV_TICK_CUSTOM 0
#define LV_SPRINTF_USE_FLOAT 1
#define LV_DPI_DEF 130
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_USE_LOG 0
#define LV_USE_PERF_MONITOR 0
#define LV_BUILD_EXAMPLES 0
#define LV_USE_DEMO_WIDGETS 0
#endif
```

Добавь `include/` в include path и убедись, что **сама библиотека LVGL**
собрана с этим заголовком. В погодном проекте это дополнительно проверяется:

```c
#if LV_COLOR_DEPTH != 16 || !LV_MEM_CUSTOM || !LV_SPRINTF_USE_FLOAT
#error "Shared LVGL configuration is required"
#endif
```

Без `CONFIG_LV_CONF_SKIP=n` ESP-IDF ранее игнорировал этот заголовок:
применялись статическая куча LVGL 32 KiB и отключённый float formatter.
На устройстве форматирование `%.0f%s` падало в `_vsnprintf`. Исправление
настройки сборки устранило причину; изменение строк или добавление задержек
её не устраняет.

## Контракт адаптера дисплея

- Сначала `board_init`, затем `lv_init`.
- `display.hor_res = 1024`, `display.ver_res = 600`.
- `lv_disp_drv_t`, `lv_disp_draw_buf_t` и `lv_indev_drv_t` должны жить
  всё время работы LVGL. В рабочем коде это `static`-объекты.
- LVGL tick: `esp_timer` каждые 2000 μs вызывает `lv_tick_inc(2)`.
- Основной цикл вызывает `lv_timer_handler()` и делает задержку 5 ms.
- Обычные функции LVGL вызываются одним владельцем UI.

Flush для текущего копирующего драйвера:

```c
static void flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *colors)
{
    esp_lcd_panel_handle_t panel = driver->user_data;
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(
        panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, colors));
    lv_disp_flush_ready(driver);
}
```

У LVGL правая и нижняя границы области включены; у этого вызова ESP-IDF
они исключены. Пропуск `+1` оставляет незаписанные крайние столбцы/строки.
Нельзя переносить момент `lv_disp_flush_ready` без учёта выбранного режима
копирования или асинхронной DMA-передачи.

## Размер интерфейса и ожидаемые поля

Физический framebuffer всегда **1024 × 600**. Погодный интерфейс имеет
логический размер 800 × 480 и масштабируется целиком:

```text
LVGL zoom = min(1024 * 256 / 800, 600 * 256 / 480) = 320
scale = 320 / 256 = 1.25
content = 1000 × 600
offset = x: 12, y: 0
```

По краям остаются два намеренных поля по 12 пикселей цвета фона.
Это следствие сохранения пропорций интерфейса. В новом проекте можно сразу
рисовать UI на 1024 × 600; тогда эти поля не нужны.
Полосы внутри контента, дрожание, сдвиг строк и постепенно уезжающий кадр
нормой не являются.

## Сборка и запись

Сохрани в PlatformIO:

```ini
[env:target-panel]
platform = espressif32@6.10.0
framework = espidf
board = esp32-s3-devkitc-1
platform_packages =
    framework-espidf@3.50400.0
board_upload.flash_size = 16MB
board_build.flash_size = 16MB
board_build.flash_mode = qio
board_build.esp-idf.sdkconfig_path = sdkconfig.target-panel
monitor_speed = 115200
build_flags =
    -I include
```

Это базовый фрагмент для нового проекта. При копировании исходного
`board_profile.h` добавь `-DWEATHER_BOARD_LCD5B`: без этого macro тот заголовок
выберет 800 × 480. Если создаёшь новый заголовок с фиксированными параметрами
из этого документа, macro не требуется. В manifest компонента закрепи
`idf: "==5.4.0"` и `lvgl/lvgl: "==8.4.0"`.

На ARM Mac пакет CMake из PlatformIO оказался x86_64-only. В исходном проекте
использован локальный пакет `tools/native-cmake` с версией `3.30.5`:

```ini
platform_packages =
    framework-espidf@3.50400.0
    tool-cmake@file://tools/native-cmake
```

Его executable `bin/cmake`:

```sh
#!/bin/sh
exec uv tool run --python 3.12 --from cmake==3.30.5 \
  --with ninja==1.11.1.3 cmake "$@"
```

При переносе этого override скопируй весь `tools/native-cmake/`,
включая `package.json` и executable permissions. Не заменяй им глобальные
пакеты других проектов. Последовательная сборка:

```sh
pio run -e target-panel -j 1
```

Используй адреса из **сгенерированного файла своего проекта**
`flasher_args.json`. В исходном проекте загрузчик находится на `0x0`,
таблица на `0x8000`, приложение на `0x10000`.
При записи загрузчика сохраняй **DIO / 80 MHz / 16 MB**.
Не используй разметку другого проекта без проверки.

Полная пошаговая процедура исходного проекта находится в
[deployment.md](deployment.md).

## Смена проектов: обязательно учитывать разметку Flash

Аппаратные настройки дисплея и разметка Flash — разные части контракта.
Другой проект может работать на том же экране с другой таблицей разделов.
При переходе между проектами записывай согласованный комплект
**загрузчик + таблица разделов + приложение** по адресам принимающего проекта,
а затем его внешние данные. Запись одного приложения допустима только после
проверки совместимости уже установленной разметки.

В ходе этой работы после загрузки пользователем другой прошивки на плате
прочитана таблица с одним разделом `factory` размером `0x700000` и без
разделов `config`/`cache`. Погодная станция ожидает:

```csv
# Name, Type, SubType, Offset, Size
nvs,data,nvs,0x9000,0x6000
phy_init,data,phy,0xf000,0x1000
factory,app,factory,0x10000,0x600000
config,data,spiffs,0x610000,0x100000
cache,data,spiffs,0x710000,0x100000
```

Здесь subtype `spiffs` используется как метка типа раздела; файловая система
в обоих разделах — **LittleFS**, не SPIFFS. В рабочем коде разделы выбираются
по label. Форматировать их инструментом SPIFFS нельзя.

Сообщение `partition "config" could not be found` в такой ситуации означает
несовпадение таблицы разделов. Оно не указывает на неисправность экрана или
неверный пароль. Повторная загрузка JSON не добавит отсутствующий раздел.
При восстановлении погодной станции нужен весь её комплект и приватная
конфигурация; сохранность старого JSON после чужой прошивки не предполагается.
Новый проект вправе использовать собственную разметку — не копируй погодные
разделы без потребности приложения.

## Диагностика чёрного экрана и полос

| Наблюдение | Что проверять первым |
| --- | --- |
| ROM reset loop, `ets_loader.c 78`, TG0WDT | DIO в заголовке загрузчика; QIO при записи уже приводил к этому сбою |
| Падение в `_vsnprintf`, `ui_value`, `lv_label_set_text_fmt` | Реальные настройки LVGL, `LV_CONF_SKIP`, float formatter и allocator |
| Изображение другого размера, обрезка | Физические 1024 × 600 во всём драйвере и выбор macro профиля |
| Экран тёмный при работающем MCU | I2C, последовательность LCD reset и EXIO2 подсветки |
| Неверные цвета | Точный порядок D0–D15, RGB565 и отсутствие byte swap |
| Незакрашенный край обновляемого прямоугольника | Границы `draw_bitmap`, обязательные `x2 + 1`, `y2 + 1` |
| Полосы/сдвиг под Wi-Fi или TLS нагрузкой | Сначала вернуть тайминги и bounce buffer из документа, затем проверить доступную внутреннюю RAM и нагрузку |
| Desktop работает, MCU падает | Сравнить фактические заголовки/defines LVGL и lifetime driver structures |

Последние диагностические направления для полос — порядок проверки,
а не утверждение установленной причины конкретного нового сбоя.
После каждого изменения проверь один тестовый экран и журнал. Не скрывай
неподтверждённую причину дополнительными задержками.

## Что доказано и что нужно проверить после переноса

В исходном проекте:

- Firmware успешно собрана для `waveshare-lcd5b`.
- esptool подтвердил hash записанного приложения.
- Устройство загрузилось с 8 MB PSRAM без panic в проверенном журнале.
- Пользователь подтвердил видимый погодный экран.
- Плата получила IP и реальную погоду: HTTP 200, 120 почасовых записей.
- Desktop использует тот же UI; тесты данных, состояния интерфейса,
  навигации и конфигурации прошли.

В принимающем проекте отдельно проверь физический экран: красную, зелёную
и синюю заливку, текст по четырём краям, касания в четырёх углах и центре,
повторную загрузку и отображение во время сетевого запроса.
Точность touch по всем углам, отсутствие редких артефактов при длительной
нагрузке и конкретная частота кадров в исходном проекте не подтверждены.
Не называй эти проверки пройденными только на основании этого документа.

## Что передать вместе с документом, если доступны исходники

Минимальные проверенные аппаратные файлы:

```text
include/board_profile.h
include/lv_conf.h
firmware/main/board.h
firmware/main/board.c
sdkconfig.defaults
platformio.ini
tools/native-cmake/package.json
tools/native-cmake/bin/cmake
```

`app_main.c` исходной станции зависит от погодных компонентов. Для нового
приложения переноси из него описанный адаптер LVGL, сохраняя lifetime, размеры,
tick и flush, либо копируй погодные компоненты целиком, если нужна сама станция.
Разметку и зависимость LittleFS переноси только вместе с соответствующей
функциональностью приложения.

Пароли, приватный `config.json`, `.pio`, сгенерированный sdkconfig и дампы Flash
в аппаратный хендоф не включаются.
