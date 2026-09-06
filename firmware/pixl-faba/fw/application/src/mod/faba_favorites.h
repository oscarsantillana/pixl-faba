#ifndef FABA_FAVORITES_H
#define FABA_FAVORITES_H
#include <stdint.h>
#include <stdbool.h>
#define FABA_FAVORITES_MAX 257
/* IDs, not titles or tag images. Allocated with the catalog's bounded state. */
typedef struct {
    uint32_t generation;
    uint16_t count, ids[FABA_FAVORITES_MAX];
    int8_t bank;
    bool available;
} faba_favorites_t;
int32_t faba_favorites_load(faba_favorites_t *state);
bool faba_favorites_has(const faba_favorites_t *state, uint16_t id);
int32_t faba_favorites_toggle(faba_favorites_t *state, uint16_t id);
#endif
