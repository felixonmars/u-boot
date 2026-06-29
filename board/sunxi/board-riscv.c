// SPDX-License-Identifier: GPL-2.0+
/*
 * RISC-V board support for Allwinner sun20i SoCs.
 */

#include <cpu.h>
#include <cpu_func.h>
#include <dm.h>
#include <env.h>
#include <env_internal.h>
#include <fdt_support.h>
#include <image.h>
#include <init.h>
#include <log.h>
#include <mmc.h>
#include <net.h>
#include <ram.h>
#include <spl.h>
#include <sunxi_image.h>
#include <u-boot/crc.h>
#include <asm/arch/clock.h>
#include <asm/arch/cpu.h>
#include <asm/arch/serial.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <linux/libfdt.h>
#include <linux/string.h>

DECLARE_GLOBAL_DATA_PTR;

#define SPL_ADDR		CONFIG_SUNXI_SRAM_ADDRESS

#define SUNXI_BOOTED_FROM_MMC0		0
#define SUNXI_BOOTED_FROM_NAND		1
#define SUNXI_BOOTED_FROM_MMC2		2
#define SUNXI_BOOTED_FROM_SPI		3
#define SUNXI_BOOTED_FROM_MMC0_HIGH	0x10
#define SUNXI_BOOTED_FROM_MMC2_HIGH	0x12
#define SUNXI_INVALID_BOOT_SOURCE	-1

#define SUN20I_D1_PB_CFG1		((void __iomem *)0x02000034)
#define SUN20I_D1_PB_DRV1		((void __iomem *)0x02000048)
#define SUN20I_D1_PB_PULL1		((void __iomem *)0x02000054)
#define SUN20I_D1_UART_BGR_REG		((void __iomem *)0x0200190c)
#define SUN20I_D1_UART0_USR		((void __iomem *)(SUNXI_UART0_BASE + 0x7c))

#ifdef CONFIG_XPL_BUILD
static void sunxi_spl_store_dram_size(size_t dram_size);
#endif

#ifdef CONFIG_DEBUG_UART_BOARD_INIT
void board_debug_uart_init(void)
{
	int timeout = 100000;

	/* Deassert reset and gate the bus clock for UART0. */
	setbits_le32(SUN20I_D1_UART_BGR_REG, BIT(16) | BIT(0));

	/* Configure PB8/PB9 for UART0. */
	clrsetbits_le32(SUN20I_D1_PB_CFG1, 0xff, 0x66);
	clrsetbits_le32(SUN20I_D1_PB_DRV1, 0xff, 0x11);
	clrbits_le32(SUN20I_D1_PB_PULL1, 0xf << 16);

	/* Wait for the DesignWare UART to become idle before reprogramming it. */
	while ((readl(SUN20I_D1_UART0_USR) & BIT(0)) && --timeout)
		;
}
#endif

int board_init(void)
{
	return cpu_probe_all();
}

int sunxi_get_sid(unsigned int *sid)
{
	int i;

	for (i = 0; i < 4; i++)
		sid[i] = readl((void __iomem *)(ulong)(SUNXI_SID_BASE + 4 * i));

	return 0;
}

unsigned int clock_get_pll6(void)
{
	void *const ccm = (void *)SUNXI_CCM_BASE;
	uint32_t rval = readl(ccm + CCU_H6_PLL6_CFG);
	int n = ((rval & CCM_PLL6_CTRL_N_MASK) >> CCM_PLL6_CTRL_N_SHIFT) + 1;
	int div1 = ((rval & CCM_PLL6_CTRL_P0_MASK) >>
		    CCM_PLL6_CTRL_P0_SHIFT) + 1;
	int div2 = ((rval & CCM_PLL6_CTRL_DIV2_MASK) >>
		    CCM_PLL6_CTRL_DIV2_SHIFT) + 1;

	return 24000000U * n / 2 / div1 / div2;
}

static bool sunxi_egon_valid(struct boot_file_head *egon_head)
{
	return !memcmp(egon_head->magic, BOOT0_MAGIC, 8);
}

static bool sunxi_toc0_valid(struct toc0_main_info *toc0_info)
{
	return !memcmp(toc0_info->name, TOC0_MAIN_INFO_NAME, 8);
}

static int sunxi_get_boot_source(void)
{
	struct boot_file_head *egon_head = (void *)SPL_ADDR;
	struct toc0_main_info *toc0_info = (void *)SPL_ADDR;

	if (sunxi_egon_valid(egon_head))
		return readb(&egon_head->boot_media);
	if (sunxi_toc0_valid(toc0_info))
		return readb(&toc0_info->platform[0]);

	return SUNXI_INVALID_BOOT_SOURCE;
}

uint32_t sunxi_get_boot_device(void)
{
	switch (sunxi_get_boot_source()) {
	case SUNXI_INVALID_BOOT_SOURCE:
		return BOOT_DEVICE_BOARD;
	case SUNXI_BOOTED_FROM_MMC0:
	case SUNXI_BOOTED_FROM_MMC0_HIGH:
		return BOOT_DEVICE_MMC1;
	case SUNXI_BOOTED_FROM_NAND:
		return BOOT_DEVICE_NAND;
	case SUNXI_BOOTED_FROM_MMC2:
	case SUNXI_BOOTED_FROM_MMC2_HIGH:
		return BOOT_DEVICE_MMC2;
	case SUNXI_BOOTED_FROM_SPI:
		return BOOT_DEVICE_SPI;
	}

	panic("Unknown boot source\n");
	return -1;
}

#ifdef CONFIG_XPL_BUILD
uint32_t sunxi_get_spl_size(void)
{
	struct boot_file_head *egon_head = (void *)SPL_ADDR;
	struct toc0_main_info *toc0_info = (void *)SPL_ADDR;

	if (sunxi_egon_valid(egon_head))
		return readl(&egon_head->length);
	if (sunxi_toc0_valid(toc0_info))
		return readl(&toc0_info->length);

	return 0;
}

unsigned long board_spl_mmc_get_uboot_raw_sector(struct mmc *mmc,
						 unsigned long raw_sect)
{
	unsigned long spl_size = sunxi_get_spl_size();
	unsigned long sector = max(raw_sect, spl_size / 512);

	switch (sunxi_get_boot_source()) {
	case SUNXI_BOOTED_FROM_MMC0_HIGH:
	case SUNXI_BOOTED_FROM_MMC2_HIGH:
		sector += (128 - 8) * 2;
		break;
	}

	return sector;
}

u32 spl_boot_device(void)
{
	return sunxi_get_boot_device();
}

int spl_board_init_f(void)
{
	struct udevice *dev;
	int ret;

	ret = cpu_probe_all();
	if (ret)
		debug("CPU init failed: %d\n", ret);

	ret = uclass_get_device(UCLASS_RAM, 0, &dev);
	if (ret) {
		debug("DRAM init failed: %d\n", ret);
		return ret;
	}

	return 0;
}

void spl_perform_board_fixups(struct spl_image_info *spl_image)
{
	struct ram_info info;
	struct udevice *dev;
	int ret;

	ret = uclass_get_device(UCLASS_RAM, 0, &dev);
	if (ret)
		panic("No RAM device");

	ret = ram_get_info(dev, &info);
	if (ret)
		panic("No RAM info");

	sunxi_spl_store_dram_size(info.size);

	ret = fdt_fixup_memory(spl_image->fdt_addr, info.base, info.size);
	if (ret)
		panic("Failed to update DTB");
}
#endif

enum env_location env_get_location(enum env_operation op, int prio)
{
	if (prio > 1)
		return ENVL_UNKNOWN;

	if (IS_ENABLED(CONFIG_ENV_IS_NOWHERE))
		return ENVL_NOWHERE;

	switch (sunxi_get_boot_device()) {
	case BOOT_DEVICE_MMC1:
	case BOOT_DEVICE_MMC2:
		if (prio == 0 && IS_ENABLED(CONFIG_ENV_IS_IN_FAT))
			return ENVL_FAT;
		if (IS_ENABLED(CONFIG_ENV_IS_IN_MMC))
			return ENVL_MMC;
		break;
	case BOOT_DEVICE_NAND:
		if (prio == 0 && IS_ENABLED(CONFIG_ENV_IS_IN_UBI))
			return ENVL_UBI;
		if (IS_ENABLED(CONFIG_ENV_IS_IN_NAND))
			return ENVL_NAND;
		break;
	case BOOT_DEVICE_SPI:
		if (prio == 0 && IS_ENABLED(CONFIG_ENV_IS_IN_SPI_FLASH))
			return ENVL_SPI_FLASH;
		if (IS_ENABLED(CONFIG_ENV_IS_IN_FAT))
			return ENVL_FAT;
		break;
	case BOOT_DEVICE_BOARD:
		break;
	default:
		break;
	}

	if (prio == 0) {
		if (IS_ENABLED(CONFIG_ENV_IS_IN_FAT))
			return ENVL_FAT;
		if (IS_ENABLED(CONFIG_ENV_IS_IN_UBI))
			return ENVL_UBI;
	}

	return ENVL_UNKNOWN;
}

#define INVALID_SPL_HEADER ((void *)~0UL)

static struct boot_file_head *get_spl_header(uint8_t req_version)
{
	struct boot_file_head *spl = (void *)(ulong)SPL_ADDR;
	uint8_t spl_header_version = spl->spl_signature[3];

	if (memcmp(spl->spl_signature, SPL_SIGNATURE, 3) != 0)
		return INVALID_SPL_HEADER;

	if (spl_header_version < req_version) {
		printf("sunxi SPL version mismatch: expected %u, got %u\n",
		       req_version, spl_header_version);
		return INVALID_SPL_HEADER;
	}

	return spl;
}

#ifdef CONFIG_XPL_BUILD
static void sunxi_spl_store_dram_size(size_t dram_size)
{
	struct boot_file_head *spl = get_spl_header(SPL_DT_HEADER_VERSION);

	if (spl == INVALID_SPL_HEADER)
		return;

	if (spl->spl_signature[3] < SPL_DRAM_HEADER_VERSION)
		spl->spl_signature[3] = SPL_DRAM_HEADER_VERSION;

	spl->dram_size = dram_size >> 20;
	flush_dcache_range((ulong)spl, (ulong)spl + sizeof(*spl));
}
#endif

static const char *get_spl_dt_name(void)
{
	struct boot_file_head *spl = get_spl_header(SPL_DT_HEADER_VERSION);

	if (spl != INVALID_SPL_HEADER && spl->dt_name_offset)
		return (char *)spl + spl->dt_name_offset;

	return NULL;
}

#ifdef CONFIG_ENV_MMC_DEVICE_INDEX
int mmc_get_env_dev(void)
{
	switch (sunxi_get_boot_device()) {
	case BOOT_DEVICE_MMC1:
		return 0;
	case BOOT_DEVICE_MMC2:
		return 1;
	default:
		return CONFIG_ENV_MMC_DEVICE_INDEX;
	}
}
#endif

static void parse_spl_header(void)
{
	struct boot_file_head *spl = get_spl_header(SPL_ENV_HEADER_VERSION);

	if (spl == INVALID_SPL_HEADER || !spl->fel_script_address)
		return;

	if (spl->fel_uEnv_length) {
		himport_r(&env_htab, (char *)(uintptr_t)spl->fel_script_address,
			  spl->fel_uEnv_length, '\n', H_NOCLEAR, 0, 0, NULL);
		return;
	}

	env_set_hex("fel_scriptaddr", spl->fel_script_address);
}

static bool get_unique_sid(unsigned int *sid)
{
	if (sunxi_get_sid(sid) != 0 || !sid[0])
		return false;

	sid[3] = crc32(0, (unsigned char *)&sid[1], 12);
	if ((sid[3] & 0xffffff) == 0)
		sid[3] |= 0x800000;

	return true;
}

static void setup_environment(const void *fdt)
{
	char serial_string[17] = { 0 };
	unsigned int sid[4];
	uint8_t mac_addr[6];
	char ethaddr[16];
	int i;

	if (!get_unique_sid(sid))
		return;

	for (i = 0; i < 4; i++) {
		sprintf(ethaddr, "ethernet%d", i);
		if (!fdt_get_alias(fdt, ethaddr))
			continue;

		if (i == 0)
			strcpy(ethaddr, "ethaddr");
		else
			sprintf(ethaddr, "eth%daddr", i);

		if (env_get(ethaddr))
			continue;

		mac_addr[0] = (i << 4) | 0x02;
		mac_addr[1] = sid[0] & 0xff;
		mac_addr[2] = (sid[3] >> 24) & 0xff;
		mac_addr[3] = (sid[3] >> 16) & 0xff;
		mac_addr[4] = (sid[3] >> 8) & 0xff;
		mac_addr[5] = sid[3] & 0xff;

		eth_env_set_enetaddr(ethaddr, mac_addr);
	}

	if (!env_get("serial#")) {
		snprintf(serial_string, sizeof(serial_string),
			 "%08x%08x", sid[0], sid[3]);
		env_set("serial#", serial_string);
	}
}

int misc_init_r(void)
{
	const char *spl_dt_name;
	uint boot;

	env_set("fel_booted", NULL);
	env_set("fel_scriptaddr", NULL);
	env_set("mmc_bootdev", NULL);

	boot = sunxi_get_boot_device();
	if (boot == BOOT_DEVICE_BOARD) {
		env_set("fel_booted", "1");
		parse_spl_header();
	} else if (boot == BOOT_DEVICE_MMC1) {
		env_set("mmc_bootdev", "0");
	} else if (boot == BOOT_DEVICE_MMC2) {
		env_set("mmc_bootdev", "1");
	}

	spl_dt_name = get_spl_dt_name();
	if (!spl_dt_name)
		spl_dt_name = CONFIG_DEFAULT_DEVICE_TREE;
	if (spl_dt_name) {
		const char *vendor = "allwinner/";
		const char *prefix = "";
		char str[64];

		if (IS_ENABLED(CONFIG_OF_UPSTREAM)) {
			if (!strncmp(spl_dt_name, vendor, strlen(vendor)))
				spl_dt_name += strlen(vendor);
		} else if (!strncmp(spl_dt_name, vendor, strlen(vendor))) {
			prefix = "";
		} else {
			prefix = "allwinner/";
		}

		snprintf(str, sizeof(str), "%s%s.dtb", prefix, spl_dt_name);
		env_set("fdtfile", str);
	}

	setup_environment(gd->fdt_blob);

	return 0;
}

int board_late_init(void)
{
#ifdef CONFIG_USB_ETHER
	usb_ether_init();
#endif

	return 0;
}

int ft_board_setup(void *blob, struct bd_info *bd)
{
	setup_environment(blob);
	fdt_fixup_ethernet(blob);

	return 0;
}

#ifdef CONFIG_SPL_LOAD_FIT
static void set_spl_dt_name(const char *name)
{
	struct boot_file_head *spl = get_spl_header(SPL_ENV_HEADER_VERSION);

	if (spl == INVALID_SPL_HEADER)
		return;

	if (spl->spl_signature[3] < SPL_DT_HEADER_VERSION)
		spl->spl_signature[3] = SPL_DT_HEADER_VERSION;

	strcpy((char *)&spl->string_pool, name);
	spl->dt_name_offset = offsetof(struct boot_file_head, string_pool);
}

int board_fit_config_name_match(const char *name)
{
	const char *best_dt_name = get_spl_dt_name();
	int ret;

	if (!best_dt_name)
		best_dt_name = CONFIG_DEFAULT_DEVICE_TREE;

	ret = strcmp(name, best_dt_name);
	if (ret == 0)
		set_spl_dt_name(best_dt_name);

	return ret;
}
#endif
