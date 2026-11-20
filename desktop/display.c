#include "display.h"
#include "board_profile.h"
#include "weather_ui.h"
#include <SDL.h>
#include <stdio.h>

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static uint16_t framebuffer[BOARD_WIDTH * BOARD_HEIGHT];
static bool pressed;
static lv_point_t pointer;
static lv_indev_t *pointer_device;

static void flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *colors)
{
    for (int y = area->y1; y <= area->y2; ++y)
        for (int x = area->x1; x <= area->x2; ++x)
            framebuffer[y * BOARD_WIDTH + x] = (colors++)->full;
    lv_disp_flush_ready(driver);
}

static void read_pointer(lv_indev_drv_t *driver, lv_indev_data_t *data)
{
    (void)driver;
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->point = pointer;
}

bool desktop_display_init(void)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) return false;
    window = SDL_CreateWindow("Weather station", SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED, BOARD_WIDTH, BOARD_HEIGHT, 0);
    if (!window) return false;
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) return false;
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565,
        SDL_TEXTUREACCESS_STREAMING, BOARD_WIDTH, BOARD_HEIGHT);
    if (!texture) return false;
    lv_init();
    static lv_color_t pixels[BOARD_WIDTH * 40];
    static lv_disp_draw_buf_t buffer;
    lv_disp_draw_buf_init(&buffer, pixels, NULL, BOARD_WIDTH * 40);
    static lv_disp_drv_t display;
    lv_disp_drv_init(&display);
    display.hor_res = BOARD_WIDTH;
    display.ver_res = BOARD_HEIGHT;
    display.draw_buf = &buffer;
    display.flush_cb = flush;
    lv_disp_drv_register(&display);
    static lv_indev_drv_t input;
    lv_indev_drv_init(&input);
    input.type = LV_INDEV_TYPE_POINTER;
    input.read_cb = read_pointer;
    pointer_device = lv_indev_drv_register(&input);
    return true;
}

bool desktop_display_step(uint32_t elapsed_ms)
{
    SDL_Event event;
    bool running = true;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT: running = false; break;
        case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP:
            pointer.x = event.button.x;
            pointer.y = event.button.y;
            pressed = event.type == SDL_MOUSEBUTTONDOWN;
            break;
        case SDL_MOUSEMOTION:
            pointer.x = event.motion.x;
            pointer.y = event.motion.y;
            break;
        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_ESCAPE) weather_ui_close_details();
            break;
        default: break;
        }
    }
    lv_tick_inc(elapsed_ms);
    lv_timer_handler();
    if (SDL_UpdateTexture(texture, NULL, framebuffer, BOARD_WIDTH * sizeof(uint16_t)) != 0)
        return false;
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    return running;
}

bool desktop_display_save(const char *path)
{
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(framebuffer,
        BOARD_WIDTH, BOARD_HEIGHT, 16, BOARD_WIDTH * 2, SDL_PIXELFORMAT_RGB565);
    if (!surface) return false;
    const int error = SDL_SaveBMP(surface, path);
    SDL_FreeSurface(surface);
    return error == 0;
}

void desktop_display_close(void)
{
    lv_indev_delete(pointer_device);
    lv_disp_remove(lv_disp_get_default());
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
