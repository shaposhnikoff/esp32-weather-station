#include "ui_internal.h"
#include <math.h>

typedef struct {
    weather_condition_t condition;
    int daylight;
    double phase;
    unsigned solar;
} icon_data_t;

static void circle(lv_draw_ctx_t *context, lv_point_t center, int radius, uint32_t color)
{
    lv_draw_rect_dsc_t draw;
    lv_draw_rect_dsc_init(&draw);
    draw.bg_color = lv_color_hex(color);
    draw.radius = LV_RADIUS_CIRCLE;
    lv_area_t area = {center.x - radius, center.y - radius,
                      center.x + radius, center.y + radius};
    lv_draw_rect(context, &draw, &area);
}

static void stroke(lv_draw_ctx_t *context, const lv_point_t *points, uint32_t color, int width)
{
    lv_draw_line_dsc_t draw;
    lv_draw_line_dsc_init(&draw);
    draw.color = lv_color_hex(color);
    draw.width = width;
    draw.round_start = true;
    draw.round_end = true;
    lv_draw_line(context, &draw, points, points + 1);
}

static void draw_icon(lv_event_t *event)
{
    icon_data_t *data = lv_event_get_user_data(event);
    if (lv_event_get_code(event) == LV_EVENT_DELETE) { lv_mem_free(data); return; }
    lv_obj_t *object = lv_event_get_target(event);
    lv_area_t area;
    lv_obj_get_coords(object, &area);
    lv_draw_ctx_t *context = lv_event_get_draw_ctx(event);
    const int size = lv_obj_get_width(object), x = area.x1, y = area.y1;
    const int cx = x + size / 2, cy = y + size / 2;
    if (data->solar) {
        circle(context, (lv_point_t){cx, cy + size / 8}, size / 4, UI_SUN);
        lv_draw_rect_dsc_t cover;
        lv_draw_rect_dsc_init(&cover);
        cover.bg_color = lv_color_hex(UI_BG);
        const lv_area_t lower = {x, cy + size / 8, x + size, y + size};
        lv_draw_rect(context, &cover, &lower);
        const lv_point_t horizon[] = {{x + 2, cy + size / 8}, {x + size - 2, cy + size / 8}};
        stroke(context, horizon, UI_SUN, 2);
        const int tip = data->solar == 1 ? y + 2 : y + size / 4;
        const int tail = data->solar == 1 ? y + size / 4 : y + 2;
        const lv_point_t arrow[] = {{cx, tail}, {cx, tip}};
        const lv_point_t left[] = {{cx, tip}, {cx - 3, tip + (data->solar == 1 ? 3 : -3)}};
        const lv_point_t right[] = {{cx, tip}, {cx + 3, tip + (data->solar == 1 ? 3 : -3)}};
        stroke(context, arrow, UI_SUN, 1);
        stroke(context, left, UI_SUN, 1);
        stroke(context, right, UI_SUN, 1);
        return;
    }
    if (isfinite(data->phase)) {
        const int radius = size / 3;
        circle(context, (lv_point_t){cx, cy}, radius, UI_DIVIDER);
        const double terminator = cos(2 * 3.141592653589793 * data->phase);
        for (int dy = -radius; dy <= radius; ++dy) {
            const int limb = (int)sqrt(radius * radius - dy * dy);
            const int left = data->phase < .5 ? (int)(terminator * limb) : -limb;
            const int right = data->phase < .5 ? limb : (int)(-terminator * limb);
            const lv_point_t points[] = {{cx + left, cy + dy}, {cx + right, cy + dy}};
            stroke(context, points, 0x91cde5, 1);
        }
        return;
    }
    if ((data->condition == WEATHER_CLEAR || data->condition == WEATHER_PARTLY_CLOUDY) &&
        data->daylight >= 0) {
        if (data->daylight == 0) {
            circle(context, (lv_point_t){cx, cy}, size / 4, 0x91cde5);
            circle(context, (lv_point_t){cx + size / 8, cy - size / 8}, size / 4, UI_BG);
        } else {
            circle(context, (lv_point_t){cx, cy}, size / 5, UI_SUN);
            for (int i = 0; i < 8; ++i) {
                const double angle = i * 3.141592653589793 / 4;
                const lv_point_t points[] = {
                    {cx + (int)(cos(angle) * size * .32), cy + (int)(sin(angle) * size * .32)},
                    {cx + (int)(cos(angle) * size * .43), cy + (int)(sin(angle) * size * .43)}
                };
                stroke(context, points, UI_SUN, size > 40 ? 3 : 2);
            }
        }
        if (data->condition == WEATHER_CLEAR) return;
    }
    if (data->condition == WEATHER_UNKNOWN) {
        lv_draw_label_dsc_t label;
        lv_draw_label_dsc_init(&label);
        label.font = &weather_font_20;
        label.color = lv_color_hex(UI_SECONDARY);
        label.align = LV_TEXT_ALIGN_CENTER;
        lv_draw_label(context, &label, &area, "?", NULL);
        return;
    }
    circle(context, (lv_point_t){x + size * 3 / 8, cy}, size / 5, 0xdce7f5);
    circle(context, (lv_point_t){x + size * 5 / 8, cy - size / 10}, size / 4, 0xdce7f5);
    circle(context, (lv_point_t){x + size * 4 / 5, cy + size / 12}, size / 7, 0xdce7f5);
    const lv_point_t base[] = {{x + size / 4, cy + size / 7}, {x + size * 4 / 5, cy + size / 7}};
    stroke(context, base, 0xdce7f5, size / 5);
    switch (data->condition) {
    case WEATHER_RAIN: case WEATHER_SNOW:
        for (int i = 0; i < 3; ++i) {
            const int px = x + size / 3 + i * size / 5;
            const lv_point_t points[] = {{px, y + size * 3 / 4},
                {px - size / 12, y + size * 7 / 8}};
            stroke(context, points, UI_RAIN, 2);
            if (data->condition == WEATHER_SNOW) {
                const lv_point_t cross[] = {{px - size / 12, y + size * 3 / 4},
                                           {px, y + size * 7 / 8}};
                stroke(context, cross, 0xdce7f5, 2);
            }
        }
        break;
    case WEATHER_FOG:
        for (int i = 0; i < 3; ++i) {
            const lv_point_t points[] = {{x + size / 5, y + size * 7 / 10 + i * size / 10},
                {x + size * 4 / 5, y + size * 7 / 10 + i * size / 10}};
            stroke(context, points, 0x91cde5, 1);
        }
        break;
    case WEATHER_THUNDER: {
        const lv_point_t points[] = {
            {x + size * 3 / 5, y + size * 3 / 5},
            {x + size * 2 / 5, y + size * 4 / 5},
            {x + size * 3 / 5, y + size * 4 / 5},
            {x + size * 2 / 5, y + size * 19 / 20},
        };
        for (int i = 0; i < 3; ++i) stroke(context, points + i, UI_SUN, 2);
        break;
    }
    case WEATHER_CLEAR: case WEATHER_PARTLY_CLOUDY: case WEATHER_CLOUDY: case WEATHER_UNKNOWN:
        break;
    }
}

lv_obj_t *ui_icon_create(lv_obj_t *parent, int x, int y, int size)
{
    lv_obj_t *object = ui_box(parent, x, y, size, size);
    lv_obj_set_style_bg_opa(object, LV_OPA_TRANSP, 0);
    icon_data_t *data = lv_mem_alloc(sizeof(*data));
    LV_ASSERT_MALLOC(data);
    *data = (icon_data_t){.condition = WEATHER_UNKNOWN, .daylight = -1, .phase = NAN};
    lv_obj_set_user_data(object, data);
    lv_obj_add_event_cb(object, draw_icon, LV_EVENT_DRAW_MAIN, data);
    lv_obj_add_event_cb(object, draw_icon, LV_EVENT_DELETE, data);
    return object;
}

void ui_icon_set(lv_obj_t *object, weather_condition_t condition, int daylight)
{
    icon_data_t *data = lv_obj_get_user_data(object);
    if (data->condition == condition && data->daylight == daylight &&
        isnan(data->phase) && !data->solar) return;
    data->condition = condition;
    data->daylight = daylight;
    data->phase = NAN;
    data->solar = 0;
    lv_obj_invalidate(object);
}

void ui_icon_moon(lv_obj_t *object, double phase)
{
    icon_data_t *data = lv_obj_get_user_data(object);
    data->phase = phase;
    data->solar = 0;
    lv_obj_invalidate(object);
}

void ui_icon_solar(lv_obj_t *object, bool rising)
{
    icon_data_t *data = lv_obj_get_user_data(object);
    data->solar = rising ? 1 : 2;
    data->phase = NAN;
    lv_obj_invalidate(object);
}
