// SPDX-License-Identifier: (GPL-2.0)
/*
 * oxnas SoC reset driver
 * based on:
 * Microsemi MIPS SoC reset driver
 * and ox820_assert_system_reset() written by Ma Hajun <mahaijuns@gmail.com>
 *
 * License: GPL
 * Copyright (c) 2013 Ma Hajun <mahaijuns@gmail.com>
 * Copyright (c) 2017 Microsemi Corporation
 * Copyright (c) 2019 Daniel Golle <daniel@makrotopia.org>
 */
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/notifier.h>
#include <linux/mfd/syscon.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/regmap.h>
#include <dt-bindings/reset/oxsemi,ox820.h>

/* bit numbers of clock control register */
#define SYS_CTRL_CLK_COPRO              0
#define SYS_CTRL_CLK_DMA                1
#define SYS_CTRL_CLK_CIPHER             2
#define SYS_CTRL_CLK_SD                 3
#define SYS_CTRL_CLK_SATA               4
#define SYS_CTRL_CLK_I2S                5
#define SYS_CTRL_CLK_USBHS              6
#define SYS_CTRL_CLK_MACA               7
#define SYS_CTRL_CLK_MAC                SYS_CTRL_CLK_MACA
#define SYS_CTRL_CLK_PCIEA              8
#define SYS_CTRL_CLK_STATIC             9
#define SYS_CTRL_CLK_MACB               10
#define SYS_CTRL_CLK_PCIEB              11
#define SYS_CTRL_CLK_REF600             12
#define SYS_CTRL_CLK_USBDEV             13
#define SYS_CTRL_CLK_DDR                14
#define SYS_CTRL_CLK_DDRPHY             15
#define SYS_CTRL_CLK_DDRCK              16

/* Regmap offsets */
#define CLK_SET_REGOFFSET               0x2c
#define CLK_CLR_REGOFFSET               0x30
#define RST_SET_REGOFFSET               0x34
#define RST_CLR_REGOFFSET               0x38
#define SECONDARY_SEL_REGOFFSET         0x14
#define TERTIARY_SEL_REGOFFSET          0x8c
#define QUATERNARY_SEL_REGOFFSET        0x94
#define DEBUG_SEL_REGOFFSET             0x9c
#define ALTERNATIVE_SEL_REGOFFSET       0xa4
#define PULLUP_SEL_REGOFFSET            0xac
#define SEC_SECONDARY_SEL_REGOFFSET     0x100014
#define SEC_TERTIARY_SEL_REGOFFSET      0x10008c
#define SEC_QUATERNARY_SEL_REGOFFSET    0x100094
#define SEC_DEBUG_SEL_REGOFFSET         0x10009c
#define SEC_ALTERNATIVE_SEL_REGOFFSET   0x1000a4
#define SEC_PULLUP_SEL_REGOFFSET        0x1000ac


struct oxnas_restart_context {
	struct regmap *sys_ctrl;
	struct notifier_block restart_handler;
};

static int oxnas_restart_handle(struct notifier_block *this,
				 unsigned long mode, void *cmd)
{
	struct oxnas_restart_context *ctx = container_of(this, struct
							oxnas_restart_context,
							restart_handler);
	u32 value;

	/* Assert reset to cores as per power on defaults
	 * Don't touch the DDR interface as things will come to an impromptu stop
	 * NB Possibly should be asserting reset for PLLB, but there are timing
	 *    concerns here according to the docs */
	value = BIT(RESET_LEON)		|
		BIT(RESET_USBHS)	|
		BIT(RESET_USBPHYA)	|
		BIT(RESET_MAC)		|
		BIT(RESET_PCIEA)	|
		BIT(RESET_SGDMA)	|
		BIT(RESET_CIPHER)	|
		BIT(RESET_SATA)		|
		BIT(RESET_SATA_LINK)	|
		BIT(RESET_SATA_PHY)	|
		BIT(RESET_PCIEPHY)	|
		BIT(RESET_NAND)		|
		BIT(RESET_UART1)	|
		BIT(RESET_UART2)	|
		BIT(RESET_MISC)		|
		BIT(RESET_I2S)		|
		BIT(RESET_SD)		|
		BIT(RESET_MAC_2)	|
		BIT(RESET_PCIEB)	|
		BIT(RESET_VIDEO)	|
		BIT(RESET_USBPHYB)	|
		BIT(RESET_USBDEV);

	regmap_write(ctx->sys_ctrl, RST_SET_REGOFFSET, value);

	/* Release reset to cores as per power on defaults */
	regmap_write(ctx->sys_ctrl, RST_CLR_REGOFFSET, BIT(RESET_GPIO));

	/* Disable clocks to cores as per power-on defaults - must leave DDR
	 * related clocks enabled otherwise we'll stop rather abruptly. */
	value =
		BIT(SYS_CTRL_CLK_COPRO)		|
		BIT(SYS_CTRL_CLK_DMA)		|
		BIT(SYS_CTRL_CLK_CIPHER)	|
		BIT(SYS_CTRL_CLK_SD)		|
		BIT(SYS_CTRL_CLK_SATA)		|
		BIT(SYS_CTRL_CLK_I2S)		|
		BIT(SYS_CTRL_CLK_USBHS)		|
		BIT(SYS_CTRL_CLK_MAC)		|
		BIT(SYS_CTRL_CLK_PCIEA)		|
		BIT(SYS_CTRL_CLK_STATIC)	|
		BIT(SYS_CTRL_CLK_MACB)		|
		BIT(SYS_CTRL_CLK_PCIEB)		|
		BIT(SYS_CTRL_CLK_REF600)	|
		BIT(SYS_CTRL_CLK_USBDEV);

	regmap_write(ctx->sys_ctrl, CLK_CLR_REGOFFSET, value);

	/* Enable clocks to cores as per power-on defaults */

	/* Set sys-control pin mux'ing as per power-on defaults */
	regmap_write(ctx->sys_ctrl, SECONDARY_SEL_REGOFFSET, 0);
	regmap_write(ctx->sys_ctrl, TERTIARY_SEL_REGOFFSET, 0);
	regmap_write(ctx->sys_ctrl, QUATERNARY_SEL_REGOFFSET, 0);
	regmap_write(ctx->sys_ctrl, DEBUG_SEL_REGOFFSET, 0);
	regmap_write(ctx->sys_ctrl, ALTERNATIVE_SEL_REGOFFSET, 0);
	regmap_write(ctx->sys_ctrl, PULLUP_SEL_REGOFFSET, 0);

	regmap_write(ctx->sys_ctrl, SEC_SECONDARY_SEL_REGOFFSET, 0);
	regmap_write(ctx->sys_ctrl, SEC_TERTIARY_SEL_REGOFFSET, 0);
	regmap_write(ctx->sys_ctrl, SEC_QUATERNARY_SEL_REGOFFSET, 0);
	regmap_write(ctx->sys_ctrl, SEC_DEBUG_SEL_REGOFFSET, 0);
	regmap_write(ctx->sys_ctrl, SEC_ALTERNATIVE_SEL_REGOFFSET, 0);
	regmap_write(ctx->sys_ctrl, SEC_PULLUP_SEL_REGOFFSET, 0);

	/* No need to save any state, as the ROM loader can determine whether
	 * reset is due to power cycling or programatic action, just hit the
	 * (self-clearing) CPU reset bit of the block reset register */
	value =
		BIT(RESET_SCU) |
		BIT(RESET_ARM0) |
		BIT(RESET_ARM1);

	regmap_write(ctx->sys_ctrl, RST_SET_REGOFFSET, value);

	pr_emerg("Unable to restart system\n");
	return NOTIFY_DONE;
}

static int oxnas_restart_probe(struct platform_device *pdev)
{
	struct oxnas_restart_context *ctx;
	struct regmap *sys_ctrl;
	struct device *dev = &pdev->dev;
	int err = 0;

	sys_ctrl = syscon_regmap_lookup_by_compatible("oxsemi,ox820-sys-ctrl");
	if (IS_ERR(sys_ctrl))
		return PTR_ERR(sys_ctrl);

	ctx = devm_kzalloc(&pdev->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->sys_ctrl = sys_ctrl;
	ctx->restart_handler.notifier_call = oxnas_restart_handle;
	ctx->restart_handler.priority = 192;
	err = register_restart_handler(&ctx->restart_handler);
	if (err)
		dev_err(dev, "can't register restart notifier (err=%d)\n", err);

	return err;
}

static const struct of_device_id oxnas_restart_of_match[] = {
	{ .compatible = "oxsemi,ox820-sys-ctrl" },
	{}
};

static struct platform_driver oxnas_restart_driver = {
	.probe = oxnas_restart_probe,
	.driver = {
		.name = "oxnas-chip-reset",
		.of_match_table = oxnas_restart_of_match,
	},
};
builtin_platform_driver(oxnas_restart_driver);
