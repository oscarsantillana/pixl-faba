#include "faba_trace.h"
#include "vfs.h"
#include <stdio.h>

static char saved_path[32];
const char *faba_trace_last_path(void) { return saved_path; }
void faba_trace_path_reset(void) { saved_path[0] = 0; }

int32_t faba_trace_save(void) {
    faba_trace_stop();
    if (!faba_trace_get()) return VFS_OK;
    vfs_driver_t *fs = vfs_get_driver(VFS_DRIVE_EXT);
    if (!fs || !fs->mounted()) return VFS_ERR_NODEV;
    /* KEYPAD uses SPIFFS's .folder marker. Its create_dir does not map the
     * existing-object error, so only create the marker when it is absent. */
    vfs_obj_t folder;
    int32_t result = fs->stat_file("/faba/.folder", &folder);
    if (result == VFS_ERR_NOOBJ) result = fs->create_dir("/faba");
    if (result != VFS_OK) return result;
    const faba_trace_t *trace = faba_trace_get();
    char candidate[32];
    for (unsigned i = 0; i < 100; i++) {
        snprintf(candidate, sizeof(candidate), "/faba/%c%02u.trace", trace->header.reader ? 'P' : 'F', i);
        vfs_obj_t info;
        result = fs->stat_file(candidate, &info);
        if (result == VFS_OK) continue;
        if (result != VFS_ERR_NOOBJ) return result;
        result = fs->write_file_data(candidate, (void *)trace, faba_trace_size());
        if (result != (int32_t)faba_trace_size()) return result < 0 ? result : VFS_ERR_FAIL;
        snprintf(saved_path, sizeof(saved_path), "%s", candidate);
        faba_trace_release();
        return VFS_OK;
    }
    return VFS_ERR_MAXNM;
}
