// Exercise the real app sources with strict mutex/callback lifetime checks.
// GUI drawing is deliberately scheduled at viewport publication and on every
// queue timeout, the two ordering cases that used to reset the ESP32 port.
#include <furi.h>
#include "../../../applications_user/momentum_tools/caesarcipher/caesar_cipher.c"
#include "../../../applications_user/momentum_games/bomberduck/bomberduck.c"

static FuriMutex test_mutex;
static FuriMessageQueue test_queue;
static unsigned mutex_frees, queue_reads, viewport_count, draws;
static uint32_t tick;
static TextInput* current_input;

FuriMutex* furi_mutex_alloc(FuriMutexType type) {
    UNUSED(type); assert(!test_mutex.alive);
    test_mutex = (FuriMutex){.alive = true}; return &test_mutex;
}
void furi_mutex_free(FuriMutex* mutex) {
    assert(mutex && mutex->alive && !mutex->held);
    mutex->alive = false; mutex_frees++;
}
FuriStatus furi_mutex_acquire(FuriMutex* mutex, uint32_t timeout) {
    assert(mutex && mutex->alive);
    if(mutex->held) {assert(timeout == 0); return FuriStatusErrorTimeout;}
    mutex->held = true; return FuriStatusOk;
}
FuriStatus furi_mutex_release(FuriMutex* mutex) {
    assert(mutex && mutex->alive && mutex->held);
    mutex->held = false; return FuriStatusOk;
}
FuriMessageQueue* furi_message_queue_alloc(unsigned count, unsigned size) {
    assert(count == 8 && size == sizeof(InputEvent) && !test_queue.alive);
    test_queue.alive = true; queue_reads = 0; return &test_queue;
}
void furi_message_queue_free(FuriMessageQueue* queue) {
    assert(queue->alive && viewport_count == 0); queue->alive = false;
}
FuriStatus furi_message_queue_get(FuriMessageQueue* queue, void* data, uint32_t timeout) {
    assert(queue->alive && timeout == 100 && !test_mutex.held);
    queue_reads++; tick += 100;
    InputEvent* event = data;
    if(queue_reads == 3) {*event = (InputEvent){InputTypePress, InputKeyRight}; return FuriStatusOk;}
    if(queue_reads == 68) {*event = (InputEvent){InputTypeLong, InputKeyBack}; return FuriStatusOk;}
    assert(queue_reads < 68); return FuriStatusErrorTimeout;
}
FuriStatus furi_message_queue_put(FuriMessageQueue* queue, const void* data, uint32_t timeout) {
    assert(queue->alive && data && timeout == 0); return FuriStatusOk;
}
uint32_t furi_get_tick(void) {return tick;}
void* furi_record_open(const char* name) {assert(name); return (void*)1;}
void furi_record_close(const char* name) {assert(name);}
ViewPort* view_port_alloc(void) {return calloc(1, sizeof(ViewPort));}
void view_port_free(ViewPort* viewport) {assert(!viewport->attached); free(viewport);}
void view_port_draw_callback_set(ViewPort* viewport, void (*cb)(Canvas*, void*), void* ctx) {
    viewport->draw = cb; viewport->draw_ctx = ctx;
}
void view_port_input_callback_set(ViewPort* viewport, void (*cb)(InputEvent*, void*), void* ctx) {
    viewport->input = cb; viewport->input_ctx = ctx;
}
void view_port_enabled_set(ViewPort* viewport, bool enabled) {UNUSED(viewport); UNUSED(enabled);}
void view_port_update(ViewPort* viewport) {
    Canvas canvas; assert(viewport->attached && !test_mutex.held);
    viewport->draw(&canvas, viewport->draw_ctx); draws++;
}
void gui_add_view_port(Gui* gui, ViewPort* viewport, int layer) {
    UNUSED(gui); UNUSED(layer);
    assert(world.player->x == 1 && world.player->y == 1);
    viewport->attached = true; viewport_count++; view_port_update(viewport);
}
void gui_remove_view_port(Gui* gui, ViewPort* viewport) {
    UNUSED(gui); assert(viewport->attached && test_queue.alive);
    viewport->attached = false; viewport_count--;
    // Last in-flight input must still see a valid queue during detachment.
    InputEvent event = {InputTypeRelease, InputKeyBack};
    viewport->input(&event, viewport->input_ctx);
}
void canvas_clear(Canvas* c) {UNUSED(c); assert(test_mutex.held);}
void canvas_draw_icon(Canvas* c, int x, int y, const Icon* icon) {
    UNUSED(c); UNUSED(x); UNUSED(y); assert(icon);
}
void canvas_set_font(Canvas* c, int font) {UNUSED(c); UNUSED(font);}
void canvas_draw_str(Canvas* c, int x, int y, const char* str) {
    UNUSED(c); UNUSED(x); UNUSED(y); assert(str);
}
void notification_message(NotificationApp* app, const NotificationSequence* sequence) {
    assert(app && sequence);
}
void notification_message_block(NotificationApp* app, const NotificationSequence* sequence) {
    notification_message(app, sequence);
}
void dolphin_deed(int deed) {UNUSED(deed);}
ViewDispatcher* view_dispatcher_alloc(void) {return calloc(1, sizeof(ViewDispatcher));}
void view_dispatcher_free(ViewDispatcher* dispatcher) {assert(dispatcher->views == 0); free(dispatcher);}
void view_dispatcher_add_view(ViewDispatcher* dispatcher, unsigned id, View* view) {
    UNUSED(id); assert(view); dispatcher->views++;
}
void view_dispatcher_remove_view(ViewDispatcher* dispatcher, unsigned id) {
    UNUSED(id); assert(dispatcher->views); dispatcher->views--;
}
void view_dispatcher_attach_to_gui(ViewDispatcher* d, Gui* gui, int type) {UNUSED(d); UNUSED(gui); UNUSED(type);}
void view_dispatcher_set_navigation_event_callback(ViewDispatcher* d, bool (*callback)(void*)) {d->back = callback;}
void view_dispatcher_set_event_callback_context(ViewDispatcher* d, void* context) {d->context = context;}
void view_dispatcher_switch_to_view(ViewDispatcher* d, unsigned id) {assert(d->views == 2 && id < 2);}
void view_dispatcher_run(ViewDispatcher* d) {
    TextInput* input = current_input;
    strcpy(input->buffer, "Abc?"); input->result(input->ctx);
    CaesarState* state = d->context;
    assert(strncmp(state->output, "BCD?\nCDE?\n", 10) == 0);
    memset(input->buffer, 'z', input->size - 1); input->buffer[input->size - 1] = 0;
    input->result(input->ctx);
    assert(strlen(state->output) == 25 * input->size);
    assert(state->output[0] == 'A');
    assert(d->back(d->context) && d->stopped);
}
void view_dispatcher_stop(ViewDispatcher* d) {d->stopped = true;}
TextInput* text_input_alloc(void) {current_input = calloc(1, sizeof(TextInput)); return current_input;}
void text_input_free(TextInput* input) {assert(input == current_input); current_input = NULL; free(input);}
View* text_input_get_view(TextInput* input) {return &input->view;}
void text_input_set_result_callback(TextInput* input, void (*cb)(void*), void* ctx,
                                  char* buffer, size_t size, bool clear) {
    assert(clear && buffer[0] == 0 && size == 256);
    input->result = cb; input->ctx = ctx; input->buffer = buffer; input->size = size;
}
void text_input_set_header_text(TextInput* input, const char* text) {assert(input && text);}
TextBox* text_box_alloc(void) {return calloc(1, sizeof(TextBox));}
void text_box_free(TextBox* box) {free(box);}
View* text_box_get_view(TextBox* box) {return &box->view;}
void text_box_set_font(TextBox* box, int font) {UNUSED(font); assert(box);}
void text_box_set_text(TextBox* box, const char* text) {box->text = text;}

int main(void) {
    for(unsigned i = 0; i < 1000; i++) {
        assert(caesar_cipher_app(NULL) == 0);
        assert(!test_mutex.alive && mutex_frees == i + 1);
    }
    mutex_frees = 0;
    for(unsigned i = 0; i < 1000; i++) {
        world.level = i % 21;
        assert(bomberduck_app(NULL) == 0);
        assert(!test_queue.alive && !test_mutex.alive && viewport_count == 0);
        assert(queue_reads == 68 && mutex_frees == i + 1);
    }
    // Invalid/high save state must not overrun the fixed enemy array.
    for(int level = 0; level <= 1000; level++) {
        world.level = level; init();
        assert(world.enemies_count <= (int)(sizeof(world.enemies) / sizeof(world.enemies[0])));
    }
    BomberState busy = {.mutex = furi_mutex_alloc(FuriMutexTypeNormal)};
    Canvas canvas;
    assert(furi_mutex_acquire(busy.mutex, FuriWaitForever) == FuriStatusOk);
    draw_callback(&canvas, &busy);
    assert(busy.mutex->held); // Busy GUI draw skipped, not blocked or unlocked.
    furi_mutex_release(busy.mutex);
    furi_mutex_free(busy.mutex);
    assert(draws == 68000);
    puts("Caesar/Bomberduck: 2000 lifecycles, 66000 idle ticks, max text, bounded enemies OK");
    return 0;
}
