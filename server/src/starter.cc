/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Yann Le Du <ledu@kymasys.com>
 *
 * starter.cc - Implements early initialization in a separate task
 */

#include <l4/util/util.h>
#include <l4/sys/err.h>
#include <l4/re/env.h>
#include <l4/sys/debugger.h>
#include <l4/shmc/shmc.h>
#include <pthread-l4.h>

#include <stdio.h>
#include <getopt.h>
#include <limits.h>

#include <string.h>
#include "devices/shmem.h"

#include "util/debug.h"

#define SHM_SIZE_MAX (256*1024)

#define CHK_RETURN_ERR(func) \
{ \
    int err = (func); \
    if (err) { \
        karma_log(ERROR, "[shmem] Error %s: line %d\n", l4sys_errtostr(err),  __LINE__); \
        return err; \
    } \
}

// parameters
static const char* shm_name;
static unsigned shm_size;
int debug_level = INFO;

static const char *const options = "n:s:";
static struct option long_options[] =
{
    {"debug_level", required_argument, 0, 'd'},
    {"shm_name", required_argument, 0, 'n'},
    {"shm_size", required_argument, 0, 's'},
    {0, 0, 0, 0}
};

// prototypes

int init_shm();

// implementation

// Initialize shared memory resource in starter task so they are ready for when
// producer and consumer are started.
int init_shm()
{
    l4shmc_chunk_t p_one, p_two;
    l4shmc_signal_t s_one, s_done, s_two, s_done_2;
    l4shmc_area_t shmarea;

    if (shm_name == NULL) {
        karma_log(ERROR, "[starter] Error, shm_name invalid\n");
        return -1;
    }

    if ((shm_size == 0) || (shm_size > SHM_SIZE_MAX)) {
        karma_log(ERROR, "[starter] Error, shm_size %d invalid\n", shm_size);
        return -1;
    }

    karma_log(INFO, "[starter] init_shm \'%s\' size %x\n", shm_name, shm_size);

    // create new shared memory area
    CHK_RETURN_ERR(l4shmc_create(shm_name, shm_size));

    // attach this thread to the shm object
    CHK_RETURN_ERR(l4shmc_attach(shm_name, &shmarea));

    // add a chunk
    CHK_RETURN_ERR(l4shmc_add_chunk(&shmarea, L4_shm::Chunk_one, 16384, &p_one));

    // add a signal
    CHK_RETURN_ERR(l4shmc_add_signal(&shmarea, L4_shm::Signal_one, &s_one));

    // add a signal
    CHK_RETURN_ERR(l4shmc_add_signal(&shmarea, L4_shm::Signal_done, &s_done));


    CHK_RETURN_ERR(l4shmc_add_chunk(&shmarea, L4_shm::Chunk_two, 16384, &p_two));

    CHK_RETURN_ERR(l4shmc_add_signal(&shmarea, L4_shm::Signal_two, &s_two));

    CHK_RETURN_ERR(l4shmc_add_signal(&shmarea, L4_shm::Signal_done_2, &s_done_2));

    return 0;
}

int main(int argc, char *argv[])
{
    int err;
    int opt_index, c;
    unsigned long size;

    l4_debugger_set_object_name(pthread_getl4cap(pthread_self()), "Starter");

    // get parameters
    opt_index = 0;
    while( (c = getopt_long(argc, argv, options, long_options, &opt_index)) != -1 )
    {
        switch (c)
        {
            case 'n':
                shm_name = optarg;
                break;
            case 's':
                size = strtoul(optarg, NULL, 0);
                if ((size == 0) || (size == ULONG_MAX))
                    shm_size = 0;
                else if (size*1024 > SHM_SIZE_MAX)
                    shm_size = 0;
                else
                    shm_size = size*1024;
                break;
            case 'd':
                debug_level = atoi(optarg);
                break;
            default:
                break;
        }
    }

    err = init_shm();
    if (err)
        return err;

    l4_sleep_forever();
    return 0;
}
