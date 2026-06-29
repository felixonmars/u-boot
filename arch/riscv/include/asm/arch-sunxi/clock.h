/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _SUNXI_CLOCK_H
#define _SUNXI_CLOCK_H

#include <linux/types.h>
#include <asm/arch/cpu.h>
#include <asm/arch/clock_sun50i_h6.h>

#define CLK_GATE_OPEN			0x1
#define CLK_GATE_CLOSE			0x0

#ifndef __ASSEMBLY__
unsigned int clock_get_pll6(void);
#endif

#endif /* _SUNXI_CLOCK_H */
