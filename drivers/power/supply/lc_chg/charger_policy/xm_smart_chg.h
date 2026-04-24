#ifndef __XM_SMART_CHG_H__
#define __XM_SMART_CHG_H__

#include "lc_charger_manager.h"
#include "../common/lc_voter.h"

#if IS_ENABLED(CONFIG_MIEV)
#include "miev/mievent.h"
#include <linux/string.h>

#define DFX_ID_CHG_PD_AUTHEN_FAIL           909001004
#define DFX_ID_CHG_CP_ENABLE_FAIL           909001005

#define DFX_ID_CHG_NONE_STANDARD_CHG        909002001
#define DFX_ID_CHG_CORROSION_DISCHARGE      909002002

#define DFX_ID_CHG_LPD_DISCHARGE            909002003
#define DFX_ID_CHG_CP_VBUS_OVP              909002004
#define DFX_ID_CHG_CP_IBUS_OCP              909002005
#define DFX_ID_CHG_CP_VBAT_OVP              909002006
#define DFX_ID_CHG_CP_IBAT_OCP              909002007

#define DFX_ID_CHG_BATTERY_CYCLECOUNT       909003001

#define DFX_ID_CHG_FG_IIC_ERR               909005001
#define DFX_ID_CHG_CP_ERR                   909005002
#define DFX_ID_CHG_BATT_LINKER_ABSENT       909005003
#define DFX_ID_CHG_LOW_TEMP_DISCHARGING     909005007
#define DFX_ID_CHG_HIGH_TEMP_DISCHARGING    909005008

#define DFX_ID_CHG_BATTERY_AUTH_FAIL        909007001

#define DFX_ID_CHG_BATTERY_TEMP_HIGH        909009001
#define DFX_ID_CHG_BATTERY_TEMP_LOW         909009002
#define DFX_ID_CHG_ANTIBURN_FAIL            909009003


enum xm_chg_dfx_type {
    CHG_DFX_DEFAULT,
    CHG_DFX_FG_IIC_ERR,
    CHG_DFX_CP_ERR,
    CHG_DFX_BATT_LINKER_ABSENT,
    CHG_DFX_LPD_DISCHARGE,
    CHG_DFX_LPD_DISCHARGE_RESET,
    CHG_DFX_CORROSION_DISCHARGE,
    CHG_DFX_NONE_STANDARD_CHG,
    CHG_DFX_CP_VBUS_OVP,
    CHG_DFX_CP_IBUS_OCP,
    CHG_DFX_CP_VBAT_OVP,
    CHG_DFX_CP_IBAT_OCP,
    CHG_DFX_PPS_AUTH_FAIL,
    CHG_DFX_CP_ENABLE_FAIL,
    CHG_DFX_BATTERY_CYCLECOUNT,
    CHG_DFX_LOW_TEMP_DISCHARGING,
    CHG_DFX_HIGH_TEMP_DISCHARGING,
    CHG_DFX_BATTERY_AUTH_FAIL,
    CHG_DFX_BATTERY_TEMP_HIGH,
    CHG_DFX_BATTERY_TEMP_LOW,
    CHG_DFX_ANTIBURN_FAIL,
    CHG_DFX_MAX_INDEX,
};

/*
 * for kernel print and dfx report
 * note:
 * 1. key use 11 char, length of string for report
 * should less than 50 - 11 - 1 = 38 char
 * 2. order of string should follow the one of
 * 'enum xm_chg_dfx_type' defined above
 */

static const char *const xm_dfx_chg_report_text[] = {
    "DEFAULT_TEXT", "fgI2cErr", "cpErr",
    "battLinkerAbsent", "lpdDischarge", "lpdDischargeReset",
    "corrosionDischarge", "noneStandartChg",
    "CpVbusOvp", "CpIbusOcp", "CpVbatOvp", "CpIbatOcp",
    "PdAuthFail", "CpEnFail", "chgBattCycle", "NotChgInLowTemp",
    "NotChgInHighTemp", "chgBattAuthFail",
    "TbatHot", "TbatCold", "AntiFail"
};
#endif

void set_error(struct charger_manager *manager);
void set_success(struct charger_manager *manager);
int smart_chg_is_error(struct charger_manager *manager);
void handle_smart_chg_functype(struct charger_manager *manager,
	const int func_type, const int en_ret, const int func_val);
int handle_smart_chg_functype_status(struct charger_manager *manager);
void monitor_smart_chg(struct charger_manager *manager);
void monitor_smart_batt(struct charger_manager *manager);
void monitor_cycle_count(struct charger_manager *manager);
void monitor_night_charging(struct charger_manager *manager);
void monitor_low_fast_strategy(struct charger_manager *manager);
void xm_charge_work(struct work_struct *work);
#endif
