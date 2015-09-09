/*
 * cas.hpp - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
/*
 * cas.hpp
 *
 *  Created on: 07.11.2010
 *      Author: janis
 */

#ifndef CAS_HPP_
#define CAS_HPP_

#include <l4/util/atomic.h>
#include <l4/re/env.h>

namespace util{

inline static bool cas8Bcond(l4_uint64_t * location, l4_uint64_t & old_value, const l4_uint64_t new_value)
{
#ifdef __L4UTIL_ATOMIC_HAVE_ARCH_CMPXCHG64
    if (l4util_cmpxchg64(location, old_value, new_value) == 0)
    	return false;

    return true;
#else
    if (*location == old_value) {
        *location = new_value;
        return true;
    } else {
        return false;
    }
#endif
}

inline static l4_uint32_t cas32(l4_uint32_t * location, const l4_uint32_t new_value)
{
    l4_uint32_t result = *location;
    l4util_cmpxchg32(location, result, new_value);
    return result;
}

inline static l4_uint64_t cas8Badd(l4_uint64_t * location, const l4_uint64_t increment)
{
    l4_uint64_t old, res;

#ifdef __L4UTIL_ATOMIC_HAVE_ARCH_CMPXCHG64
    do {
    	old = *location;
    	res = old + increment;
    } while (!l4util_cmpxchg64(location, old, res));

    return res;
#else
    old = *location;
    res = old + increment;
    *location = res;

    return res;
#endif
}

inline static l4_uint32_t cas32add(l4_uint32_t * location, const l4_uint32_t increment)
{
    return l4util_add32_res(location, increment);
}

/*
 * Our "TSC" is CPU-agnostic. We use the KIP here.
 * Make sure, you run a high-res clock on Fisco with CONFIG_SYNC_TSC.
 */
inline l4_uint64_t tsc()
{
    return l4_kip_clock(l4re_kip());
}

class mutex{
private:
    unsigned _sem;
public:
    mutex() : _sem(1)
    {
    }
    void enter(){
        while(cas32(&_sem, 0) == 0);
    }
    void leave(){
        cas32(&_sem, 1);
    }
};


} // namespace util

#endif /* CAS_HPP_ */
