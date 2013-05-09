/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBQCDM_DM_LOG_ITEMS_H
#define LIBQCDM_DM_LOG_ITEMS_H

enum {
    /* CDMA and EVDO items */
    DM_LOG_ITEM_CDMA_ACCESS_CHANNEL_MSG         = 0x1004,
    DM_LOG_ITEM_CDMA_REV_CHANNEL_TRAFFIC_MSG    = 0x1005,
    DM_LOG_ITEM_CDMA_SYNC_CHANNEL_MSG           = 0x1006,
    DM_LOG_ITEM_CDMA_PAGING_CHANNEL_MSG         = 0x1007,
    DM_LOG_ITEM_CDMA_FWD_CHANNEL_TRAFFIC_MSG    = 0x1008,
    DM_LOG_ITEM_CDMA_FWD_LINK_VOCODER_PACKET    = 0x1009,
    DM_LOG_ITEM_CDMA_REV_LINK_VOCODER_PACKET    = 0x100A,
    DM_LOG_ITEM_CDMA_MARKOV_STATS               = 0x100E,
    DM_LOG_ITEM_CDMA_REVERSE_POWER_CONTROL      = 0x102C,
    DM_LOG_ITEM_CDMA_SERVICE_CONFIG             = 0x102E,
    DM_LOG_ITEM_EVDO_HANDOFF_STATE              = 0x105E,
    DM_LOG_ITEM_EVDO_ACTIVE_PILOT_SET           = 0x105F,
    DM_LOG_ITEM_EVDO_REV_LINK_PACKET_SUMMARY    = 0x1060,
    DM_LOG_ITEM_EVDO_REV_TRAFFIC_RATE_COUNT     = 0x1062,
    DM_LOG_ITEM_EVDO_REV_POWER_CONTROL          = 0x1063,
    DM_LOG_ITEM_EVDO_ARQ_EFFECTIVE_RECEIVE_RATE = 0x1066,
    DM_LOG_ITEM_EVDO_AIR_LINK_SUMMARY           = 0x1068,
    DM_LOG_ITEM_EVDO_POWER                      = 0x1069,
    DM_LOG_ITEM_EVDO_FWD_LINK_PACKET_SNAPSHOT   = 0x106A,
    DM_LOG_ITEM_EVDO_ACCESS_ATTEMPT             = 0x106C,
    DM_LOG_ITEM_EVDO_REV_ACTIVITY_BITS_BUFFER   = 0x106D,
    DM_LOG_ITEM_EVDO_PILOT_SETS                 = 0x107A,
    DM_LOG_ITEM_EVDO_STATE_INFO                 = 0x107E,
    DM_LOG_ITEM_EVDO_SECTOR_INFO                = 0x1080,
    DM_LOG_ITEM_EVDO_PILOT_SETS_V2              = 0x108B,

    /* WCDMA items */
    DM_LOG_ITEM_WCDMA_TA_FINGER_INFO       = 0x4003,
    DM_LOG_ITEM_WCDMA_AGC_INFO             = 0x4105,
    DM_LOG_ITEM_WCDMA_RRC_STATE            = 0x4125,
    DM_LOG_ITEM_WCDMA_CELL_ID              = 0x4127,

    /* GSM items */
    DM_LOG_ITEM_GSM_BURST_METRICS          = 0x506c,
    DM_LOG_ITEM_GSM_BCCH_MESSAGE           = 0x5134,
};


/* DM_LOG_ITEM_CDMA_PAGING_CHANNEL_MSG */
struct DMLogItemPagingChannelMsg {
    u_int8_t msg_len;  /* size of entire struct including this field */
    u_int8_t msg_type; /* MSG_TYPE as in 3GPP2 C.S0004 Table 3.1.2.3.1.1.2 */
    u_int8_t data[0];  /* Packed message as in 3GPP2 C.S0005 3.7.2.3.2.x */
} __attribute ((packed));
typedef struct DMLogItemPagingChannelMsg DMLogItemPagingChannelMsg;


/* DM_LOG_ITEM_CDMA_REVERSE_POWER_CONTROL */
struct DMLogItemRPCItem {
    u_int8_t channel_set_mask;
    u_int16_t frame_count;
    u_int8_t len_per_frame;
    u_int16_t dec_history;
    u_int8_t rx_agc_vals;
    u_int8_t tx_power_vals;
    u_int8_t tx_gain_adjust;
} __attribute__ ((packed));
typedef struct DMLogItemRPCItem DMLogItemRPCItem;

struct DMLogItemCdmaReversePowerControl {
    u_int8_t frame_offset;
    u_int8_t band_class;
    u_int16_t rev_chan_rc;
    u_int8_t pilot_gating_rate;
    u_int8_t step_size;
    u_int8_t num_records;
    DMLogItemRPCItem records[];
} __attribute__ ((packed));
typedef struct DMLogItemCdmaReversePowerControl DMLogItemCdmaReversePowerControl;

/* DM_LOG_ITEM_EVDO_PILOT_SETS_V2 */
#define DM_LOG_ITEM_EVDO_PILOT_SETS_V2_GET_BAND_CLASS(c) ((c & 0xF000) >> 12)
#define DM_LOG_ITEM_EVDO_PILOT_SETS_V2_GET_CHANNEL(c)    (c & 0xFFF)

struct DMLogItemEvdoPilotSetsV2PilotActive {
	u_int16_t pilot_pn;
	u_int16_t energy;
	u_int16_t unknown1;
	u_int16_t mac_index;
	u_int16_t window_center;
} __attribute__ ((packed));
typedef struct DMLogItemEvdoPilotSetsV2PilotActive DMLogItemEvdoPilotSetsV2PilotActive;

struct DMLogItemEvdoPilotSetsV2PilotCandidate {
	u_int16_t pilot_pn;
	u_int16_t energy;
	u_int16_t channel;  /* top 4 bits are band class, lower 12 are channel */
	u_int16_t unknown1;
	u_int16_t window_center;
} __attribute__ ((packed));
typedef struct DMLogItemEvdoPilotSetsV2PilotCandidate DMLogItemEvdoPilotSetsV2PilotCandidate;

struct DMLogItemEvdoPilotSetsV2PilotNeighbor {
	u_int16_t pilot_pn;
	u_int16_t energy;
	u_int16_t channel;  /* top 4 bits are band class, lower 12 are channel */
	u_int16_t window_center;
	u_int8_t unknown1;
	u_int8_t age;
} __attribute__ ((packed));
typedef struct DMLogItemEvdoPilotSetsV2PilotNeighbor DMLogItemEvdoPilotSetsV2PilotNeighbor;

struct DMLogItemEvdoPilotSetsV2Pilot {
	union {
		struct DMLogItemEvdoPilotSetsV2PilotActive active;
		struct DMLogItemEvdoPilotSetsV2PilotCandidate candidate;
		struct DMLogItemEvdoPilotSetsV2PilotNeighbor neighbor;
	} u;
} __attribute__ ((packed));
typedef struct DMLogItemEvdoPilotSetsV2Pilot DMLogItemEvdoPilotSetsV2Pilot;

struct DMLogItemEvdoPilotSetsV2 {
    struct DMCmdLog cmd;

    u_int8_t pn_offset;
    u_int8_t active_count;
    u_int8_t active_window;
    u_int16_t active_channel; /* top 4 bits are band class, lower 12 are channel */
    u_int8_t unknown1;
    u_int8_t candidate_count;
    u_int8_t candidate_window;
    u_int8_t neighbor_count;
    u_int8_t neighbor_window;
    u_int8_t unknown2;

    struct DMLogItemEvdoPilotSetsV2Pilot sets[];
} __attribute__ ((packed));
typedef struct DMLogItemEvdoPilotSetsV2 DMLogItemEvdoPilotSetsV2;


/* DM_LOG_ITEM_WCDMA_TA_FINGER_INFO */
struct DMLogItemWcdmaTaFingerInfo {
    int32_t tx_pos;
    int16_t coherent_interval_len;
    u_int8_t non_coherent_interval_len;
    u_int8_t num_paths;
    u_int32_t path_enr;
    int32_t pn_pos_path;
    int16_t pri_cpich_psc;
    u_int8_t unknown1;
    u_int8_t sec_cpich_ssc;
    u_int8_t finger_channel_code_index;
    u_int8_t finger_index;
} __attribute__ ((packed));
typedef struct DMLogItemWcdmaTaFingerInfo DMLogItemWcdmaTaFingerInfo;


/* DM_LOG_ITEM_WCDMA_AGC_INFO */
struct DMLogItemWcdmaAgcInfo {
    u_int8_t num_samples;
    u_int16_t rx_agc;
    u_int16_t tx_agc;
    u_int16_t rx_agc_adj_pdm;
    u_int16_t tx_agc_adj_pdm;
    u_int16_t max_tx;
    /* Bit 4 means tx_agc is valid */
    u_int8_t agc_info;
} __attribute__ ((packed));
typedef struct DMLogItemWcdmaAgcInfo DMLogItemWcdmaAgcInfo;


/* DM_LOG_ITEM_WCDMA_RRC_STATE */
enum {
    DM_LOG_ITEM_WCDMA_RRC_STATE_DISCONNECTED = 0,
    DM_LOG_ITEM_WCDMA_RRC_STATE_CONNECTING   = 1,
    DM_LOG_ITEM_WCDMA_RRC_STATE_CELL_FACH    = 2,
    DM_LOG_ITEM_WCDMA_RRC_STATE_CELL_DCH     = 3,
    DM_LOG_ITEM_WCDMA_RRC_STATE_CELL_PCH     = 4,
    DM_LOG_ITEM_WCDMA_RRC_STATE_URA_PCH      = 5,
};

struct DMLogItemWcdmaRrcState {
    u_int8_t rrc_state;
} __attribute__ ((packed));
typedef struct DMLogItemWcdmaRrcState DMLogItemWcdmaRrcState;


/* DM_LOG_ITEM_WCDMA_CELL_ID */
struct DMLogItemWcdmaCellId {
    u_int8_t unknown1[8];
    u_int32_t cellid;
    u_int8_t unknown2[4];
} __attribute__ ((packed));
typedef struct DMLogItemWcdmaCellId DMLogItemWcdmaCellId;


/* DM_LOG_ITEM_GSM_BURST_METRICS */
struct DMLogItemGsmBurstMetric {
    u_int32_t fn;
    u_int16_t arfcn;
    u_int32_t rssi;
    u_int16_t power;
    u_int16_t dc_offset_i;
    u_int16_t dc_offset_q;
    u_int16_t freq_offset;
    u_int16_t timing_offset;
    u_int16_t snr;
    u_int8_t gain_state;
} __attribute__ ((packed));
typedef struct DMLogItemGsmBurstMetric DMLogItemGsmBurstMetric;

struct DMLogItemGsmBurstMetrics {
    u_int8_t channel;
    DMLogItemGsmBurstMetric metrics[4];
} __attribute__ ((packed));
typedef struct DMLogItemGsmBurstMetrics DMLogItemGsmBurstMetrics;


/* DM_LOG_ITEM_GSM_BCCH_MESSAGE */
enum {
    DM_LOG_ITEM_GSM_BCCH_BAND_UNKNOWN  = 0,
    DM_LOG_ITEM_GSM_BCCH_BAND_GSM_900  = 8,
    DM_LOG_ITEM_GSM_BCCH_BAND_DCS_1800 = 9,
    DM_LOG_ITEM_GSM_BCCH_BAND_PCS_1900 = 10,
    DM_LOG_ITEM_GSM_BCCH_BAND_GSM_850  = 11,
    DM_LOG_ITEM_GSM_BCCH_BAND_GSM_450  = 12,
};

struct DMLogItemGsmBcchMessage {
    /* Band is top 4 bits; lower 12 is ARFCN */
    u_int16_t bcch_arfcn;
    u_int16_t bsic;
    u_int16_t cell_id;
    u_int8_t lai[5];
    u_int8_t cell_selection_prio;
    u_int8_t ncc_permitted;
} __attribute__ ((packed));
typedef struct DMLogItemGsmBcchMessage DMLogItemGsmBcchMessage;

#endif  /* LIBQCDM_DM_LOG_ITEMS_H */
