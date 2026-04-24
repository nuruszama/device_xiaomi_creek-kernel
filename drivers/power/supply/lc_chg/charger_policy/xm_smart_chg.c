// SPDX-License-Identifier: GPL-2.0
#include <linux/delay.h>
#include "xm_smart_chg.h"
#include "../lc_printk.h"

#ifdef TAG
#undef TAG
#define  TAG "[SMART]"
#endif
#define XM_SMART_CHG_DEBUG 0

void set_error(struct charger_manager *manager)
{
	manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret = 1;
	lc_err("xm %s en_ret=%d\n", __func__, manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret);
}

void set_success(struct charger_manager *manager)
{
	manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret = 0;
	lc_err("xm %s en_ret=%d\n", __func__, manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret);
}

int smart_chg_is_error(struct charger_manager *manager)
{
	return manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret? true : false;
}

void handle_smart_chg_functype(struct charger_manager *manager,
	const int func_type, const int en_ret, const int func_val)
{
	switch (func_type)
	{
	case SMART_CHG_FEATURE_MIN_NUM ... SMART_CHG_FEATURE_MAX_NUM:
		manager->smart_charge[func_type].en_ret = en_ret;
		manager->smart_charge[func_type].active_status = false;
		manager->smart_charge[func_type].func_val = func_val;
		set_success(manager);
		lc_err("xm set func_type:%d, en_ret = %d\n", func_type, en_ret);
		break;
	default:
		lc_err("xm ERROR: Not supported func type: %d\n", func_type);
		set_error(manager);
		break;
	}
}

int handle_smart_chg_functype_status(struct charger_manager *manager)
{
	int i;
	int all_func_status = 0;
	all_func_status |= !!manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret;	//handle bit0

	lc_err("all_func_status =%#X, en_ret=%d\n",all_func_status, manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret);

	/* save functype[i] enable status in all_func_status bit[i] */
	for(i = SMART_CHG_FEATURE_MIN_NUM; i <= SMART_CHG_FEATURE_MAX_NUM; i++){  //handle bit1 ~ bit SMART_CHG_FEATURE_MAX_NUM
		if(manager->smart_charge[i].en_ret)
			all_func_status |= BIT_MASK(i);
		else
			all_func_status &= ~BIT_MASK(i);

		lc_err("type:%d, en_ret=%d, active_status=%d,func_val=%d, all_func_status=%#X\n",
			i, manager->smart_charge[i].en_ret, manager->smart_charge[i].active_status, manager->smart_charge[i].func_val,all_func_status);
	}

	lc_err("all_func_status:%#X\n", all_func_status);
	return all_func_status;
}

static void xm_smart_chg_endurance_protect(struct charger_manager *manager)
{
    static int g_smart_soc = 80;
    union power_supply_propval pval = {0,};
    int ret = 0;

    ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
    if (ret < 0) {
        lc_err("%s get battery soc error.\n", __func__);
        return;
    } else {
        manager->soc = pval.intval;
    }

    lc_debug("enter, en_ret = %d, fun_val = %d, active_status = %d, smart_soc = %d, ui_soc =%d, smart_ctrl_en = %d\n",
            manager->smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret,
            manager->smart_charge[SMART_CHG_ENDURANCE_PRO].func_val,
            manager->smart_charge[SMART_CHG_ENDURANCE_PRO].active_status,
            g_smart_soc,
            manager->soc,
            manager->smart_ctrl_en);

    if(manager->smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret &&
                manager->soc >= manager->smart_charge[SMART_CHG_ENDURANCE_PRO].func_val &&
                manager->endurance_protect_flag == false) {
        manager->smart_charge[SMART_CHG_ENDURANCE_PRO].active_status = true;
        manager->endurance_protect_flag = true;
        g_smart_soc = manager->smart_charge[SMART_CHG_ENDURANCE_PRO].func_val;
        lc_info("soc>=80 enter xm_smart_chg_endurance_protect\n");
        vote(manager->total_fcc_votable, ENDURANCE_PRO_VOTER, true, 0);
        // if (g_policy->state == POLICY_RUNNING){
        //         chargerpump_policy_stop(g_policy);
        //         lc_err("monitor disable cp\n");
        // }
        lc_info("disable charger,soc >= %d\n", manager->smart_charge[SMART_CHG_ENDURANCE_PRO].func_val);
    } else if (manager->endurance_protect_flag && (!manager->smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret ||
                manager->soc <= 75)) {
        manager->smart_charge[SMART_CHG_ENDURANCE_PRO].active_status = false;
        manager->endurance_protect_flag = false;
        lc_info("soc<=75 exit xm_smart_chg_endurance_protect\n");
        vote(manager->total_fcc_votable, ENDURANCE_PRO_VOTER, false, 0);
        // if ((manager->pd_active == CHARGE_PD_PPS_ACTIVE) && (g_policy->state == POLICY_NO_START)){
        //         chargerpump_policy_start(g_policy);
        //         lc_err("monitor enable cp\n");
        // }
        lc_info("enable charger, soc = %d\n", manager->soc);
    }

    return;
}

void monitor_smart_chg(struct charger_manager *manager)
{
	union power_supply_propval pval = {0,};
	int ret = 0;
	static int g_smart_soc = 50;

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret < 0)
		lc_err("get battery soc error.\n");
	else
		manager->soc = pval.intval;

	if (g_policy == NULL)
		return;

	lc_err("monitor_smart_chg, en_ret = %d, fun_val = %d, active_status = %d, smart_soc = %d, ui_soc =%d, smart_ctrl_en = %d, state = %d\n",
		manager->smart_charge[SMART_CHG_NAVIGATION].en_ret,
		manager->smart_charge[SMART_CHG_NAVIGATION].func_val,
		manager->smart_charge[SMART_CHG_NAVIGATION].active_status,
		g_smart_soc,
		manager->soc,
		manager->smart_ctrl_en,
		g_policy->state);

	if(manager->smart_charge[SMART_CHG_NAVIGATION].en_ret && manager->soc >= manager->smart_charge[SMART_CHG_NAVIGATION].func_val && !manager->smart_charge[SMART_CHG_NAVIGATION].active_status)
	{
		manager->smart_charge[SMART_CHG_NAVIGATION].active_status = true;
		g_smart_soc = manager->smart_charge[SMART_CHG_NAVIGATION].func_val;
		//charger_set_chg(manager->charger, false);
                vote(manager->total_fcc_votable, "CHG_NAVIGATION_VOTER", true, 0);
		if (g_policy->state == POLICY_RUNNING){
			chargerpump_policy_stop(g_policy);
			lc_err("monitor disable cp\n");
		}
		manager->smart_ctrl_en = true;
		lc_err("monitor disable charger,soc >= %d\n", manager->smart_charge[SMART_CHG_NAVIGATION].func_val);
	}
	else if((((!manager->smart_charge[SMART_CHG_NAVIGATION].en_ret || manager->soc <= manager->smart_charge[SMART_CHG_NAVIGATION].func_val - 5) && manager->smart_charge[SMART_CHG_NAVIGATION].active_status) ||
		(!manager->smart_charge[SMART_CHG_NAVIGATION].en_ret && !manager->smart_charge[SMART_CHG_NAVIGATION].active_status)) && manager->smart_ctrl_en)
	{
		manager->smart_charge[SMART_CHG_NAVIGATION].active_status = false;
		//charger_set_chg(manager->charger, true);
                vote(manager->total_fcc_votable, "CHG_NAVIGATION_VOTER", false, 0);
		if ((manager->pd_active == CHARGE_PD_PPS_ACTIVE) && (g_policy->state == POLICY_NO_START)){
			chargerpump_policy_start(g_policy);
			lc_err("monitor enable cp\n");
		}
		manager->smart_ctrl_en = false;
		lc_err("monitor enable charger, soc <= %d\n", manager->smart_charge[SMART_CHG_NAVIGATION].func_val - 5);
	}

        if(manager->smart_charge[SMART_CHG_LOW_FAST].en_ret)
	{
                manager->smart_charge[SMART_CHG_LOW_FAST].active_status = true;
                lc_err("set smart_charge[SMART_CHG_LOW_FAST].en_ret = %d, enable low_fast\n", manager->smart_charge[SMART_CHG_LOW_FAST].en_ret);
	}
	else if(!manager->smart_charge[SMART_CHG_LOW_FAST].en_ret)
	{
		manager->smart_charge[SMART_CHG_LOW_FAST].active_status = false;
                lc_debug( "set smart_charge[SMART_CHG_LOW_FAST].en_ret = %d, disable low_fast\n", manager->smart_charge[SMART_CHG_LOW_FAST].en_ret);
	}
}

void get_fv_againg(struct charger_manager *manager, int cyclecount, int *fv_aging)
{
        int i = 0;

        while (cyclecount > manager->cyclecount[i])
                i++;
        *fv_aging = manager->dropfv[i];

        lc_debug("%s i = %d, fv_aging = %d\n", __func__, i, *fv_aging);
        return;
}

void get_drop_floatvolatge(struct charger_manager *manager)
{
        int ret = 0;
        union power_supply_propval pval;
        int cycle_count = 0;
        static last_fv_againg = 0;
        struct votable	*fv_votable = NULL;

        ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
        if (ret < 0) {
                lc_err("%s failed to get cycle_count prop", __func__);
                return;
        }
        cycle_count = pval.intval;

        get_fv_againg(manager, cycle_count, &manager->fv_againg);
        if(manager->fv_againg != last_fv_againg){
                fv_votable = find_votable("MAIN_FV");
                if (!fv_votable) {
                        lc_err("%s failed to get fv_votable\n", __func__);
                }else{
                        rerun_election(fv_votable);
                }
        }
        last_fv_againg = manager->fv_againg;
}

void monitor_night_charging(struct charger_manager *manager)
{
        if ((manager == NULL) || !manager->main_chg_disable_votable || !manager->cp_disable_votable)
		return;

        lc_debug("%s night_charging = %d, soc = %d\n", __func__, manager->night_charging, manager->soc);
        if (manager->night_charging && (manager->soc >= 80)) {
                manager->night_charging_flag = true;
                lc_err("%s disable charging\n", __func__);
                //charger_set_chg(manager->charger, false);
                vote(manager->total_fcc_votable, "NIGHT_CHARGING_VOTER", true, 0);
		if (g_policy->state == POLICY_RUNNING){
			chargerpump_policy_stop(g_policy);
			lc_err("%s monitor disable cp\n", __func__);
		}
	} else if(manager->night_charging_flag && (!manager->night_charging || manager->soc <=75)) {
                manager->night_charging_flag = false;
                lc_err("%s enable charging\n", __func__);
		//charger_set_chg(manager->charger, true);
                vote(manager->total_fcc_votable, "NIGHT_CHARGING_VOTER", false, 0);
		if ((manager->pd_active == CHARGE_PD_PPS_ACTIVE) && (g_policy->state == POLICY_NO_START)){
			chargerpump_policy_start(g_policy);
			lc_err("%s monitor enable cp\n", __func__);
		}
	}
}

void monitor_low_fast_strategy(struct charger_manager *manager)
{
        bool fast_flag = false;
        time64_t time_now = 0, delta_time = 0;
        static time64_t time_last = 0;
        static int last_level = 0;
        static bool hot_flag = false;

        if (manager == NULL)
		return;
        if (manager->system_temp_level <= 0)
		goto err;

        lc_err("%s soc = %d, thermal_level = %d, thermal_board_temp = %d, pd_active = %d, low_fast_plugin_flag = %d, low_fast_enable = %d, screen_state = %d, b_flag = %d\n", 
                __func__, manager->soc, manager->system_temp_level, manager->thermal_board_temp, manager->pd_active, manager->low_fast_plugin_flag, manager->smart_charge[SMART_CHG_LOW_FAST].active_status, 
                manager->sm.screen_state, manager->b_flag);

        if ((manager->pd_active == CHARGE_PD_PPS_ACTIVE) && (manager->soc <= 40) && (manager->low_fast_plugin_flag) && manager->smart_charge[SMART_CHG_LOW_FAST].active_status) {
		if (manager->thermal_parse_flags & PD_THERM_PARSE_ERROR) {
			lc_err("%s: pd thermal dtsi parse error\n", __func__);
			goto err;
		}
		if (manager->system_temp_level > manager->pd_thermal_levels) {
			lc_err("%s: system_temp_level is invalid\n", __func__);
			goto err;
		}

                /*manager->sm.screen_state 0:bright, 1:black*/
                if(((manager->b_flag == NORMAL) || (manager->b_flag == BLACK)) && !manager->sm.screen_state) {  //black to bright
                        manager->b_flag = BLACK_TO_BRIGHT;
                        time_last = ktime_get_seconds();
                        fast_flag = true;
                        lc_err("%s switch to bright time_last = %d\n", __func__, time_last);
                }
                else if((manager->b_flag == BLACK_TO_BRIGHT || manager->b_flag == BRIGHT) && !manager->sm.screen_state) {  //still bright
                        manager->b_flag = BRIGHT;
                        time_now = ktime_get_seconds();
                        delta_time = time_now - time_last;
                        lc_err("%s still_bright time_now = %d, time_last = %d, delta_time = %d\n", __func__, time_now, time_last, delta_time);
                        if(delta_time <= 10) {
                                fast_flag = true;
                                lc_err("%s still_bright delta_time = %d, stay fast\n", __func__, delta_time);
                        }
                        else {
                                fast_flag = false;
                                lc_err("%s still_bright delta_time = %d, exit fast\n", __func__, delta_time);
                        }
                }
                else { //black
                        manager->b_flag = BLACK;
                        fast_flag = true;
                        lc_err("%s black stay fast\n", __func__, delta_time);
                }

                /*avoid thermal_board_temp raise too fast*/
                if((last_level == 8) && (manager->system_temp_level == 7) && (manager->thermal_board_temp > 410)){
                        hot_flag = true;
                        fast_flag = false;
                        lc_err("%s avoid thermal_board_temp raise too fast, exit fast mode\n", __func__);
                }
                else if((last_level == 7) && ((manager->system_temp_level == 7) || (manager->system_temp_level == 8)) && hot_flag && (manager->thermal_board_temp > 410)){
                        fast_flag = false;
                }
                else{
                        hot_flag = false;
                }

                if(manager->thermal_board_temp > 420){
                        fast_flag = false;
                }

                if(fast_flag) {  //stay fast strategy
                        manager->pps_fast_mode = true;
                        manager->low_fast_ffc = manager->pd_thermal_mitigation_fast[manager->system_temp_level];

                        if((manager->soc > 38) && (manager->thermal_board_temp > 380)){
                                if(manager->low_fast_ffc >= 5450){
                                        manager->low_fast_ffc -= 3300;
                                }
                                else{
                                        manager->low_fast_ffc = 2150;
                                }
                                lc_err("%s stay fast but decrease 3.3, manager->thermal_board_temp = %d, manager->low_fast_ffc = %d\n", __func__, manager->thermal_board_temp, manager->low_fast_ffc);
                        }
                        else if((manager->soc > 35) && (manager->thermal_board_temp > 390)){
                                if(manager->low_fast_ffc >= 3950){
                                        manager->low_fast_ffc -= 1800;
                                }
                                else{
                                        manager->low_fast_ffc = 2150;
                                }
                                lc_err("%s stay fast but decrease 1.8, manager->thermal_board_temp = %d, manager->low_fast_ffc = %d\n", __func__, manager->thermal_board_temp, manager->low_fast_ffc);
                        }
                        else if((manager->soc > 30) && (manager->thermal_board_temp > 409)){
                                if(manager->low_fast_ffc >= 3150){
                                        manager->low_fast_ffc -= 2000;
                                }
                                else{
                                        manager->low_fast_ffc = 2150;
                                }
                                lc_err("%s stay fast but decrease 1, manager->thermal_board_temp = %d, manager->low_fast_ffc = %d\n", __func__, manager->thermal_board_temp, manager->low_fast_ffc);
                        }
                        else if((manager->soc < 30) && (manager->thermal_board_temp > 409)){
                                if(manager->low_fast_ffc >= 3150){
                                        manager->low_fast_ffc -= 1000;
                                }
                                else{
                                        manager->low_fast_ffc = 2150;
                                }
                                lc_err("%s stay fast but decrease 1, manager->thermal_board_temp = %d, manager->low_fast_ffc = %d\n", __func__, manager->thermal_board_temp, manager->low_fast_ffc);
                        }
                        else if((manager->soc > 30) && (manager->soc < 39) && (manager->thermal_board_temp < 389)){
                                if(manager->low_fast_ffc <= 10200){
                                        manager->low_fast_ffc += 2000;
                                }
                                else{
                                        manager->low_fast_ffc = 12200;
                                }
                                lc_err("%s stay fast but add 2, manager->thermal_board_temp = %d, manager->low_fast_ffc = %d\n", __func__, manager->thermal_board_temp, manager->low_fast_ffc);
                        }
                        else if((manager->soc <= 30) && (manager->thermal_board_temp < 366)){
                                if(manager->low_fast_ffc <= 10200){
                                        manager->low_fast_ffc += 2000;
                                }
                                else{
                                        manager->low_fast_ffc = 12200;
                                }
                                lc_err("%s stay fast but add 3, manager->thermal_board_temp = %d, manager->low_fast_ffc = %d\n", __func__, manager->thermal_board_temp, manager->low_fast_ffc);
                        }
                        vote(manager->total_fcc_votable, CALL_THERMAL_DAEMON_VOTER, true, manager->low_fast_ffc);
                        lc_err("%s stay fast, manager->low_fast_ffc = %d\n", __func__, manager->low_fast_ffc);
                }
                else { //exit fast strategy
                        manager->pps_fast_mode = false;
                        manager->low_fast_ffc = manager->pd_thermal_mitigation[manager->system_temp_level];
                        vote(manager->total_fcc_votable, CALL_THERMAL_DAEMON_VOTER, true, manager->low_fast_ffc);
                        lc_err("%s exit fast, manager->low_fast_ffc = %d\n", __func__, manager->low_fast_ffc);
                }
                last_level = manager->system_temp_level;
	}

	return;
err:
        vote(manager->total_fcc_votable, CALL_THERMAL_DAEMON_VOTER, true, manager->pd_thermal_mitigation[manager->system_temp_level]);
	return;
}

#define OUTDOOR_CURR 1900
void monitor_outdoor_charging(struct charger_manager *manager)
{
    enum vbus_type vbus_type = VBUS_TYPE_NONE;
    int ret = 0;

    if (IS_ERR_OR_NULL(manager)){
        lc_err("manager is NULL\n");
        return;
    }

    if (IS_ERR_OR_NULL(manager->charger)) {
        lc_err("%s:manager->charger is_err_or_null\n", __func__);
        return;
    }
    ret = charger_get_vbus_type(manager->charger, &vbus_type);
    if (ret < 0){
        lc_err("Couldn't get usb type ret=%d\n", ret);
        return;
    }

    if (vbus_type == VBUS_TYPE_DCP && !manager->pd_active) {
        lc_info("smart_chg en:%d, outdoor_flag:%d\n", manager->smart_charge[SMART_CHG_OUTDOOR_CHARGE].en_ret, manager->outdoor_flag);
        if (manager->smart_charge[SMART_CHG_OUTDOOR_CHARGE].en_ret ) {
            lc_err("not outdoor charging not wifi...%dmA\n", OUTDOOR_CURR);
            if (manager->outdoor_flag != 1) {
                vote(manager->main_fcc_votable, CHARGER_TYPE_VOTER, false, 0);
                vote(manager->main_fcc_votable, CHARGER_TYPE_VOTER, true, OUTDOOR_CURR);
                vote(manager->main_icl_votable, CHARGER_TYPE_VOTER, false, 0);
                vote(manager->main_icl_votable, CHARGER_TYPE_VOTER, true, OUTDOOR_CURR);
                manager->outdoor_flag = 1;
            }
        } else {
            lc_info("outdoor charging wifi...%dmA\n", manager->dcp_current);
            vote(manager->main_fcc_votable, CHARGER_TYPE_VOTER, true, manager->dcp_current);
            vote(manager->main_icl_votable, CHARGER_TYPE_VOTER, true, manager->dcp_current);
            manager->outdoor_flag = 0;
        }
    }
}

#if IS_ENABLED(CONFIG_MIEV)
static void mievent_upload(int miev_code, int para_cnt, ...)
{
    int i = 0;
    char* key;
    char* type;
    char* value_str;
    int val_long;
    va_list arg;

    struct misight_mievent *event = cdev_tevent_alloc(miev_code);
    lc_info("CHG:DFX:miev_code=%d, para_cnt=%d \n", miev_code, para_cnt);
    va_start(arg, para_cnt);
    while (i < para_cnt) {
        type = va_arg(arg, char*);
        key = va_arg(arg, char*);
        if (strcmp(type, "char") == 0) {
            value_str = va_arg(arg, char*);
            cdev_tevent_add_str(event, key, value_str);
        } else if (strcmp(type, "int") == 0) {
            val_long = va_arg(arg, int);
            lc_info("CHG:DFX:key=%s, val_long=%d\n", key, val_long);
            cdev_tevent_add_int(event, key, val_long);
        } else {
            lc_err("CHG:DFX:unsupported type");
            break;
        }
        i++;
    }
    va_end(arg);
    cdev_tevent_write(event);
    cdev_tevent_destroy(event);

    return;
}

static void xm_handle_dfx_report(u8 type, int data)
{
    if (type >= CHG_DFX_MAX_INDEX) {
        lc_err("CHG:DFX: unknown type to report\n");
        return;
    }
    switch (type) {
    case CHG_DFX_FG_IIC_ERR:
        mievent_upload(DFX_ID_CHG_FG_IIC_ERR, 1, "char", "chgErrInfo", xm_dfx_chg_report_text[type]);
        break;
    case CHG_DFX_CP_ERR:
        mievent_upload(DFX_ID_CHG_CP_ERR, 1, "char", "chgErrInfo", xm_dfx_chg_report_text[type]);
        break;
    case CHG_DFX_BATT_LINKER_ABSENT:
        mievent_upload(DFX_ID_CHG_BATT_LINKER_ABSENT, 1, "char", "chgErrInfo", xm_dfx_chg_report_text[type]);
        break;
    case CHG_DFX_LPD_DISCHARGE:
        mievent_upload(DFX_ID_CHG_LPD_DISCHARGE,  1, "int", "lpdFlag", 1);
        break;
    case CHG_DFX_LPD_DISCHARGE_RESET:
        mievent_upload(DFX_ID_CHG_LPD_DISCHARGE, 1, "int","lpdFlag", 0);
        break;
    case CHG_DFX_CORROSION_DISCHARGE:
        mievent_upload(DFX_ID_CHG_CORROSION_DISCHARGE, 1, "char", "chgErrInfo", xm_dfx_chg_report_text[type]);
        break;
    case CHG_DFX_NONE_STANDARD_CHG:
        mievent_upload(DFX_ID_CHG_NONE_STANDARD_CHG, 1, "char", "chgErrInfo", xm_dfx_chg_report_text[type]);
        break;
    case CHG_DFX_CP_VBUS_OVP:
        mievent_upload(DFX_ID_CHG_CP_VBUS_OVP, 1, "char", "chgErrInfo", xm_dfx_chg_report_text[type]);
        break;
    case CHG_DFX_CP_IBUS_OCP:
        mievent_upload(DFX_ID_CHG_CP_IBUS_OCP, 1, "char", "chgErrInfo", xm_dfx_chg_report_text[type]);
        break;
    case CHG_DFX_CP_VBAT_OVP:
        mievent_upload(DFX_ID_CHG_CP_VBAT_OVP, 2, "char", "chgErrInfo", xm_dfx_chg_report_text[type], "int", "vbat", data);
        break;
    case CHG_DFX_CP_IBAT_OCP:
        mievent_upload(DFX_ID_CHG_CP_IBAT_OCP, 1, "char", "chgErrInfo", xm_dfx_chg_report_text[type]);
        break;
    case CHG_DFX_PPS_AUTH_FAIL:
        mievent_upload(DFX_ID_CHG_PD_AUTHEN_FAIL, 2, "char", "chgErrInfo", xm_dfx_chg_report_text[type], "int", "adapterId", data);
        break;
    case CHG_DFX_CP_ENABLE_FAIL:
        mievent_upload(DFX_ID_CHG_CP_ENABLE_FAIL, 1, "char", "chgErrInfo", xm_dfx_chg_report_text[type]);
        break;
    case CHG_DFX_BATTERY_CYCLECOUNT:
        mievent_upload(DFX_ID_CHG_BATTERY_CYCLECOUNT, 2, "char", "chgStatInfo", xm_dfx_chg_report_text[type], "int", "cycleCnt", data);
		break;
    case CHG_DFX_LOW_TEMP_DISCHARGING:
        mievent_upload(DFX_ID_CHG_LOW_TEMP_DISCHARGING, 2, "char", "chgErrInfo", xm_dfx_chg_report_text[type], "int", "tbat", data);
        break;
    case CHG_DFX_HIGH_TEMP_DISCHARGING:
        mievent_upload(DFX_ID_CHG_HIGH_TEMP_DISCHARGING, 2, "char", "chgErrInfo", xm_dfx_chg_report_text[type], "int", "tbat", data);
		break;
    case CHG_DFX_BATTERY_AUTH_FAIL:
        mievent_upload(DFX_ID_CHG_BATTERY_AUTH_FAIL, 2, "char", "chgErrInfo", xm_dfx_chg_report_text[type], "int", "status", data);
        break;
    case CHG_DFX_BATTERY_TEMP_HIGH:
        mievent_upload(DFX_ID_CHG_BATTERY_TEMP_HIGH, 2, "char", "chgStatInfo", xm_dfx_chg_report_text[type], "int", "tbat", data);
        break;
    case CHG_DFX_BATTERY_TEMP_LOW:
        mievent_upload(DFX_ID_CHG_BATTERY_TEMP_LOW, 2, "char", "chgStatInfo", xm_dfx_chg_report_text[type], "int", "tbat", data);
        lc_err("CHG:DFX: CHG_DFX_BATTERY_TEMP_LOW data:%d\n", data);
        break;
    case CHG_DFX_ANTIBURN_FAIL:
        mievent_upload(DFX_ID_CHG_ANTIBURN_FAIL, 1, "char", "chgErrInfo", xm_dfx_chg_report_text[type]);
        break;
    default:
        lc_err("CHG:DFX: unknown type to report\n");
    }
    return;
}

static int dfx_none_standard_check(struct charger_manager *manager)
{
    int ret = 0;
    enum vbus_type vbus_type = VBUS_TYPE_NONE;

    if (IS_ERR_OR_NULL(manager)){
        lc_err("CHG:DFX: manager is NULL\n");
        return PTR_ERR(manager);
    }

    if (IS_ERR_OR_NULL(manager->charger)) {
        lc_err("CHG:DFX: manager->charger is_err_or_null\n");
        return -EINVAL;
    }

    ret = charger_get_vbus_type(manager->charger, &vbus_type);
    if (ret < 0){
        lc_err("CHG:DFX: Couldn't get usb type ret=%d\n", ret);
        return -EINVAL;
    }

    if (vbus_type == VBUS_TYPE_FLOAT && (manager->dfx_none_standard_flag < 1)) {
        manager->dfx_none_standard_flag = (manager->dfx_none_standard_flag) + 1;
        xm_handle_dfx_report(CHG_DFX_NONE_STANDARD_CHG, 0);
        lc_err("CHG:DFX: type: %d, none standard send dfx report\n", vbus_type);
    }
    lc_debug("CHG:DFX: type: %d, dfx_none_standard_flag: %d\n", vbus_type, manager->dfx_none_standard_flag);
    return ret;
}

static int dfx_cp_present_check(struct charger_manager *manager)
{
    struct power_supply *cp_psy = NULL;

    if (IS_ERR_OR_NULL(manager)) {
        lc_err("CHG:DFX: point manager is err or null\n");
        return -EINVAL;
    }

    cp_psy = power_supply_get_by_name("sc-cp-master");
    if (IS_ERR_OR_NULL(cp_psy)) {
        cp_psy = power_supply_get_by_name("sc-cp-slave");
        if (IS_ERR_OR_NULL(cp_psy) && (manager->dfx_cp_err_flag < 1)) {
            manager->dfx_cp_err_flag = (manager->dfx_cp_err_flag) + 1;
            lc_err("CHG:DFX: charger pump not present, send dfx report\n");
            xm_handle_dfx_report(CHG_DFX_CP_ERR, 0);
        }
    }

    lc_debug("CHG:DFX:dfx_cp_err_flag:%d\n", manager->dfx_cp_err_flag);
    return 0;
}

static int dfx_batt_linker_absent_check(struct charger_manager *manager)
{
    int ret = -EINVAL;
    bool chip_ok_status = true;

    if (IS_ERR_OR_NULL(manager)) {
        lc_err("CHG:DFX: point manager is err or null\n");
        return ret;
    }

    if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
        lc_err("CHG:DFX: point fuel_gauge is null\n");
        return ret;
    }

    chip_ok_status = fuel_gauge_get_chip_ok(manager->fuel_gauge);
    if (!chip_ok_status && manager->dfx_batt_linker_absent_flag < 1) {
        manager->dfx_batt_linker_absent_flag = (manager->dfx_batt_linker_absent_flag) + 1;
        xm_handle_dfx_report(CHG_DFX_BATT_LINKER_ABSENT, 0);
        lc_err("CHG:DFX: batt_linker_absent not present, send dfx report\n");
    }

    lc_debug("CHG:DFX:dfx_batt_linker_absent_flag:%d\n", manager->dfx_batt_linker_absent_flag);
    return 0;
}

static int dfx_lpd_check(struct charger_manager *manager)
{
    if (IS_ERR_OR_NULL(manager)) {
        lc_err("CHG:DFX: point manager is err or null\n");
        return PTR_ERR(manager);
    }
#ifndef CONFIG_FACTORY_BUILD
#if IS_ENABLED(CONFIG_RUST_DETECTION)
    if (manager->decrease_volt && manager->dfx_lpd_check_flag < 1) {
        manager->dfx_lpd_check_flag = manager->dfx_lpd_check_flag + 1;
        xm_handle_dfx_report(CHG_DFX_LPD_DISCHARGE, 0);
        lc_err("CHG:DFX: dfx_lpd_check, send dfx report\n");
    }
#endif
#endif
    lc_debug("CHG:DFX:dfx_lpd_check_flag:%d\n", manager->dfx_lpd_check_flag);
    return 0;
}

__maybe_unused
static int dfx_fg_i2c_communication_check(struct charger_manager *manager)
{
    int i2c_status = 0;

    if (IS_ERR_OR_NULL(manager)) {
        lc_err("CHG:DFX: point manager is err or null\n");
        return PTR_ERR(manager);
    }

    if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
        manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
        if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
            lc_err("CHG:DFX:point fuel_gauge is err or null\n");
            return PTR_ERR(manager->fuel_gauge);
        }
    }

    i2c_status = fuel_gauge_check_i2c_function(manager->fuel_gauge);
    if (!i2c_status && manager->dfx_fg_i2c_err_flag != 0) {
        manager->dfx_fg_i2c_err_flag = 0;
    } else if (i2c_status && manager->dfx_fg_i2c_err_flag < 1) {
        manager->dfx_fg_i2c_err_flag = manager->dfx_fg_i2c_err_flag + 1;
        xm_handle_dfx_report(CHG_DFX_FG_IIC_ERR, 0);
        lc_err("CHG:DFX: I2C communication failure, send dfx report\n");
    }

    lc_debug("CHG:DFX:fg dfx_fg_i2c_err_flag:%d\n",  manager->dfx_fg_i2c_err_flag);
    return 0;
}

__maybe_unused
static int dfx_batt_temp_abnormal_check(struct charger_manager *manager)
{
    if (IS_ERR_OR_NULL(manager)) {
        lc_err("CHG:DFX: point manager is err or null\n");
        return PTR_ERR(manager);
    }

    if (manager->tbat > 550 && manager->dfx_bat_temp_abnormal_flag < 1) {
        manager->dfx_bat_temp_abnormal_flag = manager->dfx_bat_temp_abnormal_flag + 1;
        xm_handle_dfx_report(CHG_DFX_BATTERY_TEMP_HIGH, manager->tbat);
        lc_err("CHG:DFX: batt_temp_abnormal, send dfx report temp:%d\n", manager->tbat);
    } else if ((manager->tbat < 0 || manager->tbat < -200) && manager->dfx_bat_temp_abnormal_flag < 1) {
        manager->dfx_bat_temp_abnormal_flag = manager->dfx_bat_temp_abnormal_flag + 1;
        xm_handle_dfx_report(CHG_DFX_BATTERY_TEMP_LOW, manager->tbat);
        lc_err("CHG:DFX: batt_temp_abnormal, send dfx report temp:%d\n", manager->tbat);
    }

    lc_debug("CHG:DFX:dfx_bat_temp_abnormal_flag:%d\n", manager->dfx_bat_temp_abnormal_flag);
    return 0;
}

static void xm_dfx_check(struct charger_manager *manager)
 {
    int ret = 0;
    ret = dfx_none_standard_check(manager);
    if (ret < 0) {
        lc_err("CHG:DFX: dfx none standard check fail\n");
    }
    ret = dfx_cp_present_check(manager);
    if (ret < 0) {
        lc_err("CHG:DFX: dfx cp err check fail\n");
    }
    ret = dfx_batt_linker_absent_check(manager);
    if (ret < 0) {
        lc_err("CHG:DFX: dfx batt linker absent check fail\n");
    }

    ret = dfx_lpd_check(manager);
    if (ret < 0) {
        lc_err("CHG:DFX: dfx lpd check fail\n");
    }
    /* * not support
    ret = dfx_fg_i2c_communication_check(manager);
    if (ret < 0) {
        lc_err("CHG:DFX: dfx fg i2c communication check fail\n");
    }
    ret = dfx_batt_temp_abnormal_check(manager);
    if (ret < 0) {
        lc_err("CHG:DFX: dfx battery temp abnormal check fail\n");
    }
    * */
    return;
 }
#endif

void xm_charge_work(struct work_struct *work)
{
	struct charger_manager *chip = container_of(work, struct charger_manager, xm_charge_work.work);

	lc_debug("check xm_charge_work\n");
	monitor_smart_chg(chip);
	monitor_night_charging(chip);
	monitor_low_fast_strategy(chip);
#if IS_ENABLED(CONFIG_MIEV)
	monitor_outdoor_charging(chip);
#endif
	xm_smart_chg_endurance_protect(chip);
	get_drop_floatvolatge(chip);

#if IS_ENABLED(CONFIG_MIEV)
	xm_dfx_check(chip);
#endif
	/* *
	* Move to lc_charger_manager fv_votable callback
	* monitor_smart_batt(chip);
	* monitor_cycle_count(chip);
	* */

	schedule_delayed_work(&chip->xm_charge_work, msecs_to_jiffies(1000));
}
