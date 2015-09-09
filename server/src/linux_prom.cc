/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Yann Le Du <ledu@kymasys.com>
 */

#include <stdarg.h>
#include <stdio.h>
#include <endian.h>

#include "linux_prom.h"
#include "util/debug.h"

#define tswap32(x) htole32(x)

void prom_set(l4_uint32_t* prom_buf, int index, const char *string, ...)
{
    va_list ap;
    l4_int32_t table_addr;

    if (index >= ENVP_NB_ENTRIES) {
        karma_log(ERROR, "Error: prom index %i is out of range\n", index);
        return;
    }

    if (string == NULL) {
        prom_buf[index] = 0;
        return;
    }

    table_addr = sizeof(l4_int32_t) * ENVP_NB_ENTRIES + index * ENVP_ENTRY_SIZE;
    prom_buf[index] = tswap32(ENVP_ADDR + table_addr);

    va_start(ap, string);
    vsnprintf((char *)prom_buf + table_addr, ENVP_ENTRY_SIZE, string, ap);
    va_end(ap);
}

void prom_dump(l4_uint32_t* prom_buf)
{
    int index;
    l4_int32_t table_addr;

    for (index = 0; prom_buf[index] != 0; index++) {
        table_addr = sizeof(l4_int32_t) * ENVP_NB_ENTRIES + index * ENVP_ENTRY_SIZE;
        karma_log(INFO, "prom[%i] %x %s\n", index, prom_buf[index], (char*)prom_buf + table_addr);
    }
}

int prom_nb_entries(l4_uint32_t* prom_buf)
{
    int index;

    for (index = 0; prom_buf[index] != 0; index++)
        ;

    return index;
}
