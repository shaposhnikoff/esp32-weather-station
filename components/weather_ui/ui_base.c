#include "weather_ui.h"

static lv_obj_t *counter;
static lv_obj_t *touch;

static void frame_tick(lv_timer_t *timer)
{
    static unsigned frames;
    (void)timer;
    lv_label_set_text_fmt(counter, "UI updates: %u", ++frames);
}

static void touch_event(lv_event_t *event)
{
    (void)event;
    lv_point_t point;
    lv_indev_get_point(lv_indev_get_act(), &point);
    lv_label_set_text_fmt(touch, "Touch: %d, %d", point.x, point.y);
}

void weather_ui_diagnostics(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x0b1120), 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(0xf5f7fc), 0);
    lv_obj_add_event_cb(screen, touch_event, LV_EVENT_PRESSED, NULL);
    for (int i = 0; i < 3; ++i) {
        lv_obj_t *bar = lv_obj_create(screen);
        const uint32_t colors[] = {0xff0000, 0x00ff00, 0x0000ff};
        lv_obj_set_pos(bar, 30 + i * 250, 70);
        lv_obj_set_size(bar, 240, 230);
        lv_obj_set_style_bg_color(bar, lv_color_hex(colors[i]), 0);
        lv_obj_add_event_cb(bar, touch_event, LV_EVENT_PRESSED, NULL);
    }
    counter = lv_label_create(screen);
    lv_obj_set_pos(counter, 30, 340);
    touch = lv_label_create(screen);
    lv_obj_set_pos(touch, 30, 390);
    lv_label_set_text(touch, "Touch each corner and color field");
    frame_tick(NULL);
    lv_timer_create(frame_tick, 250, NULL);
}
