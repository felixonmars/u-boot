/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _SUNXI_CLOCK_SUN50I_H6_H
#define _SUNXI_CLOCK_SUN50I_H6_H

#ifndef __ASSEMBLY__
#include <linux/bitops.h>
#endif

#define CCU_H6_PLL6_CFG			0x020
#define CCU_MMC0_CLK_CFG		0x830
#define CCU_MMC1_CLK_CFG		0x834
#define CCU_MMC2_CLK_CFG		0x838
#define CCU_H6_MMC_GATE_RESET		0x84c

#define CCM_PLL_LOCK			BIT(28)
#define CCM_PLL6_CTRL_P0_SHIFT		16
#define CCM_PLL6_CTRL_P0_MASK		(0x7 << CCM_PLL6_CTRL_P0_SHIFT)
#define CCM_PLL6_CTRL_N_SHIFT		8
#define CCM_PLL6_CTRL_N_MASK		(0xff << CCM_PLL6_CTRL_N_SHIFT)
#define CCM_PLL6_CTRL_DIV2_SHIFT	1
#define CCM_PLL6_CTRL_DIV2_MASK		(0x1 << CCM_PLL6_CTRL_DIV2_SHIFT)

#define RESET_SHIFT			16
#define GATE_SHIFT			0

#define CCM_MMC_CTRL_M(x)		((x) - 1)
#define CCM_MMC_CTRL_N(x)		((x) << 8)
#define CCM_MMC_CTRL_OSCM24		(0x0 << 24)
#define CCM_MMC_CTRL_PLL6		(0x1 << 24)
#define CCM_MMC_CTRL_PLL_PERIPH2X2	(0x2 << 24)
#define CCM_MMC_CTRL_ENABLE		(0x1 << 31)
#define CCM_MMC_CTRL_OCLK_DLY(a)	((void)(a), 0)
#define CCM_MMC_CTRL_SCLK_DLY(a)	((void)(a), 0)

#endif /* _SUNXI_CLOCK_SUN50I_H6_H */
