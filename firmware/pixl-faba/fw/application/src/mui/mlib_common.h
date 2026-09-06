#ifndef MLIB_COMMON_H
#define MLIB_COMMON_H

#include "mui_mem.h"
#ifdef FABA_MEMORY_DIAGNOSTICS
#include "faba_diag.h"
#define M_MEMORY_FULL(size) faba_diag_memory_full(__FILE__, __LINE__, (uint32_t)(size))
#endif

#define M_MEMORY_ALLOC(type) mui_mem_malloc (sizeof (type))
#define M_MEMORY_DEL(ptr)  mui_mem_free(ptr)
#define M_MEMORY_REALLOC(type, ptr, n) (M_UNLIKELY ((n) > SIZE_MAX / sizeof(type)) ? NULL : mui_mem_realloc ((ptr), (n)*sizeof (type)))
#define M_MEMORY_FREE(ptr) mui_mem_free(ptr)


#include "m-array.h"
#include "m-string.h"
#include "m-dict.h"
#include "m-deque.h"

ARRAY_DEF(string_array, string_t, STRING_OPLIST)
ARRAY_DEF(ptr_array, void*, M_BASIC_OPLIST )

#endif