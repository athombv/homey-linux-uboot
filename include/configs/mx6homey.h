/*
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 *
 * Configuration settings for the SolidRun mx6 based boards
 * 
 * Modified for Athom Homey by Jeroen Vollenbrock <jeroen@athom.com
 *
 * SPDX-License-Identifier:    GPL-2.0+
 */
#ifndef __MX6HOMEY_CONFIG_H
#define __MX6HOMEY_CONFIG_H

#include <config_distro_defaults.h>
#include "mx6_common.h"

#include "imx6_spl.h"

#define CONFIG_SYS_MALLOC_LEN        (10 * SZ_1M)
#define CONFIG_MXC_UART

/* MMC Configs */
#define CONFIG_SYS_FSL_ESDHC_ADDR    USDHC3_BASE_ADDR


/* CONSOLE */
#define CONFIG_PREBOOT \
    "setenv stdin ${consoledev}; " \
    "setenv stdout ${consoledev}; " \
    "setenv stderr serial; "

/* Command definition */

#define CONFIG_MXC_UART_BASE    UART1_BASE
#define CONSOLE_DEV    "ttymxc0"
#define CONFIG_SYS_FSL_USDHC_NUM    1
#define CONFIG_SYS_MMC_ENV_DEV        0    /* SDHC3 */


#define CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
#ifndef CONFIG_SPL_BUILD
#define CONFIG_EXTRA_ENV_SETTINGS \
    "image=uImage\0" \
    "ramdisk_file=busybox.rd\0" \
    "ramdisk_addr_r=0x19000000\0" \
    "fdtfile=undefined\0" \
    "fdt_addr_r=0x18000000\0" \
    "ramdisk_force_addr=0x18000010\0" \
    "ramdisk_force_ref_addr=0x18000020\0" \
    "baudrate=115200n8\0" \
    "bootm_size=0x10000000\0" \
    "mmcdev=" __stringify(CONFIG_SYS_MMC_ENV_DEV) "\0" \
    "mmcpart=1\0" \
    "finduuid=part uuid mmc ${mmcdev}:${mmcpart} uuid\0" \
    "findfdt=setenv fdtfile ${board_rev}-homey${som_rev}.dtb; \0" \
    "loadimage=fatload mmc ${mmcdev}:${mmcpart} ${loadaddr} ${image}\0" \
    "loadfdt=fatload mmc ${mmcdev}:${mmcpart} ${fdt_addr_r} ${fdtfile}\0" \
    "loadramdisk=fatload mmc ${mmcdev}:${mmcpart} ${ramdisk_addr_r} ${ramdisk_file}\0" \
    "mmcboot=echo Booting from mmc ...; " \
        "if run loadfdt; then " \
            "if test $bootmode = ramdisk; then " \
                "run loadramdisk; " \
                "echo Loaded ramdisk.; " \
                "setenv bootargs rdinit=/sbin/init rootwait console=${consoletty}; " \
            "else " \
                "setenv bootargs console=${consoletty} root=PARTUUID=${uuid}/PARTNROFF=1 rootwait rw rootfstype=ext4 " \
                        "rd.dm=0 rd.luks=0 rd.lvm=0 raid=noautodetect; " \
                "setenv ramdisk_addr_r -; " \
            "fi; " \
            "bootm ${loadaddr} ${ramdisk_addr_r} ${fdt_addr_r}; " \
        "else " \
            "echo WARN: Cannot load the DT; " \
        "fi; \0"

#define CONFIG_BOOTCOMMAND \
    "mmc dev ${mmcdev}; " \
    "if test $bootmode = linux || test $bootmode = ramdisk; then " \
        "mmc read ${fdt_addr_r} 0 1; " \
        "mw.l ${ramdisk_force_ref_addr} 1 1; " \
        "if cmp.l ${ramdisk_force_addr} ${ramdisk_force_ref_addr} 1; then " \
            "setenv bootmode ramdisk; " \
            "echo Entering forced ramdisk boot...; " \
        "fi; " \
        "run finduuid; " \
        "run findfdt; " \
        "if mmc rescan; then " \
            "if run loadimage; then " \
                "run mmcboot; " \
            "else echo Unable to boot.; fi; " \
        "else echo Unable to boot.; fi; " \
    "else if test $bootmode = flash_and_reset; then " \
        "fuse prog -y 0 6 0x10; " \
        "mw.l 20D8040 0x3060; " \
        "mw.b 20D8047 0x10; " \
        "mw.l ${ramdisk_force_addr} 1 1; " \
        "mmc write ${fdt_addr_r} 0 0xA800; " \
        "echo possible POR incoming...; " \
        "gpio clear 52; " \
        "sleep 1; " \
        "gpio input 52; " \
        "sleep 2; " \
        "echo resetting usom...; " \
        "reset; " \
    "else echo Unable to boot.; fi; " \
    "fi; "

#else
#define CONFIG_EXTRA_ENV_SETTINGS
#endif /* CONFIG_SPL_BUILD */


/* Physical Memory Map */
#define CONFIG_NR_DRAM_BANKS           1
#define CONFIG_SYS_SDRAM_BASE          MMDC0_ARB_BASE_ADDR
#define CONFIG_SYS_INIT_RAM_ADDR       IRAM_BASE_ADDR
#define CONFIG_SYS_INIT_RAM_SIZE       IRAM_SIZE

#define CONFIG_SYS_INIT_SP_OFFSET \
    (CONFIG_SYS_INIT_RAM_SIZE - GENERATED_GBL_DATA_SIZE)
#define CONFIG_SYS_INIT_SP_ADDR \
    (CONFIG_SYS_INIT_RAM_ADDR + CONFIG_SYS_INIT_SP_OFFSET)

/* Environment organization */
#define CONFIG_ENV_SIZE            (8 * 1024)

#endif                         /* __MX6HOMEY_CONFIG_H */
