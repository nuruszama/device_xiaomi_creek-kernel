obj-$(CONFIG_LC_CHARGER_MANAGER)        += main_chg.o
main_chg-objs:= lc_charger_manager.o lc_jeita.o lc_charge_sysfs.o lc_charge_class_sysfs.o 
ifeq ($(CONFIG_XIAOMI_SMART_CHG),m)
main_chg-objs += xm_smart_chg.o
endif
obj-$(CONFIG_LC_CHARGER_MANAGER)        += ../common/lc_voter.o
obj-$(CONFIG_LC_CHARGER_MANAGER)        += ../common/lc_notify.o
obj-$(CONFIG_LC_CP_POLICY)              += lc_cp_policy.o
obj-$(CONFIG_LC_CP_POLICY)              += lc_pd_adapter.o
obj-$(CONFIG_PD_BATTERY_SECRET)             += xm_pd_adapter.o
obj-$(CONFIG_LC_QC_POLICY)              += lc_qc_policy.o
