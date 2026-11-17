#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef min
#undef max

#define UNUSED(x) (void)(x)
#define furi_assert(x) assert(x)
#define FURI_LOG_D(...) ((void)0)
#define FURI_LOG_E(...) ((void)0)
#define FuriWaitForever UINT32_MAX
#define RECORD_GUI "gui"
#define RECORD_NOTIFICATION "notification"
typedef enum {FuriStatusOk, FuriStatusErrorTimeout} FuriStatus;
typedef enum {FuriMutexTypeNormal} FuriMutexType;
typedef struct {bool alive; bool held;} FuriMutex;
typedef enum {InputTypePress, InputTypeRelease, InputTypeLong} InputType;
typedef enum {InputKeyOk, InputKeyUp, InputKeyDown, InputKeyLeft, InputKeyRight, InputKeyBack} InputKey;
typedef struct {InputType type; InputKey key;} InputEvent;
typedef struct {bool alive;} FuriMessageQueue;
typedef struct {int unused;} Canvas;
typedef struct {int unused;} Gui;
typedef struct {int unused;} Icon;
typedef struct {int unused;} NotificationApp;
typedef struct {int unused;} View;
typedef struct {
    void (*draw)(Canvas*, void*);
    void* draw_ctx;
    void (*input)(InputEvent*, void*);
    void* input_ctx;
    bool attached;
} ViewPort;
typedef struct {
    void (*result)(void*);
    void* ctx;
    char* buffer;
    size_t size;
    View view;
} TextInput;
typedef struct {const char* text; View view;} TextBox;
typedef struct {
    bool (*back)(void*);
    void* context;
    unsigned views;
    bool stopped;
} ViewDispatcher;
enum {FontPrimary, TextBoxFontText, GuiLayerFullscreen, ViewDispatcherTypeFullscreen};
enum {DolphinDeedPluginGameStart, DolphinDeedPluginGameWin};
typedef int NotificationMessage;
typedef const NotificationMessage* NotificationSequence[];
static const NotificationMessage message_vibro_on = 0, message_vibro_off = 0, message_note_ds4 = 0,
    message_delay_10 = 0, message_delay_25 = 0, message_delay_50 = 0, message_sound_off = 0;
static const NotificationSequence sequence_display_backlight_enforce_on = {NULL};
static const NotificationSequence sequence_display_backlight_enforce_auto = {NULL};
static const Icon I_end = {0}, I_box = {0}, I_unbreakbox = {0}, I_bomb0 = {0},
    I_bomb1 = {0}, I_bomb2 = {0}, I_explore = {0}, I_playerright = {0},
    I_playerleft = {0}, I_enemy1 = {0}, I_enemyright = {0}, I_enemyleft = {0};

FuriMutex* furi_mutex_alloc(FuriMutexType type);
void furi_mutex_free(FuriMutex* mutex);
FuriStatus furi_mutex_acquire(FuriMutex* mutex, uint32_t timeout);
FuriStatus furi_mutex_release(FuriMutex* mutex);
FuriMessageQueue* furi_message_queue_alloc(unsigned count, unsigned size);
void furi_message_queue_free(FuriMessageQueue* queue);
FuriStatus furi_message_queue_get(FuriMessageQueue* queue, void* event, uint32_t timeout);
FuriStatus furi_message_queue_put(FuriMessageQueue* queue, const void* event, uint32_t timeout);
uint32_t furi_get_tick(void);
void* furi_record_open(const char* name);
void furi_record_close(const char* name);
ViewPort* view_port_alloc(void);
void view_port_free(ViewPort* viewport);
void view_port_draw_callback_set(ViewPort* viewport, void (*callback)(Canvas*, void*), void* context);
void view_port_input_callback_set(ViewPort* viewport, void (*callback)(InputEvent*, void*), void* context);
void view_port_enabled_set(ViewPort* viewport, bool enabled);
void view_port_update(ViewPort* viewport);
void gui_add_view_port(Gui* gui, ViewPort* viewport, int layer);
void gui_remove_view_port(Gui* gui, ViewPort* viewport);
void canvas_clear(Canvas* canvas);
void canvas_draw_icon(Canvas* canvas, int x, int y, const Icon* icon);
void canvas_set_font(Canvas* canvas, int font);
void canvas_draw_str(Canvas* canvas, int x, int y, const char* text);
void notification_message(NotificationApp* app, const NotificationSequence* sequence);
void notification_message_block(NotificationApp* app, const NotificationSequence* sequence);
void dolphin_deed(int deed);
ViewDispatcher* view_dispatcher_alloc(void);
void view_dispatcher_free(ViewDispatcher* dispatcher);
void view_dispatcher_add_view(ViewDispatcher* dispatcher, unsigned id, View* view);
void view_dispatcher_remove_view(ViewDispatcher* dispatcher, unsigned id);
void view_dispatcher_attach_to_gui(ViewDispatcher* dispatcher, Gui* gui, int type);
void view_dispatcher_set_navigation_event_callback(ViewDispatcher* dispatcher, bool (*callback)(void*));
void view_dispatcher_set_event_callback_context(ViewDispatcher* dispatcher, void* context);
void view_dispatcher_switch_to_view(ViewDispatcher* dispatcher, unsigned id);
void view_dispatcher_run(ViewDispatcher* dispatcher);
void view_dispatcher_stop(ViewDispatcher* dispatcher);
TextInput* text_input_alloc(void);
void text_input_free(TextInput* input);
View* text_input_get_view(TextInput* input);
void text_input_set_result_callback(TextInput* input, void (*callback)(void*), void* context,
                                  char* buffer, size_t size, bool clear);
void text_input_set_header_text(TextInput* input, const char* text);
TextBox* text_box_alloc(void);
void text_box_free(TextBox* box);
View* text_box_get_view(TextBox* box);
void text_box_set_font(TextBox* box, int font);
void text_box_set_text(TextBox* box, const char* text);
