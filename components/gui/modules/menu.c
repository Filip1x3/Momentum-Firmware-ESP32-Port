#include "menu.h"
#include "menu_style_port.h"

#include "../elements.h"
#include "../icon_animation_i.h"
#include "../icon_i.h"
#include "../canvas_i.h"
#include <assets_icons.h>
#include <furi.h>
#include <furi_hal.h>
#include <dolphin/dolphin.h>
#include <power/power_service/power.h>
#include <m-array.h>
#include <saved_struct.h>

static MenuStyle g_menu_style = MenuStyleList;

void menu_set_style(MenuStyle style) {
    if(((int)style < 0) || ((int)style >= (int)MenuStyleCount)) {
        style = MenuStyleList;
    }
    g_menu_style = style;
}

MenuStyle menu_get_style(void) {
    return g_menu_style;
}

void menu_set_momentum_style(uint32_t style) {
    menu_set_style(style < MenuStyleCount ? (MenuStyle)style : MenuStyleList);
}

uint32_t menu_get_momentum_style(void) {
    return (uint32_t)menu_get_style();
}

void lock_screen_set_style(LockScreenStyle style) {
    /* Kept for API compatibility; the port now uses Momentum exclusively. */
    UNUSED(style);
}

LockScreenStyle lock_screen_get_style(void) {
    return LockScreenStyleMomentum;
}

struct Menu {
    View* view;
    FuriTimer* scroll_timer;
};

typedef struct {
    const char* label;
    IconAnimation* icon;
    uint32_t index;
    MenuItemCallback callback;
    void* callback_context;
} MenuItem;

ARRAY_DEF(MenuItemArray, MenuItem, M_POD_OPLIST); //-V658

#define M_OPL_MenuItemArray_t() ARRAY_OPLIST(MenuItemArray, M_POD_OPLIST)

typedef struct {
    MenuItemArray_t items;
    size_t position;
    size_t scroll_counter;
    size_t vertical_offset;
    uint8_t hour, minute, charge, level;
    bool otg;
} MenuModel;

static void menu_process_up(Menu* menu);
static void menu_process_down(Menu* menu);
static void menu_process_left(Menu* menu);
static void menu_process_right(Menu* menu);
static void menu_process_ok(Menu* menu);
static void menu_set_position(Menu* menu, uint32_t position);

static void menu_get_name(MenuItem* item, FuriString* name, bool shorter) {
    furi_string_set(name, item->label);
    if(shorter) {
        if(!furi_string_cmp(name, "Momentum")) {
            furi_string_set(name, "MNTM");
            return;
        } else if(!furi_string_cmp(name, "125 kHz RFID")) {
            furi_string_set(name, "RFID");
            return;
        } else if(!furi_string_cmp(name, "Sub-GHz")) {
            furi_string_set(name, "SubGHz");
            return;
        }
    }
    if(furi_string_start_with_str(name, "[")) {
        size_t trim = furi_string_search_str(name, "] ", 1);
        if(trim != FURI_STRING_FAILURE) {
            furi_string_right(name, trim + 2);
        }
    }
}

static void menu_centered_icon(
    Canvas* canvas,
    MenuItem* item,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height) {
    canvas_draw_icon_animation(
        canvas,
        x + (width - item->icon->icon->width) / 2,
        y + (height - item->icon->icon->height) / 2,
        item->icon);
}

/* These layouts are expressed in the native 128x64 canvas, independently of
 * the physical TFT size. Keep circular indexing valid even with one item. */
static size_t menu_relative_index(size_t position, size_t count, int offset) {
    int32_t index = ((int32_t)position + offset) % (int32_t)count;
    return (size_t)(index < 0 ? index + (int32_t)count : index);
}

static void menu_status_update(Menu* menu) {
    DateTime now;
    PowerInfo info;
    furi_hal_rtc_get_datetime(&now);
    Power* power = furi_record_open(RECORD_POWER);
    power_get_info(power, &info);
    furi_record_close(RECORD_POWER);
    Dolphin* dolphin = furi_record_open(RECORD_DOLPHIN);
    DolphinStats stats = dolphin_stats(dolphin);
    furi_record_close(RECORD_DOLPHIN);
    with_view_model(menu->view, MenuModel* model, {
        model->hour = now.hour;
        model->minute = now.minute;
        model->charge = info.charge;
        model->otg = info.is_otg_enabled;
        model->level = stats.level;
    }, false);
}

/* Perspective-project both the frame and its icon, rather than squeezing a
 * full-size animation into a narrow rectangular neighbour card. */
static void menu_cover_card(Canvas* canvas, MenuItem* item, int x, int width, bool right) {
    int top_left = right ? 9 : 3;
    int top_right = right ? 3 : 9;
    canvas_set_color(canvas, ColorWhite);
    for(int column = 0; column < width; ++column) {
        int top = top_left + (top_right - top_left) * column / (width - 1);
        canvas_draw_line(canvas, x + column, top, x + column, 45 - top);
    }
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, x, top_left, x + width - 1, top_right);
    canvas_draw_line(canvas, x, 45 - top_left, x + width - 1, 45 - top_right);
    canvas_draw_line(canvas, x, top_left, x, 45 - top_left);
    canvas_draw_line(canvas, x + width - 1, top_right, x + width - 1, 45 - top_right);
    const Icon* icon = item->icon->icon;
    uint8_t* bitmap;
    compress_icon_decode(canvas->compress_icon, icon_animation_get_data(item->icon), &bitmap);
    int icon_width = width - 4;
    for(int dy = 0; dy < 14; ++dy) {
        int sy = dy * icon->height / 14;
        for(int dx = 0; dx < icon_width; ++dx) {
            int sx = dx * icon->width / icon_width;
            if(bitmap[sy * ((icon->width + 7) / 8) + sx / 8] & (1U << (sx % 8))) {
                int skew = (right ? -1 : 1) * (dx - icon_width / 2) * 3 / icon_width;
                canvas_draw_dot(canvas, x + 2 + dx, 16 + dy + skew);
            }
        }
    }
}

static size_t menu_scroll_counter(MenuModel* model, bool selected) {
    if(!selected) return 0;
    size_t scroll_counter = model->scroll_counter;
    if(scroll_counter > 0) {
        scroll_counter--;
    }
    return scroll_counter;
}

static void menu_draw_callback(Canvas* canvas, void* _model) {
    MenuModel* model = _model;

    canvas_clear(canvas);

    size_t position = model->position;
    size_t items_count = MenuItemArray_size(model->items);
    if(items_count) {
        MenuItem* item;
        size_t shift_position;
        FuriString* name = furi_string_alloc();

        switch(g_menu_style) {
        case MenuStyleDsi: {
            for(int8_t i = -2; i <= 2; i++) {
                shift_position = (size_t)(
                    ((int32_t)position + (int32_t)items_count + i) % (int32_t)items_count);
                item = MenuItemArray_get(model->items, shift_position);
                size_t width = 24;
                size_t height = 26;
                int32_t pos_x = 64;
                int32_t pos_y = 36;
                if(i == 0) {
                    width += 6;
                    height += 4;
                    elements_bold_rounded_frame(
                        canvas, pos_x - width / 2, pos_y - height / 2, width, height + 5);
                    canvas_set_font(canvas, FontBatteryPercent);
                    canvas_draw_str_aligned(
                        canvas, pos_x - 9, pos_y + height / 2 + 1, AlignCenter, AlignBottom, "S");
                    canvas_draw_str_aligned(
                        canvas, pos_x, pos_y + height / 2 + 1, AlignCenter, AlignBottom, "TAR");
                    canvas_draw_str_aligned(
                        canvas, pos_x + 9, pos_y + height / 2 + 1, AlignCenter, AlignBottom, "T");

                    canvas_draw_rframe(canvas, 0, 0, 128, 18, 3);
                    canvas_draw_line(canvas, 60, 18, 64, 26);
                    canvas_draw_line(canvas, 64, 26, 68, 18);
                    canvas_set_color(canvas, ColorWhite);
                    canvas_draw_line(canvas, 60, 17, 68, 17);
                    canvas_draw_box(canvas, 62, 21, 5, 2);
                    canvas_set_color(canvas, ColorBlack);

                    canvas_set_font(canvas, FontPrimary);
                    menu_get_name(item, name, false);
                    size_t scroll_counter = menu_scroll_counter(model, true);
                    elements_scrollable_text_line_str(
                        canvas,
                        (uint8_t)pos_x,
                        (uint8_t)(pos_y - height / 2 - 8),
                        124,
                        furi_string_get_cstr(name),
                        scroll_counter,
                        false,
                        true);
                } else {
                    pos_x += (width + 6) * i;
                    pos_y += 2;
                    elements_slightly_rounded_frame(
                        canvas, pos_x - width / 2, pos_y - height / 2, width, height);
                }
                menu_centered_icon(canvas, item, pos_x - 7, pos_y - 7, 14, 14);
            }
            elements_scrollbar_horizontal(canvas, 0, 61, 128, position, items_count);
            break;
        }
        case MenuStyleWii: {
            if(items_count > 6) {
                size_t last_row = (items_count - 1) / 3;
                size_t sel_row = position / 3;
                if(sel_row == last_row) {
                    shift_position = (last_row >= 2) ? (last_row - 1) * 3 : 0;
                } else if(sel_row == 0) {
                    shift_position = 0;
                } else {
                    shift_position = (sel_row - 1) * 3;
                }
            } else {
                shift_position = 0;
            }
            canvas_set_font(canvas, FontSecondary);
            size_t item_i;
            size_t x_off, y_off;
            for(uint8_t i = 0; i < 6; i++) {
                item_i = shift_position + i;
                if(item_i >= items_count) continue;
                x_off = (i % 3) * 43 + 1;
                y_off = (i / 3) * 32;
                bool selected = item_i == position;
                if(selected) {
                    elements_slightly_rounded_box(canvas, 0 + x_off, 0 + y_off, 40, 30);
                    canvas_set_color(canvas, ColorWhite);
                }
                item = MenuItemArray_get(model->items, item_i);
                menu_centered_icon(canvas, item, x_off, y_off, 40, 20);
                menu_get_name(item, name, true);
                size_t scroll_counter = menu_scroll_counter(model, selected);
                elements_scrollable_text_line_str(
                    canvas,
                    (uint8_t)(20 + x_off),
                    (uint8_t)(26 + y_off),
                    36,
                    furi_string_get_cstr(name),
                    scroll_counter,
                    false,
                    true);
                if(selected) {
                    canvas_set_color(canvas, ColorBlack);
                } else {
                    elements_slightly_rounded_frame(canvas, 0 + x_off, 0 + y_off, 40, 30);
                }
            }
            break;
        }
        case MenuStylePs4: {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 0, 8, "MNTM");
            char level[16];
            snprintf(level, sizeof(level), "Level %u", model->level);
            canvas_draw_str_aligned(canvas, 127, 8, AlignRight, AlignBottom, level);
            for(int8_t i = 0; i <= 4; ++i) {
                item = MenuItemArray_get(model->items, menu_relative_index(position, items_count, i));
                int32_t x = i == 0 ? 22 : 54 + (i - 1) * 21;
                canvas_draw_frame(canvas, x, 14, i == 0 ? 30 : 20, i == 0 ? 40 : 23);
                menu_centered_icon(canvas, item, x, 14, i == 0 ? 30 : 20, i == 0 ? 27 : 23);
                if(i == 0) {
                    canvas_set_custom_u8g2_font(canvas, u8g2_font_5x7_tr);
                    canvas_draw_str_aligned(canvas, 37, 51, AlignCenter, AlignBottom, "Start");
                    canvas_set_font(canvas, FontSecondary);
                    menu_get_name(item, name, false);
                    elements_scrollable_text_line_str(
                        canvas,
                        54,
                        51,
                        73,
                        furi_string_get_cstr(name),
                        menu_scroll_counter(model, true),
                        false,
                        false);
                }
            }
            elements_scrollbar_horizontal(canvas, 0, 63, 128, position, items_count);
            break;
        }
        case MenuStyleVertical: {
            canvas_set_font(canvas, FontSecondary);
            canvas_set_font_direction(canvas, CanvasDirectionBottomToTop);
            for(size_t column = 0; column < 8; ++column) {
                size_t index = menu_relative_index(position, items_count, column);
                item = MenuItemArray_get(model->items, index);
                int32_t x = column * 16;
                if(column == 0) canvas_draw_box(canvas, x, 0, 16, 64);
                if(column == 0) canvas_set_color(canvas, ColorWhite);
                canvas_draw_icon_ex(canvas, x + 1, 50, item->icon->icon, IconRotation270);
                menu_get_name(item, name, true);
                /* The complete menu is rotated, including text, not just the
                 * icons. Limit names to the 49px above the icon. */
                while(canvas_string_width(canvas, furi_string_get_cstr(name)) > 49 &&
                      furi_string_size(name)) {
                    furi_string_left(name, furi_string_size(name) - 1);
                }
                canvas_draw_str(canvas, x + 13, 47, furi_string_get_cstr(name));
                canvas_set_color(canvas, ColorBlack);
            }
            canvas_set_font_direction(canvas, CanvasDirectionLeftToRight);
            break;
        }
        case MenuStyleC64: {
            canvas_set_custom_u8g2_font(canvas, u8g2_font_5x7_tr);
            canvas_draw_str(canvas, 0, 7, "**FLIPPERWARE 64 BASIC**");
            canvas_draw_str(canvas, 0, 15, "123944 BASIC BYTES FREE");
            size_t page = position - (position % 10);
            for(size_t slot = 0; slot < 10; ++slot) {
                size_t index = page + slot;
                if(index >= items_count) break;
                int32_t x = slot >= 5 ? 64 : 0;
                int32_t y = 23 + (slot % 5) * 8;
                bool selected = index == position;
                if(selected) {
                    canvas_draw_box(canvas, x, y - 7, 64, 8);
                    canvas_set_color(canvas, ColorWhite);
                }
                item = MenuItemArray_get(model->items, index);
                menu_get_name(item, name, true);
                char number[16];
                snprintf(number, sizeof(number), "%u.", (unsigned)index);
                canvas_draw_str(canvas, x, y, number);
                elements_scrollable_text_line_str(
                    canvas,
                    (uint8_t)(x + canvas_string_width(canvas, number)),
                    (uint8_t)y,
                    64 - canvas_string_width(canvas, number),
                    furi_string_get_cstr(name),
                    menu_scroll_counter(model, selected),
                    false,
                    false);
                if(selected) canvas_set_color(canvas, ColorBlack);
            }
            break;
        }
        case MenuStyleCompact: {
            canvas_set_custom_u8g2_font(canvas, u8g2_font_micro_tr);
            size_t page = position - (position % 16);
            for(size_t slot = 0; slot < 16; ++slot) {
                size_t index = page + slot;
                if(index >= items_count) break;
                int32_t x = slot >= 8 ? 64 : 0;
                int32_t y = (slot % 8) * 8;
                bool selected = index == position;
                if(selected) {
                    canvas_draw_box(canvas, x, y, 64, 8);
                    canvas_set_color(canvas, ColorWhite);
                }
                item = MenuItemArray_get(model->items, index);
                menu_get_name(item, name, true);
                elements_scrollable_text_line_str(
                    canvas,
                    (uint8_t)x,
                    (uint8_t)(y + 7),
                    62,
                    furi_string_get_cstr(name),
                    menu_scroll_counter(model, selected),
                    false,
                    false);
                if(selected) canvas_set_color(canvas, ColorBlack);
            }
            break;
        }
        case MenuStyleMNTM: {
            DateTime now;
            furi_hal_rtc_get_datetime(&now);
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 3, 11, "MNTM");
            canvas_draw_line(canvas, 3, 15, 60, 15);
            canvas_draw_line(canvas, 3, 17, 60, 17);
            /* Outlined selection arrow joining the dashboard and the list. */
            canvas_draw_line(canvas, 62, 9, 71, 9);
            canvas_draw_line(canvas, 71, 9, 71, 5);
            canvas_draw_line(canvas, 71, 5, 79, 12);
            canvas_draw_line(canvas, 79, 12, 71, 19);
            canvas_draw_line(canvas, 71, 19, 71, 15);
            canvas_draw_line(canvas, 71, 15, 62, 15);
            canvas_draw_line(canvas, 62, 15, 62, 9);
            item = MenuItemArray_get(model->items, position);
            canvas_draw_rframe(canvas, 43, 23, 34, 34, 6);
            canvas_draw_rframe(canvas, 42, 22, 36, 36, 6);
            menu_centered_icon(canvas, item, 43, 23, 34, 34);
            canvas_set_font(canvas, FontSecondary);
            char status[16];
            snprintf(status, sizeof(status), "%02u:%02u", now.hour, now.minute);
            canvas_draw_str(canvas, 3, 35, status);
            snprintf(status, sizeof(status), "%u%%", model->charge);
            canvas_draw_str(canvas, 3, 46, status);
            canvas_draw_str(canvas, 3, 57, model->otg ? "5v:On" : "5v:Off");
            for(size_t row = 0; row < 5; ++row) {
                size_t index = menu_relative_index(position, items_count, row);
                item = MenuItemArray_get(model->items, index);
                menu_get_name(item, name, true);
                elements_scrollable_text_line_str(
                    canvas,
                    82,
                    (uint8_t)(14 + row * 10),
                    43,
                    furi_string_get_cstr(name),
                    menu_scroll_counter(model, row == 0),
                    false,
                    false);
            }
            break;
        }
        case MenuStyleCoverFlow: {
            for(int distance = 4; distance >= 1; --distance) {
                item = MenuItemArray_get(model->items, menu_relative_index(position, items_count, -distance));
                menu_cover_card(canvas, item, 44 - distance * 14, 19, false);
                item = MenuItemArray_get(model->items, menu_relative_index(position, items_count, distance));
                menu_cover_card(canvas, item, 65 + distance * 14, 19, true);
            }
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_rbox(canvas, 44, 2, 40, 40, 3);
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_rframe(canvas, 44, 2, 40, 40, 3);
            item = MenuItemArray_get(model->items, position);
            menu_centered_icon(canvas, item, 44, 2, 40, 40);
            menu_get_name(item, name, false);
            canvas_set_font(canvas, FontPrimary);
            elements_scrollable_text_line_str(canvas, 64, 55, 124,
                furi_string_get_cstr(name), menu_scroll_counter(model, true), false, true);
            elements_scrollbar_horizontal(canvas, 0, 61, 128, position, items_count);
            break;
        }
        case MenuStyleList:
        default: {
            canvas_set_font(canvas, FontSecondary);
            shift_position = (0 + position + items_count - 1) % items_count;
            item = MenuItemArray_get(model->items, shift_position);
            canvas_draw_icon_animation(canvas, 4, 3, item->icon);
            canvas_draw_str(canvas, 22, 14, item->label);
            canvas_set_font(canvas, FontPrimary);
            shift_position = (1 + position + items_count - 1) % items_count;
            item = MenuItemArray_get(model->items, shift_position);
            canvas_draw_icon_animation(canvas, 4, 25, item->icon);
            canvas_draw_str(canvas, 22, 36, item->label);
            canvas_set_font(canvas, FontSecondary);
            shift_position = (2 + position + items_count - 1) % items_count;
            item = MenuItemArray_get(model->items, shift_position);
            canvas_draw_icon_animation(canvas, 4, 47, item->icon);
            canvas_draw_str(canvas, 22, 58, item->label);
            elements_frame(canvas, 0, 21, 128 - 5, 21);
            elements_scrollbar(canvas, position, items_count);
            break;
        }
        }

        furi_string_free(name);
    } else {
        canvas_draw_str(canvas, 2, 32, "Empty");
        elements_scrollbar(canvas, 0, 0);
    }
}

static bool menu_input_callback(InputEvent* event, void* context) {
    Menu* menu = context;
    bool consumed = true;

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        switch(event->key) {
        case InputKeyUp:
            menu_process_up(menu);
            break;
        case InputKeyDown:
            menu_process_down(menu);
            break;
        case InputKeyLeft:
            menu_process_left(menu);
            break;
        case InputKeyRight:
            menu_process_right(menu);
            break;
        case InputKeyOk:
            if(event->type != InputTypeRepeat) {
                menu_process_ok(menu);
            }
            break;
        default:
            consumed = false;
            break;
        }
    } else {
        consumed = false;
    }

    return consumed;
}

static void menu_scroll_timer_callback(void* context) {
    Menu* menu = context;
    with_view_model(menu->view, MenuModel* model, { model->scroll_counter++; }, true);
}

static void menu_enter(void* context) {
    Menu* menu = context;
    if(g_menu_style == MenuStyleMNTM || g_menu_style == MenuStylePs4) menu_status_update(menu);
    with_view_model(
        menu->view,
        MenuModel* model,
        {
            if(MenuItemArray_size(model->items)) {
                MenuItem* item = MenuItemArray_get(model->items, model->position);
                icon_animation_start(item->icon);
            }
            model->scroll_counter = 0;
        },
        true);
    furi_timer_start(menu->scroll_timer, 333);
}

static void menu_exit(void* context) {
    Menu* menu = context;
    with_view_model(
        menu->view,
        MenuModel* model,
        {
            if(MenuItemArray_size(model->items)) {
                MenuItem* item = MenuItemArray_get(model->items, model->position);
                icon_animation_stop(item->icon);
            }
        },
        false);
    furi_timer_stop(menu->scroll_timer);
}

Menu* menu_alloc(void) {
    Menu* menu = malloc(sizeof(Menu));
    menu->view = view_alloc();
    view_set_context(menu->view, menu);
    view_allocate_model(menu->view, ViewModelTypeLocking, sizeof(MenuModel));
    view_set_draw_callback(menu->view, menu_draw_callback);
    view_set_input_callback(menu->view, menu_input_callback);
    view_set_enter_callback(menu->view, menu_enter);
    view_set_exit_callback(menu->view, menu_exit);

    menu->scroll_timer = furi_timer_alloc(menu_scroll_timer_callback, FuriTimerTypePeriodic, menu);


    with_view_model(
        menu->view,
        MenuModel* model,
        {
            MenuItemArray_init(model->items);
            model->position = 0;
            model->scroll_counter = 0;
            model->vertical_offset = 0;
        },
        true);

    return menu;
}

void menu_free(Menu* menu) {
    furi_check(menu);

    menu_reset(menu);
    furi_timer_stop(menu->scroll_timer);
    furi_timer_free(menu->scroll_timer);
    with_view_model(menu->view, MenuModel* model, { MenuItemArray_clear(model->items); }, false);
    view_free(menu->view);

    free(menu);
}

View* menu_get_view(Menu* menu) {
    furi_check(menu);
    return menu->view;
}

void menu_add_item(
    Menu* menu,
    const char* label,
    const Icon* icon,
    uint32_t index,
    MenuItemCallback callback,
    void* context) {
    furi_check(menu);
    furi_check(label);

    MenuItem* item = NULL;
    with_view_model(
        menu->view,
        MenuModel* model,
        {
            item = MenuItemArray_push_new(model->items);
            item->label = label;
            item->icon = icon ? icon_animation_alloc(icon) : icon_animation_alloc(&A_Plugins_14);
            view_tie_icon_animation(menu->view, item->icon);
            item->index = index;
            item->callback = callback;
            item->callback_context = context;
        },
        true);
}

void menu_reset(Menu* menu) {
    furi_check(menu);
    with_view_model(
        menu->view,
        MenuModel* model,
        {
            for
                M_EACH(item, model->items, MenuItemArray_t) {
                    icon_animation_stop(item->icon);
                    icon_animation_free(item->icon);
                }

            MenuItemArray_reset(model->items);
            model->position = 0;
        },
        true);
}

static void menu_set_position(Menu* menu, uint32_t position) {
    furi_check(menu);

    with_view_model(
        menu->view,
        MenuModel* model,
        {
            if(position < MenuItemArray_size(model->items) && position != model->position) {
                model->scroll_counter = 0;

                MenuItem* item = MenuItemArray_get(model->items, model->position);
                icon_animation_stop(item->icon);

                item = MenuItemArray_get(model->items, position);
                icon_animation_start(item->icon);

                model->position = position;
            }
        },
        true);
}

void menu_set_selected_item(Menu* menu, uint32_t index) {
    furi_check(menu);

    with_view_model(
        menu->view,
        MenuModel* model,
        {
            if(index < MenuItemArray_size(model->items)) {
                model->position = index;
            }
        },
        true);
}

static void menu_process_up(Menu* menu) {
    size_t position;
    with_view_model(
        menu->view,
        MenuModel* model,
        {
            position = model->position;
            size_t count = MenuItemArray_size(model->items);

            switch(g_menu_style) {
            case MenuStylePs4:
            case MenuStyleVertical:
            case MenuStyleC64:
            case MenuStyleCompact:
            case MenuStyleMNTM:
            case MenuStyleCoverFlow:
                if(count) position = menu_relative_index(position, count, -1);
                break;
            case MenuStyleWii:
                if(position == 0) {
                    position = count - 1;
                } else {
                    position--;
                }
                break;
            case MenuStyleDsi:
                if(position > 0) {
                    position--;
                } else {
                    position = count - 1;
                }
                break;
            case MenuStyleList:
            default:
                if(position > 0) {
                    position--;
                } else {
                    position = count - 1;
                }
                break;
            }
        },
        false);
    menu_set_position(menu, position);
}

static void menu_process_down(Menu* menu) {
    size_t position;
    with_view_model(
        menu->view,
        MenuModel* model,
        {
            position = model->position;
            size_t count = MenuItemArray_size(model->items);

            switch(g_menu_style) {
            case MenuStyleWii:
                if(position < count - 1) {
                    position++;
                } else {
                    position = 0;
                }
                break;
            case MenuStyleDsi:
                if(position < count - 1) {
                    position++;
                } else {
                    position = 0;
                }
                break;
            case MenuStyleList:
            default:
                if(position < count - 1) {
                    position++;
                } else {
                    position = 0;
                }
                break;
            }
        },
        false);
    menu_set_position(menu, position);
}

static void menu_process_left(Menu* menu) {
    size_t position;
    with_view_model(
        menu->view,
        MenuModel* model,
        {
            position = model->position;
            size_t count = MenuItemArray_size(model->items);

            switch(g_menu_style) {
            case MenuStylePs4:
            case MenuStyleVertical:
            case MenuStyleC64:
            case MenuStyleCompact:
            case MenuStyleMNTM:
            case MenuStyleCoverFlow:
                if(count) position = menu_relative_index(position, count, -1);
                break;
            case MenuStyleWii:
                if(position > 0) {
                    position--;
                } else {
                    position = count - 1;
                }
                break;
            case MenuStyleDsi: {
                size_t vertical_offset = model->vertical_offset;
                if(position > 0) {
                    position--;
                    if(vertical_offset && vertical_offset == position) {
                        vertical_offset--;
                    }
                } else {
                    position = count - 1;
                    vertical_offset = count - 8;
                }
                model->vertical_offset = vertical_offset;
                break;
            }
            case MenuStyleList:
            default:
                break;
            }
        },
        false);
    menu_set_position(menu, position);
}

static void menu_process_right(Menu* menu) {
    size_t position;
    with_view_model(
        menu->view,
        MenuModel* model,
        {
            position = model->position;
            size_t count = MenuItemArray_size(model->items);

            switch(g_menu_style) {
            case MenuStylePs4:
            case MenuStyleVertical:
            case MenuStyleC64:
            case MenuStyleCompact:
            case MenuStyleMNTM:
            case MenuStyleCoverFlow:
                if(count) position = menu_relative_index(position, count, 1);
                break;
            case MenuStyleWii:
                if(position < count - 1) {
                    position++;
                } else {
                    position = 0;
                }
                break;
            case MenuStyleDsi: {
                size_t vertical_offset = model->vertical_offset;
                if(position < count - 1) {
                    position++;
                    if(vertical_offset < count - 8 && vertical_offset == position - 7) {
                        vertical_offset++;
                    }
                } else {
                    position = 0;
                    vertical_offset = 0;
                }
                model->vertical_offset = vertical_offset;
                break;
            }
            case MenuStyleList:
            default:
                break;
            }
        },
        false);
    menu_set_position(menu, position);
}

static void menu_process_ok(Menu* menu) {
    MenuItem* item = NULL;
    with_view_model(
        menu->view,
        MenuModel* model,
        {
            if(MenuItemArray_size(model->items)) {
                item = MenuItemArray_get(model->items, model->position);
            }
        },
        true);
    if(item && item->callback) {
        item->callback(item->callback_context, item->index);
    }
}
