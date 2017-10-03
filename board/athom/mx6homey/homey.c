/*
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 *
 * Author: Fabio Estevam <fabio.estevam@freescale.com>
 *
 * Copyright (C) 2013 Jon Nettleton <jon.nettleton@gmail.com>
 *
 * Based on SPL code from Solidrun tree, which is:
 * Author: Tungyi Lin <tungyilin1127@gmail.com>
 *
 * Derived from EDM_CF_IMX6 code by TechNexion,Inc
 * Ported to SolidRun microSOM by Rabeeh Khoury <rabeeh@solid-run.com>
 * Ported to Athom Homey by Jeroen Vollenbrock <jeroen@athom.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <asm/arch/clock.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/iomux.h>
#include <asm/arch/mx6-pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/gpio.h>
#include <asm/mach-imx/iomux-v3.h>
#include <mmc.h>
#include <fsl_esdhc.h>
#include <malloc.h>
#include <asm/arch/crm_regs.h>
#include <asm/io.h>
#include <asm/arch/sys_proto.h>
#include <spl.h>
#include <fat.h>

DECLARE_GLOBAL_DATA_PTR;

#define UART_PAD_CTRL	(PAD_CTL_PUS_100K_UP |			\
	PAD_CTL_SPEED_MED | PAD_CTL_DSE_40ohm |			\
	PAD_CTL_SRE_FAST	| PAD_CTL_HYS)

#define USDHC_PAD_CTRL (PAD_CTL_PUS_47K_UP |			\
	PAD_CTL_SPEED_LOW | PAD_CTL_DSE_80ohm |			\
	PAD_CTL_SRE_FAST	| PAD_CTL_HYS)


#define BOOTMODE_GPIO IMX_GPIO_NR(2, 20)
#define HWREF_SOM_GPIO_1 IMX_GPIO_NR(6, 0)
#define HWREF_SOM_GPIO_2 IMX_GPIO_NR(6, 4)

#define ZIGBEE_RESET_GPIO IMX_GPIO_NR(3, 9)
#define ZWAVE_RESET_GPIO IMX_GPIO_NR(3, 30)


#define MX6_FAST_BOOT	(0)
#define MX6_FLASH_BOOT	(0)
#define MX6_REC_BOOT	(1)
#define MX6_SLOW_BOOT	(2)

#define SNVS_LPGPR	0x68
#define GET_BOOTREG() (readl(SNVS_BASE_ADDR + SNVS_LPGPR) >> 8 & 0x3 )
#define SET_BOOTREG(data) writel( ((data & 0x3) << 8) | (readl(SNVS_BASE_ADDR + SNVS_LPGPR) & ~(0x3 << 8)), SNVS_BASE_ADDR + SNVS_LPGPR);

int ver_15_ = 0; /* 0 is original som, 1 is the rev 1.5 */

#define MEM_STRIDE 0x4000000
static u32 get_ram_size_stride_test(u32 *base, u32 maxsize) {
	volatile u32 *addr;
	u32		save[64];
	u32		cnt;
	u32		size;
	int		i = 0;

	/* First save the data */
	for (cnt = 0; cnt < maxsize; cnt += MEM_STRIDE) {
		addr = (volatile u32 *)((u32)base + cnt);		/* pointer arith! */
		sync ();
		save[i++] = *addr;
		sync ();
	}

	/* First write a signature */
	* (volatile u32 *)base = 0x12345678;
	for (size = MEM_STRIDE; size < maxsize; size += MEM_STRIDE) {
		* (volatile u32 *)((u32)base + size) = size;
		sync ();
		if (* (volatile u32 *)((u32)base) == size) {	/* We reached the overlapping address */
			break;
		}
	}

	/* Restore the data */
	for (cnt = (maxsize - MEM_STRIDE); i > 0; cnt -= MEM_STRIDE) {
		addr = (volatile u32 *)((u32)base + cnt);		/* pointer arith! */
		sync ();
		*addr = save[--i];
		sync ();
	}

	return (size);
}


int dram_init(void) {
	u32 max_size = imx_ddr_size();

	gd->ram_size = get_ram_size_stride_test((u32 *) CONFIG_SYS_SDRAM_BASE,
						(u32)max_size);

	return 0;
}


static iomux_v3_cfg_t const uart1_pads[] = {
	IOMUX_PADS(PAD_CSI0_DAT10__UART1_TX_DATA | MUX_PAD_CTRL(UART_PAD_CTRL)),
	IOMUX_PADS(PAD_CSI0_DAT11__UART1_RX_DATA | MUX_PAD_CTRL(UART_PAD_CTRL)),
};

static iomux_v3_cfg_t const usdhc3_pads[] = {
	IOMUX_PADS(PAD_SD3_CLK__SD3_CLK	| MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD3_CMD__SD3_CMD	| MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD3_DAT0__SD3_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD3_DAT1__SD3_DATA1	| MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD3_DAT2__SD3_DATA2	| MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD3_DAT3__SD3_DATA3	| MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD3_RST__SD3_RESET	| MUX_PAD_CTRL(USDHC_PAD_CTRL)),
};

static iomux_v3_cfg_t const hw_revision_sense[] = {
	/* These pins are for sensing if it is a CuBox-i or a HummingBoard */
	IOMUX_PADS(PAD_KEY_ROW1__GPIO4_IO09	| MUX_PAD_CTRL(UART_PAD_CTRL)),
	IOMUX_PADS(PAD_EIM_DA4__GPIO3_IO04	| MUX_PAD_CTRL(UART_PAD_CTRL)),

	IOMUX_PADS(PAD_CSI0_DAT14__GPIO6_IO00| MUX_PAD_CTRL(UART_PAD_CTRL)),
	IOMUX_PADS(PAD_CSI0_DAT18__GPIO6_IO04| MUX_PAD_CTRL(UART_PAD_CTRL)),
};

static iomux_v3_cfg_t const uc_bootmode_pads[] = {
	IOMUX_PADS(PAD_EIM_A18__GPIO2_IO20 | MUX_PAD_CTRL(UART_PAD_CTRL)),
};

static iomux_v3_cfg_t const modem_pads[] = {
	IOMUX_PADS(PAD_EIM_D30__GPIO3_IO30 | MUX_PAD_CTRL(UART_PAD_CTRL)),
	IOMUX_PADS(PAD_EIM_DA9__GPIO3_IO09 | MUX_PAD_CTRL(UART_PAD_CTRL)),
};


static void setup_iomux_uart(void) {
	SETUP_IOMUX_PADS(uart1_pads);
}

static struct fsl_esdhc_cfg usdhc_cfg[1] = {
	{USDHC3_BASE_ADDR},
};

int board_mmc_getcd(struct mmc *mmc) {
	return 1; /* uSDHC3 is always present */
}

int board_mmc_init(bd_t *bis) {
	SETUP_IOMUX_PADS(usdhc3_pads);
	usdhc_cfg[0].esdhc_base = USDHC3_BASE_ADDR;
	usdhc_cfg[0].sdhc_clk = mxc_get_clock(MXC_ESDHC3_CLK);
	gd->arch.sdhc_clk = usdhc_cfg[0].sdhc_clk;

	return fsl_esdhc_initialize(bis, &usdhc_cfg[0]);
}


int board_early_init_f(void) {
	int ret = 0;
	setup_iomux_uart();
	SETUP_IOMUX_PADS(uc_bootmode_pads);

    //set reset lines for modems low to avoid imx-uart sdma race
	SETUP_IOMUX_PADS(modem_pads);
	gpio_direction_output(ZIGBEE_RESET_GPIO, 0);
	gpio_direction_output(ZWAVE_RESET_GPIO, 0);

	return ret;
}

int board_init(void) {
	/* address of boot parameters */
	gd->bd->bi_boot_params = CONFIG_SYS_SDRAM_BASE + 0x100;

	return 0;
}

static bool is_15_som(void) {
	int val1, val2;

	SETUP_IOMUX_PADS(hw_revision_sense);

	gpio_direction_input(HWREF_SOM_GPIO_1);
	gpio_direction_input(HWREF_SOM_GPIO_2);

	val1 = gpio_get_value(HWREF_SOM_GPIO_1);
	val2 = gpio_get_value(HWREF_SOM_GPIO_2);

	return (val1 == 1 && val2 == 0);
}

static bool is_mx6q(void) {
	if (is_cpu_type(MXC_CPU_MX6Q) || is_cpu_type(MXC_CPU_MX6D))
		return true;
	else
		return false;
}


int board_late_init(void) {
#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG

	if (is_mx6q())
		env_set("board_rev", "imx6q");
	else
		env_set("board_rev", "imx6dl");

	if(is_15_som()) {
		env_set("som_rev", "-som-1.5");
	} else {
		env_set("som_rev", "");
	}

	if(readl(0x00907000) == 0xbaadadba) {
		puts("Found homey in bad MMC mode, flash boot selected...\n");
		SET_BOOTREG(MX6_FLASH_BOOT);
	}

	const char *bmode = "unknown";
	switch(GET_BOOTREG()) {
		case MX6_FLASH_BOOT:
			bmode = "flash_and_reset";
			SET_BOOTREG(MX6_REC_BOOT);
			if(readl(0x18000000) != 0x594d4f48) {
				puts("Invalid signature of MMC image.\n");
				bmode = "unknown";
			}
		break;
		case MX6_REC_BOOT:
			bmode = "ramdisk";
			SET_BOOTREG(MX6_SLOW_BOOT);
		break;
		case MX6_SLOW_BOOT:
		default:
			bmode = "linux";
		break;
	}

	env_set("bootmode", bmode);
	printf("Bootmode is: %s\n", bmode);

	if(readl(0x21BC6E0) & 1) {
		env_set("consoledev", "serial");
		env_set("consoletty", "ttymxc0,115200n8");
	} else {
		env_set("consoledev", "nulldev");
		env_set("consoletty", "tty0");
	}

	int cpurev = get_cpu_rev();
	env_set("cpu",get_imx_type((cpurev & 0xFF000) >> 12));
#endif

	return 0;
}

#ifdef CONFIG_SPL_BUILD
#include <asm/arch/mx6-ddr.h>
static const struct mx6dq_iomux_ddr_regs mx6q_ddr_ioregs = {
	.dram_sdclk_0 =  0x00020030,
	.dram_sdclk_1 =  0x00020030,
	.dram_cas =  0x00020030,
	.dram_ras =  0x00020030,
	.dram_reset =  0x00020030,
	.dram_sdcke0 =  0x00003000,
	.dram_sdcke1 =  0x00003000,
	.dram_sdba2 =  0x00000000,
	.dram_sdodt0 =  0x00003030,
	.dram_sdodt1 =  0x00003030,
	.dram_sdqs0 =  0x00000030,
	.dram_sdqs1 =  0x00000030,
	.dram_sdqs2 =  0x00000030,
	.dram_sdqs3 =  0x00000030,
	.dram_sdqs4 =  0x00000030,
	.dram_sdqs5 =  0x00000030,
	.dram_sdqs6 =  0x00000030,
	.dram_sdqs7 =  0x00000030,
	.dram_dqm0 =  0x00020030,
	.dram_dqm1 =  0x00020030,
	.dram_dqm2 =  0x00020030,
	.dram_dqm3 =  0x00020030,
	.dram_dqm4 =  0x00020030,
	.dram_dqm5 =  0x00020030,
	.dram_dqm6 =  0x00020030,
	.dram_dqm7 =  0x00020030,
};

static const struct mx6sdl_iomux_ddr_regs mx6dl_ddr_ioregs = {
	.dram_sdclk_0 = 0x00000028,
	.dram_sdclk_1 = 0x00000028,
	.dram_cas =	0x00000028,
	.dram_ras =	0x00000028,
	.dram_reset =	0x000c0028,
	.dram_sdcke0 =	0x00003000,
	.dram_sdcke1 =	0x00003000,
	.dram_sdba2 =	0x00000000,
	.dram_sdodt0 =	0x00003030,
	.dram_sdodt1 =	0x00003030,
	.dram_sdqs0 =	0x00000028,
	.dram_sdqs1 =	0x00000028,
	.dram_sdqs2 =	0x00000028,
	.dram_sdqs3 =	0x00000028,
	.dram_sdqs4 =	0x00000028,
	.dram_sdqs5 =	0x00000028,
	.dram_sdqs6 =	0x00000028,
	.dram_sdqs7 =	0x00000028,
	.dram_dqm0 =	0x00000028,
	.dram_dqm1 =	0x00000028,
	.dram_dqm2 =	0x00000028,
	.dram_dqm3 =	0x00000028,
	.dram_dqm4 =	0x00000028,
	.dram_dqm5 =	0x00000028,
	.dram_dqm6 =	0x00000028,
	.dram_dqm7 =	0x00000028,
};

static const struct mx6dq_iomux_grp_regs mx6q_grp_ioregs = {
	.grp_ddr_type =  0x000C0000,
	.grp_ddrmode_ctl =  0x00020000,
	.grp_ddrpke =  0x00000000,
	.grp_addds =  0x00000030,
	.grp_ctlds =  0x00000030,
	.grp_ddrmode =  0x00020000,
	.grp_b0ds =  0x00000030,
	.grp_b1ds =  0x00000030,
	.grp_b2ds =  0x00000030,
	.grp_b3ds =  0x00000030,
	.grp_b4ds =  0x00000030,
	.grp_b5ds =  0x00000030,
	.grp_b6ds =  0x00000030,
	.grp_b7ds =  0x00000030,
};

static const struct mx6sdl_iomux_grp_regs mx6sdl_grp_ioregs = {
	.grp_ddr_type = 0x000c0000,
	.grp_ddrmode_ctl = 0x00020000,
	.grp_ddrpke = 0x00000000,
	.grp_addds = 0x00000028,
	.grp_ctlds = 0x00000028,
	.grp_ddrmode = 0x00020000,
	.grp_b0ds = 0x00000028,
	.grp_b1ds = 0x00000028,
	.grp_b2ds = 0x00000028,
	.grp_b3ds = 0x00000028,
	.grp_b4ds = 0x00000028,
	.grp_b5ds = 0x00000028,
	.grp_b6ds = 0x00000028,
	.grp_b7ds = 0x00000028,
};

/* microSOM with Dual processor and 1GB memory */
static const struct mx6_mmdc_calibration mx6q_1g_mmcd_calib = {
	.p0_mpwldectrl0 =  0x00000000,
	.p0_mpwldectrl1 =  0x00000000,
	.p1_mpwldectrl0 =  0x00000000,
	.p1_mpwldectrl1 =  0x00000000,
	.p0_mpdgctrl0 =    0x0314031c,
	.p0_mpdgctrl1 =    0x023e0304,
	.p1_mpdgctrl0 =    0x03240330,
	.p1_mpdgctrl1 =    0x03180260,
	.p0_mprddlctl =    0x3630323c,
	.p1_mprddlctl =    0x3436283a,
	.p0_mpwrdlctl =    0x36344038,
	.p1_mpwrdlctl =    0x422a423c,
};

/* microSOM with Quad processor and 2GB memory */
static const struct mx6_mmdc_calibration mx6q_2g_mmcd_calib = {
	.p0_mpwldectrl0 =  0x00000000,
	.p0_mpwldectrl1 =  0x00000000,
	.p1_mpwldectrl0 =  0x00000000,
	.p1_mpwldectrl1 =  0x00000000,
	.p0_mpdgctrl0 =    0x0314031c,
	.p0_mpdgctrl1 =    0x023e0304,
	.p1_mpdgctrl0 =    0x03240330,
	.p1_mpdgctrl1 =    0x03180260,
	.p0_mprddlctl =    0x3630323c,
	.p1_mprddlctl =    0x3436283a,
	.p0_mpwrdlctl =    0x36344038,
	.p1_mpwrdlctl =    0x422a423c,
};

/* microSOM with Solo processor and 512MB memory */
static const struct mx6_mmdc_calibration mx6dl_512m_mmcd_calib = {
	.p0_mpwldectrl0 = 0x0045004D,
	.p0_mpwldectrl1 = 0x003A0047,
	.p0_mpdgctrl0 =   0x023C0224,
	.p0_mpdgctrl1 =   0x02000220,
	.p0_mprddlctl =   0x44444846,
	.p0_mpwrdlctl =   0x32343032,
};

/* microSOM with Dual lite processor and 1GB memory */
static const struct mx6_mmdc_calibration mx6dl_1g_mmcd_calib = {
	.p0_mpwldectrl0 =  0x0045004D,
	.p0_mpwldectrl1 =  0x003A0047,
	.p1_mpwldectrl0 =  0x001F001F,
	.p1_mpwldectrl1 =  0x00210035,
	.p0_mpdgctrl0 =    0x023C0224,
	.p0_mpdgctrl1 =    0x02000220,
	.p1_mpdgctrl0 =    0x02200220,
	.p1_mpdgctrl1 =    0x02040208,
	.p0_mprddlctl =    0x44444846,
	.p1_mprddlctl =    0x4042463C,
	.p0_mpwrdlctl =    0x32343032,
	.p1_mpwrdlctl =    0x36363430,
};

static struct mx6_ddr3_cfg mem_ddr_2g = {
	.mem_speed = 1600,
	.density   = 2,
	.width     = 16,
	.banks     = 8,
	.rowaddr   = 14,
	.coladdr   = 10,
	.pagesz    = 2,
	.trcd      = 1375,
	.trcmin    = 4875,
	.trasmin   = 3500,
	.SRT       = 1,
};

static struct mx6_ddr3_cfg mem_ddr_4g = {
	.mem_speed = 1600,
	.density   = 4,
	.width     = 16,
	.banks     = 8,
	.rowaddr   = 15,
	.coladdr   = 10,
	.pagesz    = 2,
	.trcd      = 1375,
	.trcmin    = 4875,
	.trasmin   = 3500,
};

static void ccgr_init(void) {
	struct mxc_ccm_reg *ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;

	writel(0x00C03F3F, &ccm->CCGR0);
	writel(0x0030FC03, &ccm->CCGR1);
	writel(0x0FFFC000, &ccm->CCGR2);
	writel(0x3FF00000, &ccm->CCGR3);
	writel(0x00FFF300, &ccm->CCGR4);
	writel(0x0F0000C3, &ccm->CCGR5);
	writel(0x000003FF, &ccm->CCGR6);
}

static void spl_dram_init(int width) {
	struct mx6_ddr_sysinfo sysinfo = {
		/* width of data bus: 0=16, 1=32, 2=64 */
		.dsize = width / 32,
		/* config for full 4GB range so that get_mem_size() works */
		.cs_density = 32,	/* 32Gb per CS */
		.ncs = 1,		/* single chip select */
		.cs1_mirror = 0,
		.rtt_wr = 1 /*DDR3_RTT_60_OHM*/,	/* RTT_Wr = RZQ/4 */
		.rtt_nom = 1 /*DDR3_RTT_60_OHM*/,	/* RTT_Nom = RZQ/4 */
		.walat = 1,	/* Write additional latency */
		.ralat = 5,	/* Read additional latency */
		.mif3_mode = 3,	/* Command prediction working mode */
		.bi_on = 1,	/* Bank interleaving enabled */
		.sde_to_rst = 0x10,	/* 14 cycles, 200us (JEDEC default) */
		.rst_to_cke = 0x23,	/* 33 cycles, 500us (JEDEC default) */
		.ddr_type = DDR_TYPE_DDR3,
		.refsel = 1,	/* Refresh cycles at 32KHz */
		.refr = 7,	/* 8 refresh commands per refresh cycle */
	};

	if (is_mx6dq())
		mx6dq_dram_iocfg(width, &mx6q_ddr_ioregs, &mx6q_grp_ioregs);
	else
		mx6sdl_dram_iocfg(width, &mx6dl_ddr_ioregs, &mx6sdl_grp_ioregs);

	if (is_cpu_type(MXC_CPU_MX6D))
		mx6_dram_cfg(&sysinfo, &mx6q_1g_mmcd_calib, &mem_ddr_2g);
	else if (is_cpu_type(MXC_CPU_MX6Q))
		mx6_dram_cfg(&sysinfo, &mx6q_2g_mmcd_calib, &mem_ddr_4g);
	else if (is_cpu_type(MXC_CPU_MX6DL))
		mx6_dram_cfg(&sysinfo, &mx6dl_1g_mmcd_calib, &mem_ddr_2g);
	else if (is_cpu_type(MXC_CPU_MX6SOLO))
		mx6_dram_cfg(&sysinfo, &mx6dl_512m_mmcd_calib, &mem_ddr_2g);
}

void board_init_f(ulong dummy) {
	/* setup AIPS and disable watchdog */
	arch_cpu_init();

	ccgr_init();
	gpr_init();

	/* iomux and setup of i2c */
	board_early_init_f();

	/* setup GP timer */
	timer_init();

	/* UART clocks enabled and gd valid - init serial console */
	preloader_console_init();

	/* DDR initialization */
	if (is_cpu_type(MXC_CPU_MX6SOLO))
		spl_dram_init(32);
	else
		spl_dram_init(64);

#ifndef CONFIG_SPL_OS_BOOT
	if(GET_BOOTREG() == MX6_FAST_BOOT) {
		//override FAST BOOT TO SLOW BOOT WHEN FALCON IS DISABLED
		SET_BOOTREG(MX6_SLOW_BOOT);
	}
#endif

	/* Clear the BSS. */
	memset(__bss_start, 0, __bss_end - __bss_start);

	if(spl_boot_device() != BOOT_DEVICE_MMC1) {
		//force boot device...
		puts("unknown boot device. setting to MMC1\n");
		writel(readl(&src_base->gpr10) | IMX6_SRC_GPR10_BMODE, &src_base->gpr10);
		u32 reg = readl(&src_base->gpr9);
		reg &= ~0x0000FFFF;
		reg |= 0x00003060;
		writel(reg, &src_base->gpr9);
	}

	/* load/boot image from boot device */
	board_init_r(NULL, 0);
}
#endif
