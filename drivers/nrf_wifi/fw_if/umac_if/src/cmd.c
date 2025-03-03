/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @brief File containing command specific definitions for the
 * FMAC IF Layer of the Wi-Fi driver.
 */

#include "host_rpu_umac_if.h"
#include "hal_api.h"
#ifndef NRF70_OFFLOADED_RAW_TX
#include "fmac_structs.h"
#endif /* !NRF70_OFFLOADED_RAW_TX */
#include "fmac_util.h"

struct host_rpu_msg *umac_cmd_alloc(struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx,
				    int type,
				    int len)
{
	struct host_rpu_msg *umac_cmd = NULL;

	umac_cmd = nrf_wifi_osal_mem_zalloc(sizeof(*umac_cmd) + len);

	if (!umac_cmd) {
		nrf_wifi_osal_log_err("%s: Failed to allocate UMAC cmd",
				      __func__);
		goto out;
	}

	umac_cmd->type = type;
	umac_cmd->hdr.len = sizeof(*umac_cmd) + len;

out:
	return umac_cmd;
}


enum nrf_wifi_status umac_cmd_cfg(struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx,
				void *params,
				int len)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct host_rpu_msg *umac_cmd = NULL;

	if (!fmac_dev_ctx->fw_init_done) {
		struct nrf_wifi_umac_hdr *umac_hdr = NULL;

		umac_hdr = (struct nrf_wifi_umac_hdr *)params;
		nrf_wifi_osal_log_err("%s: UMAC buff config not yet done(%d)",
				      __func__,
				      umac_hdr->cmd_evnt);
		goto out;
	}

	umac_cmd = umac_cmd_alloc(fmac_dev_ctx,
				  NRF_WIFI_HOST_RPU_MSG_TYPE_UMAC,
				  len);

	if (!umac_cmd) {
		nrf_wifi_osal_log_err("%s: umac_cmd_alloc failed",
				      __func__);
		goto out;
	}

	nrf_wifi_osal_mem_cpy(umac_cmd->msg,
			      params,
			      len);

	status = nrf_wifi_hal_ctrl_cmd_send(fmac_dev_ctx->hal_dev_ctx,
					    umac_cmd,
					    (sizeof(*umac_cmd) + len));

	nrf_wifi_osal_log_dbg("%s: Command %d sent to RPU",
			      __func__,
			      ((struct nrf_wifi_umac_hdr *)params)->cmd_evnt);

out:
	return status;
}


enum nrf_wifi_status umac_cmd_init(struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx,
				   struct nrf_wifi_phy_rf_params *rf_params,
				   bool rf_params_valid,
#if !defined(NRF70_RADIO_TEST) && !defined(NRF70_OFFLOADED_RAW_TX)
				   struct nrf_wifi_data_config_params *config,
#endif /* !NRF70_RADIO_TEST && !NRF70_OFFLOADED_RAW_TX*/
#ifdef NRF_WIFI_LOW_POWER
				   int sleep_type,
#endif /* NRF_WIFI_LOW_POWER */
				   unsigned int phy_calib,
				   enum op_band op_band,
				   bool beamforming,
				   struct nrf_wifi_tx_pwr_ctrl_params *tx_pwr_ctrl_params,
				   struct nrf_wifi_board_params *board_params)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct host_rpu_msg *umac_cmd = NULL;
	struct nrf_wifi_cmd_sys_init *umac_cmd_data = NULL;
	unsigned int len = 0;
	struct nrf_wifi_fmac_priv_def *def_priv = NULL;

	def_priv = wifi_fmac_priv(fmac_dev_ctx->fpriv);

	len = sizeof(*umac_cmd_data);

	umac_cmd = umac_cmd_alloc(fmac_dev_ctx,
				  NRF_WIFI_HOST_RPU_MSG_TYPE_SYSTEM,
				  len);

	if (!umac_cmd) {
		nrf_wifi_osal_log_err("%s: umac_cmd_alloc failed",
				      __func__);
		goto out;
	}

	umac_cmd_data = (struct nrf_wifi_cmd_sys_init *)(umac_cmd->msg);

	umac_cmd_data->sys_head.cmd_event = NRF_WIFI_CMD_INIT;
	umac_cmd_data->sys_head.len = len;


	umac_cmd_data->sys_params.rf_params_valid = rf_params_valid;

	if (rf_params_valid) {
		nrf_wifi_osal_mem_cpy(umac_cmd_data->sys_params.rf_params,
				      rf_params,
				      NRF_WIFI_RF_PARAMS_SIZE);
	}


	umac_cmd_data->sys_params.phy_calib = phy_calib;
	umac_cmd_data->sys_params.hw_bringup_time = HW_DELAY;
	umac_cmd_data->sys_params.sw_bringup_time = SW_DELAY;
	umac_cmd_data->sys_params.bcn_time_out = BCN_TIMEOUT;
	umac_cmd_data->sys_params.calib_sleep_clk = CALIB_SLEEP_CLOCK_ENABLE;
#ifdef NRF_WIFI_LOW_POWER
	umac_cmd_data->sys_params.sleep_enable = sleep_type;
#endif /* NRF_WIFI_LOW_POWER */
#ifdef NRF70_TCP_IP_CHECKSUM_OFFLOAD
	umac_cmd_data->tcp_ip_checksum_offload = 1;
#endif /* NRF70_TCP_IP_CHECKSUM_OFFLOAD */
	umac_cmd_data->discon_timeout = CONFIG_NRF_WIFI_AP_DEAD_DETECT_TIMEOUT;
#ifdef NRF_WIFI_RPU_RECOVERY
	umac_cmd_data->watchdog_timer_val =
		(NRF_WIFI_RPU_RECOVERY_PS_ACTIVE_TIMEOUT_MS) / 1000;
#else
	/* Disable watchdog */
	umac_cmd_data->watchdog_timer_val = 0xFFFFFF;
#endif /* NRF_WIFI_RPU_RECOVERY */

	nrf_wifi_osal_log_dbg("RPU LPM type: %s",
		umac_cmd_data->sys_params.sleep_enable == 2 ? "HW" :
		umac_cmd_data->sys_params.sleep_enable == 1 ? "SW" : "DISABLED");

#ifdef NRF_WIFI_MGMT_BUFF_OFFLOAD
	umac_cmd_data->mgmt_buff_offload =  1;
	nrf_wifi_osal_log_info("Management buffer offload enabled\n");
#endif /* NRF_WIFI_MGMT_BUFF_OFFLOAD */
#ifdef NRF_WIFI_FEAT_KEEPALIVE
	umac_cmd_data->keep_alive_enable = KEEP_ALIVE_ENABLED;
	umac_cmd_data->keep_alive_period = NRF_WIFI_KEEPALIVE_PERIOD_S;
	nrf_wifi_osal_log_dbg(fmac_dev_ctx->fpriv->opriv,
			       "Keepalive enabled with period %d\n",
				   umac_cmd_data->keepalive_period);
#endif /* NRF_WIFI_FEAT_KEEPALIVE */

#if !defined(NRF70_RADIO_TEST) && !defined(NRF70_OFFLOADED_RAW_TX)
	nrf_wifi_osal_mem_cpy(umac_cmd_data->rx_buf_pools,
			      def_priv->rx_buf_pools,
			      sizeof(umac_cmd_data->rx_buf_pools));

	nrf_wifi_osal_mem_cpy(&umac_cmd_data->data_config_params,
			      config,
			      sizeof(umac_cmd_data->data_config_params));

	umac_cmd_data->temp_vbat_config_params.temp_based_calib_en = NRF_WIFI_TEMP_CALIB_ENABLE;
	umac_cmd_data->temp_vbat_config_params.temp_calib_bitmap = NRF_WIFI_DEF_PHY_TEMP_CALIB;
	umac_cmd_data->temp_vbat_config_params.vbat_calibp_bitmap = NRF_WIFI_DEF_PHY_VBAT_CALIB;
	umac_cmd_data->temp_vbat_config_params.temp_vbat_mon_period = NRF_WIFI_TEMP_CALIB_PERIOD;
	umac_cmd_data->temp_vbat_config_params.vth_low = NRF_WIFI_VBAT_LOW;
	umac_cmd_data->temp_vbat_config_params.vth_hi = NRF_WIFI_VBAT_HIGH;
	umac_cmd_data->temp_vbat_config_params.temp_threshold = NRF_WIFI_TEMP_CALIB_THRESHOLD;
	umac_cmd_data->temp_vbat_config_params.vth_very_low = NRF_WIFI_VBAT_VERYLOW;
#endif /* !NRF70_RADIO_TEST && !NRF70_OFFLOADED_RAW_TX*/

	umac_cmd_data->op_band = op_band;

	nrf_wifi_osal_mem_cpy(&umac_cmd_data->sys_params.rf_params[PCB_LOSS_BYTE_2G_OFST],
			      &board_params->pcb_loss_2g,
			      NUM_PCB_LOSS_OFFSET);

	nrf_wifi_osal_mem_cpy(&umac_cmd_data->sys_params.rf_params[ANT_GAIN_2G_OFST],
			      &tx_pwr_ctrl_params->ant_gain_2g,
			      NUM_ANT_GAIN);

	nrf_wifi_osal_mem_cpy(&umac_cmd_data->sys_params.rf_params[BAND_2G_LW_ED_BKF_DSSS_OFST],
			      &tx_pwr_ctrl_params->band_edge_2g_lo_dss,
			      NUM_EDGE_BACKOFF);

	nrf_wifi_osal_mem_cpy(umac_cmd_data->country_code,
			      STRINGIFY(NRF70_REG_DOMAIN),
			      NRF_WIFI_COUNTRY_CODE_LEN);

#ifdef NRF70_RPU_EXTEND_TWT_SP
	 umac_cmd_data->feature_flags |= TWT_EXTEND_SP_EDCA;
#endif
#ifdef CONFIG_WIFI_NRF70_SCAN_DISABLE_DFS_CHANNELS
	umac_cmd_data->feature_flags |= DISABLE_DFS_CHANNELS;
#endif /* NRF70_SCAN_DISABLE_DFS_CHANNELS */

	if (!beamforming) {
		umac_cmd_data->disable_beamforming = 1;
	}

#if defined(NRF_WIFI_PS_INT_PS)
	umac_cmd_data->ps_exit_strategy = INT_PS;
#else
	umac_cmd_data->ps_exit_strategy = EVERY_TIM;
#endif  /* NRF_WIFI_PS_INT_PS */

	status = nrf_wifi_hal_ctrl_cmd_send(fmac_dev_ctx->hal_dev_ctx,
					    umac_cmd,
					    (sizeof(*umac_cmd) + len));

out:
	return status;
}


enum nrf_wifi_status umac_cmd_deinit(struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct host_rpu_msg *umac_cmd = NULL;
	struct nrf_wifi_cmd_sys_deinit *umac_cmd_data = NULL;
	unsigned int len = 0;

	len = sizeof(*umac_cmd_data);
	umac_cmd = umac_cmd_alloc(fmac_dev_ctx,
				  NRF_WIFI_HOST_RPU_MSG_TYPE_SYSTEM,
				  len);
	if (!umac_cmd) {
		nrf_wifi_osal_log_err("%s: umac_cmd_alloc failed",
				      __func__);
		goto out;
	}
	umac_cmd_data = (struct nrf_wifi_cmd_sys_deinit *)(umac_cmd->msg);
	umac_cmd_data->sys_head.cmd_event = NRF_WIFI_CMD_DEINIT;
	umac_cmd_data->sys_head.len = len;
	status = nrf_wifi_hal_ctrl_cmd_send(fmac_dev_ctx->hal_dev_ctx,
					    umac_cmd,
					    (sizeof(*umac_cmd) + len));
out:
	return status;
}

#ifndef NRF70_OFFLOADED_RAW_TX
enum nrf_wifi_status umac_cmd_srcoex(struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx,
				void *cmd, unsigned int cmd_len)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct host_rpu_msg *umac_cmd = NULL;
	struct nrf_wifi_cmd_coex_config *umac_cmd_data = NULL;
	int len = 0;

	len = sizeof(*umac_cmd_data)+cmd_len;

	umac_cmd = umac_cmd_alloc(fmac_dev_ctx,
				  NRF_WIFI_HOST_RPU_MSG_TYPE_SYSTEM,
				  len);

	if (!umac_cmd) {
		nrf_wifi_osal_log_err("%s: umac_cmd_alloc failed",
				      __func__);
		goto out;
	}

	umac_cmd_data = (struct nrf_wifi_cmd_coex_config *)(umac_cmd->msg);

	umac_cmd_data->sys_head.cmd_event = NRF_WIFI_CMD_SRCOEX;
	umac_cmd_data->sys_head.len = len;
	umac_cmd_data->coex_config_info.len = cmd_len;

	nrf_wifi_osal_mem_cpy(umac_cmd_data->coex_config_info.coex_cmd,
			      cmd,
			      cmd_len);

	status = nrf_wifi_hal_ctrl_cmd_send(fmac_dev_ctx->hal_dev_ctx,
					    umac_cmd,
					    (sizeof(*umac_cmd) + len));
out:
	return status;


}

enum nrf_wifi_status umac_cmd_he_ltf_gi(struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx,
					unsigned char he_ltf,
					unsigned char he_gi,
					unsigned char enabled)
{
	struct host_rpu_msg *umac_cmd = NULL;
	struct nrf_wifi_cmd_he_gi_ltf_config *umac_cmd_data = NULL;
	int len = 0;
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;

	len = sizeof(*umac_cmd_data);

	umac_cmd = umac_cmd_alloc(fmac_dev_ctx,
				  NRF_WIFI_HOST_RPU_MSG_TYPE_SYSTEM,
				  len);

	if (!umac_cmd) {
		nrf_wifi_osal_log_err("%s: umac_cmd_alloc failed",
				      __func__);
		goto out;
	}

	umac_cmd_data = (struct nrf_wifi_cmd_he_gi_ltf_config *)(umac_cmd->msg);

	umac_cmd_data->sys_head.cmd_event = NRF_WIFI_CMD_HE_GI_LTF_CONFIG;
	umac_cmd_data->sys_head.len = len;

	if (enabled) {
		nrf_wifi_osal_mem_cpy(&umac_cmd_data->he_ltf,
				      &he_ltf,
				      sizeof(he_ltf));
		nrf_wifi_osal_mem_cpy(&umac_cmd_data->he_gi_type,
				      &he_gi,
				      sizeof(he_gi));
	}

	nrf_wifi_osal_mem_cpy(&umac_cmd_data->enable,
			      &enabled,
			      sizeof(enabled));

	status = nrf_wifi_hal_ctrl_cmd_send(fmac_dev_ctx->hal_dev_ctx,
					    umac_cmd,
					    (sizeof(*umac_cmd) + len));
out:
	return status;
}

#ifdef NRF70_RADIO_TEST
enum nrf_wifi_status umac_cmd_prog_init(struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx,
					struct nrf_wifi_radio_test_init_info *init_params)
{
	struct host_rpu_msg *umac_cmd = NULL;
	struct nrf_wifi_cmd_radio_test_init *umac_cmd_data = NULL;
	int len = 0;
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;

	len = sizeof(*umac_cmd_data);

	umac_cmd = umac_cmd_alloc(fmac_dev_ctx,
				  NRF_WIFI_HOST_RPU_MSG_TYPE_SYSTEM,
				  len);

	if (!umac_cmd) {
		nrf_wifi_osal_log_err("%s: umac_cmd_alloc failed",
				      __func__);
		goto out;
	}

	umac_cmd_data = (struct nrf_wifi_cmd_radio_test_init *)(umac_cmd->msg);

	umac_cmd_data->sys_head.cmd_event = NRF_WIFI_CMD_RADIO_TEST_INIT;
	umac_cmd_data->sys_head.len = len;

	nrf_wifi_osal_mem_cpy(&umac_cmd_data->conf,
			      init_params,
			      sizeof(umac_cmd_data->conf));

	status = nrf_wifi_hal_ctrl_cmd_send(fmac_dev_ctx->hal_dev_ctx,
					    umac_cmd,
					    (sizeof(*umac_cmd) + len));
out:
	return status;
}


enum nrf_wifi_status umac_cmd_prog_tx(struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx,
				      struct rpu_conf_params *params)
{
	struct host_rpu_msg *umac_cmd = NULL;
	struct nrf_wifi_cmd_mode_params *umac_cmd_data = NULL;
	int len = 0;
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;

	len = sizeof(*umac_cmd_data);

	umac_cmd = umac_cmd_alloc(fmac_dev_ctx,
				  NRF_WIFI_HOST_RPU_MSG_TYPE_SYSTEM,
				  len);

	if (!umac_cmd) {
		nrf_wifi_osal_log_err("%s: umac_cmd_alloc failed",
				      __func__);
		goto out;
	}

	umac_cmd_data = (struct nrf_wifi_cmd_mode_params *)(umac_cmd->msg);

	umac_cmd_data->sys_head.cmd_event = NRF_WIFI_CMD_TX;
	umac_cmd_data->sys_head.len = len;

	nrf_wifi_osal_mem_cpy(&umac_cmd_data->conf,
			      params,
			      sizeof(umac_cmd_data->conf));

	status = nrf_wifi_hal_ctrl_cmd_send(fmac_dev_ctx->hal_dev_ctx,
					    umac_cmd,
					    (sizeof(*umac_cmd) + len));

out:
	return status;
}


enum nrf_wifi_status umac_cmd_prog_rx(struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx,
				      struct rpu_conf_rx_radio_test_params *rx_params)
{
	struct host_rpu_msg *umac_cmd = NULL;
	struct nrf_wifi_cmd_rx *umac_cmd_data = NULL;
	int len = 0;
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;

	len = sizeof(*umac_cmd_data);

	umac_cmd = umac_cmd_alloc(fmac_dev_ctx,
				  NRF_WIFI_HOST_RPU_MSG_TYPE_SYSTEM,
				  len);

	if (!umac_cmd) {
		nrf_wifi_osal_log_err("%s: umac_cmd_alloc failed",
				      __func__);
		goto out;
	}

	umac_cmd_data = (struct nrf_wifi_cmd_rx *)(umac_cmd->msg);

	umac_cmd_data->sys_head.cmd_event = NRF_WIFI_CMD_RX;
	umac_cmd_data->sys_head.len = len;

	nrf_wifi_osal_mem_cpy(&umac_cmd_data->conf,
			      rx_params,
			      sizeof(umac_cmd_data->conf));

	status = nrf_wifi_hal_ctrl_cmd_send(fmac_dev_ctx->hal_dev_ctx,
					    umac_cmd,
					    (sizeof(*umac_cmd) + len));

out:
	return status;
}


enum nrf_wifi_status umac_cmd_prog_rf_test(struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx,
					   void *rf_test_params,
					   unsigned int rf_test_params_sz)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct host_rpu_msg *umac_cmd = NULL;
	struct nrf_wifi_cmd_rftest *umac_cmd_data = NULL;
	int len = 0;

	len = (sizeof(*umac_cmd_data) + rf_test_params_sz);

	umac_cmd = umac_cmd_alloc(fmac_dev_ctx,
				  NRF_WIFI_HOST_RPU_MSG_TYPE_SYSTEM,
				  len);

	if (!umac_cmd) {
		nrf_wifi_osal_log_err("%s: umac_cmd_alloc failed",
				      __func__);
		goto out;
	}

	umac_cmd_data = (struct nrf_wifi_cmd_rftest *)(umac_cmd->msg);

	umac_cmd_data->sys_head.cmd_event = NRF_WIFI_CMD_RF_TEST;
	umac_cmd_data->sys_head.len = len;

	nrf_wifi_osal_mem_cpy((void *)umac_cmd_data->rf_test_info.rfcmd,
			      rf_test_params,
			      rf_test_params_sz);

	umac_cmd_data->rf_test_info.len = rf_test_params_sz;

	status = nrf_wifi_hal_ctrl_cmd_send(fmac_dev_ctx->hal_dev_ctx,
					    umac_cmd,
					    (sizeof(*umac_cmd) + len));

out:
	return status;
}
#endif /* NRF70_RADIO_TEST */

#endif /* !NRF70_OFFLOADED_RAW_TX */

enum nrf_wifi_status umac_cmd_prog_stats_get(struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx,
#ifdef NRF70_RADIO_TEST
					     int op_mode,
#endif /* NRF70_RADIO_TEST */
					     int stats_type)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct host_rpu_msg *umac_cmd = NULL;
	struct nrf_wifi_cmd_get_stats *umac_cmd_data = NULL;
	int len = 0;

	len = sizeof(*umac_cmd_data);

	umac_cmd = umac_cmd_alloc(fmac_dev_ctx,
				  NRF_WIFI_HOST_RPU_MSG_TYPE_SYSTEM,
				  len);

	if (!umac_cmd) {
		nrf_wifi_osal_log_err("%s: umac_cmd_alloc failed",
				      __func__);
		goto out;
	}

	umac_cmd_data = (struct nrf_wifi_cmd_get_stats *)(umac_cmd->msg);

	umac_cmd_data->sys_head.cmd_event = NRF_WIFI_CMD_GET_STATS;
	umac_cmd_data->sys_head.len = len;
	umac_cmd_data->stats_type = stats_type;
#ifdef NRF70_RADIO_TEST
	umac_cmd_data->op_mode = op_mode;
#endif /* NRF70_RADIO_TEST */

	status = nrf_wifi_hal_ctrl_cmd_send(fmac_dev_ctx->hal_dev_ctx,
					    umac_cmd,
					    (sizeof(*umac_cmd) + len));

out:
	return status;
}

#ifndef NRF70_OFFLOADED_RAW_TX
enum nrf_wifi_status umac_cmd_prog_stats_reset(struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct host_rpu_msg *umac_cmd = NULL;
	struct nrf_wifi_cmd_reset_stats *umac_cmd_data = NULL;
	int len = 0;

	len = sizeof(*umac_cmd_data);

	umac_cmd = umac_cmd_alloc(fmac_dev_ctx,
				  NRF_WIFI_HOST_RPU_MSG_TYPE_SYSTEM,
				  len);
	if (!umac_cmd) {
		nrf_wifi_osal_log_err("%s: umac_cmd_alloc failed",
				      __func__);
		goto out;
	}

	umac_cmd_data = (struct nrf_wifi_cmd_reset_stats *)(umac_cmd->msg);

	umac_cmd_data->sys_head.cmd_event = NRF_WIFI_CMD_RESET_STATISTICS;
	umac_cmd_data->sys_head.len = len;

	status = nrf_wifi_hal_ctrl_cmd_send(fmac_dev_ctx->hal_dev_ctx,
					    umac_cmd,
					    (sizeof(*umac_cmd) + len));

out:
	return status;
}
#endif /* NRF70_OFFLOADED_RAW_TX */

#ifdef NRF70_OFFLOADED_RAW_TX
enum nrf_wifi_status umac_cmd_off_raw_tx_conf(struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx,
					      struct nrf_wifi_offload_ctrl_params *off_ctrl_params,
					      struct nrf_wifi_offload_tx_ctrl *offload_tx_params)
{
	struct host_rpu_msg *umac_cmd = NULL;
	struct nrf_wifi_cmd_offload_raw_tx_params *umac_cmd_data = NULL;
	int len = 0;
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;

	if (!fmac_dev_ctx->fw_init_done) {
		nrf_wifi_osal_log_err("%s: UMAC buff config not yet done",__func__);
		goto out;
	}

	if (!off_ctrl_params) {
		nrf_wifi_osal_log_err("%s: offloaded raw tx control params is NULL", __func__);
		goto out;
	}

	if (!offload_tx_params) {
		nrf_wifi_osal_log_err("%s: offload raw tx params is NULL", __func__);
		goto out;
	}

	len = sizeof(*umac_cmd_data);

	umac_cmd = umac_cmd_alloc(fmac_dev_ctx,
				  NRF_WIFI_HOST_RPU_MSG_TYPE_SYSTEM,
				  len);

	if (!umac_cmd) {
		nrf_wifi_osal_log_err("%s: umac_cmd_alloc failed", __func__);
		goto out;
	}

	umac_cmd_data = (struct nrf_wifi_cmd_offload_raw_tx_params *)(umac_cmd->msg);

	umac_cmd_data->sys_head.cmd_event = NRF_WIFI_CMD_OFFLOAD_RAW_TX_PARAMS;
	umac_cmd_data->sys_head.len = len;

	nrf_wifi_osal_mem_cpy(&umac_cmd_data->ctrl_info,
			      off_ctrl_params,
			      sizeof(*off_ctrl_params));

	nrf_wifi_osal_mem_cpy(&umac_cmd_data->tx_params,
			      offload_tx_params,
			      sizeof(*offload_tx_params));

	status = nrf_wifi_hal_ctrl_cmd_send(fmac_dev_ctx->hal_dev_ctx,
					    umac_cmd,
					    (sizeof(*umac_cmd) + len));
out:
	return status;
}

enum nrf_wifi_status umac_cmd_off_raw_tx_ctrl(struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx,
					      unsigned char ctrl_type)
{
	struct host_rpu_msg *umac_cmd = NULL;
	struct nrf_wifi_cmd_offload_raw_tx_ctrl *umac_cmd_data = NULL;
	int len = 0;
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;

	if (!fmac_dev_ctx->fw_init_done) {
		nrf_wifi_osal_log_err("%s: UMAC buff config not yet done", __func__);
		goto out;
	}

	len = sizeof(*umac_cmd_data);

	umac_cmd = umac_cmd_alloc(fmac_dev_ctx,
				  NRF_WIFI_HOST_RPU_MSG_TYPE_SYSTEM,
				  len);

	if (!umac_cmd) {
		nrf_wifi_osal_log_err("%s: umac_cmd_alloc failed", __func__);
		goto out;
	}

	umac_cmd_data = (struct nrf_wifi_cmd_offload_raw_tx_ctrl *)(umac_cmd->msg);

	umac_cmd_data->sys_head.cmd_event = NRF_WIFI_CMD_OFFLOAD_RAW_TX_CTRL;
	umac_cmd_data->sys_head.len = len;
	umac_cmd_data->ctrl_type = ctrl_type;

	status = nrf_wifi_hal_ctrl_cmd_send(fmac_dev_ctx->hal_dev_ctx,
					    umac_cmd,
					    (sizeof(*umac_cmd) + len));
out:
	return status;
}
#endif /* NRF70_OFFLOADED_RAW_TX */
