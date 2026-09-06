#include <stdint.h>
typedef unsigned nrf_ppi_channel_t;
#define NRFX_SUCCESS 0
int nrfx_ppi_channel_alloc(nrf_ppi_channel_t *);
int nrfx_ppi_channel_assign(nrf_ppi_channel_t, uint32_t, uint32_t);
int nrfx_ppi_channel_enable(nrf_ppi_channel_t);
int nrfx_ppi_channel_free(nrf_ppi_channel_t);
