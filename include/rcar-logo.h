/*
 * rcar-logo.h  --  R-Car video support header
 *
 * Copyright (C) 2020 GlobalLogic
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 */

#ifndef _RCAR_LOGO_H_
#define _RCAR_LOGO_H_

/**
 * do_logo_start - Enable an FCP
 * @img_index: Index of logo stored in the MMC partition
 *
 * Function loads logo from MMC into RAM corresponding address.
 * Function provides initialization steps for R-Car DU, VSP and DW HDMI
 * modules. Starts all corresponding clocks.
 *
 * Return 0 on success or a negative error if an error occurs.
 */
int do_logo_start(int img_index);

/**
 * do_logo_stop - Stop displaying logo
 *
 * Function provides de-initialization steps for R-Car DU, VSP and DW HDMI
 * modules. Stops all corresponding clocks.
 */
void do_logo_stop(void);

/* Clock public API */
int rcar_clks_enable(void);
void rcar_clks_disable(void);

/* HDMI public API */
int dw_hdmi_probe(void);
int dw_hdmi_connector_detect(void);
void dw_hdmi_bridge_enable(void);
void dw_hdmi_bridge_disable(void);
void dw_hdmi_remove(void);

/* DU public API */
int rcar_du_probe(void);
int rcar_du_start(void);
void rcar_du_stop(void);

/* FCP public API */
int rcar_fcp_reset(void);

/* VSP public API */
int vsp1_probe(void);
void vsp1_remove(void);

#endif /* _RCAR_LOGO_H_ */
