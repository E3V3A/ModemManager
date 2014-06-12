/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details:
 *
 * Copyright (C) 2011 Red Hat, Inc.
 * Copyright (C) 2011 Google, Inc.
 */

#ifndef _MODEMMANAGER_ENUMS_H_
#define _MODEMMANAGER_ENUMS_H_

#if !defined (__MODEM_MANAGER_H_INSIDE__)
#error "Only <ModemManager.h> can be included directly."
#endif

/**
 * SECTION:mm-enums
 * @short_description: Common enumerations and types in the API.
 *
 * This section defines enumerations and types that are used in the
 * ModemManager interface.
 **/

/**
 * MMModemCapability:
 * @MM_MODEM_CAPABILITY_NONE: Modem has no capabilities.
 * @MM_MODEM_CAPABILITY_POTS: Modem supports the analog wired telephone network (ie 56k dialup) and does not have wireless/cellular capabilities.
 * @MM_MODEM_CAPABILITY_CDMA_EVDO: Modem supports at least one of CDMA 1xRTT, EVDO revision 0, EVDO revision A, or EVDO revision B.
 * @MM_MODEM_CAPABILITY_GSM_UMTS: Modem supports at least one of GSM, GPRS, EDGE, UMTS, HSDPA, HSUPA, or HSPA+ packet switched data capability.
 * @MM_MODEM_CAPABILITY_LTE: Modem has LTE data capability.
 * @MM_MODEM_CAPABILITY_LTE_ADVANCED: Modem has LTE Advanced data capability.
 * @MM_MODEM_CAPABILITY_IRIDIUM: Modem has Iridium capabilities.
 * @MM_MODEM_CAPABILITY_ANY: Mask specifying all capabilities.
 *
 * Flags describing one or more of the general access technology families that a
 * modem supports.
 */
typedef enum { /*< underscore_name=mm_modem_capability >*/
    MM_MODEM_CAPABILITY_NONE         = 0,
    MM_MODEM_CAPABILITY_POTS         = 1 << 0,
    MM_MODEM_CAPABILITY_CDMA_EVDO    = 1 << 1,
    MM_MODEM_CAPABILITY_GSM_UMTS     = 1 << 2,
    MM_MODEM_CAPABILITY_LTE          = 1 << 3,
    MM_MODEM_CAPABILITY_LTE_ADVANCED = 1 << 4,
    MM_MODEM_CAPABILITY_IRIDIUM      = 1 << 5,
    MM_MODEM_CAPABILITY_ANY          = 0xFFFFFFFF
} MMModemCapability;

/**
 * MMModemLock:
 * @MM_MODEM_LOCK_UNKNOWN: Lock reason unknown.
 * @MM_MODEM_LOCK_NONE: Modem is unlocked.
 * @MM_MODEM_LOCK_SIM_PIN: SIM requires the PIN code.
 * @MM_MODEM_LOCK_SIM_PIN2: SIM requires the PIN2 code.
 * @MM_MODEM_LOCK_SIM_PUK: SIM requires the PUK code.
 * @MM_MODEM_LOCK_SIM_PUK2: SIM requires the PUK2 code.
 * @MM_MODEM_LOCK_PH_SP_PIN: Modem requires the service provider PIN code.
 * @MM_MODEM_LOCK_PH_SP_PUK: Modem requires the service provider PUK code.
 * @MM_MODEM_LOCK_PH_NET_PIN: Modem requires the network PIN code.
 * @MM_MODEM_LOCK_PH_NET_PUK: Modem requires the network PUK code.
 * @MM_MODEM_LOCK_PH_SIM_PIN: Modem requires the PIN code.
 * @MM_MODEM_LOCK_PH_CORP_PIN: Modem requires the corporate PIN code.
 * @MM_MODEM_LOCK_PH_CORP_PUK: Modem requires the corporate PUK code.
 * @MM_MODEM_LOCK_PH_FSIM_PIN: Modem requires the PH-FSIM PIN code.
 * @MM_MODEM_LOCK_PH_FSIM_PUK: Modem requires the PH-FSIM PUK code.
 * @MM_MODEM_LOCK_PH_NETSUB_PIN: Modem requires the network subset PIN code.
 * @MM_MODEM_LOCK_PH_NETSUB_PUK: Modem requires the network subset PUK code.
 *
 * Enumeration of possible lock reasons.
 */
typedef enum { /*< underscore_name=mm_modem_lock >*/
    MM_MODEM_LOCK_UNKNOWN        = 0,
    MM_MODEM_LOCK_NONE           = 1,
    MM_MODEM_LOCK_SIM_PIN        = 2,
    MM_MODEM_LOCK_SIM_PIN2       = 3,
    MM_MODEM_LOCK_SIM_PUK        = 4,
    MM_MODEM_LOCK_SIM_PUK2       = 5,
    MM_MODEM_LOCK_PH_SP_PIN      = 6,
    MM_MODEM_LOCK_PH_SP_PUK      = 7,
    MM_MODEM_LOCK_PH_NET_PIN     = 8,
    MM_MODEM_LOCK_PH_NET_PUK     = 9,
    MM_MODEM_LOCK_PH_SIM_PIN     = 10,
    MM_MODEM_LOCK_PH_CORP_PIN    = 11,
    MM_MODEM_LOCK_PH_CORP_PUK    = 12,
    MM_MODEM_LOCK_PH_FSIM_PIN    = 13,
    MM_MODEM_LOCK_PH_FSIM_PUK    = 14,
    MM_MODEM_LOCK_PH_NETSUB_PIN  = 15,
    MM_MODEM_LOCK_PH_NETSUB_PUK  = 16
} MMModemLock;

/**
 * MMModemState:
 * @MM_MODEM_STATE_FAILED: The modem is unusable.
 * @MM_MODEM_STATE_UNKNOWN: State unknown or not reportable.
 * @MM_MODEM_STATE_INITIALIZING: The modem is currently being initialized.
 * @MM_MODEM_STATE_LOCKED: The modem needs to be unlocked.
 * @MM_MODEM_STATE_DISABLED: The modem is not enabled and is powered down.
 * @MM_MODEM_STATE_DISABLING: The modem is currently transitioning to the @MM_MODEM_STATE_DISABLED state.
 * @MM_MODEM_STATE_ENABLING: The modem is currently transitioning to the @MM_MODEM_STATE_ENABLED state.
 * @MM_MODEM_STATE_ENABLED: The modem is enabled and powered on but not registered with a network provider and not available for data connections.
 * @MM_MODEM_STATE_SEARCHING: The modem is searching for a network provider to register with.
 * @MM_MODEM_STATE_REGISTERED: The modem is registered with a network provider, and data connections and messaging may be available for use.
 * @MM_MODEM_STATE_DISCONNECTING: The modem is disconnecting and deactivating the last active packet data bearer. This state will not be entered if more than one packet data bearer is active and one of the active bearers is deactivated.
 * @MM_MODEM_STATE_CONNECTING: The modem is activating and connecting the first packet data bearer. Subsequent bearer activations when another bearer is already active do not cause this state to be entered.
 * @MM_MODEM_STATE_CONNECTED: One or more packet data bearers is active and connected.
 *
 * Enumeration of possible modem states.
 */
typedef enum { /*< underscore_name=mm_modem_state >*/
    MM_MODEM_STATE_FAILED        = -1,
    MM_MODEM_STATE_UNKNOWN       = 0,
    MM_MODEM_STATE_INITIALIZING  = 1,
    MM_MODEM_STATE_LOCKED        = 2,
    MM_MODEM_STATE_DISABLED      = 3,
    MM_MODEM_STATE_DISABLING     = 4,
    MM_MODEM_STATE_ENABLING      = 5,
    MM_MODEM_STATE_ENABLED       = 6,
    MM_MODEM_STATE_SEARCHING     = 7,
    MM_MODEM_STATE_REGISTERED    = 8,
    MM_MODEM_STATE_DISCONNECTING = 9,
    MM_MODEM_STATE_CONNECTING    = 10,
    MM_MODEM_STATE_CONNECTED     = 11
} MMModemState;

/**
 * MMModemStateFailedReason:
 * @MM_MODEM_STATE_FAILED_REASON_NONE: No error.
 * @MM_MODEM_STATE_FAILED_REASON_UNKNOWN: Unknown error.
 * @MM_MODEM_STATE_FAILED_REASON_SIM_MISSING: SIM is required but missing.
 * @MM_MODEM_STATE_FAILED_REASON_SIM_ERROR: SIM is available, but unusable (e.g. permanently locked).
 *
 * Enumeration of possible errors when the modem is in @MM_MODEM_STATE_FAILED.
 */
typedef enum { /*< underscore_name=mm_modem_state_failed_reason >*/
    MM_MODEM_STATE_FAILED_REASON_NONE        = 0,
    MM_MODEM_STATE_FAILED_REASON_UNKNOWN     = 1,
    MM_MODEM_STATE_FAILED_REASON_SIM_MISSING = 2,
    MM_MODEM_STATE_FAILED_REASON_SIM_ERROR   = 3,
} MMModemStateFailedReason;

/**
 * MMModemPowerState:
 * @MM_MODEM_POWER_STATE_UNKNOWN: Unknown power state.
 * @MM_MODEM_POWER_STATE_OFF: Off.
 * @MM_MODEM_POWER_STATE_LOW: Low-power mode.
 * @MM_MODEM_POWER_STATE_ON: Full power mode.
 *
 * Power state of the modem.
 */
typedef enum { /*< underscore_name=mm_modem_power_state >*/
    MM_MODEM_POWER_STATE_UNKNOWN = 0,
    MM_MODEM_POWER_STATE_OFF     = 1,
    MM_MODEM_POWER_STATE_LOW     = 2,
    MM_MODEM_POWER_STATE_ON      = 3
} MMModemPowerState;

/**
 * MMModemStateChangeReason:
 * @MM_MODEM_STATE_CHANGE_REASON_UNKNOWN: Reason unknown or not reportable.
 * @MM_MODEM_STATE_CHANGE_REASON_USER_REQUESTED: State change was requested by an interface user.
 * @MM_MODEM_STATE_CHANGE_REASON_SUSPEND: State change was caused by a system suspend.
 * @MM_MODEM_STATE_CHANGE_REASON_FAILURE: State change was caused by an unrecoverable error.
 *
 * Enumeration of possible reasons to have changed the modem state.
 */
typedef enum { /*< underscore_name=mm_modem_state_change_reason >*/
    MM_MODEM_STATE_CHANGE_REASON_UNKNOWN        = 0,
    MM_MODEM_STATE_CHANGE_REASON_USER_REQUESTED = 1,
    MM_MODEM_STATE_CHANGE_REASON_SUSPEND        = 2,
    MM_MODEM_STATE_CHANGE_REASON_FAILURE        = 3,
} MMModemStateChangeReason;

/**
 * MMModemAccessTechnology:
 * @MM_MODEM_ACCESS_TECHNOLOGY_UNKNOWN: The access technology used is unknown.
 * @MM_MODEM_ACCESS_TECHNOLOGY_POTS: Analog wireline telephone.
 * @MM_MODEM_ACCESS_TECHNOLOGY_GSM: GSM.
 * @MM_MODEM_ACCESS_TECHNOLOGY_GSM_COMPACT: Compact GSM.
 * @MM_MODEM_ACCESS_TECHNOLOGY_GPRS: GPRS.
 * @MM_MODEM_ACCESS_TECHNOLOGY_EDGE: EDGE (ETSI 27.007: "GSM w/EGPRS").
 * @MM_MODEM_ACCESS_TECHNOLOGY_UMTS: UMTS (ETSI 27.007: "UTRAN").
 * @MM_MODEM_ACCESS_TECHNOLOGY_HSDPA: HSDPA (ETSI 27.007: "UTRAN w/HSDPA").
 * @MM_MODEM_ACCESS_TECHNOLOGY_HSUPA: HSUPA (ETSI 27.007: "UTRAN w/HSUPA").
 * @MM_MODEM_ACCESS_TECHNOLOGY_HSPA: HSPA (ETSI 27.007: "UTRAN w/HSDPA and HSUPA").
 * @MM_MODEM_ACCESS_TECHNOLOGY_HSPA_PLUS: HSPA+ (ETSI 27.007: "UTRAN w/HSPA+").
 * @MM_MODEM_ACCESS_TECHNOLOGY_1XRTT: CDMA2000 1xRTT.
 * @MM_MODEM_ACCESS_TECHNOLOGY_EVDO0: CDMA2000 EVDO revision 0.
 * @MM_MODEM_ACCESS_TECHNOLOGY_EVDOA: CDMA2000 EVDO revision A.
 * @MM_MODEM_ACCESS_TECHNOLOGY_EVDOB: CDMA2000 EVDO revision B.
 * @MM_MODEM_ACCESS_TECHNOLOGY_LTE: LTE (ETSI 27.007: "E-UTRAN")
 * @MM_MODEM_ACCESS_TECHNOLOGY_ANY: Mask specifying all access technologies.
 *
 * Describes various access technologies that a device uses when registered with
 * or connected to a network.
 */
typedef enum { /*< underscore_name=mm_modem_access_technology >*/
    MM_MODEM_ACCESS_TECHNOLOGY_UNKNOWN     = 0,
    MM_MODEM_ACCESS_TECHNOLOGY_POTS        = 1 << 0,
    MM_MODEM_ACCESS_TECHNOLOGY_GSM         = 1 << 1,
    MM_MODEM_ACCESS_TECHNOLOGY_GSM_COMPACT = 1 << 2,
    MM_MODEM_ACCESS_TECHNOLOGY_GPRS        = 1 << 3,
    MM_MODEM_ACCESS_TECHNOLOGY_EDGE        = 1 << 4,
    MM_MODEM_ACCESS_TECHNOLOGY_UMTS        = 1 << 5,
    MM_MODEM_ACCESS_TECHNOLOGY_HSDPA       = 1 << 6,
    MM_MODEM_ACCESS_TECHNOLOGY_HSUPA       = 1 << 7,
    MM_MODEM_ACCESS_TECHNOLOGY_HSPA        = 1 << 8,
    MM_MODEM_ACCESS_TECHNOLOGY_HSPA_PLUS   = 1 << 9,
    MM_MODEM_ACCESS_TECHNOLOGY_1XRTT       = 1 << 10,
    MM_MODEM_ACCESS_TECHNOLOGY_EVDO0       = 1 << 11,
    MM_MODEM_ACCESS_TECHNOLOGY_EVDOA       = 1 << 12,
    MM_MODEM_ACCESS_TECHNOLOGY_EVDOB       = 1 << 13,
    MM_MODEM_ACCESS_TECHNOLOGY_LTE         = 1 << 14,
    MM_MODEM_ACCESS_TECHNOLOGY_ANY         = 0xFFFFFFFF,
} MMModemAccessTechnology;

/**
 * MMModemMode:
 * @MM_MODEM_MODE_NONE: None.
 * @MM_MODEM_MODE_CS: CSD, GSM, and other circuit-switched technologies.
 * @MM_MODEM_MODE_2G: GPRS, EDGE.
 * @MM_MODEM_MODE_3G: UMTS, HSxPA.
 * @MM_MODEM_MODE_4G: LTE.
 * @MM_MODEM_MODE_ANY: Any mode can be used (only this value allowed for POTS modems).
 *
 * Bitfield to indicate which access modes are supported, allowed or
 * preferred in a given device.
 */
typedef enum { /*< underscore_name=mm_modem_mode >*/
    MM_MODEM_MODE_NONE = 0,
    MM_MODEM_MODE_CS   = 1 << 0,
    MM_MODEM_MODE_2G   = 1 << 1,
    MM_MODEM_MODE_3G   = 1 << 2,
    MM_MODEM_MODE_4G   = 1 << 3,
    MM_MODEM_MODE_ANY  = 0xFFFFFFFF
} MMModemMode;

/**
 * MMModemBand:
 * @MM_MODEM_BAND_UNKNOWN: Unknown or invalid band.
 * @MM_MODEM_BAND_EGSM: GSM/GPRS/EDGE 900 MHz.
 * @MM_MODEM_BAND_DCS: GSM/GPRS/EDGE 1800 MHz.
 * @MM_MODEM_BAND_PCS: GSM/GPRS/EDGE 1900 MHz.
 * @MM_MODEM_BAND_G850: GSM/GPRS/EDGE 850 MHz.
 * @MM_MODEM_BAND_U2100: WCDMA 2100 MHz (Class I).
 * @MM_MODEM_BAND_U1800: WCDMA 3GPP 1800 MHz (Class III).
 * @MM_MODEM_BAND_U17IV: WCDMA 3GPP AWS 1700/2100 MHz (Class IV).
 * @MM_MODEM_BAND_U800: WCDMA 3GPP UMTS 800 MHz (Class VI).
 * @MM_MODEM_BAND_U850: WCDMA 3GPP UMTS 850 MHz (Class V).
 * @MM_MODEM_BAND_U900: WCDMA 3GPP UMTS 900 MHz (Class VIII).
 * @MM_MODEM_BAND_U17IX: WCDMA 3GPP UMTS 1700 MHz (Class IX).
 * @MM_MODEM_BAND_U1900: WCDMA 3GPP UMTS 1900 MHz (Class II).
 * @MM_MODEM_BAND_U2600: WCDMA 3GPP UMTS 2600 MHz (Class VII, internal).
 * @MM_MODEM_BAND_EUTRAN_I: E-UTRAN band I.
 * @MM_MODEM_BAND_EUTRAN_II: E-UTRAN band II.
 * @MM_MODEM_BAND_EUTRAN_III: E-UTRAN band III.
 * @MM_MODEM_BAND_EUTRAN_IV: E-UTRAN band IV.
 * @MM_MODEM_BAND_EUTRAN_V: E-UTRAN band V.
 * @MM_MODEM_BAND_EUTRAN_VI: E-UTRAN band VI.
 * @MM_MODEM_BAND_EUTRAN_VII: E-UTRAN band VII.
 * @MM_MODEM_BAND_EUTRAN_VIII: E-UTRAN band VIII.
 * @MM_MODEM_BAND_EUTRAN_IX: E-UTRAN band IX.
 * @MM_MODEM_BAND_EUTRAN_X: E-UTRAN band X.
 * @MM_MODEM_BAND_EUTRAN_XI: E-UTRAN band XI.
 * @MM_MODEM_BAND_EUTRAN_XII: E-UTRAN band XII.
 * @MM_MODEM_BAND_EUTRAN_XIII: E-UTRAN band XIII.
 * @MM_MODEM_BAND_EUTRAN_XIV: E-UTRAN band XIV.
 * @MM_MODEM_BAND_EUTRAN_XVII: E-UTRAN band XVII.
 * @MM_MODEM_BAND_EUTRAN_XVIII: E-UTRAN band XVIII.
 * @MM_MODEM_BAND_EUTRAN_XIX: E-UTRAN band XIX.
 * @MM_MODEM_BAND_EUTRAN_XX: E-UTRAN band XX.
 * @MM_MODEM_BAND_EUTRAN_XXI: E-UTRAN band XXI.
 * @MM_MODEM_BAND_EUTRAN_XXII: E-UTRAN band XXII.
 * @MM_MODEM_BAND_EUTRAN_XXIII: E-UTRAN band XXIII.
 * @MM_MODEM_BAND_EUTRAN_XXIV: E-UTRAN band XXIV.
 * @MM_MODEM_BAND_EUTRAN_XXV: E-UTRAN band XXV.
 * @MM_MODEM_BAND_EUTRAN_XXVI: E-UTRAN band XXVI.
 * @MM_MODEM_BAND_EUTRAN_XXXIII: E-UTRAN band XXXIII.
 * @MM_MODEM_BAND_EUTRAN_XXXIV: E-UTRAN band XXXIV.
 * @MM_MODEM_BAND_EUTRAN_XXXV: E-UTRAN band XXXV.
 * @MM_MODEM_BAND_EUTRAN_XXXVI: E-UTRAN band XXXVI.
 * @MM_MODEM_BAND_EUTRAN_XXXVII: E-UTRAN band XXXVII.
 * @MM_MODEM_BAND_EUTRAN_XXXVIII: E-UTRAN band XXXVIII.
 * @MM_MODEM_BAND_EUTRAN_XXXIX: E-UTRAN band XXXIX.
 * @MM_MODEM_BAND_EUTRAN_XL: E-UTRAN band XL.
 * @MM_MODEM_BAND_EUTRAN_XLI: E-UTRAN band XLI.
 * @MM_MODEM_BAND_EUTRAN_XLII: E-UTRAN band XLII.
 * @MM_MODEM_BAND_EUTRAN_XLIII: E-UTRAN band XLIII.
 * @MM_MODEM_BAND_EUTRAN_XLIV: E-UTRAN band XLIV.
 * @MM_MODEM_BAND_CDMA_BC0_CELLULAR_800: CDMA Band Class 0 (US Cellular 850MHz).
 * @MM_MODEM_BAND_CDMA_BC1_PCS_1900: CDMA Band Class 1 (US PCS 1900MHz).
 * @MM_MODEM_BAND_CDMA_BC2_TACS: CDMA Band Class 2 (UK TACS 900MHz).
 * @MM_MODEM_BAND_CDMA_BC3_JTACS: CDMA Band Class 3 (Japanese TACS).
 * @MM_MODEM_BAND_CDMA_BC4_KOREAN_PCS: CDMA Band Class 4 (Korean PCS).
 * @MM_MODEM_BAND_CDMA_BC5_NMT450: CDMA Band Class 5 (NMT 450MHz).
 * @MM_MODEM_BAND_CDMA_BC6_IMT2000: CDMA Band Class 6 (IMT2000 2100MHz).
 * @MM_MODEM_BAND_CDMA_BC7_CELLULAR_700: CDMA Band Class 7 (Cellular 700MHz).
 * @MM_MODEM_BAND_CDMA_BC8_1800: CDMA Band Class 8 (1800MHz).
 * @MM_MODEM_BAND_CDMA_BC9_900: CDMA Band Class 9 (900MHz).
 * @MM_MODEM_BAND_CDMA_BC10_SECONDARY_800: CDMA Band Class 10 (US Secondary 800).
 * @MM_MODEM_BAND_CDMA_BC11_PAMR_400: CDMA Band Class 11 (European PAMR 400MHz).
 * @MM_MODEM_BAND_CDMA_BC12_PAMR_800: CDMA Band Class 12 (PAMR 800MHz).
 * @MM_MODEM_BAND_CDMA_BC13_IMT2000_2500: CDMA Band Class 13 (IMT2000 2500MHz Expansion).
 * @MM_MODEM_BAND_CDMA_BC14_PCS2_1900: CDMA Band Class 14 (More US PCS 1900MHz).
 * @MM_MODEM_BAND_CDMA_BC15_AWS: CDMA Band Class 15 (AWS 1700MHz).
 * @MM_MODEM_BAND_CDMA_BC16_US_2500: CDMA Band Class 16 (US 2500MHz).
 * @MM_MODEM_BAND_CDMA_BC17_US_FLO_2500: CDMA Band Class 17 (US 2500MHz Forward Link Only).
 * @MM_MODEM_BAND_CDMA_BC18_US_PS_700: CDMA Band Class 18 (US 700MHz Public Safety).
 * @MM_MODEM_BAND_CDMA_BC19_US_LOWER_700: CDMA Band Class 19 (US Lower 700MHz).
 * @MM_MODEM_BAND_ANY: For certain operations, allow the modem to select a band automatically.
 *
 * Radio bands supported by the device when connecting to a mobile network.
 */
typedef enum { /*< underscore_name=mm_modem_band >*/
    MM_MODEM_BAND_UNKNOWN = 0,
    /* GSM/UMTS bands */
    MM_MODEM_BAND_EGSM  = 1,
    MM_MODEM_BAND_DCS   = 2,
    MM_MODEM_BAND_PCS   = 3,
    MM_MODEM_BAND_G850  = 4,
    MM_MODEM_BAND_U2100 = 5,
    MM_MODEM_BAND_U1800 = 6,
    MM_MODEM_BAND_U17IV = 7,
    MM_MODEM_BAND_U800  = 8,
    MM_MODEM_BAND_U850  = 9,
    MM_MODEM_BAND_U900  = 10,
    MM_MODEM_BAND_U17IX = 11,
    MM_MODEM_BAND_U1900 = 12,
    MM_MODEM_BAND_U2600 = 13,
    /* LTE bands */
    MM_MODEM_BAND_EUTRAN_I       = 31,
    MM_MODEM_BAND_EUTRAN_II      = 32,
    MM_MODEM_BAND_EUTRAN_III     = 33,
    MM_MODEM_BAND_EUTRAN_IV      = 34,
    MM_MODEM_BAND_EUTRAN_V       = 35,
    MM_MODEM_BAND_EUTRAN_VI      = 36,
    MM_MODEM_BAND_EUTRAN_VII     = 37,
    MM_MODEM_BAND_EUTRAN_VIII    = 38,
    MM_MODEM_BAND_EUTRAN_IX      = 39,
    MM_MODEM_BAND_EUTRAN_X       = 40,
    MM_MODEM_BAND_EUTRAN_XI      = 41,
    MM_MODEM_BAND_EUTRAN_XII     = 42,
    MM_MODEM_BAND_EUTRAN_XIII    = 43,
    MM_MODEM_BAND_EUTRAN_XIV     = 44,
    MM_MODEM_BAND_EUTRAN_XVII    = 47,
    MM_MODEM_BAND_EUTRAN_XVIII   = 48,
    MM_MODEM_BAND_EUTRAN_XIX     = 49,
    MM_MODEM_BAND_EUTRAN_XX      = 50,
    MM_MODEM_BAND_EUTRAN_XXI     = 51,
    MM_MODEM_BAND_EUTRAN_XXII    = 52,
    MM_MODEM_BAND_EUTRAN_XXIII   = 53,
    MM_MODEM_BAND_EUTRAN_XXIV    = 54,
    MM_MODEM_BAND_EUTRAN_XXV     = 55,
    MM_MODEM_BAND_EUTRAN_XXVI    = 56,
    MM_MODEM_BAND_EUTRAN_XXXIII  = 63,
    MM_MODEM_BAND_EUTRAN_XXXIV   = 64,
    MM_MODEM_BAND_EUTRAN_XXXV    = 65,
    MM_MODEM_BAND_EUTRAN_XXXVI   = 66,
    MM_MODEM_BAND_EUTRAN_XXXVII  = 67,
    MM_MODEM_BAND_EUTRAN_XXXVIII = 68,
    MM_MODEM_BAND_EUTRAN_XXXIX   = 69,
    MM_MODEM_BAND_EUTRAN_XL      = 70,
    MM_MODEM_BAND_EUTRAN_XLI     = 71,
    MM_MODEM_BAND_EUTRAN_XLII    = 72,
    MM_MODEM_BAND_EUTRAN_XLIII   = 73,
    MM_MODEM_BAND_EUTRAN_XLIV    = 74,
    /* CDMA Band Classes (see 3GPP2 C.S0057-C) */
    MM_MODEM_BAND_CDMA_BC0_CELLULAR_800   = 128,
    MM_MODEM_BAND_CDMA_BC1_PCS_1900       = 129,
    MM_MODEM_BAND_CDMA_BC2_TACS           = 130,
    MM_MODEM_BAND_CDMA_BC3_JTACS          = 131,
    MM_MODEM_BAND_CDMA_BC4_KOREAN_PCS     = 132,
    MM_MODEM_BAND_CDMA_BC5_NMT450         = 134,
    MM_MODEM_BAND_CDMA_BC6_IMT2000        = 135,
    MM_MODEM_BAND_CDMA_BC7_CELLULAR_700   = 136,
    MM_MODEM_BAND_CDMA_BC8_1800           = 137,
    MM_MODEM_BAND_CDMA_BC9_900            = 138,
    MM_MODEM_BAND_CDMA_BC10_SECONDARY_800 = 139,
    MM_MODEM_BAND_CDMA_BC11_PAMR_400      = 140,
    MM_MODEM_BAND_CDMA_BC12_PAMR_800      = 141,
    MM_MODEM_BAND_CDMA_BC13_IMT2000_2500  = 142,
    MM_MODEM_BAND_CDMA_BC14_PCS2_1900     = 143,
    MM_MODEM_BAND_CDMA_BC15_AWS           = 144,
    MM_MODEM_BAND_CDMA_BC16_US_2500       = 145,
    MM_MODEM_BAND_CDMA_BC17_US_FLO_2500   = 146,
    MM_MODEM_BAND_CDMA_BC18_US_PS_700     = 147,
    MM_MODEM_BAND_CDMA_BC19_US_LOWER_700  = 148,
    /* All/Any */
    MM_MODEM_BAND_ANY = 256
} MMModemBand;

/**
 * MMModemPortType:
 * @MM_MODEM_PORT_TYPE_UNKNOWN: Unknown.
 * @MM_MODEM_PORT_TYPE_NET: Net port.
 * @MM_MODEM_PORT_TYPE_AT: AT port.
 * @MM_MODEM_PORT_TYPE_QCDM: QCDM port.
 * @MM_MODEM_PORT_TYPE_GPS: GPS port.
 * @MM_MODEM_PORT_TYPE_QMI: QMI port.
 * @MM_MODEM_PORT_TYPE_MBIM: MBIM port.
 *
 * Type of modem port.
 */
typedef enum { /*< underscore_name=mm_modem_port_type >*/
    MM_MODEM_PORT_TYPE_UNKNOWN = 1,
    MM_MODEM_PORT_TYPE_NET     = 2,
    MM_MODEM_PORT_TYPE_AT      = 3,
    MM_MODEM_PORT_TYPE_QCDM    = 4,
    MM_MODEM_PORT_TYPE_GPS     = 5,
    MM_MODEM_PORT_TYPE_QMI     = 6,
    MM_MODEM_PORT_TYPE_MBIM    = 7
} MMModemPortType;

/**
 * MMSmsPduType:
 * @MM_SMS_PDU_TYPE_UNKNOWN: Unknown type.
 * @MM_SMS_PDU_TYPE_DELIVER: 3GPP Mobile-Terminated (MT) message.
 * @MM_SMS_PDU_TYPE_SUBMIT: 3GPP Mobile-Originated (MO) message.
 * @MM_SMS_PDU_TYPE_STATUS_REPORT: 3GPP status report (MT).
 * @MM_SMS_PDU_TYPE_CDMA_DELIVER: 3GPP2 Mobile-Terminated (MT) message.
 * @MM_SMS_PDU_TYPE_CDMA_SUBMIT: 3GPP2 Mobile-Originated (MO) message.
 * @MM_SMS_PDU_TYPE_CDMA_CANCELLATION: 3GPP2 Cancellation (MO) message.
 * @MM_SMS_PDU_TYPE_CDMA_DELIVERY_ACKNOWLEDGEMENT: 3GPP2 Delivery Acknowledgement (MT) message.
 * @MM_SMS_PDU_TYPE_CDMA_USER_ACKNOWLEDGEMENT: 3GPP2 User Acknowledgement (MT or MO) message.
 * @MM_SMS_PDU_TYPE_CDMA_READ_ACKNOWLEDGEMENT: 3GPP2 Read Acknowledgement (MT or MO) message.
 *
 * Type of PDUs used in the SMS.
 */
typedef enum { /*< underscore_name=mm_sms_pdu_type >*/
    MM_SMS_PDU_TYPE_UNKNOWN = 0,
    MM_SMS_PDU_TYPE_DELIVER       = 1,
    MM_SMS_PDU_TYPE_SUBMIT        = 2,
    MM_SMS_PDU_TYPE_STATUS_REPORT = 3,
    MM_SMS_PDU_TYPE_CDMA_DELIVER                  = 32,
    MM_SMS_PDU_TYPE_CDMA_SUBMIT                   = 33,
    MM_SMS_PDU_TYPE_CDMA_CANCELLATION             = 34,
    MM_SMS_PDU_TYPE_CDMA_DELIVERY_ACKNOWLEDGEMENT = 35,
    MM_SMS_PDU_TYPE_CDMA_USER_ACKNOWLEDGEMENT     = 36,
    MM_SMS_PDU_TYPE_CDMA_READ_ACKNOWLEDGEMENT     = 37,
} MMSmsPduType;

/**
 * MMSmsState:
 * @MM_SMS_STATE_UNKNOWN: State unknown or not reportable.
 * @MM_SMS_STATE_STORED: The message has been neither received nor yet sent.
 * @MM_SMS_STATE_RECEIVING: The message is being received but is not yet complete.
 * @MM_SMS_STATE_RECEIVED: The message has been completely received.
 * @MM_SMS_STATE_SENDING: The message is queued for delivery.
 * @MM_SMS_STATE_SENT: The message was successfully sent.
 *
 * State of a given SMS.
 */
typedef enum { /*< underscore_name=mm_sms_state >*/
    MM_SMS_STATE_UNKNOWN   = 0,
    MM_SMS_STATE_STORED    = 1,
    MM_SMS_STATE_RECEIVING = 2,
    MM_SMS_STATE_RECEIVED  = 3,
    MM_SMS_STATE_SENDING   = 4,
    MM_SMS_STATE_SENT      = 5,
} MMSmsState;

/**
 * MMSmsDeliveryState:
 * @MM_SMS_DELIVERY_STATE_COMPLETED_RECEIVED: Delivery completed, message received by the SME.
 * @MM_SMS_DELIVERY_STATE_COMPLETED_FORWARDED_UNCONFIRMED: Forwarded by the SC to the SME but the SC is unable to confirm delivery.
 * @MM_SMS_DELIVERY_STATE_COMPLETED_REPLACED_BY_SC: Message replaced by the SC.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_ERROR_CONGESTION: Temporary error, congestion.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_ERROR_SME_BUSY: Temporary error, SME busy.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_ERROR_NO_RESPONSE_FROM_SME: Temporary error, no response from the SME.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_ERROR_SERVICE_REJECTED: Temporary error, service rejected.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_ERROR_QOS_NOT_AVAILABLE: Temporary error, QoS not available.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_ERROR_IN_SME: Temporary error in the SME.
 * @MM_SMS_DELIVERY_STATE_ERROR_REMOTE_PROCEDURE: Permanent remote procedure error.
 * @MM_SMS_DELIVERY_STATE_ERROR_INCOMPATIBLE_DESTINATION: Permanent error, incompatible destination.
 * @MM_SMS_DELIVERY_STATE_ERROR_CONNECTION_REJECTED: Permanent error, connection rejected by the SME.
 * @MM_SMS_DELIVERY_STATE_ERROR_NOT_OBTAINABLE: Permanent error, not obtainable.
 * @MM_SMS_DELIVERY_STATE_ERROR_QOS_NOT_AVAILABLE: Permanent error, QoS not available.
 * @MM_SMS_DELIVERY_STATE_ERROR_NO_INTERWORKING_AVAILABLE: Permanent error, no interworking available.
 * @MM_SMS_DELIVERY_STATE_ERROR_VALIDITY_PERIOD_EXPIRED: Permanent error, message validity period expired.
 * @MM_SMS_DELIVERY_STATE_ERROR_DELETED_BY_ORIGINATING_SME: Permanent error, deleted by originating SME.
 * @MM_SMS_DELIVERY_STATE_ERROR_DELETED_BY_SC_ADMINISTRATION: Permanent error, deleted by SC administration.
 * @MM_SMS_DELIVERY_STATE_ERROR_MESSAGE_DOES_NOT_EXIST: Permanent error, message does no longer exist.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_FATAL_ERROR_CONGESTION: Permanent error, congestion.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_FATAL_ERROR_SME_BUSY: Permanent error, SME busy.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_FATAL_ERROR_NO_RESPONSE_FROM_SME: Permanent error, no response from the SME.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_FATAL_ERROR_SERVICE_REJECTED: Permanent error, service rejected.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_FATAL_ERROR_QOS_NOT_AVAILABLE: Permanent error, QoS not available.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_FATAL_ERROR_IN_SME: Permanent error in SME.
 * @MM_SMS_DELIVERY_STATE_UNKNOWN: Unknown state.
 * @MM_SMS_DELIVERY_STATE_NETWORK_PROBLEM_ADDRESS_VACANT: Permanent error in network, address vacant.
 * @MM_SMS_DELIVERY_STATE_NETWORK_PROBLEM_ADDRESS_TRANSLATION_FAILURE: Permanent error in network, address translation failure.
 * @MM_SMS_DELIVERY_STATE_NETWORK_PROBLEM_NETWORK_RESOURCE_OUTAGE: Permanent error in network, network resource outage.
 * @MM_SMS_DELIVERY_STATE_NETWORK_PROBLEM_NETWORK_FAILURE: Permanent error in network, network failure.
 * @MM_SMS_DELIVERY_STATE_NETWORK_PROBLEM_INVALID_TELESERVICE_ID: Permanent error in network, invalid teleservice id.
 * @MM_SMS_DELIVERY_STATE_NETWORK_PROBLEM_OTHER: Permanent error, other network problem.
 * @MM_SMS_DELIVERY_STATE_TERMINAL_PROBLEM_NO_PAGE_RESPONSE: Permanent error in terminal, no page response.
 * @MM_SMS_DELIVERY_STATE_TERMINAL_PROBLEM_DESTINATION_BUSY: Permanent error in terminal, destination busy.
 * @MM_SMS_DELIVERY_STATE_TERMINAL_PROBLEM_NO_ACKNOWLEDGMENT: Permanent error in terminal, no acknowledgement.
 * @MM_SMS_DELIVERY_STATE_TERMINAL_PROBLEM_DESTINATION_RESOURCE_SHORTAGE: Permanent error in terminal, destination resource shortage.
 * @MM_SMS_DELIVERY_STATE_TERMINAL_PROBLEM_SMS_DELIVERY_POSTPONED: Permanent error in terminal, SMS delivery postponed.
 * @MM_SMS_DELIVERY_STATE_TERMINAL_PROBLEM_DESTINATION_OUT_OF_SERVICE: Permanent error in terminal, destination out of service.
 * @MM_SMS_DELIVERY_STATE_TERMINAL_PROBLEM_DESTINATION_NO_LONGER_AT_THIS_ADDRESS: Permanent error in terminal, destination no longer at this address.
 * @MM_SMS_DELIVERY_STATE_TERMINAL_PROBLEM_OTHER: Permanent error, other terminal problem.
 * @MM_SMS_DELIVERY_STATE_RADIO_INTERFACE_PROBLEM_RESOURCE_SHORTAGE: Permanent error in radio interface, resource shortage.
 * @MM_SMS_DELIVERY_STATE_RADIO_INTERFACE_PROBLEM_INCOMPATIBILITY: Permanent error in radio interface, problem incompatibility.
 * @MM_SMS_DELIVERY_STATE_RADIO_INTERFACE_PROBLEM_OTHER: Permanent error, other radio interface problem.
 * @MM_SMS_DELIVERY_STATE_GENERAL_PROBLEM_ENCODING: Permanent error, encoding.
 * @MM_SMS_DELIVERY_STATE_GENERAL_PROBLEM_SMS_ORIGINATION_DENIED: Permanent error, SMS origination denied.
 * @MM_SMS_DELIVERY_STATE_GENERAL_PROBLEM_SMS_TERMINATION_DENIED: Permanent error, SMS termination denied.
 * @MM_SMS_DELIVERY_STATE_GENERAL_PROBLEM_SUPPLEMENTARY_SERVICE_NOT_SUPPORTED: Permanent error, supplementary service not supported.
 * @MM_SMS_DELIVERY_STATE_GENERAL_PROBLEM_SMS_NOT_SUPPORTED: Permanent error, SMS not supported.
 * @MM_SMS_DELIVERY_STATE_GENERAL_PROBLEM_MISSING_EXPECTED_PARAMETER: Permanent error, missing expected parameter.
 * @MM_SMS_DELIVERY_STATE_GENERAL_PROBLEM_MISSING_MANDATORY_PARAMETER: Permanent error, missing mandatory parameter.
 * @MM_SMS_DELIVERY_STATE_GENERAL_PROBLEM_UNRECOGNIZED_PARAMETER_VALUE: Permanent error, unrecognized parameter value.
 * @MM_SMS_DELIVERY_STATE_GENERAL_PROBLEM_UNEXPECTED_PARAMETER_VALUE: Permanent error, unexpected parameter value.
 * @MM_SMS_DELIVERY_STATE_GENERAL_PROBLEM_USER_DATA_SIZE_ERROR: Permanent error, user data size error.
 * @MM_SMS_DELIVERY_STATE_GENERAL_PROBLEM_OTHER: Permanent error, other general problem.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_NETWORK_PROBLEM_ADDRESS_VACANT: Temporary error in network, address vacant.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_NETWORK_PROBLEM_ADDRESS_TRANSLATION_FAILURE: Temporary error in network, address translation failure.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_NETWORK_PROBLEM_NETWORK_RESOURCE_OUTAGE: Temporary error in network, network resource outage.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_NETWORK_PROBLEM_NETWORK_FAILURE: Temporary error in network, network failure.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_NETWORK_PROBLEM_INVALID_TELESERVICE_ID: Temporary error in network, invalid teleservice id.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_NETWORK_PROBLEM_OTHER: Temporary error, other network problem.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_TERMINAL_PROBLEM_NO_PAGE_RESPONSE: Temporary error in terminal, no page response.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_TERMINAL_PROBLEM_DESTINATION_BUSY: Temporary error in terminal, destination busy.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_TERMINAL_PROBLEM_NO_ACKNOWLEDGMENT: Temporary error in terminal, no acknowledgement.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_TERMINAL_PROBLEM_DESTINATION_RESOURCE_SHORTAGE: Temporary error in terminal, destination resource shortage.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_TERMINAL_PROBLEM_SMS_DELIVERY_POSTPONED: Temporary error in terminal, SMS delivery postponed.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_TERMINAL_PROBLEM_DESTINATION_OUT_OF_SERVICE: Temporary error in terminal, destination out of service.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_TERMINAL_PROBLEM_DESTINATION_NO_LONGER_AT_THIS_ADDRESS: Temporary error in terminal, destination no longer at this address.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_TERMINAL_PROBLEM_OTHER: Temporary error, other terminal problem.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_RADIO_INTERFACE_PROBLEM_RESOURCE_SHORTAGE: Temporary error in radio interface, resource shortage.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_RADIO_INTERFACE_PROBLEM_INCOMPATIBILITY: Temporary error in radio interface, problem incompatibility.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_RADIO_INTERFACE_PROBLEM_OTHER: Temporary error, other radio interface problem.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_GENERAL_PROBLEM_ENCODING: Temporary error, encoding.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_GENERAL_PROBLEM_SMS_ORIGINATION_DENIED: Temporary error, SMS origination denied.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_GENERAL_PROBLEM_SMS_TERMINATION_DENIED: Temporary error, SMS termination denied.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_GENERAL_PROBLEM_SUPPLEMENTARY_SERVICE_NOT_SUPPORTED: Temporary error, supplementary service not supported.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_GENERAL_PROBLEM_SMS_NOT_SUPPORTED: Temporary error, SMS not supported.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_GENERAL_PROBLEM_MISSING_EXPECTED_PARAMETER: Temporary error, missing expected parameter.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_GENERAL_PROBLEM_MISSING_MANDATORY_PARAMETER: Temporary error, missing mandatory parameter.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_GENERAL_PROBLEM_UNRECOGNIZED_PARAMETER_VALUE: Temporary error, unrecognized parameter value.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_GENERAL_PROBLEM_UNEXPECTED_PARAMETER_VALUE: Temporary error, unexpected parameter value.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_GENERAL_PROBLEM_USER_DATA_SIZE_ERROR: Temporary error, user data size error.
 * @MM_SMS_DELIVERY_STATE_TEMPORARY_GENERAL_PROBLEM_OTHER: Temporary error, other general problem.
 *
 * Enumeration of known SMS delivery states as defined in 3GPP TS 03.40 and
 * 3GPP2 N.S0005-O, section 6.5.2.125.
 *
 * States out of the known ranges may also be valid (either reserved or SC-specific).
 */
typedef enum { /*< underscore_name=mm_sms_delivery_state >*/
    /* --------------- 3GPP specific errors ---------------------- */

    /* Completed deliveries */
    MM_SMS_DELIVERY_STATE_COMPLETED_RECEIVED              = 0x00,
    MM_SMS_DELIVERY_STATE_COMPLETED_FORWARDED_UNCONFIRMED = 0x01,
    MM_SMS_DELIVERY_STATE_COMPLETED_REPLACED_BY_SC        = 0x02,

    /* Temporary failures */
    MM_SMS_DELIVERY_STATE_TEMPORARY_ERROR_CONGESTION           = 0x20,
    MM_SMS_DELIVERY_STATE_TEMPORARY_ERROR_SME_BUSY             = 0x21,
    MM_SMS_DELIVERY_STATE_TEMPORARY_ERROR_NO_RESPONSE_FROM_SME = 0x22,
    MM_SMS_DELIVERY_STATE_TEMPORARY_ERROR_SERVICE_REJECTED     = 0x23,
    MM_SMS_DELIVERY_STATE_TEMPORARY_ERROR_QOS_NOT_AVAILABLE    = 0x24,
    MM_SMS_DELIVERY_STATE_TEMPORARY_ERROR_IN_SME               = 0x25,

    /* Permanent failures */
    MM_SMS_DELIVERY_STATE_ERROR_REMOTE_PROCEDURE             = 0x40,
    MM_SMS_DELIVERY_STATE_ERROR_INCOMPATIBLE_DESTINATION     = 0x41,
    MM_SMS_DELIVERY_STATE_ERROR_CONNECTION_REJECTED          = 0x42,
    MM_SMS_DELIVERY_STATE_ERROR_NOT_OBTAINABLE               = 0x43,
    MM_SMS_DELIVERY_STATE_ERROR_QOS_NOT_AVAILABLE            = 0x44,
    MM_SMS_DELIVERY_STATE_ERROR_NO_INTERWORKING_AVAILABLE    = 0x45,
    MM_SMS_DELIVERY_STATE_ERROR_VALIDITY_PERIOD_EXPIRED      = 0x46,
    MM_SMS_DELIVERY_STATE_ERROR_DELETED_BY_ORIGINATING_SME   = 0x47,
    MM_SMS_DELIVERY_STATE_ERROR_DELETED_BY_SC_ADMINISTRATION = 0x48,
    MM_SMS_DELIVERY_STATE_ERROR_MESSAGE_DOES_NOT_EXIST       = 0x49,

    /* Temporary failures that became permanent */
    MM_SMS_DELIVERY_STATE_TEMPORARY_FATAL_ERROR_CONGESTION           = 0x60,
    MM_SMS_DELIVERY_STATE_TEMPORARY_FATAL_ERROR_SME_BUSY             = 0x61,
    MM_SMS_DELIVERY_STATE_TEMPORARY_FATAL_ERROR_NO_RESPONSE_FROM_SME = 0x62,
    MM_SMS_DELIVERY_STATE_TEMPORARY_FATAL_ERROR_SERVICE_REJECTED     = 0x63,
    MM_SMS_DELIVERY_STATE_TEMPORARY_FATAL_ERROR_QOS_NOT_AVAILABLE    = 0x64,
    MM_SMS_DELIVERY_STATE_TEMPORARY_FATAL_ERROR_IN_SME               = 0x65,

    /* Unknown, out of any possible valid value [0x00-0xFF] */
    MM_SMS_DELIVERY_STATE_UNKNOWN = 0x100,

    /* --------------- 3GPP2 specific errors ---------------------- */

    /* Network problems */
    MM_SMS_DELIVERY_STATE_NETWORK_PROBLEM_ADDRESS_VACANT              = 0x200,
    MM_SMS_DELIVERY_STATE_NETWORK_PROBLEM_ADDRESS_TRANSLATION_FAILURE = 0x201,
    MM_SMS_DELIVERY_STATE_NETWORK_PROBLEM_NETWORK_RESOURCE_OUTAGE     = 0x202,
    MM_SMS_DELIVERY_STATE_NETWORK_PROBLEM_NETWORK_FAILURE             = 0x203,
    MM_SMS_DELIVERY_STATE_NETWORK_PROBLEM_INVALID_TELESERVICE_ID      = 0x204,
    MM_SMS_DELIVERY_STATE_NETWORK_PROBLEM_OTHER                       = 0x205,
    /* Terminal problems */
    MM_SMS_DELIVERY_STATE_TERMINAL_PROBLEM_NO_PAGE_RESPONSE                      = 0x220,
    MM_SMS_DELIVERY_STATE_TERMINAL_PROBLEM_DESTINATION_BUSY                      = 0x221,
    MM_SMS_DELIVERY_STATE_TERMINAL_PROBLEM_NO_ACKNOWLEDGMENT                     = 0x222,
    MM_SMS_DELIVERY_STATE_TERMINAL_PROBLEM_DESTINATION_RESOURCE_SHORTAGE         = 0x223,
    MM_SMS_DELIVERY_STATE_TERMINAL_PROBLEM_SMS_DELIVERY_POSTPONED                = 0x224,
    MM_SMS_DELIVERY_STATE_TERMINAL_PROBLEM_DESTINATION_OUT_OF_SERVICE            = 0x225,
    MM_SMS_DELIVERY_STATE_TERMINAL_PROBLEM_DESTINATION_NO_LONGER_AT_THIS_ADDRESS = 0x226,
    MM_SMS_DELIVERY_STATE_TERMINAL_PROBLEM_OTHER                                 = 0x227,
    /* Radio problems */
    MM_SMS_DELIVERY_STATE_RADIO_INTERFACE_PROBLEM_RESOURCE_SHORTAGE = 0x240,
    MM_SMS_DELIVERY_STATE_RADIO_INTERFACE_PROBLEM_INCOMPATIBILITY   = 0x241,
    MM_SMS_DELIVERY_STATE_RADIO_INTERFACE_PROBLEM_OTHER             = 0x242,
    /* General problems */
    MM_SMS_DELIVERY_STATE_GENERAL_PROBLEM_ENCODING                            = 0x260,
    MM_SMS_DELIVERY_STATE_GENERAL_PROBLEM_SMS_ORIGINATION_DENIED              = 0x261,
    MM_SMS_DELIVERY_STATE_GENERAL_PROBLEM_SMS_TERMINATION_DENIED              = 0x262,
    MM_SMS_DELIVERY_STATE_GENERAL_PROBLEM_SUPPLEMENTARY_SERVICE_NOT_SUPPORTED = 0x263,
    MM_SMS_DELIVERY_STATE_GENERAL_PROBLEM_SMS_NOT_SUPPORTED                   = 0x264,
    MM_SMS_DELIVERY_STATE_GENERAL_PROBLEM_MISSING_EXPECTED_PARAMETER          = 0x266,
    MM_SMS_DELIVERY_STATE_GENERAL_PROBLEM_MISSING_MANDATORY_PARAMETER         = 0x267,
    MM_SMS_DELIVERY_STATE_GENERAL_PROBLEM_UNRECOGNIZED_PARAMETER_VALUE        = 0x268,
    MM_SMS_DELIVERY_STATE_GENERAL_PROBLEM_UNEXPECTED_PARAMETER_VALUE          = 0x269,
    MM_SMS_DELIVERY_STATE_GENERAL_PROBLEM_USER_DATA_SIZE_ERROR                = 0x26A,
    MM_SMS_DELIVERY_STATE_GENERAL_PROBLEM_OTHER                               = 0x26B,

    /* Temporary network problems */
    MM_SMS_DELIVERY_STATE_TEMPORARY_NETWORK_PROBLEM_ADDRESS_VACANT              = 0x300,
    MM_SMS_DELIVERY_STATE_TEMPORARY_NETWORK_PROBLEM_ADDRESS_TRANSLATION_FAILURE = 0x301,
    MM_SMS_DELIVERY_STATE_TEMPORARY_NETWORK_PROBLEM_NETWORK_RESOURCE_OUTAGE     = 0x302,
    MM_SMS_DELIVERY_STATE_TEMPORARY_NETWORK_PROBLEM_NETWORK_FAILURE             = 0x303,
    MM_SMS_DELIVERY_STATE_TEMPORARY_NETWORK_PROBLEM_INVALID_TELESERVICE_ID      = 0x304,
    MM_SMS_DELIVERY_STATE_TEMPORARY_NETWORK_PROBLEM_OTHER                       = 0x305,
    /* Temporary terminal problems */
    MM_SMS_DELIVERY_STATE_TEMPORARY_TERMINAL_PROBLEM_NO_PAGE_RESPONSE                      = 0x320,
    MM_SMS_DELIVERY_STATE_TEMPORARY_TERMINAL_PROBLEM_DESTINATION_BUSY                      = 0x321,
    MM_SMS_DELIVERY_STATE_TEMPORARY_TERMINAL_PROBLEM_NO_ACKNOWLEDGMENT                     = 0x322,
    MM_SMS_DELIVERY_STATE_TEMPORARY_TERMINAL_PROBLEM_DESTINATION_RESOURCE_SHORTAGE         = 0x323,
    MM_SMS_DELIVERY_STATE_TEMPORARY_TERMINAL_PROBLEM_SMS_DELIVERY_POSTPONED                = 0x324,
    MM_SMS_DELIVERY_STATE_TEMPORARY_TERMINAL_PROBLEM_DESTINATION_OUT_OF_SERVICE            = 0x325,
    MM_SMS_DELIVERY_STATE_TEMPORARY_TERMINAL_PROBLEM_DESTINATION_NO_LONGER_AT_THIS_ADDRESS = 0x326,
    MM_SMS_DELIVERY_STATE_TEMPORARY_TERMINAL_PROBLEM_OTHER                                 = 0x327,
    /* Temporary radio problems */
    MM_SMS_DELIVERY_STATE_TEMPORARY_RADIO_INTERFACE_PROBLEM_RESOURCE_SHORTAGE = 0x340,
    MM_SMS_DELIVERY_STATE_TEMPORARY_RADIO_INTERFACE_PROBLEM_INCOMPATIBILITY   = 0x341,
    MM_SMS_DELIVERY_STATE_TEMPORARY_RADIO_INTERFACE_PROBLEM_OTHER             = 0x342,
    /* Temporary general problems */
    MM_SMS_DELIVERY_STATE_TEMPORARY_GENERAL_PROBLEM_ENCODING                            = 0x360,
    MM_SMS_DELIVERY_STATE_TEMPORARY_GENERAL_PROBLEM_SMS_ORIGINATION_DENIED              = 0x361,
    MM_SMS_DELIVERY_STATE_TEMPORARY_GENERAL_PROBLEM_SMS_TERMINATION_DENIED              = 0x362,
    MM_SMS_DELIVERY_STATE_TEMPORARY_GENERAL_PROBLEM_SUPPLEMENTARY_SERVICE_NOT_SUPPORTED = 0x363,
    MM_SMS_DELIVERY_STATE_TEMPORARY_GENERAL_PROBLEM_SMS_NOT_SUPPORTED                   = 0x364,
    MM_SMS_DELIVERY_STATE_TEMPORARY_GENERAL_PROBLEM_MISSING_EXPECTED_PARAMETER          = 0x366,
    MM_SMS_DELIVERY_STATE_TEMPORARY_GENERAL_PROBLEM_MISSING_MANDATORY_PARAMETER         = 0x367,
    MM_SMS_DELIVERY_STATE_TEMPORARY_GENERAL_PROBLEM_UNRECOGNIZED_PARAMETER_VALUE        = 0x368,
    MM_SMS_DELIVERY_STATE_TEMPORARY_GENERAL_PROBLEM_UNEXPECTED_PARAMETER_VALUE          = 0x369,
    MM_SMS_DELIVERY_STATE_TEMPORARY_GENERAL_PROBLEM_USER_DATA_SIZE_ERROR                = 0x36A,
    MM_SMS_DELIVERY_STATE_TEMPORARY_GENERAL_PROBLEM_OTHER                               = 0x36B,
} MMSmsDeliveryState;

/**
 * MMSmsStorage:
 * @MM_SMS_STORAGE_UNKNOWN: Storage unknown.
 * @MM_SMS_STORAGE_SM: SIM card storage area.
 * @MM_SMS_STORAGE_ME: Mobile equipment storage area.
 * @MM_SMS_STORAGE_MT: Sum of SIM and Mobile equipment storages
 * @MM_SMS_STORAGE_SR: Status report message storage area.
 * @MM_SMS_STORAGE_BM: Broadcast message storage area.
 * @MM_SMS_STORAGE_TA: Terminal adaptor message storage area.
 *
 * Storage for SMS messages.
 */
typedef enum { /*< underscore_name=mm_sms_storage >*/
    MM_SMS_STORAGE_UNKNOWN = 0,
    MM_SMS_STORAGE_SM      = 1,
    MM_SMS_STORAGE_ME      = 2,
    MM_SMS_STORAGE_MT      = 3,
    MM_SMS_STORAGE_SR      = 4,
    MM_SMS_STORAGE_BM      = 5,
    MM_SMS_STORAGE_TA      = 6,
} MMSmsStorage;

/**
 * MMSmsValidityType:
 * @MM_SMS_VALIDITY_TYPE_UNKNOWN: Validity type unknown.
 * @MM_SMS_VALIDITY_TYPE_RELATIVE: Relative validity.
 * @MM_SMS_VALIDITY_TYPE_ABSOLUTE: Absolute validity.
 * @MM_SMS_VALIDITY_TYPE_ENHANCED: Enhanced validity.
 *
 * Type of SMS validity value.
 */
typedef enum { /*< underscore_name=mm_sms_validity_type >*/
    MM_SMS_VALIDITY_TYPE_UNKNOWN  = 0,
    MM_SMS_VALIDITY_TYPE_RELATIVE = 1,
    MM_SMS_VALIDITY_TYPE_ABSOLUTE = 2,
    MM_SMS_VALIDITY_TYPE_ENHANCED = 3,
} MMSmsValidityType;

/**
 * MMSmsCdmaTeleserviceId:
 * @MM_SMS_CDMA_TELESERVICE_ID_UNKNOWN: Unknown.
 * @MM_SMS_CDMA_TELESERVICE_ID_CMT91: IS-91 Extended Protocol Enhanced Services.
 * @MM_SMS_CDMA_TELESERVICE_ID_WPT: Wireless Paging Teleservice.
 * @MM_SMS_CDMA_TELESERVICE_ID_WMT: Wireless Messaging Teleservice.
 * @MM_SMS_CDMA_TELESERVICE_ID_VMN: Voice Mail Notification.
 * @MM_SMS_CDMA_TELESERVICE_ID_WAP: Wireless Application Protocol.
 * @MM_SMS_CDMA_TELESERVICE_ID_WEMT: Wireless Enhanced Messaging Teleservice.
 * @MM_SMS_CDMA_TELESERVICE_ID_SCPT: Service Category Programming Teleservice.
 * @MM_SMS_CDMA_TELESERVICE_ID_CATPT: Card Application Toolkit Protocol Teleservice.
 *
 * Teleservice IDs supported for CDMA SMS, as defined in 3GPP2 X.S0004-550-E
 * (section 2.256) and 3GPP2 C.S0015-B (section 3.4.3.1).
 */
typedef enum { /*< underscore_name=mm_sms_cdma_teleservice_id >*/
    MM_SMS_CDMA_TELESERVICE_ID_UNKNOWN = 0x0000,
    MM_SMS_CDMA_TELESERVICE_ID_CMT91   = 0x1000,
    MM_SMS_CDMA_TELESERVICE_ID_WPT     = 0x1001,
    MM_SMS_CDMA_TELESERVICE_ID_WMT     = 0x1002,
    MM_SMS_CDMA_TELESERVICE_ID_VMN     = 0x1003,
    MM_SMS_CDMA_TELESERVICE_ID_WAP     = 0x1004,
    MM_SMS_CDMA_TELESERVICE_ID_WEMT    = 0x1005,
    MM_SMS_CDMA_TELESERVICE_ID_SCPT    = 0x1006,
    MM_SMS_CDMA_TELESERVICE_ID_CATPT   = 0x1007,
} MMSmsCdmaTeleserviceId;

/**
 * MMSmsCdmaServiceCategory:
 * @MM_SMS_CDMA_SERVICE_CATEGORY_UNKNOWN: Unknown.
 * @MM_SMS_CDMA_SERVICE_CATEGORY_EMERGENCY_BROADCAST: Emergency broadcast.
 * @MM_SMS_CDMA_SERVICE_CATEGORY_ADMINISTRATIVE: Administrative.
 * @MM_SMS_CDMA_SERVICE_CATEGORY_MAINTENANCE: Maintenance.
 * @MM_SMS_CDMA_SERVICE_CATEGORY_GENERAL_NEWS_LOCAL: General news (local).
 * @MM_SMS_CDMA_SERVICE_CATEGORY_GENERAL_NEWS_REGIONAL: General news (regional).
 * @MM_SMS_CDMA_SERVICE_CATEGORY_GENERAL_NEWS_NATIONAL: General news (national).
 * @MM_SMS_CDMA_SERVICE_CATEGORY_GENERAL_NEWS_INTERNATIONAL: General news (international).
 * @MM_SMS_CDMA_SERVICE_CATEGORY_BUSINESS_NEWS_LOCAL: Business/Financial news (local).
 * @MM_SMS_CDMA_SERVICE_CATEGORY_BUSINESS_NEWS_REGIONAL: Business/Financial news (regional).
 * @MM_SMS_CDMA_SERVICE_CATEGORY_BUSINESS_NEWS_NATIONAL: Business/Financial news (national).
 * @MM_SMS_CDMA_SERVICE_CATEGORY_BUSINESS_NEWS_INTERNATIONAL: Business/Financial news (international).
 * @MM_SMS_CDMA_SERVICE_CATEGORY_SPORTS_NEWS_LOCAL: Sports news (local).
 * @MM_SMS_CDMA_SERVICE_CATEGORY_SPORTS_NEWS_REGIONAL: Sports news (regional).
 * @MM_SMS_CDMA_SERVICE_CATEGORY_SPORTS_NEWS_NATIONAL: Sports news (national).
 * @MM_SMS_CDMA_SERVICE_CATEGORY_SPORTS_NEWS_INTERNATIONAL: Sports news (international).
 * @MM_SMS_CDMA_SERVICE_CATEGORY_ENTERTAINMENT_NEWS_LOCAL: Entertainment news (local).
 * @MM_SMS_CDMA_SERVICE_CATEGORY_ENTERTAINMENT_NEWS_REGIONAL: Entertainment news (regional).
 * @MM_SMS_CDMA_SERVICE_CATEGORY_ENTERTAINMENT_NEWS_NATIONAL: Entertainment news (national).
 * @MM_SMS_CDMA_SERVICE_CATEGORY_ENTERTAINMENT_NEWS_INTERNATIONAL: Entertainment news (international).
 * @MM_SMS_CDMA_SERVICE_CATEGORY_LOCAL_WEATHER: Local weather.
 * @MM_SMS_CDMA_SERVICE_CATEGORY_TRAFFIC_REPORT: Area traffic report.
 * @MM_SMS_CDMA_SERVICE_CATEGORY_FLIGHT_SCHEDULES: Local airport flight schedules.
 * @MM_SMS_CDMA_SERVICE_CATEGORY_RESTAURANTS: Restaurants.
 * @MM_SMS_CDMA_SERVICE_CATEGORY_LODGINGS: Lodgings.
 * @MM_SMS_CDMA_SERVICE_CATEGORY_RETAIL_DIRECTORY: Retail directory.
 * @MM_SMS_CDMA_SERVICE_CATEGORY_ADVERTISEMENTS: Advertisements.
 * @MM_SMS_CDMA_SERVICE_CATEGORY_STOCK_QUOTES: Stock quotes.
 * @MM_SMS_CDMA_SERVICE_CATEGORY_EMPLOYMENT: Employment.
 * @MM_SMS_CDMA_SERVICE_CATEGORY_HOSPITALS: Medical / Health / Hospitals.
 * @MM_SMS_CDMA_SERVICE_CATEGORY_TECHNOLOGY_NEWS: Technology news.
 * @MM_SMS_CDMA_SERVICE_CATEGORY_MULTICATEGORY: Multi-category.
 * @MM_SMS_CDMA_SERVICE_CATEGORY_CMAS_PRESIDENTIAL_ALERT: Presidential alert.
 * @MM_SMS_CDMA_SERVICE_CATEGORY_CMAS_EXTREME_THREAT: Extreme threat.
 * @MM_SMS_CDMA_SERVICE_CATEGORY_CMAS_SEVERE_THREAT: Severe threat.
 * @MM_SMS_CDMA_SERVICE_CATEGORY_CMAS_CHILD_ABDUCTION_EMERGENCY: Child abduction emergency.
 * @MM_SMS_CDMA_SERVICE_CATEGORY_CMAS_TEST: CMAS test.
 *
 * Service category for CDMA SMS, as defined in 3GPP2 C.R1001-D (section 9.3).
 */
typedef enum { /*< underscore_name=mm_sms_cdma_service_category >*/
    MM_SMS_CDMA_SERVICE_CATEGORY_UNKNOWN                          = 0x0000,
    MM_SMS_CDMA_SERVICE_CATEGORY_EMERGENCY_BROADCAST              = 0x0001,
    MM_SMS_CDMA_SERVICE_CATEGORY_ADMINISTRATIVE                   = 0x0002,
    MM_SMS_CDMA_SERVICE_CATEGORY_MAINTENANCE                      = 0x0003,
    MM_SMS_CDMA_SERVICE_CATEGORY_GENERAL_NEWS_LOCAL               = 0x0004,
    MM_SMS_CDMA_SERVICE_CATEGORY_GENERAL_NEWS_REGIONAL            = 0x0005,
    MM_SMS_CDMA_SERVICE_CATEGORY_GENERAL_NEWS_NATIONAL            = 0x0006,
    MM_SMS_CDMA_SERVICE_CATEGORY_GENERAL_NEWS_INTERNATIONAL       = 0x0007,
    MM_SMS_CDMA_SERVICE_CATEGORY_BUSINESS_NEWS_LOCAL              = 0x0008,
    MM_SMS_CDMA_SERVICE_CATEGORY_BUSINESS_NEWS_REGIONAL           = 0x0009,
    MM_SMS_CDMA_SERVICE_CATEGORY_BUSINESS_NEWS_NATIONAL           = 0x000A,
    MM_SMS_CDMA_SERVICE_CATEGORY_BUSINESS_NEWS_INTERNATIONAL      = 0x000B,
    MM_SMS_CDMA_SERVICE_CATEGORY_SPORTS_NEWS_LOCAL                = 0x000C,
    MM_SMS_CDMA_SERVICE_CATEGORY_SPORTS_NEWS_REGIONAL             = 0x000D,
    MM_SMS_CDMA_SERVICE_CATEGORY_SPORTS_NEWS_NATIONAL             = 0x000E,
    MM_SMS_CDMA_SERVICE_CATEGORY_SPORTS_NEWS_INTERNATIONAL        = 0x000F,
    MM_SMS_CDMA_SERVICE_CATEGORY_ENTERTAINMENT_NEWS_LOCAL         = 0x0010,
    MM_SMS_CDMA_SERVICE_CATEGORY_ENTERTAINMENT_NEWS_REGIONAL      = 0x0011,
    MM_SMS_CDMA_SERVICE_CATEGORY_ENTERTAINMENT_NEWS_NATIONAL      = 0x0012,
    MM_SMS_CDMA_SERVICE_CATEGORY_ENTERTAINMENT_NEWS_INTERNATIONAL = 0x0013,
    MM_SMS_CDMA_SERVICE_CATEGORY_LOCAL_WEATHER                    = 0x0014,
    MM_SMS_CDMA_SERVICE_CATEGORY_TRAFFIC_REPORT                   = 0x0015,
    MM_SMS_CDMA_SERVICE_CATEGORY_FLIGHT_SCHEDULES                 = 0x0016,
    MM_SMS_CDMA_SERVICE_CATEGORY_RESTAURANTS                      = 0x0017,
    MM_SMS_CDMA_SERVICE_CATEGORY_LODGINGS                         = 0x0018,
    MM_SMS_CDMA_SERVICE_CATEGORY_RETAIL_DIRECTORY                 = 0x0019,
    MM_SMS_CDMA_SERVICE_CATEGORY_ADVERTISEMENTS                   = 0x001A,
    MM_SMS_CDMA_SERVICE_CATEGORY_STOCK_QUOTES                     = 0x001B,
    MM_SMS_CDMA_SERVICE_CATEGORY_EMPLOYMENT                       = 0x001C,
    MM_SMS_CDMA_SERVICE_CATEGORY_HOSPITALS                        = 0x001D,
    MM_SMS_CDMA_SERVICE_CATEGORY_TECHNOLOGY_NEWS                  = 0x001E,
    MM_SMS_CDMA_SERVICE_CATEGORY_MULTICATEGORY                    = 0x001F,
    MM_SMS_CDMA_SERVICE_CATEGORY_CMAS_PRESIDENTIAL_ALERT          = 0x1000,
    MM_SMS_CDMA_SERVICE_CATEGORY_CMAS_EXTREME_THREAT              = 0x1001,
    MM_SMS_CDMA_SERVICE_CATEGORY_CMAS_SEVERE_THREAT               = 0x1002,
    MM_SMS_CDMA_SERVICE_CATEGORY_CMAS_CHILD_ABDUCTION_EMERGENCY   = 0x1003,
    MM_SMS_CDMA_SERVICE_CATEGORY_CMAS_TEST                        = 0x1004,
} MMSmsCdmaServiceCategory;

/**
 * MMModemLocationSource:
 * @MM_MODEM_LOCATION_SOURCE_NONE: None.
 * @MM_MODEM_LOCATION_SOURCE_3GPP_LAC_CI: Location Area Code and Cell ID.
 * @MM_MODEM_LOCATION_SOURCE_GPS_RAW: GPS location given by predefined keys.
 * @MM_MODEM_LOCATION_SOURCE_GPS_NMEA: GPS location given as NMEA traces.
 * @MM_MODEM_LOCATION_SOURCE_CDMA_BS: CDMA base station position.
 *
 * Sources of location information supported by the modem.
 */
typedef enum { /*< underscore_name=mm_modem_location_source >*/
    MM_MODEM_LOCATION_SOURCE_NONE        = 0,
    MM_MODEM_LOCATION_SOURCE_3GPP_LAC_CI = 1 << 0,
    MM_MODEM_LOCATION_SOURCE_GPS_RAW     = 1 << 1,
    MM_MODEM_LOCATION_SOURCE_GPS_NMEA    = 1 << 2,
    MM_MODEM_LOCATION_SOURCE_CDMA_BS     = 1 << 3,
} MMModemLocationSource;

/**
 * MMModemContactsStorage:
 * @MM_MODEM_CONTACTS_STORAGE_UNKNOWN: Unknown location.
 * @MM_MODEM_CONTACTS_STORAGE_ME: Device's local memory.
 * @MM_MODEM_CONTACTS_STORAGE_SM: Card inserted in the device (like a SIM/RUIM).
 * @MM_MODEM_CONTACTS_STORAGE_MT: Combined device/ME and SIM/SM phonebook.
 *
 * Specifies different storage locations for contact information.
 */
typedef enum { /*< underscore_name=mm_modem_contacts_storage >*/
    MM_MODEM_CONTACTS_STORAGE_UNKNOWN = 0,
    MM_MODEM_CONTACTS_STORAGE_ME      = 1,
    MM_MODEM_CONTACTS_STORAGE_SM      = 2,
    MM_MODEM_CONTACTS_STORAGE_MT      = 3,
} MMModemContactsStorage;

/**
 * MMBearerIpMethod:
 * @MM_BEARER_IP_METHOD_UNKNOWN: Unknown method.
 * @MM_BEARER_IP_METHOD_PPP: Use PPP to get the address.
 * @MM_BEARER_IP_METHOD_STATIC: Use the provided static IP configuration given
 * by the modem to configure the IP data interface.
 * @MM_BEARER_IP_METHOD_DHCP: Begin DHCP or IPv6 SLAAC on the data interface to
 * obtain necessary IP configuration details.  For IPv4 bearers DHCP should
 * be used.  For IPv6 bearers SLAAC should be used to determine the prefix and
 * any additional details.
 *
 * Type of IP method configuration to be used in a given Bearer.
 */
typedef enum { /*< underscore_name=mm_bearer_ip_method >*/
    MM_BEARER_IP_METHOD_UNKNOWN = 0,
    MM_BEARER_IP_METHOD_PPP     = 1,
    MM_BEARER_IP_METHOD_STATIC  = 2,
    MM_BEARER_IP_METHOD_DHCP    = 3,
} MMBearerIpMethod;

/**
 * MMBearerIpFamily:
 * @MM_BEARER_IP_FAMILY_NONE: None or unknown.
 * @MM_BEARER_IP_FAMILY_IPV4: IPv4.
 * @MM_BEARER_IP_FAMILY_IPV6: IPv6.
 * @MM_BEARER_IP_FAMILY_IPV4V6: IPv4 and IPv6.
 * @MM_BEARER_IP_FAMILY_ANY: Mask specifying all IP families.
 *
 * Type of IP family to be used in a given Bearer.
 */
typedef enum { /*< underscore_name=mm_bearer_ip_family >*/
    MM_BEARER_IP_FAMILY_NONE    = 0,
    MM_BEARER_IP_FAMILY_IPV4    = 1 << 0,
    MM_BEARER_IP_FAMILY_IPV6    = 1 << 1,
    MM_BEARER_IP_FAMILY_IPV4V6  = 1 << 2,
    MM_BEARER_IP_FAMILY_ANY     = 0xFFFFFFFF
} MMBearerIpFamily;

/**
 * MMBearerAllowedAuth:
 * @MM_BEARER_ALLOWED_AUTH_UNKNOWN: Unknown.
 * @MM_BEARER_ALLOWED_AUTH_NONE: None.
 * @MM_BEARER_ALLOWED_AUTH_PAP: PAP.
 * @MM_BEARER_ALLOWED_AUTH_CHAP: CHAP.
 * @MM_BEARER_ALLOWED_AUTH_MSCHAP: MS-CHAP.
 * @MM_BEARER_ALLOWED_AUTH_MSCHAPV2: MS-CHAP v2.
 * @MM_BEARER_ALLOWED_AUTH_EAP: EAP.
 *
 * Allowed authentication methods when authenticating with the network.
 */
typedef enum { /*< underscore_name=mm_bearer_allowed_auth >*/
    MM_BEARER_ALLOWED_AUTH_UNKNOWN  = 0,
    /* bits 0..4 order match Ericsson device bitmap */
    MM_BEARER_ALLOWED_AUTH_NONE     = 1 << 0,
    MM_BEARER_ALLOWED_AUTH_PAP      = 1 << 1,
    MM_BEARER_ALLOWED_AUTH_CHAP     = 1 << 2,
    MM_BEARER_ALLOWED_AUTH_MSCHAP   = 1 << 3,
    MM_BEARER_ALLOWED_AUTH_MSCHAPV2 = 1 << 4,
    MM_BEARER_ALLOWED_AUTH_EAP      = 1 << 5,
} MMBearerAllowedAuth;

/**
 * MMModemCdmaRegistrationState:
 * @MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN: Registration status is unknown or the device is not registered.
 * @MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED: Registered, but roaming status is unknown or cannot be provided by the device. The device may or may not be roaming.
 * @MM_MODEM_CDMA_REGISTRATION_STATE_HOME: Currently registered on the home network.
 * @MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING: Currently registered on a roaming network.
 *
 * Registration state of a CDMA modem.
 */
typedef enum { /*< underscore_name=mm_modem_cdma_registration_state >*/
    MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN    = 0,
    MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED = 1,
    MM_MODEM_CDMA_REGISTRATION_STATE_HOME       = 2,
    MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING    = 3,
} MMModemCdmaRegistrationState;

/**
 * MMModemCdmaActivationState:
 * @MM_MODEM_CDMA_ACTIVATION_STATE_UNKNOWN: Unknown activation state.
 * @MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED: Device is not activated
 * @MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATING: Device is activating
 * @MM_MODEM_CDMA_ACTIVATION_STATE_PARTIALLY_ACTIVATED: Device is partially activated; carrier-specific steps required to continue.
 * @MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATED: Device is ready for use.
 *
 * Activation state of a CDMA modem.
 */
typedef enum { /*< underscore_name=mm_modem_cdma_activation_state >*/
    MM_MODEM_CDMA_ACTIVATION_STATE_UNKNOWN             = 0,
    MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED       = 1,
    MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATING          = 2,
    MM_MODEM_CDMA_ACTIVATION_STATE_PARTIALLY_ACTIVATED = 3,
    MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATED           = 4,
} MMModemCdmaActivationState;

/**
 * MMModemCdmaRmProtocol:
 * @MM_MODEM_CDMA_RM_PROTOCOL_UNKNOWN: Unknown protocol.
 * @MM_MODEM_CDMA_RM_PROTOCOL_ASYNC: Asynchronous data or fax.
 * @MM_MODEM_CDMA_RM_PROTOCOL_PACKET_RELAY: Packet data service, Relay Layer Rm interface.
 * @MM_MODEM_CDMA_RM_PROTOCOL_PACKET_NETWORK_PPP: Packet data service, Network Layer Rm interface, PPP.
 * @MM_MODEM_CDMA_RM_PROTOCOL_PACKET_NETWORK_SLIP: Packet data service, Network Layer Rm interface, SLIP.
 * @MM_MODEM_CDMA_RM_PROTOCOL_STU_III: STU-III service.
 *
 * Protocol of the Rm interface in modems with CDMA capabilities.
 */
typedef enum { /*< underscore_name=mm_modem_cdma_rm_protocol >*/
    MM_MODEM_CDMA_RM_PROTOCOL_UNKNOWN             = 0,
    MM_MODEM_CDMA_RM_PROTOCOL_ASYNC               = 1,
    MM_MODEM_CDMA_RM_PROTOCOL_PACKET_RELAY        = 2,
    MM_MODEM_CDMA_RM_PROTOCOL_PACKET_NETWORK_PPP  = 3,
    MM_MODEM_CDMA_RM_PROTOCOL_PACKET_NETWORK_SLIP = 4,
    MM_MODEM_CDMA_RM_PROTOCOL_STU_III             = 5,
} MMModemCdmaRmProtocol;

/**
 * MMModem3gppRegistrationState:
 * @MM_MODEM_3GPP_REGISTRATION_STATE_IDLE: Not registered, not searching for new operator to register.
 * @MM_MODEM_3GPP_REGISTRATION_STATE_HOME: Registered on home network.
 * @MM_MODEM_3GPP_REGISTRATION_STATE_SEARCHING: Not registered, searching for new operator to register with.
 * @MM_MODEM_3GPP_REGISTRATION_STATE_DENIED: Registration denied.
 * @MM_MODEM_3GPP_REGISTRATION_STATE_UNKNOWN: Unknown registration status.
 * @MM_MODEM_3GPP_REGISTRATION_STATE_ROAMING: Registered on a roaming network.
 *
 * GSM registration code as defined in 3GPP TS 27.007 section 10.1.19.
 */
typedef enum { /*< underscore_name=mm_modem_3gpp_registration_state >*/
    MM_MODEM_3GPP_REGISTRATION_STATE_IDLE      = 0,
    MM_MODEM_3GPP_REGISTRATION_STATE_HOME      = 1,
    MM_MODEM_3GPP_REGISTRATION_STATE_SEARCHING = 2,
    MM_MODEM_3GPP_REGISTRATION_STATE_DENIED    = 3,
    MM_MODEM_3GPP_REGISTRATION_STATE_UNKNOWN   = 4,
    MM_MODEM_3GPP_REGISTRATION_STATE_ROAMING   = 5,
} MMModem3gppRegistrationState;

/**
 * MMModem3gppFacility:
 * @MM_MODEM_3GPP_FACILITY_NONE: No facility.
 * @MM_MODEM_3GPP_FACILITY_SIM: SIM lock.
 * @MM_MODEM_3GPP_FACILITY_FIXED_DIALING: Fixed dialing (PIN2) SIM lock.
 * @MM_MODEM_3GPP_FACILITY_PH_SIM: Device is locked to a specific SIM.
 * @MM_MODEM_3GPP_FACILITY_PH_FSIM: Device is locked to first SIM inserted.
 * @MM_MODEM_3GPP_FACILITY_NET_PERS: Network personalization.
 * @MM_MODEM_3GPP_FACILITY_NET_SUB_PERS: Network subset personalization.
 * @MM_MODEM_3GPP_FACILITY_PROVIDER_PERS: Service provider personalization.
 * @MM_MODEM_3GPP_FACILITY_CORP_PERS: Corporate personalization.
 *
 * A bitfield describing which facilities have a lock enabled, i.e.,
 * requires a pin or unlock code. The facilities include the
 * personalizations (device locks) described in 3GPP spec TS 22.022,
 * and the PIN and PIN2 locks, which are SIM locks.
 */
typedef enum { /*< underscore_name=mm_modem_3gpp_facility >*/
    MM_MODEM_3GPP_FACILITY_NONE          = 0,
    MM_MODEM_3GPP_FACILITY_SIM           = 1 << 0,
    MM_MODEM_3GPP_FACILITY_FIXED_DIALING = 1 << 1,
    MM_MODEM_3GPP_FACILITY_PH_SIM        = 1 << 2,
    MM_MODEM_3GPP_FACILITY_PH_FSIM       = 1 << 3,
    MM_MODEM_3GPP_FACILITY_NET_PERS      = 1 << 4,
    MM_MODEM_3GPP_FACILITY_NET_SUB_PERS  = 1 << 5,
    MM_MODEM_3GPP_FACILITY_PROVIDER_PERS = 1 << 6,
    MM_MODEM_3GPP_FACILITY_CORP_PERS     = 1 << 7,
} MMModem3gppFacility;

/**
 * MMModem3gppNetworkAvailability:
 * @MM_MODEM_3GPP_NETWORK_AVAILABILITY_UNKNOWN: Unknown availability.
 * @MM_MODEM_3GPP_NETWORK_AVAILABILITY_AVAILABLE: Network is available.
 * @MM_MODEM_3GPP_NETWORK_AVAILABILITY_CURRENT: Network is the current one.
 * @MM_MODEM_3GPP_NETWORK_AVAILABILITY_FORBIDDEN: Network is forbidden.
 *
 * Network availability status as defined in 3GPP TS 27.007 section 7.3
 */
typedef enum { /*< underscore_name=mm_modem_3gpp_network_availability >*/
    MM_MODEM_3GPP_NETWORK_AVAILABILITY_UNKNOWN   = 0,
    MM_MODEM_3GPP_NETWORK_AVAILABILITY_AVAILABLE = 1,
    MM_MODEM_3GPP_NETWORK_AVAILABILITY_CURRENT   = 2,
    MM_MODEM_3GPP_NETWORK_AVAILABILITY_FORBIDDEN = 3,
} MMModem3gppNetworkAvailability;

/**
 * MMModem3gppSubscriptionState:
 * @MM_MODEM_3GPP_SUBSCRIPTION_STATE_UNKNOWN: The subscription state is unknown.
 * @MM_MODEM_3GPP_SUBSCRIPTION_STATE_UNPROVISIONED: The account is unprovisioned.
 * @MM_MODEM_3GPP_SUBSCRIPTION_STATE_PROVISIONED: The account is provisioned and has data available.
 * @MM_MODEM_3GPP_SUBSCRIPTION_STATE_OUT_OF_DATA: The account is provisioned but there is no data left.
 *
 * Describes the current subscription status of the SIM.  This value is only available after the
 * modem attempts to register with the network.
 */
typedef enum { /*< underscore_name=mm_modem_3gpp_subscription_state >*/
    MM_MODEM_3GPP_SUBSCRIPTION_STATE_UNKNOWN       = 0,
    MM_MODEM_3GPP_SUBSCRIPTION_STATE_UNPROVISIONED = 1,
    MM_MODEM_3GPP_SUBSCRIPTION_STATE_PROVISIONED   = 2,
    MM_MODEM_3GPP_SUBSCRIPTION_STATE_OUT_OF_DATA   = 3,
} MMModem3gppSubscriptionState;

/**
 * MMModem3gppUssdSessionState:
 * @MM_MODEM_3GPP_USSD_SESSION_STATE_UNKNOWN: Unknown state.
 * @MM_MODEM_3GPP_USSD_SESSION_STATE_IDLE: No active session.
 * @MM_MODEM_3GPP_USSD_SESSION_STATE_ACTIVE: A session is active and the mobile is waiting for a response.
 * @MM_MODEM_3GPP_USSD_SESSION_STATE_USER_RESPONSE: The network is waiting for the client's response.
 *
 * State of a USSD session.
 */
typedef enum { /*< underscore_name=mm_modem_3gpp_ussd_session_state >*/
    MM_MODEM_3GPP_USSD_SESSION_STATE_UNKNOWN       = 0,
    MM_MODEM_3GPP_USSD_SESSION_STATE_IDLE          = 1,
    MM_MODEM_3GPP_USSD_SESSION_STATE_ACTIVE        = 2,
    MM_MODEM_3GPP_USSD_SESSION_STATE_USER_RESPONSE = 3,
} MMModem3gppUssdSessionState;

/**
 * MMFirmwareImageType:
 * @MM_FIRMWARE_IMAGE_TYPE_UNKNOWN: Unknown firmware type.
 * @MM_FIRMWARE_IMAGE_TYPE_GENERIC: Generic firmware image.
 * @MM_FIRMWARE_IMAGE_TYPE_GOBI: Firmware image of Gobi devices.
 *
 * Type of firmware image.
 */
typedef enum { /*< underscore_name=mm_firmware_image_type >*/
    MM_FIRMWARE_IMAGE_TYPE_UNKNOWN = 0,
    MM_FIRMWARE_IMAGE_TYPE_GENERIC = 1,
    MM_FIRMWARE_IMAGE_TYPE_GOBI    = 2,
} MMFirmwareImageType;

/**
 * MMOmaFeature:
 * @MM_OMA_FEATURE_NONE: None.
 * @MM_OMA_FEATURE_DEVICE_PROVISIONING: Device provisioning service.
 * @MM_OMA_FEATURE_PRL_UPDATE: PRL update service.
 * @MM_OMA_FEATURE_HANDS_FREE_ACTIVATION: Hands free activation service.
 *
 * Features that can be enabled or disabled in the OMA device management support.
 */
typedef enum { /*< underscore_name=mm_oma_feature >*/
    MM_OMA_FEATURE_NONE                  = 0,
    MM_OMA_FEATURE_DEVICE_PROVISIONING   = 1 << 0,
    MM_OMA_FEATURE_PRL_UPDATE            = 1 << 1,
    MM_OMA_FEATURE_HANDS_FREE_ACTIVATION = 1 << 2,
} MMOmaFeature;

/**
 * MMOmaSessionType:
 * @MM_OMA_SESSION_TYPE_UNKNOWN: Unknown session type.
 * @MM_OMA_SESSION_TYPE_CLIENT_INITIATED_DEVICE_CONFIGURE: Client-initiated device configure.
 * @MM_OMA_SESSION_TYPE_CLIENT_INITIATED_PRL_UPDATE: Client-initiated PRL update.
 * @MM_OMA_SESSION_TYPE_CLIENT_INITIATED_HANDS_FREE_ACTIVATION: Client-initiated hands free activation.
 * @MM_OMA_SESSION_TYPE_NETWORK_INITIATED_DEVICE_CONFIGURE: Network-initiated device configure.
 * @MM_OMA_SESSION_TYPE_NETWORK_INITIATED_PRL_UPDATE: Network-initiated PRL update.
 * @MM_OMA_SESSION_TYPE_DEVICE_INITIATED_PRL_UPDATE: Device-initiated PRL update.
 * @MM_OMA_SESSION_TYPE_DEVICE_INITIATED_HANDS_FREE_ACTIVATION: Device-initiated hands free activation.
 *
 * Type of OMA device management session.
 */
typedef enum { /*< underscore_name=mm_oma_session_type >*/
    MM_OMA_SESSION_TYPE_UNKNOWN                                = 0,
    MM_OMA_SESSION_TYPE_CLIENT_INITIATED_DEVICE_CONFIGURE      = 10,
    MM_OMA_SESSION_TYPE_CLIENT_INITIATED_PRL_UPDATE            = 11,
    MM_OMA_SESSION_TYPE_CLIENT_INITIATED_HANDS_FREE_ACTIVATION = 12,
    MM_OMA_SESSION_TYPE_NETWORK_INITIATED_DEVICE_CONFIGURE     = 20,
    MM_OMA_SESSION_TYPE_NETWORK_INITIATED_PRL_UPDATE           = 21,
    MM_OMA_SESSION_TYPE_DEVICE_INITIATED_PRL_UPDATE            = 30,
    MM_OMA_SESSION_TYPE_DEVICE_INITIATED_HANDS_FREE_ACTIVATION = 31,
} MMOmaSessionType;

/**
 * MMOmaSessionState:
 * @MM_OMA_SESSION_STATE_FAILED: Failed.
 * @MM_OMA_SESSION_STATE_UNKNOWN: Unknown.
 * @MM_OMA_SESSION_STATE_STARTED: Started.
 * @MM_OMA_SESSION_STATE_RETRYING: Retrying.
 * @MM_OMA_SESSION_STATE_CONNECTING: Connecting.
 * @MM_OMA_SESSION_STATE_CONNECTED: Connected.
 * @MM_OMA_SESSION_STATE_AUTHENTICATED: Authenticated.
 * @MM_OMA_SESSION_STATE_MDN_DOWNLOADED: MDN downloaded.
 * @MM_OMA_SESSION_STATE_MSID_DOWNLOADED: MSID downloaded.
 * @MM_OMA_SESSION_STATE_PRL_DOWNLOADED: PRL downloaded.
 * @MM_OMA_SESSION_STATE_MIP_PROFILE_DOWNLOADED: MIP profile downloaded.
 * @MM_OMA_SESSION_STATE_COMPLETED: Session completed.
 *
 * State of the OMA device management session.
 */
typedef enum { /*< underscore_name=mm_oma_session_state >*/
    MM_OMA_SESSION_STATE_FAILED                 = -1,
    MM_OMA_SESSION_STATE_UNKNOWN                = 0,
    MM_OMA_SESSION_STATE_STARTED                = 1,
    MM_OMA_SESSION_STATE_RETRYING               = 2,
    MM_OMA_SESSION_STATE_CONNECTING             = 3,
    MM_OMA_SESSION_STATE_CONNECTED              = 4,
    MM_OMA_SESSION_STATE_AUTHENTICATED          = 5,
    MM_OMA_SESSION_STATE_MDN_DOWNLOADED         = 10,
    MM_OMA_SESSION_STATE_MSID_DOWNLOADED        = 11,
    MM_OMA_SESSION_STATE_PRL_DOWNLOADED         = 12,
    MM_OMA_SESSION_STATE_MIP_PROFILE_DOWNLOADED = 13,
    MM_OMA_SESSION_STATE_COMPLETED              = 20,
} MMOmaSessionState;

/**
 * MMOmaSessionStateFailedReason:
 * @MM_OMA_SESSION_STATE_FAILED_REASON_UNKNOWN: No reason or unknown.
 * @MM_OMA_SESSION_STATE_FAILED_REASON_NETWORK_UNAVAILABLE: Network unavailable.
 * @MM_OMA_SESSION_STATE_FAILED_REASON_SERVER_UNAVAILABLE: Server unavailable.
 * @MM_OMA_SESSION_STATE_FAILED_REASON_AUTHENTICATION_FAILED: Authentication failed.
 * @MM_OMA_SESSION_STATE_FAILED_REASON_MAX_RETRY_EXCEEDED: Maximum retries exceeded.
 * @MM_OMA_SESSION_STATE_FAILED_REASON_SESSION_CANCELLED: Session cancelled.
 *
 * Reason of failure in the OMA device management session.
 */
typedef enum { /*< underscore_name=mm_oma_session_state_failed_reason >*/
    MM_OMA_SESSION_STATE_FAILED_REASON_UNKNOWN               = 0,
    MM_OMA_SESSION_STATE_FAILED_REASON_NETWORK_UNAVAILABLE   = 1,
    MM_OMA_SESSION_STATE_FAILED_REASON_SERVER_UNAVAILABLE    = 2,
    MM_OMA_SESSION_STATE_FAILED_REASON_AUTHENTICATION_FAILED = 3,
    MM_OMA_SESSION_STATE_FAILED_REASON_MAX_RETRY_EXCEEDED    = 4,
    MM_OMA_SESSION_STATE_FAILED_REASON_SESSION_CANCELLED     = 5
} MMOmaSessionStateFailedReason;

#endif /*  _MODEMMANAGER_ENUMS_H_ */
