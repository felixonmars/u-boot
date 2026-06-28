// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (C) 2021 Samuel Holland <samuel@sholland.org>
 */

#include <clk-uclass.h>
#include <dm.h>
#include <errno.h>
#include <asm/io.h>
#include <clk/sunxi.h>
#include <dt-bindings/clock/sun20i-d1-ccu.h>
#include <dt-bindings/reset/sun20i-d1-ccu.h>
#include <linux/delay.h>
#include <linux/bitops.h>

#define D1_PLL_CPUX_CTRL			0x000
#define D1_PLL_CPUX_CTRL_EN			BIT(31)
#define D1_PLL_CPUX_LDO_EN			BIT(30)
#define D1_PLL_CPUX_LOCK_EN			BIT(29)
#define D1_PLL_CPUX_LOCK			BIT(28)
#define D1_PLL_CPUX_OUT_EN			BIT(27)
#define D1_PLL_CPUX_N_MASK			GENMASK(15, 8)
#define D1_PLL_CPUX_N(n)			(((n) - 1) << 8)

static struct ccu_clk_gate d1_gates[] = {
	[CLK_APB0]		= GATE_DUMMY,

	[CLK_BUS_MMC0]		= GATE(0x84c, BIT(0)),
	[CLK_BUS_MMC1]		= GATE(0x84c, BIT(1)),
	[CLK_BUS_MMC2]		= GATE(0x84c, BIT(2)),
	[CLK_BUS_UART0]		= GATE(0x90c, BIT(0)),
	[CLK_BUS_UART1]		= GATE(0x90c, BIT(1)),
	[CLK_BUS_UART2]		= GATE(0x90c, BIT(2)),
	[CLK_BUS_UART3]		= GATE(0x90c, BIT(3)),
	[CLK_BUS_UART4]		= GATE(0x90c, BIT(4)),
	[CLK_BUS_UART5]		= GATE(0x90c, BIT(5)),
	[CLK_BUS_I2C0]		= GATE(0x91c, BIT(0)),
	[CLK_BUS_I2C1]		= GATE(0x91c, BIT(1)),
	[CLK_BUS_I2C2]		= GATE(0x91c, BIT(2)),
	[CLK_BUS_I2C3]		= GATE(0x91c, BIT(3)),
	[CLK_SPI0]		= GATE(0x940, BIT(31)),
	[CLK_SPI1]		= GATE(0x944, BIT(31)),
	[CLK_BUS_SPI0]		= GATE(0x96c, BIT(0)),
	[CLK_BUS_SPI1]		= GATE(0x96c, BIT(1)),

	[CLK_BUS_EMAC]		= GATE(0x97c, BIT(0)),

	[CLK_USB_OHCI0]		= GATE(0xa70, BIT(31)),
	[CLK_USB_OHCI1]		= GATE(0xa74, BIT(31)),
	[CLK_BUS_OHCI0]		= GATE(0xa8c, BIT(0)),
	[CLK_BUS_OHCI1]		= GATE(0xa8c, BIT(1)),
	[CLK_BUS_EHCI0]		= GATE(0xa8c, BIT(4)),
	[CLK_BUS_EHCI1]		= GATE(0xa8c, BIT(5)),
	[CLK_BUS_OTG]		= GATE(0xa8c, BIT(8)),
	[CLK_BUS_LRADC]		= GATE(0xa9c, BIT(0)),

	[CLK_RISCV]		= GATE(0xd04, BIT(31)),
};

static struct ccu_reset d1_resets[] = {
	[RST_BUS_MMC0]		= RESET(0x84c, BIT(16)),
	[RST_BUS_MMC1]		= RESET(0x84c, BIT(17)),
	[RST_BUS_MMC2]		= RESET(0x84c, BIT(18)),
	[RST_BUS_UART0]		= RESET(0x90c, BIT(16)),
	[RST_BUS_UART1]		= RESET(0x90c, BIT(17)),
	[RST_BUS_UART2]		= RESET(0x90c, BIT(18)),
	[RST_BUS_UART3]		= RESET(0x90c, BIT(19)),
	[RST_BUS_UART4]		= RESET(0x90c, BIT(20)),
	[RST_BUS_UART5]		= RESET(0x90c, BIT(21)),
	[RST_BUS_I2C0]		= RESET(0x91c, BIT(16)),
	[RST_BUS_I2C1]		= RESET(0x91c, BIT(17)),
	[RST_BUS_I2C2]		= RESET(0x91c, BIT(18)),
	[RST_BUS_I2C3]		= RESET(0x91c, BIT(19)),
	[RST_BUS_SPI0]		= RESET(0x96c, BIT(16)),
	[RST_BUS_SPI1]		= RESET(0x96c, BIT(17)),

	[RST_BUS_EMAC]		= RESET(0x97c, BIT(16)),

	[RST_USB_PHY0]		= RESET(0xa70, BIT(30)),
	[RST_USB_PHY1]		= RESET(0xa74, BIT(30)),
	[RST_BUS_OHCI0]		= RESET(0xa8c, BIT(16)),
	[RST_BUS_OHCI1]		= RESET(0xa8c, BIT(17)),
	[RST_BUS_EHCI0]		= RESET(0xa8c, BIT(20)),
	[RST_BUS_EHCI1]		= RESET(0xa8c, BIT(21)),
	[RST_BUS_OTG]		= RESET(0xa8c, BIT(24)),
	[RST_BUS_LRADC]		= RESET(0xa9c, BIT(16)),
};

static void d1_ccu_init(struct udevice *dev)
{
	struct ccu_plat *plat = dev_get_plat(dev);
	u32 val;

	if (!IS_ENABLED(CONFIG_TARGET_SUN20I_D1))
		return;

	val = readl(plat->base + D1_PLL_CPUX_CTRL);
	val &= ~D1_PLL_CPUX_N_MASK;
	val |= D1_PLL_CPUX_CTRL_EN | D1_PLL_CPUX_LDO_EN |
	       D1_PLL_CPUX_LOCK_EN | D1_PLL_CPUX_OUT_EN |
	       D1_PLL_CPUX_N(42);
	writel(val, plat->base + D1_PLL_CPUX_CTRL);

	while (!(readl(plat->base + D1_PLL_CPUX_CTRL) & D1_PLL_CPUX_LOCK))
		;
	udelay(20);
}

const struct ccu_desc d1_ccu_desc = {
	.gates	= d1_gates,
	.resets	= d1_resets,
	.num_gates = ARRAY_SIZE(d1_gates),
	.num_resets = ARRAY_SIZE(d1_resets),
	.xpl_init = d1_ccu_init,
};
