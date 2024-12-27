/*
 *  Copyright (C) 2002-2010  The DOSBox Team
 *  OPL2/OPL3 emulation library
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 * 
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 * 
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


/*
 * Originally based on ADLIBEMU.C, an AdLib/OPL2 emulation library by Ken Silverman
 * Copyright (C) 1998-2001 Ken Silverman
 * Ken Silverman's official web site: "http://www.advsys.net/ken"
 */

#ifdef WITH_HW_OPL

#include "opl.h"
#include <math.h>
#include <stdbool.h>

// todo: use isa_bios
static unsigned int isa_base = 0x81000000UL;
static unsigned int opl_port = 0x388;
static int opl_delay_idx = 6;
static int opl_delay_val = 35;
static int opl_chip = 0;

static inline void outp(unsigned int port, unsigned char data) {
    *((volatile unsigned char*)(isa_base + port)) = data;
}

static inline unsigned char inp(unsigned int port) {
    return *((volatile unsigned char*)(isa_base + port));
}

static inline void adlib_delay(int count) {
/*
    int i;
    for (i = 0; i < count; i++) {
        inp(0x80);
    }
*/
}

void adlib_init(Bit32u samplerate) {
    // detect opl2/3 and set up delays

    opl_chip = 0;

    if (opl_chip == 3) {
        opl_delay_idx = 6;
        opl_delay_idx = 35;
    }
}

void adlib_write(Bitu idx, Bit8u val) {
    if (opl_chip != 0) {
        outp(opl_port + 0, idx);
        adlib_delay(opl_delay_idx);     // pc 6, mxplay 12
        outp(opl_port + 1, val);
        adlib_delay(opl_delay_val);    // pc 35, mxplay 64
    }
}

Bitu adlib_reg_read(Bitu port) {
    /* unused */
    return 0;
}

void adlib_write_index(Bitu port, Bit8u val) {
    /* unused */
}

#endif /* WITH_HW_OPL */
