#include "faba_trace.h"
#include "vfs.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static bool mounted = true, folder;
static bool used[2][100];
static int short_write, writes;
static uint8_t captured[sizeof(faba_trace_t)];
uint32_t faba_trace_cycles(void) { return 64; }
uint32_t faba_trace_rtc(void) { return 1; }
void *faba_trace_alloc(size_t n) { return malloc(n); }
void faba_trace_free(void *p) { free(p); }
static bool is_mounted(void) { return mounted; }
static int create_dir(const char *path) { assert(strcmp(path, "/faba") == 0); folder = true; return 0; }
static void parse(const char *path, int *reader, unsigned *index) {
    char kind;
    assert(sscanf(path, "/faba/%c%u.trace", &kind, index) == 2);
    assert((kind == 'F' || kind == 'P') && *index < 100);
    *reader = kind == 'P';
}
static int stat_file(const char *path, vfs_obj_t *info) {
    (void)info;
    if (strcmp(path, "/faba/.folder") == 0) return folder ? 0 : VFS_ERR_NOOBJ;
    int reader; unsigned index; parse(path, &reader, &index);
    return used[reader][index] ? 0 : VFS_ERR_NOOBJ;
}
static int write_file(const char *path, void *data, size_t size) {
    int reader; unsigned index; parse(path, &reader, &index);
    assert(!used[reader][index]); /* Never overwrite an earlier capture. */
    used[reader][index] = true; writes++;
    assert(size <= sizeof(captured)); memcpy(captured, data, size);
    return short_write ? (int)size - 1 : (int)size;
}
static vfs_driver_t driver = {.mounted = is_mounted, .create_dir = create_dir,
    .stat_file = stat_file, .write_file_data = write_file};
vfs_driver_t *vfs_get_driver(vfs_drive_t drive) { assert(drive == VFS_DRIVE_EXT); return &driver; }

static void start(uint8_t reader) {
    faba_trace_header_t h = {.reader = reader, .cycle_hz = 64000000, .rtc_hz = 32768};
    assert(faba_trace_start(&h));
    faba_trace_record(FABA_FIELD_ON, 0, NULL, 0);
    faba_trace_path_reset();
}
int main(void) {
    start(0);
    mounted = false;
    assert(faba_trace_save() == VFS_ERR_NODEV && faba_trace_get());
    assert(!faba_trace_active() && writes == 0);
    mounted = true;
    assert(faba_trace_save() == 0 && !faba_trace_get());
    assert(strcmp(faba_trace_last_path(), "/faba/F00.trace") == 0);
    assert(memcmp(captured, "FABATRC1", 8) == 0);
    assert(faba_trace_header()->count == 1);
    assert(faba_trace_save() == 0 && writes == 1);
    start(0);
    assert(faba_trace_save() == 0);
    assert(strcmp(faba_trace_last_path(), "/faba/F01.trace") == 0);
    start(1);
    assert(faba_trace_save() == 0);
    assert(strcmp(faba_trace_last_path(), "/faba/P00.trace") == 0);
    start(0);
    short_write = 1;
    assert(faba_trace_save() == VFS_ERR_FAIL && faba_trace_get());
    assert(!*faba_trace_last_path());
    short_write = 0;
    assert(faba_trace_save() == 0 && !faba_trace_get());
    assert(strcmp(faba_trace_last_path(), "/faba/F03.trace") == 0);
    start(0);
    memset(used[0], 1, sizeof(used[0]));
    int previous_writes = writes;
    assert(faba_trace_save() == VFS_ERR_MAXNM && faba_trace_get());
    assert(writes == previous_writes);
    faba_trace_release();
    puts("PASS: missing storage, no overwrite, reader separation, partial write, retry and full directory");
}
