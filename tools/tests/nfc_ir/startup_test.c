/* Fake SD/ELF/IR boundaries; function bodies come from production extraction.
 * No radio, NFC poller or IR transmitter is exercised by this host test. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define furi_check(c) assert(c)
#define furi_assert(c) assert(c)
#define FURI_LOG_D(...) ((void)0)
#define TAG "test"
#define RECORD_STORAGE "storage"
#define NFC_SUPPORTED_CARDS_PLUGIN_SUFFIX "_parser.fal"
typedef struct { char text[256]; } FuriString;
static FuriString* furi_string_alloc(void) { return calloc(1, sizeof(FuriString)); }
static FuriString* furi_string_alloc_set(const char* value) {
    FuriString* result = furi_string_alloc();
    strcpy(result->text, value);
    return result;
}
static const char* furi_string_get_cstr(const FuriString* s) { return s->text; }
static void furi_string_free(FuriString* s) { free(s); }
typedef int Storage;
static Storage fake_storage;
static void* furi_record_open(const char* name) { assert(!strcmp(name, RECORD_STORAGE)); return &fake_storage; }
static void furi_record_close(const char* name) { assert(!strcmp(name, RECORD_STORAGE)); }
typedef struct { bool is_dir; } FileInfo;
typedef struct { size_t position; } File;
typedef struct { const char* name; bool is_dir; } DirEntry;
static const DirEntry directory[] = {
    {"README.txt", false}, {"subdir_parser.fal", true}, {"x", false},
    {"alpha_parser.fal", false}, {"bravo_parser.fal", false},
    {"charlie_parser.fal", false}, {"broken_parser.fal", false},
};
static bool storage_file_is_open(const File* file) { return file != NULL; }
static bool file_info_is_dir(const FileInfo* info) { return info->is_dir; }
static bool storage_dir_read(File* file, FileInfo* info, char* name, size_t size) {
    if(file->position == sizeof(directory) / sizeof(*directory)) return false;
    const DirEntry* entry = &directory[file->position++];
    assert(strlen(entry->name) < size);
    strcpy(name, entry->name);
    info->is_dir = entry->is_dir;
    return true;
}

typedef unsigned NfcProtocol;
typedef struct { NfcProtocol protocol; } NfcDevice;
typedef int Nfc;
typedef int CompositeApiResolver;
typedef int ElfApiInterface;
static NfcProtocol nfc_device_get_protocol(NfcDevice* device) { return device->protocol; }
static const ElfApiInterface* composite_api_resolver_get(CompositeApiResolver* api) { return api; }
typedef struct {
    NfcProtocol protocol;
    bool (*verify)(Nfc*);
    bool (*read)(Nfc*, NfcDevice*);
    bool (*parse)(const NfcDevice*, FuriString*);
} NfcSupportedCardsPlugin;
typedef enum {
    NfcSupportedCardsPluginFeatureHasVerify = 1,
    NfcSupportedCardsPluginFeatureHasRead = 2,
    NfcSupportedCardsPluginFeatureHasParse = 4,
} NfcSupportedCardsPluginFeature;
typedef struct {
    FuriString* name;
    NfcProtocol protocol;
    NfcSupportedCardsPluginFeature feature;
    bool metadata_loaded;
} NfcSupportedCardsPluginCache;
typedef struct {
    NfcSupportedCardsPluginCache values[8];
    size_t size;
} Cache;
typedef size_t NfcSupportedCardsPluginCache_it_t[2];
#define NfcSupportedCardsPluginCache_it(i, a) ((i)[0] = 0, (i)[1] = (a)->size)
#define NfcSupportedCardsPluginCache_end_p(i) ((i)[0] == (i)[1])
#define NfcSupportedCardsPluginCache_next(i) (++(i)[0])
static Cache* current_cache;
#define NfcSupportedCardsPluginCache_ref(i) (&current_cache->values[(i)[0]])
#define NfcSupportedCardsPluginCache_size(a) ((a)->size)
#define NfcSupportedCardsPluginCache_push_back(a, value) ((a)->values[(a)->size++] = (value))
typedef enum {
    NfcSupportedCardsLoadStateIdle,
    NfcSupportedCardsLoadStateInProgress,
    NfcSupportedCardsLoadStateSuccess,
    NfcSupportedCardsLoadStateFail,
} NfcSupportedCardsLoadState;
typedef struct {
    Storage* storage;
    File* directory;
    char file_name[256];
} NfcSupportedCardsLoadContext;
typedef struct {
    CompositeApiResolver* api_resolver;
    Cache plugins_cache_arr[1];
    NfcSupportedCardsLoadState load_state;
    NfcSupportedCardsLoadContext* load_context;
} NfcSupportedCards;
static unsigned contexts_allocated, contexts_freed, plugin_maps, parse_calls;
static bool parse_reject(const NfcDevice* device, FuriString* parsed) {
    (void)device; (void)parsed; ++parse_calls; return false;
}
static bool parse_accept(const NfcDevice* device, FuriString* parsed) {
    (void)device; ++parse_calls; strcpy(parsed->text, "parsed"); return true;
}
static bool read_accept(Nfc* nfc, NfcDevice* device) { (void)nfc; (void)device; return true; }
static const NfcSupportedCardsPlugin alpha = {1, NULL, NULL, parse_reject};
static const NfcSupportedCardsPlugin bravo = {2, NULL, NULL, parse_accept};
static const NfcSupportedCardsPlugin charlie = {1, NULL, read_accept, parse_accept};
static NfcSupportedCardsLoadContext* nfc_supported_cards_load_context_alloc(void) {
    NfcSupportedCardsLoadContext* ctx = calloc(1, sizeof(*ctx));
    ctx->directory = calloc(1, sizeof(File));
    ++contexts_allocated;
    return ctx;
}
static void nfc_supported_cards_load_context_free(NfcSupportedCardsLoadContext* ctx) {
    free(ctx->directory); free(ctx); ++contexts_freed;
}
static const NfcSupportedCardsPlugin* nfc_supported_cards_get_plugin(
    NfcSupportedCardsLoadContext* ctx, const char* name, const ElfApiInterface* api) {
    (void)ctx; (void)api; ++plugin_maps;
    if(!strcmp(name, "alpha")) return &alpha;
    if(!strcmp(name, "bravo")) return &bravo;
    if(!strcmp(name, "charlie")) return &charlie;
    return NULL;
}

typedef enum {
    InfraredErrorCodeNone = 0,
    InfraredErrorCodeFileOperationFailed = 1,
    InfraredErrorCodeSignalNameNotFound = 2,
} InfraredErrorCode;
#define INFRARED_ERROR_PRESENT(e) ((e) != InfraredErrorCodeNone)
typedef struct { size_t values[32], size; } Positions;
typedef struct { size_t index; Positions signals[1]; } InfraredBruteForceRecord;
typedef struct { FuriString key; InfraredBruteForceRecord value; } Record;
typedef struct { Record records[2]; size_t size; } Records;
typedef size_t InfraredBruteForceRecordDict_it_t[2];
static Records* current_records;
#define InfraredBruteForceRecordDict_it(i, a) ((i)[0] = 0, (i)[1] = (a)->size)
#define InfraredBruteForceRecordDict_end_p(i) ((i)[0] == (i)[1])
#define InfraredBruteForceRecordDict_next(i) (++(i)[0])
#define InfraredBruteForceRecordDict_ref(i) (&current_records->records[(i)[0]])
#define SignalPositionArray_reset(a) ((a)->size = 0)
#define SignalPositionArray_push_back(a, value) ((a)->values[(a)->size++] = (value))
#define SignalPositionArray_size(a) ((a)->size)
#define SignalPositionArray_cget(a, i) (&(a)->values[(i)])
static InfraredBruteForceRecord* InfraredBruteForceRecordDict_get(Records* a, FuriString* key) {
    for(size_t i = 0; i < a->size; ++i) {
        if(!strcmp(a->records[i].key.text, key->text)) return &a->records[i].value;
    }
    return NULL;
}
typedef struct { size_t position; } FlipperFormat;
typedef struct { bool valid; } InfraredSignal;
typedef struct {
    FlipperFormat* ff;
    const char* db_filename;
    FuriString* current_record_name;
    InfraredBruteForceRecord current_record;
    InfraredSignal* current_signal;
    Records records[1];
    bool is_started;
} InfraredBruteForce;
static unsigned bodies_decoded, signals_transmitted;
static bool open_ok = true, body_ok = true, signal_valid = true, seek_ok = true;
static int io_error;
enum { FSE_OK, FlipperFormatOffsetFromStart };
static FlipperFormat* flipper_format_buffered_file_alloc(Storage* storage) {
    assert(storage == &fake_storage); return calloc(1, sizeof(FlipperFormat));
}
static bool flipper_format_buffered_file_open_existing(FlipperFormat* ff, const char* path) {
    (void)ff; assert(path != NULL); return open_ok;
}
static void flipper_format_free(FlipperFormat* ff) { free(ff); }
static size_t flipper_format_tell(FlipperFormat* ff) { return ff->position * 100; }
static FlipperFormat* flipper_format_get_raw_stream(FlipperFormat* ff) { return ff; }
static int buffered_file_stream_get_error(FlipperFormat* ff) { (void)ff; return io_error; }
static bool flipper_format_seek(FlipperFormat* ff, size_t pos, int anchor) {
    assert(anchor == FlipperFormatOffsetFromStart); ff->position = pos / 100; return seek_ok;
}
static const char* names[] = {"Power", "Unknown", "Power", "Mute"};
static InfraredErrorCode infrared_signal_read_name(FlipperFormat* ff, FuriString* name) {
    if(ff->position == sizeof(names) / sizeof(*names)) return InfraredErrorCodeSignalNameNotFound;
    strcpy(name->text, names[ff->position++]); return InfraredErrorCodeNone;
}
static InfraredErrorCode infrared_signal_read_body(InfraredSignal* signal, FlipperFormat* ff) {
    (void)ff; ++bodies_decoded; signal->valid = signal_valid;
    return body_ok ? InfraredErrorCodeNone : InfraredErrorCodeFileOperationFailed;
}
static bool infrared_signal_is_valid(InfraredSignal* signal) { return signal->valid; }
static void infrared_signal_transmit(InfraredSignal* signal) {
    assert(signal->valid); ++signals_transmitted;
}

#include "startup_under_test.h"

static void test_nfc(void) {
    NfcSupportedCards cards = {0};
    CompositeApiResolver resolver = 0;
    cards.api_resolver = &resolver;
    current_cache = cards.plugins_cache_arr;
    nfc_supported_cards_load_cache(&cards);
    assert(cards.load_state == NfcSupportedCardsLoadStateSuccess);
    assert(cards.plugins_cache_arr->size == 4); /* unrelated files/dirs did not stop iteration */
    assert(plugin_maps == 0); /* filename cache never maps an ELF */
    assert(cards.load_context == NULL);
    NfcDevice device = {1};
    FuriString parsed = {0};
    assert(nfc_supported_cards_parse(&cards, &device, &parsed));
    assert(plugin_maps == 3 && parse_calls == 2); /* not 4 metadata maps + 2 retry maps */
    assert(cards.plugins_cache_arr->values[0].metadata_loaded);
    assert(!cards.plugins_cache_arr->values[3].metadata_loaded);
    device.protocol = 2;
    assert(nfc_supported_cards_parse(&cards, &device, &parsed));
    assert(plugin_maps == 4 && parse_calls == 3); /* cached mismatch skipped */
    device.protocol = 1;
    Nfc nfc = 0;
    assert(nfc_supported_cards_read(&cards, &device, &nfc));
    assert(plugin_maps == 5); /* no-read plugins and wrong protocols skipped */
    assert(contexts_allocated == contexts_freed);
    for(size_t i = 0; i < cards.plugins_cache_arr->size; ++i)
        furi_string_free(cards.plugins_cache_arr->values[i].name);
}

static void test_ir(void) {
    InfraredBruteForce bf = {0};
    bf.db_filename = "test.ir";
    bf.records->size = 2;
    strcpy(bf.records->records[0].key.text, "Power");
    strcpy(bf.records->records[1].key.text, "Mute");
    current_records = bf.records;
    for(unsigned i = 0; i < 1000; ++i) {
        assert(infrared_brute_force_calculate_messages(&bf) == InfraredErrorCodeNone);
        assert(bf.records->records[0].value.signals->size == 2);
        assert(bf.records->records[1].value.signals->size == 1);
        assert(bf.records->records[0].value.signals->values[0] == 100);
        assert(bf.records->records[0].value.signals->values[1] == 300);
    }
    assert(bodies_decoded == 0 && signals_transmitted == 0);
    bf.is_started = true;
    FlipperFormat ff = {0};
    InfraredSignal signal = {0};
    bf.ff = &ff;
    bf.current_signal = &signal;
    bf.current_record = bf.records->records[0].value;
    assert(infrared_brute_force_send(&bf, 0));
    assert(ff.position == 1 && signals_transmitted == 1);
    assert(infrared_brute_force_send(&bf, 1));
    assert(ff.position == 3 && signals_transmitted == 2);
    assert(!infrared_brute_force_send(&bf, 2));
    body_ok = false;
    assert(!infrared_brute_force_send(&bf, 0));
    body_ok = true; signal_valid = false;
    assert(!infrared_brute_force_send(&bf, 0));
    signal_valid = true; seek_ok = false;
    assert(!infrared_brute_force_send(&bf, 0));
    assert(signals_transmitted == 2); /* no malformed/out-of-range/seek-failed transmission */
    bf.is_started = false;
    open_ok = false;
    assert(infrared_brute_force_calculate_messages(&bf) == InfraredErrorCodeFileOperationFailed);
    assert(bf.records->records[0].value.signals->size == 0);
    open_ok = true; io_error = 1;
    assert(infrared_brute_force_calculate_messages(&bf) == InfraredErrorCodeFileOperationFailed);
}

int main(void) {
    test_nfc(); test_ir();
    puts("NFC lazy parser dispatch + IR lazy signal indexing: PASS (1000 index retries, no physical transmission)");
    return 0;
}
