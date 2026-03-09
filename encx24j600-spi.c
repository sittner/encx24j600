/**
 * Microchip ENCX24J600 ethernet driver
 *
 * Copyright (C) 2015 Gridpoint
 * Author: Jon Ringle <jringle@gridpoint.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include "encx24j600.h"
#include "encx24j600_hw.h"

#include <linux/of.h>
#include <linux/spi/spi.h>

#define DRV_NAME	"encx24j600-spi"

static const u8 cmd_ops[CMD_COUNT] = {
	SETETHRST,
	FCDISABLE,
	FCSINGLE,
	FCMULTIPLE,
	FCCLEAR,
	SETPKTDEC,
	DMASTOP,
	DMACKSUM,
	DMACKSUMS,
	DMACOPY,
	DMACOPYS,
	SETTXRTS,
	ENABLERX,
	DISABLERX,
	SETEIE,
	CLREIE
};

struct ptr_op {
	u8 reg;
	u8 wop;
	u8 rop;
};

static const struct ptr_op ptr_ops[] = {
	{ .reg = EGPRDPT, .wop = WGPRDPT, .rop = RGPRDPT },
	{ .reg = ERXRDPT, .wop = WRXRDPT, .rop = RRXRDPT },
	{ .reg = EUDARDPT, .wop = WUDARDPT, .rop = RUDARDPT },
	{ .reg = EGPWRPT, .wop = WGPWRPT, .rop = RGPWRPT },
	{ .reg = ERXWRPT, .wop = WRXWRPT, .rop = RRXWRPT },
	{ .reg = EUDAWRPT, .wop = WUDAWRPT, .rop = RUDAWRPT },
	{ .reg = 0 }
};

struct memwin_op {
	u8 reg;
	u8 wop;
	u8 rop;
};

static const struct memwin_op memwin_ops[WIN_COUNT] = {
	{ .wop = WUDADATA, .rop = RUDADATA },
	{ .wop = WGPDATA, .rop = RGPDATA },
	{ .wop = WRXDATA, .rop = RRXDATA }
};

struct encx24j600_spi_ctx {
	struct encx24j600_priv priv;

	struct spi_device *spi;
	struct mutex lock;
	int bank;
};

static u16 spi_xfer_nbyte(struct encx24j600_spi_ctx *ctx, u8 reg, u8 op_banked, u8 op_unbanked, u16 tx)
{
	u8 op[2];
	u16 rx;
	struct spi_transfer xfers[] = {
		{ .tx_buf = &op, .len = 0, },
		{ .tx_buf = &tx, .rx_buf = &rx, .len = 2 },
	};

	/* check if banked transfer is needed */
	if (reg < 0x80) {
		u8 reg_banked = reg & BANK_ADDR_MASK;
		/* check for need of bank switch */
		if (reg_banked < 0x16) {
			u8 bank = (reg & BANK_MASK) >> BANK_SHIFT;
			if (ctx->bank != bank) {
				u8 bank_op = B0SEL | (bank << 1);
				if (unlikely(spi_write(ctx->spi, &bank_op, 1))) {
					return 0;
				}
				ctx->bank = bank;
			}
		}

		/* setup banked op */
		op[0] = op_banked | reg_banked;
		xfers[0].len = 1;
	} else {
		/* setup unbanked op */
		op[0] = op_unbanked;
		op[1] = reg;
		xfers[0].len = 2;
	}

	if (unlikely(spi_sync_transfer(ctx->spi, xfers, 2))) {
		return 0;
	}

	return rx;
}

static u16 encx24j600_spi_read_reg(struct encx24j600_priv *priv, u8 reg)
{
	struct encx24j600_spi_ctx *ctx = container_of(priv, struct encx24j600_spi_ctx, priv);
	u16 val = 0;
	const struct ptr_op *ptr_op;

	/* check for three byte operation */
	for (ptr_op = ptr_ops; ptr_op->reg != 0; ptr_op++) {
		if (ptr_op->reg == reg) {
			struct spi_transfer xfers[] = {
				{ .tx_buf = &ptr_op->rop, .len = 1, },
				{ .rx_buf = &val, .len = 2 },
			};

			mutex_lock(&ctx->lock);
			spi_sync_transfer(ctx->spi, xfers, 2);
			mutex_unlock(&ctx->lock);
			return le16_to_cpu(val);
		}
	}

	mutex_lock(&ctx->lock);
	val = spi_xfer_nbyte(ctx, reg, RCR, RCRU, 0);
	mutex_unlock(&ctx->lock);
	return le16_to_cpu(val);
}

static void encx24j600_spi_write_reg(struct encx24j600_priv *priv, u8 reg, u16 val)
{
	struct encx24j600_spi_ctx *ctx = container_of(priv, struct encx24j600_spi_ctx, priv);
	const struct ptr_op *ptr_op;

	cpu_to_le16s(&val);

	/* check for three byte operation */
	for (ptr_op = ptr_ops; ptr_op->reg != 0; ptr_op++) {
		if (ptr_op->reg == reg) {
			struct spi_transfer xfers[] = {
				{ .tx_buf = &ptr_op->wop, .len = 1, },
				{ .tx_buf = &val, .len = 2 },
			};

			mutex_lock(&ctx->lock);
			spi_sync_transfer(ctx->spi, xfers, 2);
			mutex_unlock(&ctx->lock);
			return;
		}
	}

	mutex_lock(&ctx->lock);
	spi_xfer_nbyte(ctx, reg, WCR, WCRU, val);
	mutex_unlock(&ctx->lock);
}

static void encx24j600_spi_clr_bits(struct encx24j600_priv *priv, u8 reg, u16 mask)
{
	struct encx24j600_spi_ctx *ctx = container_of(priv, struct encx24j600_spi_ctx, priv);

	mutex_lock(&ctx->lock);
	spi_xfer_nbyte(ctx, reg, BFC, BFCU, cpu_to_le16(mask));
	mutex_unlock(&ctx->lock);
}

static void encx24j600_spi_set_bits(struct encx24j600_priv *priv, u8 reg, u16 mask)
{
	struct encx24j600_spi_ctx *ctx = container_of(priv, struct encx24j600_spi_ctx, priv);

	mutex_lock(&ctx->lock);
	spi_xfer_nbyte(ctx, reg, BFS, BFSU, cpu_to_le16(mask));
	mutex_unlock(&ctx->lock);
}

static void encx24j600_spi_cmd(struct encx24j600_priv *priv, enum encx24j600_byte_cmd cmd)
{
	struct encx24j600_spi_ctx *ctx = container_of(priv, struct encx24j600_spi_ctx, priv);
	u8 op = cmd_ops[cmd];

	mutex_lock(&ctx->lock);
	spi_write(ctx->spi, &op, 1);
	mutex_unlock(&ctx->lock);
}

static void encx24j600_spi_read_mem(struct encx24j600_priv *priv, enum encx24j600_memwin win, u8 *data, size_t count)
{
	struct encx24j600_spi_ctx *ctx = container_of(priv, struct encx24j600_spi_ctx, priv);
	u8 op = memwin_ops[win].rop;

	struct spi_transfer xfers[] = {
		{ .tx_buf = &op, .len = 1, },
		{ .rx_buf = data, .len = count },
	};

	mutex_lock(&ctx->lock);
	spi_sync_transfer(ctx->spi, xfers, 2);
	mutex_unlock(&ctx->lock);
}

static void encx24j600_spi_write_mem(struct encx24j600_priv *priv, enum encx24j600_memwin win, const u8 *data, size_t count)
{
	struct encx24j600_spi_ctx *ctx = container_of(priv, struct encx24j600_spi_ctx, priv);
	u8 op = memwin_ops[win].wop;

	struct spi_transfer xfers[] = {
		{ .tx_buf = &op, .len = 1, },
		{ .tx_buf = data, .len = count },
	};

	mutex_lock(&ctx->lock);
	spi_sync_transfer(ctx->spi, xfers, 2);
	mutex_unlock(&ctx->lock);
}

static int encx24j600_spi_probe(struct spi_device *spi)
{
	struct net_device *ndev;
	struct encx24j600_spi_ctx *ctx;
	int ret;

	ndev = alloc_etherdev(sizeof(struct encx24j600_spi_ctx));
	if (!ndev) {
		return -ENOMEM;
	}

	ctx = netdev_priv(ndev);
	ctx->priv.ndev = ndev;

	spi_set_drvdata(spi, ctx);
	ctx->spi = spi;

	SET_NETDEV_DEV(ndev, &spi->dev);
	ndev->irq = spi->irq;

	mutex_init(&ctx->lock);

	ctx->priv.read_reg = encx24j600_spi_read_reg;
	ctx->priv.write_reg = encx24j600_spi_write_reg;
	ctx->priv.clr_bits = encx24j600_spi_clr_bits;
	ctx->priv.set_bits = encx24j600_spi_set_bits;
	ctx->priv.cmd = encx24j600_spi_cmd;
	ctx->priv.read_mem = encx24j600_spi_read_mem;
	ctx->priv.write_mem = encx24j600_spi_write_mem;

	ret = encx24j600_probe(&ctx->priv);
	if (ret) {
		free_netdev(ndev);
		return ret;
	}

	return 0;
}

static void encx24j600_spi_remove(struct spi_device *spi)
{
	struct encx24j600_spi_ctx *ctx = dev_get_drvdata(&spi->dev);

	encx24j600_remove(&ctx->priv);
}

static const struct spi_device_id encx24j600_spi_id_table[] = {
	{ .name = "encx24j600" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(spi, encx24j600_spi_id_table);

static const struct of_device_id encx24j600_spi_of_match[] = {
	{ .compatible = "microchip,encx24j600" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, encx24j600_spi_of_match);

static struct spi_driver encx24j600_spi_net_driver = {
	.driver = {
		.name		= DRV_NAME,
		.of_match_table	= encx24j600_spi_of_match,
	},
	.probe		= encx24j600_spi_probe,
	.remove		= encx24j600_spi_remove,
	.id_table	= encx24j600_spi_id_table,
};
module_spi_driver(encx24j600_spi_net_driver);

MODULE_DESCRIPTION(DRV_NAME " ethernet driver");
MODULE_AUTHOR("Jon Ringle <jringle@gridpoint.com>");
MODULE_LICENSE("GPL");

