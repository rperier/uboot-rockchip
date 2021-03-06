/*
 * (C) Copyright 2015 Sjoerd Simons <sjoerd.simons@collabora.co.uk>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/*
 * Rockchip GMAC ethernet IP driver for U-Boot
 */
#include <common.h>
#include <dm.h>
#include <dm/pinctrl.h>
#include <asm/gpio.h>
#include <clk.h>
#include <phy.h>
#include <syscon.h>
#include <asm/io.h>
#include <asm/arch/periph.h>
#include <asm/arch/clock.h>
#include <asm/arch/grf_rk3288.h>
#include "designware.h"

DECLARE_GLOBAL_DATA_PTR;

struct gmac_rk3288_platdata {
	struct dw_eth_pdata dw_eth_pdata;
	int tx_delay;
	int rx_delay;
};

static int gmac_rk3288_ofdata_to_platdata(struct udevice *dev)
{
	struct gmac_rk3288_platdata *pdata = dev_get_platdata(dev);

	pdata->tx_delay = fdtdec_get_int (gd->fdt_blob, dev->of_offset,
		"tx_delay", 0x30);
	pdata->rx_delay = fdtdec_get_int (gd->fdt_blob, dev->of_offset,
		"rx_delay", 0x10);

	return designware_eth_ofdata_to_platdata(dev);
}

static int gmac_rk3288_started(struct udevice *dev)
{
	struct rk3288_grf *grf;
	struct dw_eth_dev *priv = dev_get_priv(dev);
	int clk;

	switch (priv->phydev->speed) {
	case 10:
		clk = GMAC_CLK_SEL_2_5M;
		break;
	case 100:
		clk = GMAC_CLK_SEL_25M;
		break;
	case 1000:
		clk = GMAC_CLK_SEL_125M;
		break;
	default:
		printf("Unknown phy speed: %d\n", priv->phydev->speed);
		return -EINVAL;
	}

	grf = syscon_get_first_range(ROCKCHIP_SYSCON_GRF);

	rk_clrsetreg(&grf->soc_con1,
		     GMAC_CLK_SEL_MASK << GMAC_CLK_SEL_SHIFT,
		     clk << GMAC_CLK_SEL_SHIFT);

	return 0;
}

static int gmac_rk3288_probe(struct udevice *dev)
{
	int ret;
	struct gmac_rk3288_platdata *pdata = dev_get_platdata(dev);
	struct dw_eth_dev *priv = dev_get_priv(dev);
	enum periph_id periph_id;
	struct udevice *pinctrl, *clk;
	struct rk3288_grf *grf;

	ret = uclass_get_device(UCLASS_PINCTRL, 0, &pinctrl);
	if (ret)
		return ret;

	ret  = pinctrl_get_periph_id(pinctrl, dev);
	if (ret < 0)
		return ret;

	periph_id = ret;
	ret = pinctrl_request(pinctrl, periph_id, 0);
	if (ret)
		return ret;

	ret = uclass_get_device(UCLASS_CLK, CLK_GENERAL, &clk);
	if (ret)
		return ret;

	ret = clk_set_periph_rate(clk, periph_id, 0);
	if (ret)
		return ret;

	/* Set to RGMII mode */
	grf = syscon_get_first_range(ROCKCHIP_SYSCON_GRF);
	rk_clrsetreg(&grf->soc_con1,
		     RMII_MODE_MASK << RMII_MODE_SHIFT |
		     GMAC_PHY_INTF_SEL_MASK << GMAC_PHY_INTF_SEL_SHIFT,
		     GMAC_PHY_INTF_SEL_RGMII << GMAC_PHY_INTF_SEL_SHIFT);

	rk_clrsetreg(&grf->soc_con3,
		     RXCLK_DLY_ENA_GMAC_MASK <<  RXCLK_DLY_ENA_GMAC_SHIFT |
		     TXCLK_DLY_ENA_GMAC_MASK <<  TXCLK_DLY_ENA_GMAC_SHIFT |
		     CLK_RX_DL_CFG_GMAC_MASK <<  CLK_RX_DL_CFG_GMAC_SHIFT |
		     CLK_TX_DL_CFG_GMAC_MASK <<  CLK_TX_DL_CFG_GMAC_SHIFT,
		     RXCLK_DLY_ENA_GMAC_ENABLE << RXCLK_DLY_ENA_GMAC_SHIFT |
		     TXCLK_DLY_ENA_GMAC_ENABLE << TXCLK_DLY_ENA_GMAC_SHIFT |
		     pdata->rx_delay << CLK_RX_DL_CFG_GMAC_SHIFT |
		     pdata->tx_delay << CLK_TX_DL_CFG_GMAC_SHIFT);

	priv->started = gmac_rk3288_started;

	return designware_eth_probe(dev);
}

static const struct udevice_id rk3288_gmac_ids[] = {
	{ .compatible = "rockchip,rk3288-gmac" },
	{ }
};

U_BOOT_DRIVER(eth_gmac_rk3288) = {
	.name	= "gmac_rk3288",
	.id	= UCLASS_ETH,
	.of_match = rk3288_gmac_ids,
	.ofdata_to_platdata = gmac_rk3288_ofdata_to_platdata,
	.probe	= gmac_rk3288_probe,
	.ops	= &designware_eth_ops,
	.priv_auto_alloc_size = sizeof(struct dw_eth_dev),
	.platdata_auto_alloc_size = sizeof(struct gmac_rk3288_platdata),
.flags = DM_FLAG_ALLOC_PRIV_DMA,
};
