#ifndef FABA_PLAYER_H
#define FABA_PLAYER_H
#include <stdbool.h>
#include <stdint.h>
bool faba_player_start(uint16_t id);
void faba_player_stop(void);
void faba_catalog_close(void);
#endif
