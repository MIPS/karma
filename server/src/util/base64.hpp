/*
 * base64.hpp - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
/*
 * base64.hpp
 *
 *  Created on: 30.12.2010
 *      Author: janis
 */

#ifndef BASE64_HPP_
#define BASE64_HPP_

#include <cstring>

namespace util{
class base64table_t{
private:
    unsigned char _table[64];
public:
    base64table_t(){
        for(unsigned char i(0); i < 26; ++i){
            _table[i] = 'A' + i;
        }
        for(unsigned int i(26); i < 52; ++i){
            _table[i] = 'a' + i - 26;
        }
        for(unsigned int i(52); i < 62; ++i){
            _table[i] = '0' + i - 52;
        }
        _table[62] = '+';
        _table[63] = '/';
    }
    inline char operator[] (unsigned char index) const{
        return _table[index];
    }
};
class base64rtable_t{
private:
    char _table[256];
public:
    base64rtable_t(){
        base64table_t table;
        memset(_table, 0xffffffff, 256);
        for(unsigned int i(0); i < 64; ++i){
           _table[(unsigned char)table[i]] = i;
        }
    }
    inline char operator[] (unsigned char index) const{
        return _table[index];
    }
};

class b64q{
private:
    struct field_t{
        union __attribute__((__packed__)) {
            struct __attribute__((__packed__)) {
                unsigned int a : 6;
                unsigned int b : 6;
                unsigned int c : 6;
                unsigned int d : 6;
            } o3;
            struct __attribute__((__packed__)){
                unsigned int a : 6;
                unsigned int b : 6;
                unsigned int c : 4;
            } o2;
            struct __attribute__((__packed__)){
                unsigned int a : 6;
                unsigned int b : 2;
            } o1;
            struct __attribute__((__packed__)) {
                unsigned char raw[3];
            } r;
         } m;
    };
public:
    field_t * _data;
    char * _chars;
    unsigned int _occupied;

    static void dataToB64(b64q & q){
        static const base64table_t table;
        unsigned char dummy = 0;
        q._chars[2] = q._chars[3] = '=';
        switch(q._occupied){
        case 3:
            dummy = ((q._data->m.r.raw[2] & 0x3f));
            q._chars[3] = table[dummy];
            dummy = ((q._data->m.r.raw[2] >> 6) & 0x3);
        case 2:
            dummy |= ((q._data->m.r.raw[1] & 0xf) << 2);
            q._chars[2] = table[dummy];
            dummy = ((q._data->m.r.raw[1] >> 4) & 0xf);
        case 1:
            dummy |= ((q._data->m.r.raw[0] & 0x3) << 4);
            q._chars[1] = table[dummy];
            dummy = (q._data->m.r.raw[0] >> 2) & 0x3f;
            q._chars[0] = table[dummy];
        }
    }
//    static void dataToB64(b64q & q){
//        static const base64table_t table;
//        q._chars[0] = table[q._data->m.o3.a];
//        if(q._occupied == 3){
//            q._chars[1] = table[q._data->m.o3.b];
//            q._chars[2] = table[q._data->m.o3.c];
//            q._chars[3] = table[q._data->m.o3.d];
//        } else if(q._occupied == 2){
//            q._chars[1] = table[q._data->m.o2.b];
//            unsigned int dummy = q._data->m.o2.c;
//            q._chars[2] = table[dummy << 2];
//            q._chars[3] = '=';
//        } else if(q._occupied == 1){
//            unsigned int dummy = q._data->m.o1.b;
//            q._chars[1] = table[dummy << 4];
//            q._chars[2] = '=';
//            q._chars[3] = '=';
//        }
//    }
    static void b64ToData(b64q & q){
        static const base64rtable_t table;
        q._occupied = 3;
        if(q._chars[3] == '=') --(q._occupied);
        if(q._chars[2] == '=') --(q._occupied);
        unsigned char dummy1 = 0, dummy2 = 0;
        dummy1 = table[q._chars[0]];
        dummy2 = table[q._chars[1]];
        q._data->m.r.raw[0] = (dummy1 << 2) | (dummy2 >> 4);
        if(q._occupied == 1) return;
        dummy1 = table[q._chars[2]];
        q._data->m.r.raw[1] = (dummy2 << 4) | (dummy1 >> 2);
        if(q._occupied == 2) return;
        dummy2 = table[q._chars[3]];
        q._data->m.r.raw[2] = (dummy1 << 6) | (dummy2);
    }
//    static void b64ToData(b64q & q){
//        static const base64rtable_t table;
//        q._occupied = 3;
//        if(q._chars[3] == '=') --(q._occupied);
//        if(q._chars[2] == '=') --(q._occupied);
//        q._data->m.o3.a = table[q._chars[0]];
//        if(q._occupied == 1) {
//            q._data->m.o1.b = table[q._chars[1]] >> 4;
//        } else {
//            q._data->m.o3.b = table[q._chars[1]];
//            if(q._occupied == 2) {
//                q._data->m.o2.c = table[q._chars[3]] >> 2;
//            } else {
//                q._data->m.o3.c = table[q._chars[3]];
//                q._data->m.o3.d = table[q._chars[4]];
//            }
//        }
//    }
    static void encode(const unsigned char * buffer, unsigned int size, char * enc_buffer){
        for(unsigned int i(0); i < (size / 3); ++i){
            b64q dummy;
            dummy._data = (field_t *) (buffer + (i * 3));
            dummy._chars = enc_buffer + (i * 4);
            dummy._occupied = 3;
            dataToB64(dummy);
        }
        b64q dummy;
        dummy._occupied = size % 3;
        if(dummy._occupied){
            dummy._data = (field_t *) (buffer + (size - dummy._occupied));
            dummy._chars = enc_buffer + (4 * (size / 3));
            dataToB64(dummy);
        }
    }
    static void decode(const char * enc_buffer, unsigned int size, unsigned char * data_buffer){
        for(unsigned int i(0); i < (size + 2) / 3; ++i){
            b64q dummy;
            dummy._data = (field_t *) (data_buffer + (i * 3));
            dummy._chars = const_cast<char*>(enc_buffer) + (i * 4);
            b64ToData(dummy);
        }
    }
};

} // namespace util

#endif /* BASE64_HPP_ */
