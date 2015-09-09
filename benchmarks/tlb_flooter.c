/*
 * Karma VMM
 * Copyright (C) 2010 Michael Peter, Steffen Liebergeld
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later versd have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>

//#define ITERATIONS 100000
//#define ITERATIONS (1 << 20)
#define ITERATIONS (1 << 18)
#define PAGESIZE   (1 << 12)
#define REGIONSIZE (1 << 28)
#define CHUNKSIZE (16 << 12)
#define WORDSPERPAGE (PAGESIZE / sizeof(unsigned))
//#define PAGESPERITERATION 8
#define PAGESPERITERATION 1

inline unsigned long long tsc()
{
    unsigned long long ret;
    asm volatile("rdtsc":"=A"(ret));
    return ret;
}

char *ptr;
unsigned limit;

void touch(unsigned pages)
{
    unsigned i,j;
    unsigned *_ptr;
//    volatile unsigned tst = 0;
    unsigned tst = 0;
    unsigned long long a, b, c = 0;
    if(pages > limit)
        return;
    
    unsigned long long t0, t1, diff;

    t0 = tsc();

    unsigned iterations;

    if (pages <= 256) 
        iterations = 256 / pages * ITERATIONS;
    else
        iterations = ITERATIONS;



    for(j = 0; j < iterations; j++)
    {
        _ptr = (unsigned *) ptr;
//        a = tsc();
        for(i = 0; i < pages; i += PAGESPERITERATION, _ptr += (WORDSPERPAGE * PAGESPERITERATION))
        {
            tst += _ptr[0];

#if 0
            tst += _ptr[1];
            tst += _ptr[2];
            tst += _ptr[3];
            tst += _ptr[4];
            tst += _ptr[5];
            tst += _ptr[6];
            tst += _ptr[7];
#endif
        }
//        b = tsc();
//        c+= ((b-a)/pages)/2;
    }
    
    t1 = tsc();

    diff = t1 - t0;
      
//    printf("%10d %llu, %llu, %llu, tst=%08x\n", pages, c/ITERATIONS, diff, diff / ITERATIONS,  tst);
    printf("%10d, %16llu, %6llu %6llu, tst=%08x\n", pages, diff, diff / iterations,  diff / iterations / pages, tst);
}

int main(int argc, char **argv)
{
    int fd, i;
    char *_ptr;
    if(argc<2)
    {
        printf("Too few arguments\n");
        return 1;
    }
    fd = open(argv[1], O_RDONLY);
    if(fd == -1)
    {
        printf("Error opening file\n");
        return 1;
    }
    ptr = mmap(0, REGIONSIZE, PROT_READ,MAP_ANONYMOUS|MAP_SHARED, -1 ,0);
    if(ptr == (void*)-1)
    {
        printf("Error mapping anonymous memory\n");
        return 1;
    }
    
    printf("temporary mapping %p - %p\n", ptr, ptr + (1<<28) -1);
    printf("access region %d\n", (int) ptr[0]);
    printf("access region %d\n", (int) ptr[(1 << 28) - 4]);
    printf("access region %d\n", (int) ptr[(1 << 28) - 1]);

    munmap(ptr, REGIONSIZE);
    _ptr = ptr;

    printf("start mapping %p\n", _ptr);
    for(i=0; i< (REGIONSIZE / CHUNKSIZE); i+= 1, _ptr += CHUNKSIZE)
    {
        void *mmap_ptr;
        mmap_ptr = mmap(_ptr, CHUNKSIZE, PROT_READ, MAP_SHARED, fd, 0);

        if (mmap_ptr == (void *) -1) {
            printf("mmap failed %p, i=%x\n", _ptr, i);
            printf("err=%d, (%s)\n", errno, strerror(errno));
            return 1;
        }

        if (mmap_ptr != _ptr) {
            printf("pointer deviation %p %p\n", mmap_ptr, _ptr);
            return 1;
        }
        if(*(unsigned*)_ptr)
            printf("Hallo\n");

    }
    printf("0x%x chunks mapped, total size=0x%x\n", i, i * CHUNKSIZE);
    limit = i * CHUNKSIZE / PAGESIZE;

    for(i = 2; i<20; i+=2)
        touch(1<<i);
    
    return 0;
}

