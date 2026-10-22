/* Exercise the production buffered reader/cache against counted storage I/O.
 * The memory-backed file is the only fake; neither buffered implementation is
 * duplicated here. Run with build.cmd from the repository root. */
#include <stdio.h>
#include "buffered_file_stream.h"
#include "file_stream.h"
#include "stream_i.h"

typedef struct {
    Stream stream;
    Storage* storage;
    size_t pos;
    bool open;
} TestFile;

static void test_free(Stream* stream) { free(stream); }
static bool test_eof(Stream* stream) {
    TestFile* file = (TestFile*)stream;
    return file->pos == file->storage->size;
}
static void test_clean(Stream* stream) { (void)stream; assert(false); }
static bool test_seek(Stream* stream, int32_t offset, StreamOffset origin) {
    TestFile* file = (TestFile*)stream;
    int64_t base = origin == StreamOffsetFromStart ? 0 :
                   origin == StreamOffsetFromEnd ? (int64_t)file->storage->size :
                                                  (int64_t)file->pos;
    int64_t pos = base + offset;
    ++file->storage->seeks;
    bool valid = pos >= 0 && pos <= (int64_t)file->storage->size;
    file->pos = pos < 0 ? 0 : pos > (int64_t)file->storage->size ?
                                 file->storage->size : (size_t)pos;
    return valid;
}
static size_t test_tell(Stream* stream) { return ((TestFile*)stream)->pos; }
static size_t test_size(Stream* stream) { return ((TestFile*)stream)->storage->size; }
static size_t test_write(Stream* stream, const uint8_t* data, size_t size) {
    (void)data;
    (void)size;
    ++((TestFile*)stream)->storage->writes;
    assert(false); /* These configuration readers must not modify a file. */
    return 0;
}
static size_t test_read(Stream* stream, uint8_t* data, size_t size) {
    TestFile* file = (TestFile*)stream;
    ++file->storage->reads;
    size = MIN(size, file->storage->size - file->pos);
    if(file->storage->max_read) size = MIN(size, file->storage->max_read);
    memcpy(data, file->storage->data + file->pos, size);
    file->pos += size;
    return size;
}
static bool test_insert(Stream* stream, size_t size, StreamWriteCB cb, const void* context) {
    (void)stream; (void)size; (void)cb; (void)context;
    assert(false);
    return false;
}
static const StreamVTable test_vtable = {
    test_free, test_eof, test_clean, test_seek, test_tell, test_size,
    test_write, test_read, test_insert,
};

Stream* file_stream_alloc(Storage* storage) {
    TestFile* file = calloc(1, sizeof(TestFile));
    assert(file);
    file->stream.vtable = &test_vtable;
    file->storage = storage;
    return &file->stream;
}
bool file_stream_open(Stream* stream, const char* path, FS_AccessMode mode, FS_OpenMode how) {
    (void)path; (void)mode; (void)how;
    TestFile* file = (TestFile*)stream;
    file->open = !file->storage->missing;
    return file->open;
}
bool file_stream_close(Stream* stream) { ((TestFile*)stream)->open = false; return true; }
FS_Error file_stream_get_error(Stream* stream) {
    return ((TestFile*)stream)->storage->missing ? FSE_NOT_EXIST : FSE_OK;
}

/* The same small vtable dispatch operations used by stream.c. */
void stream_free(Stream* s) { s->vtable->free(s); }
void stream_clean(Stream* s) { s->vtable->clean(s); }
bool stream_eof(Stream* s) { return s->vtable->eof(s); }
bool stream_seek(Stream* s, int32_t offset, StreamOffset origin) {
    return s->vtable->seek(s, offset, origin);
}
size_t stream_tell(Stream* s) { return s->vtable->tell(s); }
size_t stream_size(Stream* s) { return s->vtable->size(s); }
size_t stream_read(Stream* s, uint8_t* data, size_t size) { return s->vtable->read(s, data, size); }
size_t stream_write(Stream* s, const uint8_t* data, size_t size) {
    return s->vtable->write(s, data, size);
}
bool stream_delete_and_insert(Stream* s, size_t size, StreamWriteCB cb, const void* context) {
    return s->vtable->delete_and_insert(s, size, cb, context);
}

static uint8_t fixture[8193];

static void compare_sequential(size_t size, size_t chunk, size_t short_read) {
    Storage plain = {.data = fixture, .size = size, .max_read = short_read};
    Storage cached = plain;
    Stream* raw = file_stream_alloc(&plain);
    Stream* buffered = buffered_file_stream_alloc(&cached);
    assert(file_stream_open(raw, "settings", FSAM_READ, FSOM_OPEN_EXISTING));
    assert(buffered_file_stream_open(buffered, "settings", FSAM_READ, FSOM_OPEN_EXISTING));
    for(unsigned pass = 0; pass < 3; ++pass) {
        assert(stream_seek(raw, 0, StreamOffsetFromStart));
        assert(stream_seek(buffered, 0, StreamOffsetFromStart));
        size_t pos = 0;
        uint8_t data[1100];
        while(pos < size) {
            size_t count = MIN(chunk, size - pos);
            /* An underlying short read is allowed, but the buffered API must
             * still fill the request unless the file actually ends. */
            assert(stream_read(buffered, data, count) == count);
            assert(memcmp(data, fixture + pos, count) == 0);
            uint8_t raw_data[1100];
            size_t copied = 0;
            while(copied < count) copied += stream_read(raw, raw_data + copied, count - copied);
            assert(memcmp(raw_data, data, count) == 0);
            pos += count;
            assert(stream_tell(buffered) == pos);
            assert(stream_size(buffered) == size);
        }
        assert(stream_eof(buffered));
        assert(stream_read(buffered, data, 1) == 0);
    }
    if(size == sizeof(fixture) && chunk == 1 && !short_read) {
        assert(cached.reads < plain.reads / 100);
        printf("PASS: identical bytes, %zu direct reads -> %zu buffered reads (3 passes)\n",
               plain.reads, cached.reads);
    }
    assert(plain.writes == 0 && cached.writes == 0);
    assert(buffered_file_stream_close(buffered));
    stream_free(buffered);
    stream_free(raw);
}

static void test_seek_patterns(void) {
    Storage storage = {.data = fixture, .size = sizeof(fixture)};
    Stream* s = buffered_file_stream_alloc(&storage);
    assert(buffered_file_stream_open(s, "settings", FSAM_READ, FSOM_OPEN_EXISTING));
    uint32_t random = 12345;
    size_t expected = 0;
    for(unsigned i = 0; i < 10000; ++i) {
        random = random * 1664525u + 1013904223u;
        size_t pos = random % sizeof(fixture);
        StreamOffset origin = (StreamOffset)(i % 3);
        int32_t offset = origin == StreamOffsetFromCurrent ? (int32_t)pos - (int32_t)expected :
                         origin == StreamOffsetFromEnd ? (int32_t)pos - (int32_t)sizeof(fixture) :
                                                         (int32_t)pos;
        assert(stream_seek(s, offset, origin));
        assert(stream_tell(s) == pos);
        uint8_t byte;
        assert(stream_read(s, &byte, 1) == 1 && byte == fixture[pos]);
        expected = pos + 1;
    }
    assert(buffered_file_stream_close(s));
    stream_free(s);
    puts("PASS: 10000 absolute, relative and end-relative seeks");
}

int main(void) {
    for(size_t i = 0; i < sizeof(fixture); ++i) fixture[i] = (uint8_t)(i * 53u + i / 17u);
    const size_t sizes[] = {0, 1, 64, 511, 512, 1023, 1024, 1025, sizeof(fixture)};
    const size_t chunks[] = {1, 32, 64, 1100};
    for(size_t i = 0; i < sizeof(sizes) / sizeof(*sizes); ++i) {
        for(size_t j = 0; j < sizeof(chunks) / sizeof(*chunks); ++j) {
            compare_sequential(sizes[i], chunks[j], 0);
            compare_sequential(sizes[i], chunks[j], 13);
        }
    }
    test_seek_patterns();
    Storage missing = {.data = fixture, .missing = true};
    Stream* s = buffered_file_stream_alloc(&missing);
    assert(!buffered_file_stream_open(s, "missing", FSAM_READ, FSOM_OPEN_EXISTING));
    assert(buffered_file_stream_get_error(s) == FSE_NOT_EXIST);
    stream_free(s);
    puts("PASS: empty, partial, cache-boundary and missing files; no writes");
    return 0;
}
