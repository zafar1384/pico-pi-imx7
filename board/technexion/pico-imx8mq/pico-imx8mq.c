/*
 * Copyright 2019 TechNexion Ltd.
 *
 * Author: Richard Hu <richard.hu@technexion.com>
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <malloc.h>
#include <errno.h>
#include <asm/io.h>
#include <miiphy.h>
#include <netdev.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm-generic/gpio.h>
#include <fsl_esdhc.h>
#include <mmc.h>
#include <asm/arch/imx8mq_pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/mach-imx/gpio.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <asm/arch/clock.h>
#include <asm/mach-imx/video.h>
#include <asm/arch/video_common.h>
#include <spl.h>
#include <usb.h>
#include <dwc3-uboot.h>
#include <dm/uclass.h>
#include <i2c.h>

DECLARE_GLOBAL_DATA_PTR;

#define QSPI_PAD_CTRL	(PAD_CTL_DSE2 | PAD_CTL_HYS)

#define UART_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_FSEL1)

#define WDOG_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_HYS | PAD_CTL_PUE)

#define FEC_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_ODE)

static iomux_v3_cfg_t const wdog_pads[] = {
	IMX8MQ_PAD_GPIO1_IO02__WDOG1_WDOG_B | MUX_PAD_CTRL(WDOG_PAD_CTRL),
};

static iomux_v3_cfg_t const uart_pads[] = {
	IMX8MQ_PAD_UART1_RXD__UART1_RX | MUX_PAD_CTRL(UART_PAD_CTRL),
	IMX8MQ_PAD_UART1_TXD__UART1_TX | MUX_PAD_CTRL(UART_PAD_CTRL),
};

int board_early_init_f(void)
{
	struct wdog_regs *wdog = (struct wdog_regs *)WDOG1_BASE_ADDR;

	imx_iomux_v3_setup_multiple_pads(wdog_pads, ARRAY_SIZE(wdog_pads));

	set_wdog_reset(wdog);

	imx_iomux_v3_setup_multiple_pads(uart_pads, ARRAY_SIZE(uart_pads));

	return 0;
}

#ifdef CONFIG_BOARD_POSTCLK_INIT
int board_postclk_init(void)
{
	/* TODO */
	return 0;
}
#endif

static int ddr_size;

int dram_init(void)
{
	/*************************************************
	ToDo: It's a dirty workaround to store the
	information of DDR size into start address of TCM.
	It'd be better to detect DDR size from DDR controller.
	**************************************************/
	ddr_size = readl(M4_BOOTROM_BASE_ADDR);

	if (ddr_size == 0x4) {
		/* rom_pointer[1] contains the size of TEE occupies */
		if (rom_pointer[1])
			gd->ram_size = PHYS_SDRAM_SIZE_4GB - rom_pointer[1];
		else
			gd->ram_size = PHYS_SDRAM_SIZE_4GB;
	}
	else if (ddr_size == 0x3) {
		if (rom_pointer[1])
			gd->ram_size = PHYS_SDRAM_SIZE_3GB - rom_pointer[1];
		else
			gd->ram_size = PHYS_SDRAM_SIZE_3GB;
	}
	else if (ddr_size == 0x2) {
		if (rom_pointer[1])
			gd->ram_size = PHYS_SDRAM_SIZE_2GB - rom_pointer[1];
		else
			gd->ram_size = PHYS_SDRAM_SIZE_2GB;
	}
	else if (ddr_size == 0x1) {
		if (rom_pointer[1])
			gd->ram_size = PHYS_SDRAM_SIZE_1GB - rom_pointer[1];
		else
			gd->ram_size = PHYS_SDRAM_SIZE_1GB;
	}
	else
		puts("Unknown DDR type!!!\n");
	return 0;
}

/* Get the top of usable RAM */
ulong board_get_usable_ram_top(ulong total_size)
{
	if(gd->ram_top > 0x100000000)
		gd->ram_top = 0x100000000;

	return gd->ram_top;
}

#ifdef CONFIG_OF_BOARD_SETUP
int ft_board_setup(void *blob, bd_t *bd)
{
	return 0;
}
#endif

#ifdef CONFIG_FEC_MXC
#define FEC_RST_PAD IMX_GPIO_NR(1, 9)
static iomux_v3_cfg_t const fec1_rst_pads[] = {
	IMX8MQ_PAD_GPIO1_IO09__GPIO1_IO9 | MUX_PAD_CTRL(FEC_PAD_CTRL),
};

static void setup_iomux_fec(void)
{
	imx_iomux_v3_setup_multiple_pads(fec1_rst_pads, ARRAY_SIZE(fec1_rst_pads));

	gpio_request(FEC_RST_PAD, "fec1_rst");
	gpio_direction_output(FEC_RST_PAD, 0);
	mdelay(35);
	gpio_direction_output(FEC_RST_PAD, 1);
	mdelay(75);
}

static int setup_fec(void)
{
	struct iomuxc_gpr_base_regs *const iomuxc_gpr_regs
		= (struct iomuxc_gpr_base_regs *) IOMUXC_GPR_BASE_ADDR;

	setup_iomux_fec();

	/* Use 125M anatop REF_CLK1 for ENET1, not from external */
	clrsetbits_le32(&iomuxc_gpr_regs->gpr[1],
			IOMUXC_GPR_GPR1_GPR_ENET1_TX_CLK_SEL_SHIFT, 0);
	return set_clk_enet(ENET_125MHZ);
}

int board_phy_config(struct phy_device *phydev)
{
#ifndef CONFIG_DM_ETH
	/* enable rgmii rxc skew and phy mode select to RGMII copper */
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x1f);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0x8);

	phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x00);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0x82ee);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x05);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0x100);
#endif
	if (phydev->drv->config)
		phydev->drv->config(phydev);
	return 0;
}
#endif

#ifdef CONFIG_USB_DWC3

#define USB_PHY_CTRL0			0xF0040
#define USB_PHY_CTRL0_REF_SSP_EN	BIT(2)

#define USB_PHY_CTRL1			0xF0044
#define USB_PHY_CTRL1_RESET		BIT(0)
#define USB_PHY_CTRL1_COMMONONN		BIT(1)
#define USB_PHY_CTRL1_ATERESET		BIT(3)
#define USB_PHY_CTRL1_VDATSRCENB0	BIT(19)
#define USB_PHY_CTRL1_VDATDETENB0	BIT(20)

#define USB_PHY_CTRL2			0xF0048
#define USB_PHY_CTRL2_TXENABLEN0	BIT(8)

static struct dwc3_device dwc3_device_data = {
	.maximum_speed = USB_SPEED_HIGH,
	.base = USB1_BASE_ADDR,
	.dr_mode = USB_DR_MODE_PERIPHERAL,
	.index = 0,
	.power_down_scale = 2,
};

int usb_gadget_handle_interrupts(void)
{
	dwc3_uboot_handle_interrupt(0);
	return 0;
}

static void dwc3_nxp_usb_phy_init(struct dwc3_device *dwc3)
{
	u32 RegData;

	RegData = readl(dwc3->base + USB_PHY_CTRL1);
	RegData &= ~(USB_PHY_CTRL1_VDATSRCENB0 | USB_PHY_CTRL1_VDATDETENB0 |
			USB_PHY_CTRL1_COMMONONN);
	RegData |= USB_PHY_CTRL1_RESET | USB_PHY_CTRL1_ATERESET;
	writel(RegData, dwc3->base + USB_PHY_CTRL1);

	RegData = readl(dwc3->base + USB_PHY_CTRL0);
	RegData |= USB_PHY_CTRL0_REF_SSP_EN;
	writel(RegData, dwc3->base + USB_PHY_CTRL0);

	RegData = readl(dwc3->base + USB_PHY_CTRL2);
	RegData |= USB_PHY_CTRL2_TXENABLEN0;
	writel(RegData, dwc3->base + USB_PHY_CTRL2);

	RegData = readl(dwc3->base + USB_PHY_CTRL1);
	RegData &= ~(USB_PHY_CTRL1_RESET | USB_PHY_CTRL1_ATERESET);
	writel(RegData, dwc3->base + USB_PHY_CTRL1);
}
#endif

#if defined(CONFIG_USB_DWC3) || defined(CONFIG_USB_XHCI_IMX8M)
int board_usb_init(int index, enum usb_init_type init)
{
	int ret = 0;
	imx8m_usb_power(index, true);

	if (index == 0 && init == USB_INIT_DEVICE) {
		dwc3_nxp_usb_phy_init(&dwc3_device_data);
		return dwc3_uboot_init(&dwc3_device_data);
	} else if (index == 0 && init == USB_INIT_HOST) {
		return ret;
	}

	return 0;
}

int board_usb_cleanup(int index, enum usb_init_type init)
{
	int ret = 0;
	if (index == 0 && init == USB_INIT_DEVICE)
			dwc3_uboot_exit(index);

	imx8m_usb_power(index, false);

	return ret;
}
#endif

#define WL_REG_ON_PAD IMX_GPIO_NR(3, 24)
static iomux_v3_cfg_t const wl_reg_on_pads[] = {
	IMX8MQ_PAD_SAI5_RXD3__GPIO3_IO24 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

#define BT_ON_PAD IMX_GPIO_NR(3, 21)
static iomux_v3_cfg_t const bt_on_pads[] = {
	IMX8MQ_PAD_SAI5_RXD0__GPIO3_IO21 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

void setup_wifi(void)
{
	imx_iomux_v3_setup_multiple_pads(wl_reg_on_pads, ARRAY_SIZE(wl_reg_on_pads));
	imx_iomux_v3_setup_multiple_pads(bt_on_pads, ARRAY_SIZE(bt_on_pads));

	gpio_request(WL_REG_ON_PAD, "wl_reg_on");
	gpio_direction_output(WL_REG_ON_PAD, 0);
	gpio_set_value(WL_REG_ON_PAD, 0);

	gpio_request(BT_ON_PAD, "bt_on");
	gpio_direction_output(BT_ON_PAD, 0);
	gpio_set_value(BT_ON_PAD, 0);
}

int board_init(void)
{
	setup_wifi();

#ifdef CONFIG_FEC_MXC
	setup_fec();
#endif

#if defined(CONFIG_USB_DWC3) || defined(CONFIG_USB_XHCI_IMX8M)
	init_usb_clk();
#endif

	return 0;
}

int board_mmc_get_env_dev(int devno)
{
	return devno;
}

static int check_mmc_autodetect(void)
{
	char *autodetect_str = env_get("mmcautodetect");

	if ((autodetect_str != NULL) &&
		(strcmp(autodetect_str, "yes") == 0)) {
		return 1;
	}

	return 0;
}

/* This should be defined for each board */
__weak int mmc_map_to_kernel_blk(int dev_no)
{
	return dev_no;
}

void board_late_mmc_env_init(void)
{
	char cmd[32];
	char mmcblk[32];
	u32 dev_no = mmc_get_env_dev();

	if (!check_mmc_autodetect())
		return;

	env_set_ulong("mmcdev", dev_no);

	/* Set mmcblk env */
	sprintf(mmcblk, "/dev/mmcblk%dp2 rootwait rw",
		mmc_map_to_kernel_blk(dev_no));
	env_set("mmcroot", mmcblk);

	sprintf(cmd, "mmc dev %d", dev_no);
	run_command(cmd, 0);
}

#define EXPANSION_IC_I2C_BUS 2
#define EXPANSION_IC_I2C_ADDR 0x23
#define DISPLAY_NAME_HDMI        "HDMI"
#define DISPLAY_NAME_MIPI2HDMI   "MIPI2HDMI"
#define DISPLAY_NAME_MIPI5       "ILI9881C_LCD"
#define DISPLAY_NAME_MIPI8       "G080UAN01_LCD"
#define DISPLAY_NAME_MIPI10      "G101UAN02_LCD"
struct mipi_panel_id {
	const char *panel_name;
	int id;
	const char *suffix;
};

static const struct mipi_panel_id mipi_panel_mapping[] = {
	{DISPLAY_NAME_HDMI, 0, ""},
	{DISPLAY_NAME_MIPI2HDMI, 0, "-lcdif-adv7535"},
	{DISPLAY_NAME_MIPI5, 0x54, "-dcss-ili9881c"},
	{DISPLAY_NAME_MIPI8, 0x58, "-dcss-g080uan01"},
	{DISPLAY_NAME_MIPI10, 0x59, "-dcss-g101uan02"},
};

int board_late_init(void)
{
	struct udevice *bus;
	struct udevice *i2c_dev = NULL;
	char *fdt_file, str_fdtfile[64];
	char const *panel = env_get("panel");
	int ret, i;

#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
	env_set("board_name", "PICO-IMX8MQ");
	env_set("board_rev", "iMX8MQ");
#endif

	fdt_file = env_get("fdt_file");
	if (fdt_file && !strcmp(fdt_file, "undefined")) {
		ret = uclass_get_device_by_seq(UCLASS_I2C, EXPANSION_IC_I2C_BUS, &bus);
		if (ret) {
			printf("%s: Can't find bus\n", __func__);
			return -EINVAL;
		}

		ret = dm_i2c_probe(bus, EXPANSION_IC_I2C_ADDR, 0, &i2c_dev);
		if (ret)
			strcpy(str_fdtfile, "imx8mq-pico-pi");
		else
			strcpy(str_fdtfile, "imx8mq-pico-wizard");

		for (i = 0; i < display_count; i++) {
			struct display_info_t const *dev = displays+i;
			if ((!panel && dev->detect && dev->detect(dev)) || !strcmp(panel, dev->mode.name)) {
				strcat(str_fdtfile, mipi_panel_mapping[i].suffix);
				env_set("panel_name", mipi_panel_mapping[i].panel_name);
				break;
			}
		}
		strcat(str_fdtfile, ".dtb");
		env_set("fdt_file", str_fdtfile);
	}

#ifdef CONFIG_ENV_IS_IN_MMC
	board_late_mmc_env_init();
#endif

	return 0;
}

#ifdef CONFIG_FSL_FASTBOOT
#ifdef CONFIG_ANDROID_RECOVERY
int is_recovery_key_pressing(void)
{
	return 0; /*TODO*/
}
#endif /*CONFIG_ANDROID_RECOVERY*/
#endif /*CONFIG_FSL_FASTBOOT*/

#if defined(CONFIG_VIDEO_IMXDCSS)
#define MIPI_DSI_I2C_BUS 2
#define FT5336_TOUCH_I2C_ADDR 0x38
#define ADV7535_MAIN_I2C_ADDR 0x3d

static int detect_i2c(struct display_info_t const *dev)
{
	struct udevice *bus, *i2c_dev = NULL;
	int ret = 0, val, i;

	if ((0 == uclass_get_device_by_seq(UCLASS_I2C, MIPI_DSI_I2C_BUS, &bus)) &&
				(0 == dm_i2c_probe(bus, dev->addr, 0, &i2c_dev))) {
		if (dev->addr == FT5336_TOUCH_I2C_ADDR) {
			val = dm_i2c_reg_read(i2c_dev, 0xA3);
			for (i = 1; i < ARRAY_SIZE(mipi_panel_mapping); i++) {
				const struct mipi_panel_id *instr = &mipi_panel_mapping[i];
				if((strcmp(instr->panel_name, dev->mode.name) == 0) &&
						(instr->id == val)) {
					ret = 1;
					break;
				}
			}
		} else if (dev->addr == ADV7535_MAIN_I2C_ADDR) {
			ret = 1;
		} else {
			ret = 1;
		}
	}

	return ret;
}

struct display_info_t const displays[] = {{
	.bus	= 0, /* Unused */
	.addr	= 0, /* Unused */
	.pixfmt	= GDF_32BIT_X888RGB,
	.detect	= NULL,
	.enable	= NULL,
#ifndef CONFIG_VIDEO_IMXDCSS_1080P
	.mode	= {
		.name           = DISPLAY_NAME_HDMI, /* 720P60 */
		.refresh        = 60,
		.xres           = 1280,
		.yres           = 720,
		.pixclock       = 13468, /* 74250  kHz */
		.left_margin    = 110,
		.right_margin   = 220,
		.upper_margin   = 5,
		.lower_margin   = 20,
		.hsync_len      = 40,
		.vsync_len      = 5,
		.sync           = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		.vmode          = FB_VMODE_NONINTERLACED
	}
#else
	.mode	= {
		.name           = DISPLAY_NAME_HDMI, /* 1080P60 */
		.refresh        = 60,
		.xres           = 1920,
		.yres           = 1080,
		.pixclock       = 6734, /* 148500 kHz */
		.left_margin    = 148,
		.right_margin   = 88,
		.upper_margin   = 36,
		.lower_margin   = 4,
		.hsync_len      = 44,
		.vsync_len      = 5,
		.sync           = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		.vmode          = FB_VMODE_NONINTERLACED
	}
#endif
}, {
	.bus = 0,
	.addr = ADV7535_MAIN_I2C_ADDR,
	.pixfmt = 24,
	.detect = detect_i2c,
	.enable	= NULL,
	.mode	= {
		.name			= DISPLAY_NAME_MIPI2HDMI,
		.refresh		= 60,
		.xres			= 1920,
		.yres			= 1080,
		.pixclock		= 6734, /* 148500 kHz */
		.left_margin	= 148,
		.right_margin	= 88,
		.upper_margin	= 36,
		.lower_margin	= 4,
		.hsync_len		= 44,
		.vsync_len		= 5,
		.sync			= FB_SYNC_EXT,
		.vmode			= FB_VMODE_NONINTERLACED

} }, {
	.bus = 0,
	.addr = FT5336_TOUCH_I2C_ADDR,
	.pixfmt = 24,
	.detect = detect_i2c,
	.enable	= NULL,
	.mode	= {
		.name			= DISPLAY_NAME_MIPI5,
		.refresh		= 60,
		.xres			= 720,
		.yres			= 1280,
		.pixclock		= 16129, /* 62000  kHz */
		.left_margin	= 30,
		.right_margin	= 10,
		.upper_margin	= 20,
		.lower_margin	= 10,
		.hsync_len		= 20,
		.vsync_len		= 10,
		.sync			= FB_SYNC_EXT,
		.vmode			= FB_VMODE_NONINTERLACED

} }, {
	.bus = 0,
	.addr = FT5336_TOUCH_I2C_ADDR,
	.pixfmt = 24,
	.detect = detect_i2c,
	.enable	= NULL,
	.mode	= {
		.name			= DISPLAY_NAME_MIPI8,
		.refresh		= 60,
		.xres			= 1200,
		.yres			= 1920,
		.pixclock		= 6273, /* 956400  kHz */
		.left_margin	= 60,
		.right_margin	= 60,
		.upper_margin	= 25,
		.lower_margin	= 35,
		.hsync_len		= 2,
		.vsync_len		= 1,
		.sync			= FB_SYNC_EXT,
		.vmode			= FB_VMODE_NONINTERLACED

} }, {
	.bus = 0,
	.addr = FT5336_TOUCH_I2C_ADDR,
	.pixfmt = 24,
	.detect = detect_i2c,
	.enable	= NULL,
	.mode	= {
		.name			= DISPLAY_NAME_MIPI10,
		.refresh		= 60,
		.xres			= 1920,
		.yres			= 1200,
		.pixclock		= 6671, /* 899400  kHz */
		.left_margin	= 60,
		.right_margin	= 60,
		.upper_margin	= 5,
		.lower_margin	= 5,
		.hsync_len		= 18,
		.vsync_len		= 2,
		.sync			= FB_SYNC_EXT,
		.vmode			= FB_VMODE_NONINTERLACED
	}

} };
size_t display_count = ARRAY_SIZE(displays);

#endif /* CONFIG_VIDEO_IMXDCSS */

/* return hard code board id for imx8m_ref */
#if defined(CONFIG_ANDROID_THINGS_SUPPORT) && defined(CONFIG_ARCH_IMX8M)
int get_imx8m_baseboard_id(void)
{
	return IMX8M_REF_3G;
}
#endif
