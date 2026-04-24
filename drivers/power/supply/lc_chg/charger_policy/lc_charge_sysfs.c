// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2017, 2019 The Linux Foundation. All rights reserved.
 */

#include "lc_charger_manager.h"
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
#include "xm_smart_chg.h"
#include "../common/lc_voter.h"
#endif
#include "../lc_printk.h"
#ifdef TAG
#undef TAG
#define  TAG "[CM]"
#endif
#include "../fuelgauge/bq28z610.h"
#include "../fuelgauge/charger_partition.h"
#include <linux/thermal.h>
//cqr vbat ctrol start

#define CQR_VBAT_MIM       3750000
#define CQR_VBAT_MAX       4300000
#define CQR_READ_VBAT_TIMES 5
#define CQR_VBAT_OUT_RANGE_CNT 2

//cqr vbat ctrol end
static enum power_supply_usb_type charge_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_UNKNOWN
};
static int charger_usb_get_property(struct power_supply *psy,
						enum power_supply_property psp,
						union power_supply_propval *val)
{
	struct charger_manager *manager = power_supply_get_drvdata(psy);
	bool online = false;
	enum vbus_type vbus_type;
	int ret = 0;
	int volt = 0;
	int curr = 0;
	if (IS_ERR_OR_NULL(manager)) {
		lc_err("manager is_err_or_null\n");
		return PTR_ERR(manager);
	}

	if (IS_ERR_OR_NULL(manager->charger)) {
		lc_err("manager charger is_err_or_null\n");
		return PTR_ERR(manager->charger);
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		ret = charger_get_vbus_type(manager->charger, &vbus_type);
		if (ret < 0)
			lc_err("Couldn't get usb type ret=%d\n", ret);
		if (vbus_type == VBUS_TYPE_SDP)
			val->intval = POWER_SUPPLY_USB_TYPE_SDP;
		else if(vbus_type == VBUS_TYPE_CDP)
			val->intval = POWER_SUPPLY_USB_TYPE_CDP;
		else if(vbus_type >= VBUS_TYPE_DCP && vbus_type <= VBUS_TYPE_HVDCP_3P5)
			val->intval = POWER_SUPPLY_USB_TYPE_DCP;
		else
			val->intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		ret = charger_get_vbus_type(manager->charger, &vbus_type);
		if (ret < 0)
			lc_err("Couldn't get usb type ret=%d\n", ret);
		if (vbus_type == VBUS_TYPE_SDP || vbus_type == VBUS_TYPE_CDP)
			val->intval = POWER_SUPPLY_TYPE_USB;
		else
			val->intval = POWER_SUPPLY_TYPE_USB_PD;
		if (manager->otg_status) {
			val->intval = POWER_SUPPLY_TYPE_UNKNOWN;
		}
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		ret = charger_get_online(manager->charger, &online);
		if (ret < 0)
			val->intval = 0;
#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
		/*****when detect MI-PD adapter plug in, it would do once hard reset,
		Causing charging interruption, so we nedd get bc_type to set online*****/
		else if (manager->vbus_type == 0)
			val->intval = 0;
#endif
		else
			val->intval = online;
		if (manager->otg_status) {
			val->intval = 0;
		}
                if(manager->pd_active == CHARGE_PD_ACTIVE) {
                        val->intval = 1;
                }
                break;

	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = VOLTAGE_MAX;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = charger_get_adc(manager->charger, ADC_GET_VBUS, &volt);
		if (ret < 0) {
			lc_err("Couldn't read input volt ret=%d\n", ret);
			return ret;
		}
		val->intval = volt * 1000;
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = charger_manager_get_current(manager, &curr);
		if (ret < 0) {
			lc_err("Couldn't read input curr ret=%d\n", ret);
			return ret;
		}
		val->intval = curr;
		break;

	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = CURRENT_MAX;
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		val->intval = INPUT_CURRENT_LIMIT;
		break;

	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = POWER_SUPPLY_MANUFACTURER;
		break;

	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = POWER_SUPPLY_MODEL_NAME;
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

static int charger_usb_set_property(struct power_supply *psy,
					enum power_supply_property prop,
					const union power_supply_propval *val)
{
	struct charger_manager *manager = power_supply_get_drvdata(psy);

	if (IS_ERR_OR_NULL(manager)) {
		lc_err("manager is_err_or_null\n");
		return PTR_ERR(manager);
	}

	switch (prop) {
	default:
		return -EINVAL;
	}
	return 0;
}

static enum power_supply_property usb_props[] = {
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
};

static int usb_psy_is_writeable(struct power_supply *psy, enum power_supply_property psp)
{
	switch(psp) {
	default:
		return 0;
	}
}

static const struct power_supply_desc usb_psy_desc = {
	.name = "usb",
	.type = POWER_SUPPLY_TYPE_USB,
	.properties = usb_props,
	.usb_types  = charge_usb_types,
	.num_usb_types = ARRAY_SIZE(charge_usb_types),
	.num_properties = ARRAY_SIZE(usb_props),
	.get_property = charger_usb_get_property,
	.set_property = charger_usb_set_property,
	.property_is_writeable = usb_psy_is_writeable,
};

int charger_manager_usb_psy_register(struct charger_manager *manager)
{
	struct power_supply_config usb_psy_cfg = { .drv_data = manager,};

	memcpy(&manager->usb_psy_desc, &usb_psy_desc, sizeof(manager->usb_psy_desc));

	manager->usb_psy = devm_power_supply_register(manager->dev, &manager->usb_psy_desc,
							&usb_psy_cfg);
	if (IS_ERR(manager->usb_psy)) {
		lc_err("usb psy register failed\n");
		return PTR_ERR(manager->usb_psy);
	}
	return 0;
}
EXPORT_SYMBOL(charger_manager_usb_psy_register);

static int get_battery_health(struct charger_manager *manager)
{
	union power_supply_propval pval;
	int battery_health = POWER_SUPPLY_HEALTH_GOOD;
	int ret = 0;

	ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret < 0) {
		lc_err("failed to get temp prop\n");
		return -EINVAL;
	}

	if (pval.intval <= manager->battery_temp[TEMP_LEVEL_COLD])
		battery_health = POWER_SUPPLY_HEALTH_COLD;
	else if (pval.intval <= manager->battery_temp[TEMP_LEVEL_COOL])
		battery_health = POWER_SUPPLY_HEALTH_COOL;
	else if (pval.intval <= manager->battery_temp[TEMP_LEVEL_GOOD])
		battery_health = POWER_SUPPLY_HEALTH_GOOD;
	else if (pval.intval <= manager->battery_temp[TEMP_LEVEL_WARM])
		battery_health = POWER_SUPPLY_HEALTH_WARM;
	else if (pval.intval < manager->battery_temp[TEMP_LEVEL_HOT])
		battery_health = POWER_SUPPLY_HEALTH_HOT;
	else
		battery_health = POWER_SUPPLY_HEALTH_OVERHEAT;

	return battery_health;
}

static int charger_batt_get_property(struct power_supply *psy,
						 enum power_supply_property psp,
						 union power_supply_propval *val)
{
	struct charger_manager *manager = power_supply_get_drvdata(psy);
	union power_supply_propval pval;
	int state = 0, status = 0;
	int vindpm_status = 0;
	int ibus;
	int vbus;
#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
	int warm_stop_charge = 0;
	bool online = false;
#endif
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	int  bat_volt = 0;
#endif
	int ret = 0;
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
        struct votable	*is_full_votable = NULL;
        int is_full_flag = 0;
#endif

	if (IS_ERR_OR_NULL(manager)) {
		lc_err("manager is_err_or_null\n");
		return PTR_ERR(manager);
	}

	if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
		manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
		if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
			lc_err("manager->fuel_gauge is_err_or_null\n");
		}
	}

	if (manager->fg_psy == NULL)
		manager->fg_psy = power_supply_get_by_name("bms");
	if (IS_ERR_OR_NULL(manager->fg_psy)) {
		lc_err("failed to get bms psy\n");
		return PTR_ERR(manager->fg_psy);
	}

	if (IS_ERR_OR_NULL(manager->main_chg_disable_votable)) {
		manager->main_chg_disable_votable = find_votable("MAIN_CHG_DISABLE");
		if(IS_ERR_OR_NULL(manager->main_chg_disable_votable)) {
			lc_err("failed to get main_chg_disable_votable\n");
			return PTR_ERR(manager->main_chg_disable_votable);
		}
	}

	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
			ret = charger_get_chg_status(manager->charger, &state, &status);
			if (ret < 0) {
				lc_err("failed to get chg status prop\n");
				break;
			}
			if ((status == POWER_SUPPLY_STATUS_DISCHARGING) && !manager->otg_status) {
				charger_get_online(manager->charger, &online);
				if (online)
					status = POWER_SUPPLY_STATUS_CHARGING;
			}

#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
			warm_stop_charge = get_warm_stop_charge_state();
			if (!IS_ERR_OR_NULL(manager->batt_psy)) {
				ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
				if (ret < 0)
						lc_err("get battery volt error.\n");
				else
						manager->vbat = pval.intval/1000;
			} else {
				ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
				if (ret < 0)
						lc_err("get battery volt error.\n");
				else
						manager->vbat = pval.intval/1000;
			}

			if ((status != POWER_SUPPLY_STATUS_DISCHARGING) &&
					(!get_effective_result(manager->main_chg_disable_votable))) {
				if (manager->tbat >= BATTERY_HOT_TEMP)
					status = POWER_SUPPLY_STATUS_CHARGING;
				else if ((manager->tbat >= BATTERY_WARM_TEMP || warm_stop_charge))
					status = POWER_SUPPLY_STATUS_CHARGING;
				else if ((manager->tbat < BATTERY_WARM_TEMP && manager->vbat < 4100))
					status = POWER_SUPPLY_STATUS_CHARGING;
			}
#endif
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
			if ((status != POWER_SUPPLY_STATUS_FULL && manager->soc < 99) && manager->usb_online) {
				status = POWER_SUPPLY_STATUS_CHARGING;
			} else if ((status == POWER_SUPPLY_STATUS_CHARGING) && (!IS_ERR_OR_NULL(manager->batt_psy))) {
				ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
				if (ret < 0)
						lc_err("get battery soc error.\n");
				else
						manager->soc = pval.intval;

				is_full_votable = find_votable("IS_FULL");
				if (!is_full_votable) {
						lc_err("failed to get is_full_votable\n");
						return -EINVAL;
				}
				is_full_flag = get_effective_result(is_full_votable);
				if (is_full_flag < 0) {
						lc_err("failed to get is_full_flag\n");
						return -EINVAL;
				}

			}
			lc_debug("new soc is 100, don't keep report full, status = %d\n", status);
			if (manager->is_eu_mode && (manager->fake_batt_status == POWER_SUPPLY_STATUS_FULL)) {
				status = manager->fake_batt_status;
			}else if (manager->usb_online && manager->fake_batt_status_hot == POWER_SUPPLY_STATUS_FULL && manager->ffc && !manager->cp_enable) {
					status = manager->fake_batt_status_hot;
			}
#endif
			vindpm_status = charger_get_vindpm_status(manager->charger);
			charger_get_adc(manager->charger, ADC_GET_VBUS, &vbus);
			charger_get_adc(manager->charger, ADC_GET_IBUS, &ibus);

			if (vindpm_status && ibus < 100 && vbus < manager->vindpm_vot) {
				lc_err("vindpm_status && ibus is 0 \n");
				status = POWER_SUPPLY_STATUS_DISCHARGING;
			}

			/* input suspend overlay charge status */
			if (manager->input_suspend || manager->otg_status)
				status = POWER_SUPPLY_STATUS_DISCHARGING;

			//lc_err("vindpm_status =%d  status = %d ibus = %d vbus = %d manager->vindpm_vot = %d\n", vindpm_status, status, ibus, vbus ,manager->vindpm_vot);
			val->intval = status;
		break;

		case POWER_SUPPLY_PROP_HEALTH:
			ret = get_battery_health(manager);
			if (ret < 0)
				break;
			val->intval = ret;
			break;

		case POWER_SUPPLY_PROP_PRESENT:
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_PRESENT, &pval);
			if (ret < 0) {
				lc_err("failed to get online prop\n");
				break;
			}
			val->intval = pval.intval;
			break;

		case POWER_SUPPLY_PROP_CHARGE_TYPE:
			ret = charger_get_chg_status(manager->charger, &state, &status);
			if (ret < 0) {
				lc_err("failed to get chg type prop\n");
				break;
			}
			val->intval = state;
			break;

		case POWER_SUPPLY_PROP_CAPACITY:
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
			ret = fuel_gauge_check_i2c_function(manager->fuel_gauge);
			if (!ret) {
#endif
				ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
				if (ret < 0) {
					lc_err("failed to get capaticy prop\n");
					break;
				}
				val->intval = pval.intval;
				if (pval.intval <= 1) {
					val->intval = 1;
				}
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
			} else {
				val->intval = FG_I2C_ERR_SOC;
			}
#endif
			break;

		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
			ret = fuel_gauge_check_i2c_function(manager->fuel_gauge);
			if (!ret) {
#endif
				ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
				if (ret < 0) {
					lc_err("failed to get voltage-now prop\n");
					break;
				}
				val->intval = pval.intval;
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
			} else {
				charger_get_adc(manager->charger, ADC_GET_VBAT, &bat_volt);
				if (!bat_volt) {
					//charger_adc_enable(manager->charger, true);
					charger_get_adc(manager->charger, ADC_GET_VBAT, &bat_volt);
				}
				if (!bat_volt) {
					bat_volt =3995;
					lc_err("failed to get voltage-now set 3.995V\n");
				}
				lc_debug("fg i2c err, ADC_GET_VBAT voltage-now\n");
				val->intval = bat_volt * 1000;
			}
#endif
			break;

		case POWER_SUPPLY_PROP_VOLTAGE_MAX:
			ret = fuel_gauge_get_fastcharge_mode(manager->fuel_gauge);
			if (ret)
				val->intval = FAST_CHG_VOLTAGE_MAX;
			else
				val->intval = NORMAL_CHG_VOLTAGE_MAX;
			break;

		case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
			val->intval = manager->system_temp_level;
			break;

		case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
			val->intval = 18;
			break;

		case POWER_SUPPLY_PROP_CURRENT_NOW:
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
			ret = fuel_gauge_check_i2c_function(manager->fuel_gauge);
			if (!ret) {
#endif
				ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
				if (ret < 0) {
					lc_err("failed to get current_now prop\n");
					break;
				}
				val->intval = pval.intval;
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
			} else {
				val->intval = FG_I2C_ERR_CURRENT_NOW;
			}
#endif
			break;

		case POWER_SUPPLY_PROP_TEMP:
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_TEMP, &pval);
			if (ret < 0) {
				lc_err("failed to get temp prop\n");
				break;
			}
			val->intval = pval.intval;
			break;

		case POWER_SUPPLY_PROP_TECHNOLOGY:
			val->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
			break;

		case POWER_SUPPLY_PROP_CYCLE_COUNT:
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
			if (ret < 0) {
				lc_err("failed to get cycle_count prop\n");
				break;
			}
			val->intval = pval.intval;
			break;

		case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
			val->intval = TYPICAL_CAPACITY;
			break;

		case POWER_SUPPLY_PROP_CHARGE_FULL:
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CHARGE_FULL, &pval);
			if (ret < 0) {
				lc_err("failed to get charge_full prop\n");
				break;
			}
			val->intval = pval.intval;
			break;

		case POWER_SUPPLY_PROP_MODEL_NAME:
			#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
			val->strval = TO_STR(MODEL_NAME(PROJECT_NAME, TYPICAL_CAPACITY_MAH, INPUT_POWER_LIMIT));
			#else
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_MODEL_NAME, &pval);
			if (ret < 0) {
				lc_err("failed to get model_name prop\n");
				break;
			}
			val->strval = pval.strval;
			#endif
			break;

		case POWER_SUPPLY_PROP_CHARGE_COUNTER:
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CHARGE_COUNTER, &pval);
			if (ret < 0) {
				lc_err("failed to get charge_counter prop\n");
				break;
			}
			val->intval = pval.intval;
			break;
		case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		#ifdef CONFIG_FACTORY_BUILD
			val->intval = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
		#else
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CAPACITY_LEVEL, &pval);
			if (ret < 0) {
				//lc_err("failed to get charge_counter prop\n");
				val->intval = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
				break;
			}
			val->intval = pval.intval;
			if(val->intval == POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL){
				lc_err("soc capacity level critical true\n");
			}
		#endif
			break;

		default:
			break;
	}
	return 0;
}

static int charger_batt_set_property(struct power_supply *psy,
					enum power_supply_property prop,
					const union power_supply_propval *val)
{
	struct charger_manager *manager = power_supply_get_drvdata(psy);
	if (IS_ERR_OR_NULL(manager)) {
		lc_err("manager is_err_or_null\n");
		return PTR_ERR(manager);
	}

	switch (prop) {
		case POWER_SUPPLY_PROP_STATUS:
			smblib_set_prop_batt_status(manager, val);
			break;
		case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
			manager->system_temp_level = val->intval;
			lc_set_prop_system_temp_level(manager, TEMP_THERMAL_DAEMON_VOTER);
			break;
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
			manager->system_temp_level = val->intval;
			lc_set_prop_system_temp_level(manager, CALL_THERMAL_DAEMON_VOTER);
			break;
		default:
			break;
	}
	return 0;
}

static int batt_prop_is_writeable(struct power_supply *psy, enum power_supply_property prop)
{
	switch (prop) {
		case POWER_SUPPLY_PROP_STATUS:
		case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
			return 1;
		default:
			break;
	}
	return 0;
}

static enum power_supply_property batt_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
};

static const struct power_supply_desc batt_psy_desc = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = batt_props,
	.num_properties = ARRAY_SIZE(batt_props),
	.get_property = charger_batt_get_property,
	.set_property = charger_batt_set_property,
	.property_is_writeable = batt_prop_is_writeable,
};
static int psy_register_cooler(struct power_supply *psy);
int charger_manager_batt_psy_register(struct charger_manager *manager)
{
	struct power_supply_config batt_psy_cfg = { .drv_data = manager,};
	if (IS_ERR_OR_NULL(manager)) {
		lc_err("manager is_err_or_null\n");
		return PTR_ERR(manager);
	}

	manager->batt_psy = devm_power_supply_register(manager->dev, &batt_psy_desc,
							&batt_psy_cfg);
	if (IS_ERR_OR_NULL(manager->batt_psy)) {
		lc_err("batt psy register failed\n");
		return PTR_ERR(manager->batt_psy);
	}
	lc_info("batt psy register success\n");
	psy_register_cooler(manager->batt_psy);
	return 0;
}
EXPORT_SYMBOL(charger_manager_batt_psy_register);

static void charger_manager_from_psy(struct device *dev,
					struct power_supply *psy, struct charger_manager **manager)
{
	if (IS_ERR_OR_NULL(dev)) {
		lc_err("dev is_err_or_null\n");
		return;
	}

	psy = dev_get_drvdata(dev);
	if (IS_ERR_OR_NULL(psy)) {
		lc_err("psy is_err_or_null\n");
		return;
	}

	*manager = power_supply_get_drvdata(psy);
	if (IS_ERR_OR_NULL(*manager)) {
		lc_err("manager is_err_or_null\n");
		return;
	}
}
#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
void xm_uevent_report(struct charger_manager *manager);
#endif
static ssize_t real_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *usb_psy = NULL;
	struct charger_manager *manager = NULL;
	enum vbus_type vbus_type = VBUS_TYPE_NONE;
	int ret;

	charger_manager_from_psy(dev, usb_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->charger)) {
		lc_err("%s:manager->charger is_err_or_null\n");
		goto out;
	}

	ret = charger_get_vbus_type(manager->charger, &vbus_type);
	if (ret < 0){
		lc_err("Couldn't get usb type ret=%d\n", ret);
		goto out;
	}

out:
	if(manager->pd_active == CHARGE_PD_ACTIVE) {
		vbus_type = VBUS_TYPE_PD;
	} else if(manager->pd_active == CHARGE_PD_PPS_ACTIVE) {
		vbus_type = VBUS_TYPE_PPS;
	}

#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
	xm_uevent_report(manager);
#endif
	if (manager->otg_status) {
		vbus_type = VBUS_TYPE_UNKNOW;
	}

	lc_debug("real_type = %d\n", vbus_type);

	return sprintf(buf, "%s\n", real_type_txt[vbus_type]);
}

static struct device_attribute real_type_attr =
	__ATTR(real_type, 0644, real_type_show, NULL);

static ssize_t typec_cc_orientation_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *usb_psy = NULL;
	struct charger_manager *manager = NULL;
	bool usb_online = false;
	bool otg_value = false;


	charger_manager_from_psy(dev, usb_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	charger_get_otg_status(manager->charger, &otg_value);
	charger_get_online(manager->charger, &usb_online);

	if (usb_online == false && otg_value == false)
		return scnprintf(buf, PAGE_SIZE, "%d\n", 0);
	else if (IS_ERR_OR_NULL(manager->tcpc)) {
		manager->tcpc = tcpc_dev_get_by_name("type_c_port0");
		if (IS_ERR_OR_NULL(manager->tcpc)) {
			lc_err("manager->tcpc is_err_or_null\n");
			return PTR_ERR(manager);
		}
	}

	return scnprintf(buf, PAGE_SIZE, "%d\n", manager->tcpc->typec_polarity + 1);
}
static struct device_attribute typec_cc_orientation_attr =
	__ATTR(typec_cc_orientation, 0644, typec_cc_orientation_show, NULL);

static ssize_t usb_otg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *usb_psy = NULL;
	struct charger_manager *manager = NULL;
	bool otg_value = false;
	int ret;

	charger_manager_from_psy(dev, usb_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	ret = charger_get_otg_status(manager->charger, &otg_value);
	if (ret < 0)
		lc_err("can not get otg status\n");

	return scnprintf(buf, PAGE_SIZE, "%d\n", otg_value);
}
static struct device_attribute usb_otg_attr =
	__ATTR(usb_otg, 0644, usb_otg_show, NULL);

#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
static int get_apdo_max(struct charger_manager *manager) {
	struct adapter_dev *adapter = NULL;
	int apdo_max = 0;
	int ret =0;

	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);
	if (manager->pd_active != CHARGE_PD_PPS_ACTIVE) {
		goto done;
	}

	adapter = adapter_find_dev_by_name("pd_adapter1");
	if (adapter == NULL || g_policy == NULL )
		goto done;

	ret = adapter_get_cap(adapter, &g_policy->cap);
		if (ret < 0) {
			lc_info("adapter get cap failed\n");
			goto done;
		}
	chargerpump_policy_check_adapter_cap(g_policy, &g_policy->cap);

	apdo_max = g_policy->cap.volt_max[g_policy->cap_nr] *
		g_policy->cap.curr_max[g_policy->cap_nr] / 1000000;

done:
	return apdo_max;
}

static ssize_t apdo_max_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *usb_psy = NULL;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, usb_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	manager->apdo_max = get_apdo_max(manager);

	lc_info("apdo_max = %d\n", manager->apdo_max);

	return scnprintf(buf, PAGE_SIZE, "%d\n", manager->apdo_max);
}
static struct device_attribute apdo_max_attr =
	__ATTR(apdo_max, 0644, apdo_max_show, NULL);

static int quick_charge_type(struct charger_manager *manager)
{
	enum quick_charge_type quick_charge_type = QUICK_CHARGE_NORMAL;
	enum vbus_type vbus_type = VBUS_TYPE_NONE;
	union power_supply_propval pval = {0, };

	bool usbpd_verifed = false;
	int ret = 0;
	int i = 0;

	if (IS_ERR_OR_NULL(manager->charger)) {
		lc_err("manager->charger is_err_or_null\n");
		return PTR_ERR(manager->charger);
	}

	if (IS_ERR_OR_NULL(manager->batt_psy)) {
		lc_err("manager->charger is_err_or_null\n");
		return PTR_ERR(manager->batt_psy);
	}

	if (IS_ERR_OR_NULL(manager->usb_psy)) {
		lc_err("manager->charger is_err_or_null\n");
		return PTR_ERR(manager->usb_psy);
	}

#if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
	if (IS_ERR_OR_NULL(manager->pd_adapter)) {
		lc_err("manager->pd_adapter is_err_or_null\n");
		return PTR_ERR(manager->pd_adapter);
	}

	ret = adapter_get_usbpd_verifed(manager->pd_adapter, &usbpd_verifed);
	if (ret < 0){
		lc_err("Couldn't get usbpd verifed ret=%d\n", ret);
		return ret;
	}
#endif

	ret = power_supply_get_property(manager->usb_psy, POWER_SUPPLY_PROP_ONLINE, &pval);
	if (ret < 0) {
		lc_err("Couldn't get usb online ret=%d\n", ret);
		return -EINVAL;
	}

	if (!(pval.intval))
		return -EINVAL;

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret < 0) {
		lc_err("Couldn't get bat temp ret=%d\n", ret);
		return -EINVAL;
	}

	ret = charger_get_vbus_type(manager->charger, &vbus_type);
	if (ret < 0){
		lc_err("Couldn't get usb type ret=%d\n", ret);
		return ret;
	}

	while (quick_charge_map[i].adap_type != 0) {
		if (vbus_type == quick_charge_map[i].adap_type) {
			quick_charge_type = quick_charge_map[i].adap_cap;
		}
		i++;
	}

	if (manager->pd_active)
		quick_charge_type = QUICK_CHARGE_FAST;

	if (manager->pd_active == CHARGE_PD_PPS_ACTIVE)
		quick_charge_type = QUICK_CHARGE_TURBE;

	if (pval.intval >= BATTERY_WARM_TEMP || pval.intval <= BATTERY_COLD_TEMP) {
		quick_charge_type = QUICK_CHARGE_NORMAL;
	}

	lc_debug("quick_charge_type = %d\n", quick_charge_type);

	return quick_charge_type;
}

static ssize_t quick_charge_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;
	struct power_supply *usb_psy = NULL;

	charger_manager_from_psy(dev, usb_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	return scnprintf(buf, PAGE_SIZE, "%d\n", quick_charge_type(manager));
}

static struct device_attribute quick_charge_type_attr =
	__ATTR(quick_charge_type, 0644, quick_charge_type_show, NULL);
#endif

static const char * const usb_typec_mode_text[] = {
	"Nothing attached", "Source attached", "Sink attached",
	"Audio Adapter", "Non compliant",
};
static ssize_t typec_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *usb_psy = NULL;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, usb_psy, &manager);
	if(IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->tcpc)) {
		manager->tcpc = tcpc_dev_get_by_name("type_c_port0");
		if (IS_ERR_OR_NULL(manager->tcpc)) {
			lc_err("manager->tcpc is_err_or_null\n");
			return PTR_ERR(manager);
		}
	}

	return scnprintf(buf, PAGE_SIZE, "%s\n",usb_typec_mode_text[manager->tcpc->typec_mode]);
}

static struct device_attribute typec_mode_attr = __ATTR(typec_mode,0644,typec_mode_show,NULL);

static ssize_t mtbf_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val;

	lc_info("mtbf_mode_store start\n");

	if (kstrtoint(buf, 10, &val)) {
		lc_info("set buf error %s\n", buf);
		return -EINVAL;
	}

	if (val != 0) {
		is_mtbf_mode = 1;
		lc_info("is_mtbf_mode = 1\n");
	} else {
		is_mtbf_mode = 0;
		lc_info("is_mtbf_mode = 0\n");
	}
	return count;
}
static int ps_get_max_charge_cntl_limit(struct thermal_cooling_device *tcd,
					unsigned long *state)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;
	psy = tcd->devdata;
	ret = power_supply_get_property(psy,
			POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX, &val);
	if (ret)
		return ret;
	*state = val.intval;
	return ret;
}

static int ps_get_cur_charge_cntl_limit(struct thermal_cooling_device *tcd,
					unsigned long *state)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;
	psy = tcd->devdata;
	ret = power_supply_get_property(psy,
			POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT, &val);
	if (ret)
		return ret;
	*state = val.intval;
	return ret;
}
static int ps_set_cur_charge_cntl_limit(struct thermal_cooling_device *tcd,
					unsigned long state)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;
	psy = tcd->devdata;
	val.intval = state;
	ret = charger_batt_set_property(psy,
		POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT, &val);
	return ret;
}
static const struct thermal_cooling_device_ops psy_tcd_ops = {
	.get_max_state = ps_get_max_charge_cntl_limit,
	.get_cur_state = ps_get_cur_charge_cntl_limit,
	.set_cur_state = ps_set_cur_charge_cntl_limit,
};
static int psy_register_cooler(struct power_supply *psy)
{
	int i;
	for (i = 0; i < psy->desc->num_properties; i++) {
		if (psy->desc->properties[i] ==
				POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT) {
			if (psy->dev.parent)
				psy->tcd = thermal_of_cooling_device_register(
						dev_of_node(psy->dev.parent),
						(char *)psy->desc->name,
						psy, &psy_tcd_ops);
			else
				psy->tcd = thermal_cooling_device_register(
						(char *)psy->desc->name,
						psy, &psy_tcd_ops);
			return PTR_ERR_OR_ZERO(psy->tcd);
		}
	}
	return 0;
}
int psy_unregister_cooler(struct charger_manager *manager)
{
	if (IS_ERR_OR_NULL(manager)) {
		lc_err("manager is_err_or_null\n");
		return PTR_ERR(manager);
	}

	if (IS_ERR_OR_NULL(manager->batt_psy)) {
	lc_err("batt psy register failed\n");
		return PTR_ERR(manager->batt_psy);
	}

	if (IS_ERR_OR_NULL(manager->batt_psy->tcd)){
		return -1;
        }
	thermal_cooling_device_unregister(manager->batt_psy->tcd);
      return 0;
}
EXPORT_SYMBOL(psy_unregister_cooler);

static ssize_t mtbf_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", is_mtbf_mode);
}

static struct device_attribute mtbf_mode_attr = __ATTR(mtbf_mode,0644,mtbf_mode_show,mtbf_mode_store);

bool is_mtbf_mode_func(void)
{
	return is_mtbf_mode;
}
EXPORT_SYMBOL_GPL(is_mtbf_mode_func);

static struct attribute *usb_psy_attrs[] = {
	&real_type_attr.attr,
	&typec_cc_orientation_attr.attr,
	&usb_otg_attr.attr,
#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
	&apdo_max_attr.attr,
	&quick_charge_type_attr.attr,
#endif
	&typec_mode_attr.attr,
	&mtbf_mode_attr.attr,
	NULL,
};

static const struct attribute_group usb_psy_attrs_group = {
	.attrs = usb_psy_attrs,
};
int lc_usb_sysfs_create_group(struct charger_manager *manager)
{
	return sysfs_create_group(&manager->usb_psy->dev.kobj,
								&usb_psy_attrs_group);
}
EXPORT_SYMBOL(lc_usb_sysfs_create_group);

static ssize_t input_suspend_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return sprintf(buf, "%d\n", 0);
	else
		return sprintf(buf, "%d\n", manager->input_suspend);
}
static ssize_t input_suspend_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	struct bq_fg_chip *bp = NULL;
	int val;

	lc_info("input_suspend_store start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	bp = fuel_gauge_get_private(manager->fuel_gauge);
	if (IS_ERR_OR_NULL(bp))
		return PTR_ERR(bp);

	if (IS_ERR_OR_NULL(manager->main_chg_disable_votable)) {
		lc_info("main_chg_disable_votable not found\n");
		return PTR_ERR(manager->main_chg_disable_votable);
	}

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	bp->input_suspend = val;
	manager->input_suspend = val;
	if (manager->input_suspend) {
		vote(manager->main_chg_disable_votable, FACTORY_KIT_VOTER, true, 1);
	} else {
		vote(manager->main_chg_disable_votable, FACTORY_KIT_VOTER, false, 0);
	}
	lc_info("manager->input_suspend = %d\n", manager->input_suspend);
	return count;
}
static struct device_attribute input_suspend_attr =
	__ATTR(input_suspend, 0644, input_suspend_show, input_suspend_store);

#ifndef CONFIG_FACTORY_BUILD
#if IS_ENABLED(CONFIG_RUST_DETECTION)
static ssize_t lpd_control_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return sprintf(buf, "%d\n", 0);
	else
		return sprintf(buf, "%d\n", manager->lpd_control);
}
static ssize_t lpd_control_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	int val;

	lc_info("lpd_control_store start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	manager->lpd_control = val;
	lc_info("manager->lpd_control = %d\n", manager->lpd_control);
	return count;
}
static struct device_attribute lpd_control_attr =
	__ATTR(lpd_control, 0644, lpd_control_show, lpd_control_store);

static ssize_t lpd_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return sprintf(buf, "%d\n", 0);
	else
		return sprintf(buf, "%d\n", manager->lpd_status);
}
static ssize_t lpd_status_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	int val;

	lc_info("lpd_status_store start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	manager->lpd_status = val;
	lc_info("manager->lpd_status = %d\n", manager->lpd_status);
	return count;
}
static struct device_attribute lpd_status_attr =
	__ATTR(lpd_status, 0644, lpd_status_show, lpd_status_store);

static ssize_t decrease_volt_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return sprintf(buf, "%d\n", 0);
	else
		return sprintf(buf, "%d\n", manager->decrease_volt);
}

static ssize_t decrease_volt_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	int val;

	lc_info("decrease_volt_store start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	manager->decrease_volt = val;
	lc_info("manager->decrease_volt = %d\n", manager->decrease_volt);
	return count;
}
static struct device_attribute decrease_volt_attr =
	__ATTR(decrease_volt, 0644, decrease_volt_show, decrease_volt_store);
#endif
#endif

static ssize_t shipmode_count_reset_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	return sprintf(buf, "%d\n", manager->shippingmode);
}

static ssize_t shipmode_count_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	union power_supply_propval volt;
	int shipmode_cnt = 0;
	int i = 0;
	int ret;
	int val;

	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	batt_psy = dev_get_drvdata(dev);
	if (IS_ERR_OR_NULL(batt_psy)) {
		lc_err("batt_psy is_err_or_null\n");
		return PTR_ERR(batt_psy);
	}

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	manager->shippingmode = val;

//cqr vbat ctrol start

	for (i = 0; i < CQR_READ_VBAT_TIMES; i++) {
			ret = power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &volt);
			if (ret) {
				lc_err("get vbat fail\n");
				continue;
			}

			msleep(1);

			if (volt.intval < CQR_VBAT_MIM || volt.intval > CQR_VBAT_MAX) {
				shipmode_cnt++;
				lc_info("%s %d %d %d %d %d\n",
					__func__, manager->shippingmode, volt.intval, CQR_VBAT_MAX, CQR_VBAT_MIM, shipmode_cnt);
			}
		}

	if (shipmode_cnt >= CQR_VBAT_OUT_RANGE_CNT) {
		manager->shippingmode = false;
		lc_info("%s after %d\n", __func__, manager->shippingmode);
		ret = charger_shipmode_count_reset(manager->charger, manager->shippingmode);
		if (ret < 0)
			lc_err("can not set shipmode\n");
		return -EINVAL;
	}
//cqr vbat ctrol end

	ret = charger_shipmode_count_reset(manager->charger, manager->shippingmode);
	if (ret < 0)
		lc_err("can not set shipmode\n");

	lc_info("shippingmode = %d\n", manager->shippingmode);

	return count;
}
static struct device_attribute shipmode_count_reset_attr =
	__ATTR(shipmode_count_reset, 0644, shipmode_count_reset_show, shipmode_count_reset_store);

static ssize_t reverse_quick_charge_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	return scnprintf(buf, PAGE_SIZE, "%d\n", manager->reverse_quick_charge);
}

static ssize_t reverse_quick_charge_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val;
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val)) {
		lc_info("set buf error %s\n", buf);
		return -EINVAL;
	}

	if (val) {
		schedule_delayed_work(&manager->reverse_charge_monitor_work, msecs_to_jiffies(REVERSE_CHARGE_MONITOR_INTERVAL));
		manager->reverse_quick_charge = true;
	}

	return count;
}
static struct device_attribute reverse_quick_charge_attr =
	__ATTR(reverse_quick_charge, 0660, reverse_quick_charge_show, reverse_quick_charge_store);

#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
static ssize_t soc_decimal_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	int soc_decimal = 0;

	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
		lc_err("manager->fuel_gauge is_err_or_null\n");
		soc_decimal = 0;
		goto out;
	}

	soc_decimal = fuel_gauge_get_soc_decimal(manager->fuel_gauge);
	if (soc_decimal < 0)
		soc_decimal = 0;

out:
	return scnprintf(buf, PAGE_SIZE, "%d\n", soc_decimal);

}
static struct device_attribute soc_decimal_attr =
	__ATTR(soc_decimal, 0644, soc_decimal_show, NULL);

static ssize_t mtbf_current_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", is_mtbf_mode);
}

static ssize_t mtbf_current_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	struct bq_fg_chip *bp = NULL;
	int val;

	lc_info("mtbf_mode_store start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	bp = fuel_gauge_get_private(manager->fuel_gauge);
	if (IS_ERR_OR_NULL(bp))
		return PTR_ERR(bp);

	if (kstrtoint(buf, 10, &val)) {
		lc_info("set buf error %s\n", buf);
		return -EINVAL;
	}

	bp->mtbf_current = val;

	if (val != 0) {
		is_mtbf_mode = 1;
		lc_info("is_mtbf_mode = 1\n");
	} else {
		is_mtbf_mode = 0;
		lc_info("is_mtbf_mode = 0\n");
	}
	return count;
}
static struct device_attribute mtbf_current_attr =
	__ATTR(mtbf_current, 0644, mtbf_current_show, mtbf_current_store);

static ssize_t soc_decimal_rate_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{

	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	int soc_decimal_rate = 0;

	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
		lc_err("%s:manager->fuel_gauge is_err_or_null\n");
		goto out;
	}

	soc_decimal_rate = fuel_gauge_get_soc_decimal_rate(manager->fuel_gauge);
	if (soc_decimal_rate < 0 || soc_decimal_rate > 100)
		soc_decimal_rate = 0;

out:
	return scnprintf(buf, PAGE_SIZE, "%d\n", soc_decimal_rate);
}
static struct device_attribute soc_decimal_rate_attr =
	__ATTR(soc_decimal_rate, 0644, soc_decimal_rate_show, NULL);

void xm_uevent_report(struct charger_manager *manager)
{
	int soc_decimal_rate = 0;
	int soc_decimal = 0;

	char quick_charge_string[64];
	char soc_decimal_string[64];
	char soc_decimal_string_rate[64];

	char *envp[] = {
		quick_charge_string,
		soc_decimal_string,
		soc_decimal_string_rate,
		NULL,
	};

	sprintf(quick_charge_string, "POWER_SUPPLY_QUICK_CHARGE_TYPE=%d", quick_charge_type(manager));

	soc_decimal = fuel_gauge_get_soc_decimal(manager->fuel_gauge);
	if (soc_decimal < 0)
		soc_decimal = 0;
	sprintf(soc_decimal_string, "POWER_SUPPLY_SOC_DECIMAL=%d", soc_decimal);

	soc_decimal_rate = fuel_gauge_get_soc_decimal_rate(manager->fuel_gauge);
	if (soc_decimal_rate < 0 || soc_decimal_rate > 100)
		soc_decimal_rate = 0;
	sprintf(soc_decimal_string_rate, "POWER_SUPPLY_SOC_DECIMAL_RATE=%d", soc_decimal_rate);

	kobject_uevent_env(&manager->dev->kobj, KOBJ_CHANGE, envp);

	lc_debug("envp[0]:%s envp[1]:%s envp[2]:%s",envp[0],envp[1],envp[2]);
}
EXPORT_SYMBOL(xm_uevent_report);
#endif

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
static ssize_t smart_chg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	// DECLEAR_BITMAP(func_type, SMART_CHG_FEATURE_MAX_NUM);
	int val;
	bool en_ret;
	unsigned long func_type;
	int func_val;
	int bit_pos;
	int all_func_status;
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
			return PTR_ERR(manager);

	if (kstrtoint(buf, 16, &val))
			return -EINVAL;

	en_ret = val & 0x1;
	func_type = (val & 0xFFFE) >> 1;
	func_val = val >> 16;

	lc_info("get val:%#X, func_type:%#X, en_ret:%d, func_val:%d\n",
			val, func_type, en_ret, func_val);

	bit_pos = find_first_bit(&func_type, SMART_CHG_FEATURE_MAX_NUM);

   if(bit_pos == SMART_CHG_FEATURE_MAX_NUM || find_next_bit(&func_type, SMART_CHG_FEATURE_MAX_NUM , bit_pos + 1) != SMART_CHG_FEATURE_MAX_NUM){
           lc_info("ERROR: zero or more than one func type!\n");
           lc_info("find_next_bit = %d, bit_pos = %d\n",
                   find_next_bit(&func_type, SMART_CHG_FEATURE_MAX_NUM , bit_pos + 1), bit_pos);
           set_error(manager);
   } else
           set_success(manager);

  // if func_type bit0 is 1, bit_pos = 0, not 1. so ++bit_pos.
   if(!smart_chg_is_error(manager))
           handle_smart_chg_functype(manager, ++bit_pos, en_ret, func_val);

   /* update smart_chg[0] status */
   all_func_status = handle_smart_chg_functype_status(manager);
   manager->smart_chg_cmd = all_func_status;
   manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret = all_func_status & 0x1;
   manager->smart_charge[SMART_CHG_STATUS_FLAG].active_status = (all_func_status & 0xFFFE) >> 1;

   return count;
}

static ssize_t smart_chg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	//int *val = 0;

	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
			return PTR_ERR(manager);

	return sprintf(buf, "%d\n", manager->smart_chg_cmd);
}

static struct device_attribute smart_chg_attr =
		__ATTR(smart_chg, 0644, smart_chg_show, smart_chg_store);

static ssize_t smart_batt_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
        struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
        int val = 0;
        struct votable	*fv_votable = NULL;

        charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
			return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val))
			return -EINVAL;

        manager->smart_batt = val;
        lc_info("smart_batt = %d\n", manager->smart_batt);
        fv_votable = find_votable("MAIN_FV");
        if (!fv_votable) {
                lc_info("failed to get fv_votable\n");
        }else{
                rerun_election(fv_votable);
        }
        return count;
}

static ssize_t smart_batt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

        charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
			return PTR_ERR(manager);

	return sprintf(buf, "%d\n", manager->smart_batt);
}

static struct device_attribute smart_batt_attr =
		__ATTR(smart_batt, 0644, smart_batt_show, smart_batt_store);

static ssize_t night_charging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
        struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
        bool val;

        charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtobool(buf, &val))
		return -EINVAL;

        manager->night_charging = val;

        return count;
}

static ssize_t night_charging_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

        charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	return sprintf(buf, "%d\n", manager->night_charging);
}

static struct device_attribute night_charging_attr =
		__ATTR(night_charging, 0644, night_charging_show, night_charging_store);
#endif
#ifndef CONFIG_FACTORY_BUILD
static ssize_t otg_ui_support_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

        charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);
	return sprintf(buf, "%d\n", 1);
}
static struct device_attribute otg_ui_support_attr =
		__ATTR(otg_ui_support, 0644, otg_ui_support_show, NULL);
static ssize_t cid_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int val = manual_get_cid_status();
	lc_info("[%s] val: %d\n", __func__, val);
	return sprintf(buf, "%d\n", val);
}
static struct device_attribute cid_status_attr =
		__ATTR(cid_status, 0644, cid_status_show, NULL);
static ssize_t cc_toggle_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	int val = 0;

        charger_manager_from_psy(dev, batt_psy, &manager);

	manual_get_cc_toggle(&manager->control_cc_toggle);

	val = !!manager->control_cc_toggle;
	lc_info("[%s] val: %d\n", __func__, val);
	return sprintf(buf, "%d\n", val);
}

static ssize_t cc_toggle_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
        struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	int val = 0;

        charger_manager_from_psy(dev, batt_psy, &manager);
        if (kstrtoint(buf, 10, &val))
		return -EINVAL;
	manager->control_cc_toggle = val;
	manual_set_cc_toggle(!!val);
	lc_info("[%s] *val: %d\n", __func__, manager->control_cc_toggle);
	return count;
}
static struct device_attribute cc_toggle_attr =
		__ATTR(cc_toggle, 0644, cc_toggle_show, cc_toggle_store);
#endif

#define	FG_MAC_CMD_MANU_DATE	0x004D
#define 	FG_MAC_CMD_MANU_INFO	0x0070

static ssize_t manufacturing_date_show(struct device *dev, struct device_attribute *attr, char *ubuf)
{
	int ret;
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	struct bq_fg_chip *bq = NULL;
	lc_info("manufacturing_date_show start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	bq = fuel_gauge_get_private(manager->fuel_gauge);
	if (IS_ERR_OR_NULL(bq))
		return PTR_ERR(bq);

	if (!bq->bat_sn[0]) {
		mutex_lock(&bq->rw_lock);
		ret = fuel_guage_mac_read_block(manager->fuel_gauge, FG_MAC_CMD_MANU_INFO, bq->bat_sn, 32);
		mutex_unlock(&bq->rw_lock);
		if (ret) {
			lc_err("failed to get FG_MAC_CMD_MANU_INFO:%d\n", ret);
			return -EINVAL;
		}
	}
	lc_info("battery manufacture date:%02X %02X %02X %02X\n",bq->bat_sn[6], bq->bat_sn[7], bq->bat_sn[8], bq->bat_sn[9]);
	// Year
	ubuf[0] = '2';
	ubuf[1] = '0';
	ubuf[2] = '2';
	ubuf[3] = bq->bat_sn[6];
	// month
	ubuf[4] = bq->bat_sn[7] <= '9' ? '0' : '1';
	ubuf[5] = bq->bat_sn[7] <= '9' ? bq->bat_sn[7] : bq->bat_sn[7] - 'A';
	// day
	ubuf[6] = bq->bat_sn[8];
	ubuf[7] = bq->bat_sn[9];
	ubuf[8] = '\0';
	lc_info("battery manufacture date:%s\n", ubuf);

	return strlen(ubuf);
}
static struct device_attribute manufacturing_date_attr =
	__ATTR(manufacturing_date, 0644, manufacturing_date_show, NULL);

static ssize_t soh_sn_show(struct device *dev, struct device_attribute *attr, char *ubuf)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	struct bq_fg_chip *bq = NULL;
	int ret;

	lc_info("soh_sn_show start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);
	
	bq = fuel_gauge_get_private(manager->fuel_gauge);
	if (IS_ERR_OR_NULL(bq))
		return PTR_ERR(bq);

	if (!bq->bat_sn[0]) {
		mutex_lock(&bq->rw_lock);
		ret = fuel_guage_mac_read_block(manager->fuel_gauge, FG_MAC_CMD_MANU_INFO, bq->bat_sn, 32);
		mutex_unlock(&bq->rw_lock);
		if (ret) {
			memset(ubuf, '0', 32);
			ubuf[32] = '\0';
			lc_err("failed to get FG_MAC_CMD_MANU_INFO:%d\n", ret);
			return -EINVAL;
		} 
	}
	memcpy(ubuf, bq->bat_sn, 32);
	ubuf[32] = '\0';
	print_hex_dump(KERN_ERR, "battery sn hex:", DUMP_PREFIX_NONE, 16, 1, ubuf, 32, 0);
	lc_info("battery sn string:%s\n", ubuf);

	return strlen(ubuf);
}
static struct device_attribute soh_sn_attr =
	__ATTR(soh_sn, 0644, soh_sn_show, NULL);

static ssize_t first_usage_date_show(struct device *dev, struct device_attribute *attr, char *ubuf)
{
	int ret;
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	struct bq_fg_chip *bq = NULL;

	lc_info("first_usage_date_show start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	bq = fuel_gauge_get_private(manager->fuel_gauge);
	if (IS_ERR_OR_NULL(bq))
		return PTR_ERR(bq);

	if (!bq->mi_infoC_valid) {
		mutex_lock(&bq->rw_lock);
		ret = fuel_guage_mac_read_block(manager->fuel_gauge, FG_MAC_CMD_MANU_INFOC, bq->mi_infoC, 32);
		mutex_unlock(&bq->rw_lock);
		if (ret) {
			memset(ubuf, '9', 8);
			ubuf[8] = '\0';
			lc_err("failed to get FG_MAC_CMD_MANU_INFOC:%d\n", ret);
			return -EINVAL;
		} else 
			bq->mi_infoC_valid = 1;
	}
	lc_info("battery activiate date hex: %02X%02X%02X\n",
	bq->mi_infoC[11], bq->mi_infoC[12], bq->mi_infoC[13]);
	if (bq->mi_infoC[11] == 0x00 && bq->mi_infoC[12] == 0x00 && bq->mi_infoC[13] == 0x00) {
		memset(ubuf, '0', 8);
		ubuf[8] = '\0';
		lc_err("reset data to 0\n");
	} else {
		ubuf[0] = '2';
		ubuf[1] = '0';
		ubuf[2] = '0' + bq->mi_infoC[11] / 10;
		ubuf[3] = '0' + bq->mi_infoC[11] % 10;
		ubuf[4] = '0' + bq->mi_infoC[12] / 10;
		ubuf[5] = '0' + bq->mi_infoC[12] % 10;
		ubuf[6] = '0' + bq->mi_infoC[13] / 10;
		ubuf[7] = '0' + bq->mi_infoC[13] % 10;
		ubuf[8] = '\0';
		lc_info("battery activiate date string:%s\n", ubuf);
	}

	return strlen(ubuf);
}

static ssize_t first_usage_date_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	char date_str[8];
	int i, j = 0, ret = -EINVAL, date_len;
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	struct bq_fg_chip *bq = NULL;

	lc_info("first_usage_date_store start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	bq = fuel_gauge_get_private(manager->fuel_gauge);
	if (IS_ERR_OR_NULL(bq))
		return PTR_ERR(bq);

	if (!bq->mi_infoC_valid) {
		mutex_lock(&bq->rw_lock);
		ret = fuel_guage_mac_read_block(manager->fuel_gauge, FG_MAC_CMD_MANU_INFOC, bq->mi_infoC, 32);
		mutex_unlock(&bq->rw_lock);
		if (ret) {
			lc_err("failed to read FG_MAC_CMD_MANU_INFOC:%d\n", ret);
			return -EPERM;
		} else
			bq->mi_infoC_valid = 1;
	}

	date_len = strlen(buf);
	for (i = 0; i < date_len; i++) {
		if (buf[i] != ' ' && buf[i] != '\t' && buf[i] != '\n') {
			if (j >= 8) {
				lc_err("date length too large\n");
				return -E2BIG;
			}
			if (buf[i] < '0' || buf[i] > '9') {
				lc_err("date has invalid char:%c(0x%02x)\n", buf[i], buf[i]);
				return -EINVAL;
			}
			date_str[j++] = buf[i];
		}
	}
	lc_info("activiate date hex: %02X %02X %02X %02X %02X %02X %02X %02X\n",
		date_str[0], date_str[1], date_str[2], date_str[3], date_str[4], date_str[5],  date_str[6],  date_str[7]);
	bq->mi_infoC[11] = (date_str[2] - '0') * 10 + (date_str[3] - '0');
	bq->mi_infoC[12] = (date_str[4] - '0') * 10 + (date_str[5] - '0');
	bq->mi_infoC[13] = (date_str[6] - '0') * 10 + (date_str[7] - '0');
	mutex_lock(&bq->rw_lock);
	ret = fuel_guage_mac_write_block(manager->fuel_gauge, FG_MAC_CMD_MANU_INFOC, bq->mi_infoC, 32);
	mutex_unlock(&bq->rw_lock);
	if (ret) {
		bq->mi_infoC_valid = 0;
		lc_err("failed to read FG_MAC_CMD_MANU_INFOC:%d\n", ret);
		return -EPERM;
	}
	return len;
}
static struct device_attribute first_usage_date_attr =
	__ATTR(first_usage_date, 0644, first_usage_date_show, first_usage_date_store);


static ssize_t charger_partition_poweroffmode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int rc = 0;
	int val = 0;
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

	lc_info("first_usage_date_store start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	rc = charger_partition1_get_prop(manager->fuel_gauge, CHARGER_PARTITION_PROP_POWER_OFF_MODE, &val);
	if(rc < 0){
		lc_err("[charger] %s get power_off_mode from charger parition failed, ret = %d\n", __func__, rc);
		return -EINVAL;
	}
	lc_err("[charger] %s power_off_mode: %d \n", __func__, val);
	return sprintf(buf, "%d\n", val);
}

static ssize_t charger_partition_poweroffmode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int rc = 0;
	int val = 0;
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

	lc_info("first_usage_date_store start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val))
			return -EINVAL;

	rc = charger_partition1_set_prop(manager->fuel_gauge, CHARGER_PARTITION_PROP_POWER_OFF_MODE, val);
	if (rc < 0) {
		lc_err("[charger] %s set power_off_mode to charger parition failed, ret = %d\n", __func__, rc);
		return -EINVAL;
	}
	return count;
}
static struct device_attribute charger_partition_poweroffmode_attr =
	__ATTR(charger_partition_poweroffmode, 0644, charger_partition_poweroffmode_show, charger_partition_poweroffmode_store);

static struct attribute *batt_psy_attrs[] = {
	&input_suspend_attr.attr,
	&shipmode_count_reset_attr.attr,
	&reverse_quick_charge_attr.attr,
	&manufacturing_date_attr.attr,
	&first_usage_date_attr.attr,
	&soh_sn_attr.attr,
	&charger_partition_poweroffmode_attr.attr,
#ifndef CONFIG_FACTORY_BUILD
#if IS_ENABLED(CONFIG_RUST_DETECTION)
	&lpd_control_attr.attr,
	&decrease_volt_attr.attr,
	&lpd_status_attr.attr,
#endif
#endif
#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
	&soc_decimal_attr.attr,
	&soc_decimal_rate_attr.attr,
#endif
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	&smart_chg_attr.attr,
	&smart_batt_attr.attr,
	&night_charging_attr.attr,
#endif
	&mtbf_current_attr.attr,
#ifndef CONFIG_FACTORY_BUILD
	&otg_ui_support_attr.attr,
	&cid_status_attr.attr,
	&cc_toggle_attr.attr,
#endif
	NULL,
};

static const struct attribute_group batt_psy_attrs_group = {
	.attrs = batt_psy_attrs,
};

int lc_batt_sysfs_create_group(struct charger_manager *manager)
{
	return sysfs_create_group(&manager->batt_psy->dev.kobj,
								&batt_psy_attrs_group);
}
EXPORT_SYMBOL(lc_batt_sysfs_create_group);

MODULE_DESCRIPTION("LC Charger sysfs");
MODULE_LICENSE("GPL v2");
