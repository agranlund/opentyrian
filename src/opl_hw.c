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
#include "isa.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

static unsigned int opl_port = 0x388;
static int opl_delay_idx = 8;
static int opl_delay_val = 32;
static int opl_chip = 0;

static inline void adlib_delay(unsigned long usec) {
    isa_delay(usec);
}

int adlib_detect(unsigned short port) {
    opl_chip = 2;
    opl_port = port;
    adlib_write(0x04, 0x60);    // reset timer1+2 and irq
    adlib_write(0x04, 0x80);
    unsigned char result1 = inp(opl_port);
    adlib_write(0x02, 0xff);    // set timer1
    adlib_write(0x04, 0x21);    // start timer1
    adlib_delay(2000);
    unsigned char result2 = inp(opl_port);
    adlib_write(0x04, 0x60);    // reset timer1+2 and irq
    adlib_write(0x04, 0x80);

    opl_chip = 0;
    if (((result1 & 0xe0) == 0x00) && ((result2 & 0xe0) == 0xc0)) {
        opl_chip = 2;
        if ((result2 & 0x06) == 0x00) {
            opl_chip = 3;
        }
    }
    return opl_chip;
}

void adlib_init(Bit32u samplerate) {
    (void)samplerate;

    if (!isa_init()) {
        return;
    }

    if (adlib_detect(0x388) == 0) {
        printf("No Adlib detected\n");
        return;
    }

    printf("Adlib (OPL%d) detected on port %03x\n", opl_chip, opl_port);

    if (opl_chip == 3) {
        opl_delay_idx = 8;
        opl_delay_idx = 8;
    }
}

void adlib_write(Bitu idx, Bit8u val) {
    if (opl_chip != 0) {
        if (idx >= 0x100) {
            // opl3 second register set
            outp(opl_port + 2, (idx - 0x100));
            adlib_delay(opl_delay_idx);
            outp(opl_port + 3, val);
            adlib_delay(opl_delay_val);
        } else {
            // opl2 / opl3 first register setc
            outp(opl_port + 0, idx);
            adlib_delay(opl_delay_idx);
            outp(opl_port + 1, val);
            adlib_delay(opl_delay_val);
        }
    }
}

Bitu adlib_reg_read(Bitu port) {
    /* unused */
    (void) port;
    return 0;
}

void adlib_write_index(Bitu port, Bit8u val) {
    /* unused */
    (void)port; (void)val;
}

#endif /* WITH_HW_OPL */
