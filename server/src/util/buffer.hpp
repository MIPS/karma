/*
 * buffer.h - TODO enter description
 * 
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
#pragma once

#include <cstdlib>
#include <cassert>
#include <pthread-l4.h>

template < unsigned SIZE >
class Buffer
{
    public:

        char *ptr() const { return _buf; }
        size_t size() const { return SIZE; }
        size_t count() const { return _count; }
        bool valid() { return _buf; }

        Buffer()
            : _pos_in(0), _pos_out(0), _count(0), _wait_count(0)
        {
            _buf = (typeof(_buf))malloc(SIZE);
            if (_buf)
                memset(_buf, 0, SIZE);
            pthread_mutex_init(&mutex, 0);
            pthread_cond_init(&_cond, 0);
        }

        ~Buffer()
        {
            pthread_cond_destroy(&_cond);
            pthread_mutex_destroy(&mutex);
            free(_buf);
        }

        bool memcpy_to(const void *src, size_t n)
        {
            size_t end_space;

            pthread_mutex_lock(&mutex);
            assert(valid());

            if(n + _count < SIZE)
            {
                end_space = SIZE - _pos_in;
                if(end_space > n)
                {
                    memcpy((_buf + _pos_in), src, n);
                    _pos_in += n;
                    _count += n;
                }
                else
                {
                    memcpy((_buf + _pos_in), src, end_space);
                    memcpy(_buf,(l4_uint8_t *)src + end_space, n - end_space);
                    _pos_in = n - end_space;
                    _count += n;
                }

                if(_wait_count && (_wait_count <= _count))
                {
                    _wait_count = 0;
                    signal();
                }

                pthread_mutex_unlock(&mutex);
                return true;
            }
            else
            {
                goto fail;
            }
fail:
            pthread_mutex_unlock(&mutex);
            return false;
        }
        bool put(const char c)
        {
            pthread_mutex_lock(&mutex);
            assert(valid());

            if (_count == SIZE) {
                assert(_pos_in == _pos_out);
                _buf[_pos_in] = c;
                _pos_in = (_pos_in + 1) % SIZE;
                _pos_out = _pos_in;
                pthread_mutex_unlock(&mutex);
                return true;
            }

            _buf[_pos_in] = c;
            _pos_in = (_pos_in + 1) % SIZE;
            _count++;
            if(_wait_count && (_wait_count <= _count))
            {
                _wait_count = 0;
                signal();
            }
            pthread_mutex_unlock(&mutex);
            return true;
        }

        bool memcpy_from(void *dest, size_t n, int has_lock)
        {
            size_t end_bytes;

            if (_count < n)
                return false;

            if(!has_lock)
                pthread_mutex_lock(&mutex);

            end_bytes = SIZE - _pos_out;

            if(n <= end_bytes)
            {
                memcpy(dest, (_buf + _pos_out), n);
                _pos_out += n;
            }
            else
            {
                memcpy(dest, (_buf + _pos_out), end_bytes);
                memcpy(((l4_uint8_t *)dest + end_bytes), _buf, n - end_bytes);
                _pos_out = n - end_bytes;
            }

            _count -= n;

            if(!has_lock)
                pthread_mutex_unlock(&mutex);

            return true;
        }

        bool get(char *c)
        {
            if (_count == 0)
                return false;

            pthread_mutex_lock(&mutex);
            *c = _buf[_pos_out];

            _pos_out = (_pos_out + 1) % SIZE;
            _count--;

            pthread_mutex_unlock(&mutex);
            return true;
        }

        void flush()
        {
            pthread_mutex_lock(&mutex);
            _pos_in = 0;
            _pos_out = 0;
            _count = 0;
            _wait_count = 0;
            pthread_mutex_unlock(&mutex);
        }

        void waitConditional(const size_t count)
        {
            pthread_mutex_lock(&mutex);
            _wait_count = count;
            while(count > _count){
                pthread_cond_wait(&_cond, &mutex);
            }
            pthread_mutex_unlock(&mutex);
        }

        void waitConditional_memcpy_from(const size_t count, void *dest)
        {
            pthread_mutex_lock(&mutex);
            _wait_count = count;
            while(count > _count){
                pthread_cond_wait(&_cond, &mutex);
            }

            memcpy_from(dest, count, 1);

            pthread_mutex_unlock(&mutex);

        }

        void lockBuffer()
        {
            pthread_mutex_lock(&mutex);
        }

        void unlockBuffer()
        {
            pthread_mutex_unlock(&mutex);
        }
    private:
        char  *_buf;
        volatile size_t _pos_in;
        volatile size_t _pos_out;
        size_t _count;
        size_t _wait_count;
        pthread_mutex_t mutex;
        pthread_cond_t _cond;

    protected:
        void signal(){
            pthread_cond_signal(&_cond);
        }
};

template < unsigned long SIZE >
class Buffer32
{
    public:

        unsigned long *ptr() const { return _buf; }
        size_t size() const { return SIZE; }
        size_t count() const { return _count; }
        bool valid() { return _buf; }

        Buffer32()
            : _pos_in(0), _pos_out(0), _count(0), _wait_count(0)
        {
            _buf = (typeof(_buf))malloc(SIZE*sizeof(unsigned long));
            if (_buf)
                memset(_buf, 0, SIZE*sizeof(unsigned long));
            pthread_mutex_init(&mutex, 0);
            pthread_cond_init(&_cond, 0);
        }

        ~Buffer32()
        {
            pthread_cond_destroy(&_cond);
            pthread_mutex_destroy(&mutex);
            free(_buf);
        }

        bool put(unsigned long c)
        {
            pthread_mutex_lock(&mutex);
            assert(valid());

            if (_count == SIZE) {
                assert(_pos_in == _pos_out);
                _buf[_pos_in] = c;
                _pos_in = (_pos_in + 1) % SIZE;
                _pos_out = _pos_in;
                pthread_mutex_unlock(&mutex);
                return true;
            }

            _buf[_pos_in] = c;
            _pos_in = (_pos_in + 1) % SIZE;
            _count++;
            if(_wait_count && (_wait_count <= _count))
            {
                _wait_count = 0;
                signal();
            }
            pthread_mutex_unlock(&mutex);
            return true;
        }

        bool get(unsigned long *c)
        {
            if (_count == 0)
                return false;

            pthread_mutex_lock(&mutex);
            *c = _buf[_pos_out];

            _pos_out = (_pos_out + 1) % SIZE;
            _count--;

            pthread_mutex_unlock(&mutex);
            return true;
        }

        void flush()
        {
            pthread_mutex_lock(&mutex);
            _pos_in = 0;
            _pos_out = 0;
            _count = 0;
            _wait_count = 0;
            pthread_mutex_unlock(&mutex);
        }

        void waitConditional(const size_t count)
        {
            pthread_mutex_lock(&mutex);
            _wait_count = count;
            while(count > _count){
                pthread_cond_wait(&_cond, &mutex);
            }
            pthread_mutex_unlock(&mutex);
        }
    private:
        unsigned long  *_buf;
        volatile size_t _pos_in;
        volatile size_t _pos_out;
        size_t _count;
        size_t _wait_count;
        pthread_mutex_t mutex;
        pthread_cond_t _cond;

    protected:
        void signal(){
            pthread_cond_signal(&_cond);
        }
};
