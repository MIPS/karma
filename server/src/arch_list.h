/*
 * arch_list.h - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
/*
 * arch_list.h
 *
 *  Created on: 26.10.2010
 */

#ifndef ARCH_LIST_H_
#define ARCH_LIST_H_

// enumeration architecture types
namespace arch{

class x86{
public:
    // enumeration extension types
    class svm{};
    class vmx{};

};

}

#endif /* ARCH_LIST_H_ */
