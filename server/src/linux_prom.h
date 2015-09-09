/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Yann Le Du <ledu@kymasys.com>
 */

#ifndef _PROM_H_
#define _PROM_H_

#include <l4/sys/types.h>

#define ENVP_ADDR           0x80002000L
#define ENVP_NB_ENTRIES     8
#define ENVP_ENTRY_SIZE     512

void prom_set(l4_uint32_t* prom_buf, int index, const char *string, ...);
void prom_dump(l4_uint32_t* prom_buf);
int prom_nb_entries(l4_uint32_t* prom_buf);

#endif /* _PROM_H_ */
