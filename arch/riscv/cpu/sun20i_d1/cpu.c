// SPDX-License-Identifier: GPL-2.0+
/*
 * Allwinner D1 early RISC-V CPU setup.
 */

#include <irq_func.h>
#include <cpu_func.h>
#include <asm/cache.h>
#include <asm/csr.h>
#include <linux/bitops.h>

#define CSR_MXSTATUS		0x7c0
#define CSR_MHCR		0x7c1
#define CSR_MCOR		0x7c2
#define CSR_MHINT		0x7c5

#define MXSTATUS_THEADISAEE	BIT(22)
#define MXSTATUS_MAEE		BIT(21)
#define MXSTATUS_CLINTEE	BIT(17)
#define MXSTATUS_UCME		BIT(16)
#define MXSTATUS_MM		BIT(15)

#define MHCR_IE			BIT(0)
#define MHCR_DE			BIT(1)
#define MHCR_WA			BIT(2)
#define MHCR_WB			BIT(3)
#define MHCR_RS			BIT(4)
#define MHCR_BPE		BIT(5)
#define MHCR_BTB_C906		BIT(6)
#define MHCR_BTB_ARRAY		BIT(7)
#define MHCR_WBR		BIT(8)
#define MHCR_BTB_E906		BIT(12)

#define MHINT_DPLD		BIT(2)
#define MHINT_AMR_LIMIT_3	(0x1 << 3)
#define MHINT_IPLD		BIT(8)
#define MHINT_IWPE		BIT(9)
#define MHINT_D_DIS_PREFETCH_16	(0x3 << 13)
#define MHINT_L2PLD		BIT(15)
#define MHINT_L2_PREF_DIST(n)	((n) << 16)
#define MHINT_NSFE		BIT(18)
#define MHINT_AEE		BIT(20)

#define MCOR_C906_INIT		0x70013

#define THEAD_SYNC_I		".long 0x01a0000b"
#define THEAD_DCACHE_CIALL	".long 0x0030000b"
#define THEAD_DCACHE_IALL	".long 0x0020000b"

static void sync_i(void)
{
	asm volatile (THEAD_SYNC_I ::: "memory");
}

int cleanup_before_linux(void)
{
	disable_interrupts();

	if (CONFIG_IS_ENABLED(RISCV_MMODE)) {
		icache_disable();
		dcache_disable();
	} else {
		cache_flush();
	}

	return 0;
}

void harts_early_init(void)
{
	if (!CONFIG_IS_ENABLED(RISCV_MMODE))
		return;

	csr_write(CSR_MCOR, MCOR_C906_INIT);
	csr_set(CSR_MXSTATUS, MXSTATUS_THEADISAEE | MXSTATUS_MAEE |
			      MXSTATUS_CLINTEE | MXSTATUS_UCME | MXSTATUS_MM);
	csr_set(CSR_MHCR, MHCR_BTB_E906 | MHCR_WBR | MHCR_BTB_ARRAY |
			  MHCR_BTB_C906 | MHCR_BPE | MHCR_RS | MHCR_WB |
			  MHCR_WA);
	csr_set(CSR_MHINT, MHINT_AEE | MHINT_NSFE | MHINT_L2_PREF_DIST(2) |
			   MHINT_L2PLD | MHINT_D_DIS_PREFETCH_16 |
			   MHINT_IWPE | MHINT_IPLD | MHINT_AMR_LIMIT_3 |
			   MHINT_DPLD);

	if (IS_ENABLED(CONFIG_XPL_BUILD)) {
		if (!CONFIG_IS_ENABLED(SYS_ICACHE_OFF))
			icache_enable();
		if (!CONFIG_IS_ENABLED(SYS_DCACHE_OFF))
			dcache_enable();
	}
}

void flush_dcache_all(void)
{
	asm volatile (THEAD_DCACHE_CIALL ::: "memory");
	sync_i();
}

void invalidate_dcache_all(void)
{
	asm volatile (THEAD_DCACHE_IALL ::: "memory");
	sync_i();
}

#if CONFIG_IS_ENABLED(RISCV_MMODE)
void icache_enable(void)
{
	invalidate_icache_all();
	csr_set(CSR_MHCR, MHCR_IE);
}

void icache_disable(void)
{
	csr_clear(CSR_MHCR, MHCR_IE);
}

int icache_status(void)
{
	return !!(csr_read(CSR_MHCR) & MHCR_IE);
}

void dcache_enable(void)
{
	invalidate_dcache_all();
	csr_set(CSR_MHCR, MHCR_DE);
}

void dcache_disable(void)
{
	cache_flush();
	csr_clear(CSR_MHCR, MHCR_DE);
}

int dcache_status(void)
{
	return !!(csr_read(CSR_MHCR) & MHCR_DE);
}
#endif
