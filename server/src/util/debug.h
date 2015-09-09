/*
 * debug.h - TODO enter description
 * 
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
#pragma once

#include <stdio.h>

extern int debug_level;

#define TRACE   5
#define DEBUG   4
#define INFO    3
#define WARN    2
#define ERROR   1
#define NONE    0

#ifdef DEBUG_LEVEL
#define LOG_DEBUG_LEVEL DEBUG_LEVEL
#else
#define LOG_DEBUG_LEVEL debug_level
#endif

#define LOG_LEVEL(level) ((level) <= LOG_DEBUG_LEVEL)

#define karma_log(level, fmt, ...) \
    do { \
    if(LOG_LEVEL(level)) \
        printf("Karma: " fmt ,##__VA_ARGS__); \
    } while(0)
