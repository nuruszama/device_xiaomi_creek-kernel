/*
 * SC8551 battery charging driver
*/

#define pr_fmt(fmt)	"[sc8551] %s: " fmt, __func__

#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/debugfs.h>
#include <linux/bitops.h>
#include <linux/math64.h>
#include <linux/err.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/ipc_logging.h>
#include <linux/printk.h>

#include "../../charger_class/lc_cp_class.h"

#define BQ25970_DEVICE_ID				0x10
#define SC8551_DEVICE_ID				0x00
#define SC8551A_DEVICE_ID				0x51
#define SC8551_CHIP_VENDOR_MASK				GENMASK(7, 4)
#define SC8551_CHARGE_MODE_DIV2				0
#define SC8551_CHARGE_MODE_BYPASS			1

/* Register 00h */
#define SC8551_REG_00					0x00
#define	SC8551_BAT_OVP_DIS_MASK			0x80
#define	SC8551_BAT_OVP_DIS_SHIFT		7
#define	SC8551_BAT_OVP_ENABLE			0
#define	SC8551_BAT_OVP_DISABLE			1

#define SC8551_BAT_OVP_MASK				0x3F
#define SC8551_BAT_OVP_SHIFT			0
#define SC8551_BAT_OVP_BASE				3500
#define SC8551_BAT_OVP_LSB				25

/* Register 01h */
#define SC8551_REG_01					0x01
#define SC8551_BAT_OVP_ALM_DIS_MASK		0x80
#define SC8551_BAT_OVP_ALM_DIS_SHIFT	7
#define SC8551_BAT_OVP_ALM_ENABLE		0
#define SC8551_BAT_OVP_ALM_DISABLE		1

#define SC8551_BAT_OVP_ALM_MASK			0x3F
#define SC8551_BAT_OVP_ALM_SHIFT		0
#define SC8551_BAT_OVP_ALM_BASE			3500
#define SC8551_BAT_OVP_ALM_LSB			25

/* Register 02h */
#define SC8551_REG_02					0x02
#define	SC8551_BAT_OCP_DIS_MASK			0x80
#define	SC8551_BAT_OCP_DIS_SHIFT		7
#define SC8551_BAT_OCP_ENABLE			0
#define SC8551_BAT_OCP_DISABLE			1

#define SC8551_BAT_OCP_MASK				0x7F
#define SC8551_BAT_OCP_SHIFT			0
#define SC8551_BAT_OCP_BASE				2000
#define SC8551_BAT_OCP_LSB				100

/* Register 03h */
#define SC8551_REG_03					0x03
#define SC8551_BAT_OCP_ALM_DIS_MASK		0x80
#define SC8551_BAT_OCP_ALM_DIS_SHIFT	7
#define SC8551_BAT_OCP_ALM_ENABLE		0
#define SC8551_BAT_OCP_ALM_DISABLE		1

#define SC8551_BAT_OCP_ALM_MASK			0x7F
#define SC8551_BAT_OCP_ALM_SHIFT		0
#define SC8551_BAT_OCP_ALM_BASE			2000
#define SC8551_BAT_OCP_ALM_LSB			100

/* Register 04h */
#define SC8551_REG_04					0x04
#define	SC8551_BAT_UCP_ALM_DIS_MASK		0x80
#define	SC8551_BAT_UCP_ALM_DIS_SHIFT	7
#define	SC8551_BAT_UCP_ALM_ENABLE		0
#define	SC8551_BAT_UCP_ALM_DISABLE		1

#define	SC8551_BAT_UCP_ALM_MASK			0x7F
#define	SC8551_BAT_UCP_ALM_SHIFT		0
#define	SC8551_BAT_UCP_ALM_BASE			0
#define	SC8551_BAT_UCP_ALM_LSB			50

/* Register 05h */
#define SC8551_REG_05					0x05
#define SC8551_AC_OVP_STAT_MASK			0x80
#define SC8551_AC_OVP_STAT_SHIFT		7

#define SC8551_AC_OVP_FLAG_MASK			0x40
#define SC8551_AC_OVP_FLAG_SHIFT		6

#define SC8551_AC_OVP_MASK_MASK			0x20
#define SC8551_AC_OVP_MASK_SHIFT		5

#define SC8551_VDROP_THRESHOLD_SET_MASK		0x10
#define SC8551_VDROP_THRESHOLD_SET_SHIFT	4
#define SC8551_VDROP_THRESHOLD_300MV		0
#define SC8551_VDROP_THRESHOLD_400MV		1

#define SC8551_VDROP_DEGLITCH_SET_MASK		0x08
#define SC8551_VDROP_DEGLITCH_SET_SHIFT		3
#define SC8551_VDROP_DEGLITCH_8US			0
#define SC8551_VDROP_DEGLITCH_5MS			1

#define SC8551_AC_OVP_MASK				0x07
#define SC8551_AC_OVP_SHIFT				0
#define SC8551_AC_OVP_BASE				11
#define SC8551_AC_OVP_LSB				1
#define SC8551_AC_OVP_6P5V				65

/* Register 06h */
#define SC8551_REG_06					0x06
#define SC8551_VBUS_PD_EN_MASK			0x80
#define SC8551_VBUS_PD_EN_SHIFT			7
#define SC8551_VBUS_PD_ENABLE			1
#define SC8551_VBUS_PD_DISABLE			0

#define SC8551_BUS_OVP_MASK				0x7F
#define SC8551_BUS_OVP_SHIFT			0
#define SC8551_BUS_OVP_BASE				6000
#define SC8551_BUS_OVP_LSB				50

/* Register 07h */
#define SC8551_REG_07					0x07
#define SC8551_BUS_OVP_ALM_DIS_MASK		0x80
#define SC8551_BUS_OVP_ALM_DIS_SHIFT	7
#define SC8551_BUS_OVP_ALM_ENABLE		0
#define SC8551_BUS_OVP_ALM_DISABLE		1

#define SC8551_BUS_OVP_ALM_MASK			0x7F
#define SC8551_BUS_OVP_ALM_SHIFT		0
#define SC8551_BUS_OVP_ALM_BASE			6000
#define SC8551_BUS_OVP_ALM_LSB			50

/* Register 08h */
#define SC8551_REG_08					0x08
#define SC8551_BUS_OCP_DIS_MASK			0x80
#define SC8551_BUS_OCP_DIS_SHIFT		7
#define	SC8551_BUS_OCP_ENABLE			0
#define	SC8551_BUS_OCP_DISABLE			1

#define SC8551_IBUS_UCP_RISE_FLAG_MASK		0x40
#define SC8551_IBUS_UCP_RISE_FLAG_SHIFT		6

#define SC8551_IBUS_UCP_RISE_MASK_MASK		0x20
#define SC8551_IBUS_UCP_RISE_MASK_SHIFT		5
#define SC8551_IBUS_UCP_RISE_MASK_ENABLE	1
#define SC8551_IBUS_UCP_RISE_MASK_DISABLE	0

#define SC8551_IBUS_UCP_FALL_FLAG_MASK		0x10
#define SC8551_IBUS_UCP_FALL_FLAG_SHIFT		4

#define SC8551_BUS_OCP_MASK					0x0F
#define SC8551_BUS_OCP_SHIFT				0
#define SC8551_BUS_OCP_BASE					1000
#define SC8551_BUS_OCP_LSB					250

/* Register 09h */
#define SC8551_REG_09						0x09
#define SC8551_BUS_OCP_ALM_DIS_MASK			0x80
#define SC8551_BUS_OCP_ALM_DIS_SHIFT		7
#define SC8551_BUS_OCP_ALM_ENABLE			0
#define SC8551_BUS_OCP_ALM_DISABLE			1

#define SC8551_BUS_OCP_ALM_MASK				0x7F
#define SC8551_BUS_OCP_ALM_SHIFT			0
#define SC8551_BUS_OCP_ALM_BASE				0
#define SC8551_BUS_OCP_ALM_LSB				50

/* Register 0Ah */
#define SC8551_REG_0A						0x0A
#define SC8551_TSHUT_FLAG_MASK				0x80
#define SC8551_TSHUT_FLAG_SHIFT				7

#define SC8551_TSHUT_STAT_MASK				0x40
#define SC8551_TSHUT_STAT_SHIFT				6

#define SC8551_VBUS_ERRORLO_STAT_MASK		0x20
#define SC8551_VBUS_ERRORLO_STAT_SHIFT		5

#define SC8551_VBUS_ERRORHI_STAT_MASK		0x10
#define SC8551_VBUS_ERRORHI_STAT_SHIFT		4

#define SC8551_SS_TIMEOUT_FLAG_MASK			0x08
#define SC8551_SS_TIMEOUT_FLAG_SHIFT		3

#define SC8551_CONV_SWITCHING_STAT_MASK		0x04
#define SC8551_CONV_SWITCHING_STAT_SHIFT	2

#define SC8551_CONV_OCP_FLAG_MASK			0x02
#define SC8551_CONV_OCP_FLAG_SHIFT			1

#define SC8551_PIN_DIAG_FALL_FLAG_MASK		0x01
#define SC8551_PIN_DIAG_FALL_FLAG_SHIFT		0

/* Register 0Bh */
#define SC8551_REG_0B						0x0B
#define SC8551_REG_RST_MASK					0x80
#define SC8551_REG_RST_SHIFT				7
#define SC8551_REG_RST_ENABLE				1
#define SC8551_REG_RST_DISABLE				0

#define SC8551_FSW_SET_MASK					0x70
#define SC8551_FSW_SET_SHIFT				4
#define SC8551_FSW_SET_300KHZ				0
#define SC8551_FSW_SET_350KHZ				1
#define SC8551_FSW_SET_400KHZ				2
#define SC8551_FSW_SET_450KHZ				3
#define SC8551_FSW_SET_500KHZ				4
#define SC8551_FSW_SET_550KHZ				5
#define SC8551_FSW_SET_600KHZ				6
#define SC8551_FSW_SET_750KHZ				7

#define SC8551_WD_TIMEOUT_FLAG_MASK			0x08
#define SC8551_WD_TIMEOUT_SHIFT				3

#define SC8551_WATCHDOG_DIS_MASK			0x04
#define SC8551_WATCHDOG_DIS_SHIFT			2
#define SC8551_WATCHDOG_ENABLE				0
#define SC8551_WATCHDOG_DISABLE				1

#define SC8551_WATCHDOG_MASK				0x03
#define SC8551_WATCHDOG_SHIFT				0
#define SC8551_WATCHDOG_0P5S				0
#define SC8551_WATCHDOG_1S					1
#define SC8551_WATCHDOG_5S					2
#define SC8551_WATCHDOG_30S					3

/* Register 0Ch */
#define SC8551_REG_0C						0x0C
#define SC8551_CHG_EN_MASK					0x80
#define SC8551_CHG_EN_SHIFT					7
#define SC8551_CHG_ENABLE					1
#define SC8551_CHG_DISABLE					0

#define SC8551_MS_MASK						0x60
#define SC8551_MS_SHIFT						5
#define SC8551_MS_STANDALONE				0
#define SC8551_MS_SLAVE						1
#define SC8551_MS_MASTER					2

#define SC8551_FREQ_SHIFT_MASK				0x18
#define SC8551_FREQ_SHIFT_SHIFT				3
#define SC8551_FREQ_SHIFT_NORMINAL			0
#define SC8551_FREQ_SHIFT_POSITIVE10		1
#define SC8551_FREQ_SHIFT_NEGATIVE10		2
#define SC8551_FREQ_SHIFT_SPREAD_SPECTRUM	3

#define SC8551_TSBUS_DIS_MASK				0x04
#define SC8551_TSBUS_DIS_SHIFT				2
#define SC8551_TSBUS_ENABLE					0
#define SC8551_TSBUS_DISABLE				1

#define SC8551_TSBAT_DIS_MASK				0x02
#define SC8551_TSBAT_DIS_SHIFT				1
#define SC8551_TSBAT_ENABLE					0
#define SC8551_TSBAT_DISABLE				1

/* Register 0Dh */
#define SC8551_REG_0D						0x0D
#define SC8551_BAT_OVP_ALM_STAT_MASK		0x80
#define SC8551_BAT_OVP_ALM_STAT_SHIFT		7

#define SC8551_BAT_OCP_ALM_STAT_MASK		0x40
#define SC8551_BAT_OCP_ALM_STAT_SHIFT		6

#define SC8551_BUS_OVP_ALM_STAT_MASK		0x20
#define SC8551_BUS_OVP_ALM_STAT_SHIFT		5

#define SC8551_BUS_OCP_ALM_STAT_MASK		0x10
#define SC8551_BUS_OCP_ALM_STAT_SHIFT		4

#define SC8551_BAT_UCP_ALM_STAT_MASK		0x08
#define SC8551_BAT_UCP_ALM_STAT_SHIFT		3

#define SC8551_ADAPTER_INSERT_STAT_MASK		0x04
#define SC8551_ADAPTER_INSERT_STAT_SHIFT	2

#define SC8551_VBAT_INSERT_STAT_MASK		0x02
#define SC8551_VBAT_INSERT_STAT_SHIFT		1

#define SC8551_ADC_DONE_STAT_MASK			0x01
#define SC8551_ADC_DONE_STAT_SHIFT			0
#define SC8551_ADC_DONE_STAT_COMPLETE		1
#define SC8551_ADC_DONE_STAT_NOTCOMPLETE	0

/* Register 0Eh */
#define SC8551_REG_0E						0x0E
#define SC8551_BAT_OVP_ALM_FLAG_MASK		0x80
#define SC8551_BAT_OVP_ALM_FLAG_SHIFT		7

#define SC8551_BAT_OCP_ALM_FLAG_MASK		0x40
#define SC8551_BAT_OCP_ALM_FLAG_SHIFT		6

#define SC8551_BUS_OVP_ALM_FLAG_MASK		0x20
#define SC8551_BUS_OVP_ALM_FLAG_SHIFT		5

#define SC8551_BUS_OCP_ALM_FLAG_MASK		0x10
#define SC8551_BUS_OCP_ALM_FLAG_SHIFT		4

#define SC8551_BAT_UCP_ALM_FLAG_MASK		0x08
#define SC8551_BAT_UCP_ALM_FLAG_SHIFT		3

#define SC8551_ADAPTER_INSERT_FLAG_MASK		0x04
#define SC8551_ADAPTER_INSERT_FLAG_SHIFT	2

#define SC8551_VBAT_INSERT_FLAG_MASK		0x02
#define SC8551_VBAT_INSERT_FLAG_SHIFT		1

#define SC8551_ADC_DONE_FLAG_MASK			0x01
#define SC8551_ADC_DONE_FLAG_SHIFT			0
#define SC8551_ADC_DONE_FLAG_COMPLETE		1
#define SC8551_ADC_DONE_FLAG_NOTCOMPLETE	0

/* Register 0Fh */
#define SC8551_REG_0F						0x0F
#define SC8551_BAT_OVP_ALM_MASK_MASK		0x80
#define SC8551_BAT_OVP_ALM_MASK_SHIFT		7
#define SC8551_BAT_OVP_ALM_MASK_ENABLE		1
#define SC8551_BAT_OVP_ALM_MASK_DISABLE		0

#define SC8551_BAT_OCP_ALM_MASK_MASK		0x40
#define SC8551_BAT_OCP_ALM_MASK_SHIFT		6
#define SC8551_BAT_OCP_ALM_MASK_ENABLE		1
#define SC8551_BAT_OCP_ALM_MASK_DISABLE		0

#define SC8551_BUS_OVP_ALM_MASK_MASK		0x20
#define SC8551_BUS_OVP_ALM_MASK_SHIFT		5
#define SC8551_BUS_OVP_ALM_MASK_ENABLE		1
#define SC8551_BUS_OVP_ALM_MASK_DISABLE		0

#define SC8551_BUS_OCP_ALM_MASK_MASK		0x10
#define SC8551_BUS_OCP_ALM_MASK_SHIFT		4
#define SC8551_BUS_OCP_ALM_MASK_ENABLE		1
#define SC8551_BUS_OCP_ALM_MASK_DISABLE		0

#define SC8551_BAT_UCP_ALM_MASK_MASK		0x08
#define SC8551_BAT_UCP_ALM_MASK_SHIFT		3
#define SC8551_BAT_UCP_ALM_MASK_ENABLE		1
#define SC8551_BAT_UCP_ALM_MASK_DISABLE		0

#define SC8551_ADAPTER_INSERT_MASK_MASK		0x04
#define SC8551_ADAPTER_INSERT_MASK_SHIFT	2
#define SC8551_ADAPTER_INSERT_MASK_ENABLE	1
#define SC8551_ADAPTER_INSERT_MASK_DISABLE	0

#define SC8551_VBAT_INSERT_MASK_MASK		0x02
#define SC8551_VBAT_INSERT_MASK_SHIFT		1
#define SC8551_VBAT_INSERT_MASK_ENABLE		1
#define SC8551_VBAT_INSERT_MASK_DISABLE		0

#define SC8551_ADC_DONE_MASK_MASK			0x01
#define SC8551_ADC_DONE_MASK_SHIFT			0
#define SC8551_ADC_DONE_MASK_ENABLE			1
#define SC8551_ADC_DONE_MASK_DISABLE		0

/* Register 10h */
#define SC8551_REG_10						0x10
#define SC8551_BAT_OVP_FLT_STAT_MASK		0x80
#define SC8551_BAT_OVP_FLT_STAT_SHIFT		7

#define SC8551_BAT_OCP_FLT_STAT_MASK		0x40
#define SC8551_BAT_OCP_FLT_STAT_SHIFT		6

#define SC8551_BUS_OVP_FLT_STAT_MASK		0x20
#define SC8551_BUS_OVP_FLT_STAT_SHIFT		5

#define SC8551_BUS_OCP_FLT_STAT_MASK		0x10
#define SC8551_BUS_OCP_FLT_STAT_SHIFT		4

#define SC8551_TSBUS_TSBAT_ALM_STAT_MASK	0x08
#define SC8551_TSBUS_TSBAT_ALM_STAT_SHIFT	3

#define SC8551_TSBAT_FLT_STAT_MASK			0x04
#define SC8551_TSBAT_FLT_STAT_SHIFT			2

#define SC8551_TSBUS_FLT_STAT_MASK			0x02
#define SC8551_TSBUS_FLT_STAT_SHIFT			1

#define SC8551_TDIE_ALM_STAT_MASK			0x01
#define SC8551_TDIE_ALM_STAT_SHIFT			0

/* Register 11h */
#define SC8551_REG_11						0x11
#define SC8551_BAT_OVP_FLT_FLAG_MASK		0x80
#define SC8551_BAT_OVP_FLT_FLAG_SHIFT		7

#define SC8551_BAT_OCP_FLT_FLAG_MASK		0x40
#define SC8551_BAT_OCP_FLT_FLAG_SHIFT		6

#define SC8551_BUS_OVP_FLT_FLAG_MASK		0x20
#define SC8551_BUS_OVP_FLT_FLAG_SHIFT		5

#define SC8551_BUS_OCP_FLT_FLAG_MASK		0x10
#define SC8551_BUS_OCP_FLT_FLAG_SHIFT		4

#define SC8551_TSBUS_TSBAT_ALM_FLAG_MASK	0x08
#define SC8551_TSBUS_TSBAT_ALM_FLAG_SHIFT	3

#define SC8551_TSBAT_FLT_FLAG_MASK			0x04
#define SC8551_TSBAT_FLT_FLAG_SHIFT			2

#define SC8551_TSBUS_FLT_FLAG_MASK			0x02
#define SC8551_TSBUS_FLT_FLAG_SHIFT			1

#define SC8551_TDIE_ALM_FLAG_MASK			0x01
#define SC8551_TDIE_ALM_FLAG_SHIFT			0

/* Register 12h */
#define SC8551_REG_12						0x12
#define SC8551_BAT_OVP_FLT_MASK_MASK		0x80
#define SC8551_BAT_OVP_FLT_MASK_SHIFT		7
#define SC8551_BAT_OVP_FLT_MASK_ENABLE		1
#define SC8551_BAT_OVP_FLT_MASK_DISABLE		0

#define SC8551_BAT_OCP_FLT_MASK_MASK		0x40
#define SC8551_BAT_OCP_FLT_MASK_SHIFT		6
#define SC8551_BAT_OCP_FLT_MASK_ENABLE		1
#define SC8551_BAT_OCP_FLT_MASK_DISABLE		0

#define SC8551_BUS_OVP_FLT_MASK_MASK		0x20
#define SC8551_BUS_OVP_FLT_MASK_SHIFT		5
#define SC8551_BUS_OVP_FLT_MASK_ENABLE		1
#define SC8551_BUS_OVP_FLT_MASK_DISABLE		0

#define SC8551_BUS_OCP_FLT_MASK_MASK		0x10
#define SC8551_BUS_OCP_FLT_MASK_SHIFT		4
#define SC8551_BUS_OCP_FLT_MASK_ENABLE		1
#define SC8551_BUS_OCP_FLT_MASK_DISABLE		0

#define SC8551_TSBUS_TSBAT_ALM_MASK_MASK	0x08
#define SC8551_TSBUS_TSBAT_ALM_MASK_SHIFT	3
#define SC8551_TSBUS_TSBAT_ALM_MASK_ENABLE	1
#define SC8551_TSBUS_TSBAT_ALM_MASK_DISABLE	0

#define SC8551_TSBAT_FLT_MASK_MASK			0x04
#define SC8551_TSBAT_FLT_MASK_SHIFT			2
#define SC8551_TSBAT_FLT_MASK_ENABLE		1
#define SC8551_TSBAT_FLT_MASK_DISABLE		0

#define SC8551_TSBUS_FLT_MASK_MASK			0x02
#define SC8551_TSBUS_FLT_MASK_SHIFT			1
#define SC8551_TSBUS_FLT_MASK_ENABLE		1
#define SC8551_TSBUS_FLT_MASK_DISABLE		0

#define SC8551_TDIE_ALM_MASK_MASK			0x01
#define SC8551_TDIE_ALM_MASK_SHIFT			0
#define SC8551_TDIE_ALM_MASK_ENABLE			1
#define SC8551_TDIE_ALM_MASK_DISABLE		0

/* Register 13h */
#define SC8551_REG_13						0x13
#define SC8551_DEV_ID_MASK					0x0F
#define SC8551_DEV_ID_SHIFT					0

/* Register 14h */
#define SC8551_REG_14						0x14
#define SC8551_ADC_EN_MASK					0x80
#define SC8551_ADC_EN_SHIFT					7
#define SC8551_ADC_ENABLE					1
#define SC8551_ADC_DISABLE					0

#define SC8551_ADC_RATE_MASK				0x40
#define SC8551_ADC_RATE_SHIFT				6
#define SC8551_ADC_RATE_CONTINOUS			0
#define SC8551_ADC_RATE_ONESHOT				1

#define SC8551_IBUS_ADC_DIS_MASK			0x01
#define SC8551_IBUS_ADC_DIS_SHIFT			0
#define SC8551_IBUS_ADC_ENABLE				0
#define SC8551_IBUS_ADC_DISABLE				1

/* Register 15h */
#define SC8551_REG_15						0x15
#define SC8551_VBUS_ADC_DIS_MASK			0x80
#define SC8551_VBUS_ADC_DIS_SHIFT			7
#define SC8551_VBUS_ADC_ENABLE				0
#define SC8551_VBUS_ADC_DISABLE				1

#define SC8551_VAC_ADC_DIS_MASK				0x40
#define SC8551_VAC_ADC_DIS_SHIFT			6
#define SC8551_VAC_ADC_ENABLE				0
#define SC8551_VAC_ADC_DISABLE				1

#define SC8551_VOUT_ADC_DIS_MASK			0x20
#define SC8551_VOUT_ADC_DIS_SHIFT			5
#define SC8551_VOUT_ADC_ENABLE				0
#define SC8551_VOUT_ADC_DISABLE				1

#define SC8551_VBAT_ADC_DIS_MASK			0x10
#define SC8551_VBAT_ADC_DIS_SHIFT			4
#define SC8551_VBAT_ADC_ENABLE				0
#define SC8551_VBAT_ADC_DISABLE				1

#define SC8551_IBAT_ADC_DIS_MASK			0x08
#define SC8551_IBAT_ADC_DIS_SHIFT			3
#define SC8551_IBAT_ADC_ENABLE				0
#define SC8551_IBAT_ADC_DISABLE				1

#define SC8551_TSBUS_ADC_DIS_MASK			0x04
#define SC8551_TSBUS_ADC_DIS_SHIFT			2
#define SC8551_TSBUS_ADC_ENABLE				0
#define SC8551_TSBUS_ADC_DISABLE			1

#define SC8551_TSBAT_ADC_DIS_MASK			0x02
#define SC8551_TSBAT_ADC_DIS_SHIFT			1
#define SC8551_TSBAT_ADC_ENABLE				0
#define SC8551_TSBAT_ADC_DISABLE			1

#define SC8551_TDIE_ADC_DIS_MASK			0x01
#define SC8551_TDIE_ADC_DIS_SHIFT			0
#define SC8551_TDIE_ADC_ENABLE				0
#define SC8551_TDIE_ADC_DISABLE				1

/* Register 16h */
#define SC8551_REG_16						0x16
#define SC8551_IBUS_POL_H_MASK				0x0F
#define SC8551_IBUS_ADC_LSB				    1.5625

/* Register 17h */
#define SC8551_REG_17						0x17
#define SC8551_IBUS_POL_L_MASK				0xFF

/* Register 18h */
#define SC8551_REG_18						0x18
#define SC8551_VBUS_POL_H_MASK				0x0F
#define SC8551_VBUS_ADC_LSB					3.75

/* Register 19h */
#define SC8551_REG_19						0x19
#define SC8551_VBUS_POL_L_MASK				0xFF

/* Register 1Ah */
#define SC8551_REG_1A						0x1A
#define SC8551_VAC_POL_H_MASK				0x0F
#define SC8551_VAC_ADC_LSB					5

/* Register 1Bh */
#define SC8551_REG_1B						0x1B
#define SC8551_VAC_POL_L_MASK				0xFF

/* Register 1Ch */
#define SC8551_REG_1C						0x1C
#define SC8551_VOUT_POL_H_MASK				0x0F
#define SC8551_VOUT_ADC_LSB					1.25

/* Register 1Dh */
#define SC8551_REG_1D						0x1D
#define SC8551_VOUT_POL_L_MASK				0x0F

/* Register 1Eh */
#define SC8551_REG_1E						0x1E
#define SC8551_VBAT_POL_H_MASK				0x0F
#define SC8551_VBAT_ADC_LSB 				1.2575

/* Register 1Fh */
#define SC8551_REG_1F						0x1F
#define SC8551_VBAT_POL_L_MASK				0xFF

/* Register 20h */
#define SC8551_REG_20						0x20
#define SC8551_IBAT_POL_H_MASK				0x0F
#define SC8551_IBAT_ADC_LSB 				3.125

/* Register 21h */
#define SC8551_REG_21						0x21
#define SC8551_IBAT_POL_L_MASK				0xFF

/* Register 22h */
#define SC8551_REG_22						0x22
#define SC8551_TSBUS_POL_H_MASK				0x03
#define SC8551_TSBUS_ADC_LSB 				0.09766

/* Register 23h */
#define SC8551_REG_23						0x23
#define SC8551_TSBUS_POL_L_MASK				0xFF

/* Register 24h */
#define SC8551_REG_24						0x24
#define SC8551_TSBAT_POL_H_MASK				0x03
#define SC8551_TSBAT_ADC_LSB 				0.09766

/* Register 25h */
#define SC8551_REG_25						0x25
#define SC8551_TSBAT_POL_L_MASK				0xFF

/* Register 26h */
#define SC8551_REG_26						0x26
#define SC8551_TDIE_POL_H_MASK				0x01
#define SC8551_TDIE_ADC_LSB 				0.5

/* Register 27h */
#define SC8551_REG_27						0x27
#define SC8551_TDIE_POL_L_MASK				0xFF

/* Register 28h */
#define SC8551_REG_28						0x28
#define SC8551_TSBUS_FLT1_MASK				0xFF
#define SC8551_TSBUS_FLT1_SHIFT				0
#define SC8551_TSBUS_FLT1_BASE				0
#define SC8551_TSBUS_FLT1_LSB				0.19531

/* Register 29h */
#define SC8551_REG_29						0x29
#define SC8551_TSBAT_FLT0_MASK				0xFF
#define SC8551_TSBAT_FLT0_SHIFT				0
#define SC8551_TSBAT_FLT0_BASE				0
#define SC8551_TSBAT_FLT0_LSB				0.19531

/* Register 2Ah */
#define SC8551_REG_2A						0x2A
#define SC8551_TDIE_ALM_MASK				0xFF
#define SC8551_TDIE_ALM_SHIFT				0
#define SC8551_TDIE_ALM_BASE				25
#define SC8551_TDIE_ALM_LSB					5 /*careful multiply is used for calc*/


/* Register 2Bh */
#define SC8551_REG_2B						0x2B
#define SC8551_SS_TIMEOUT_SET_MASK			0xE0
#define SC8551_SS_TIMEOUT_SET_SHIFT			5
#define SC8551_SS_TIMEOUT_DISABLE			0
#define SC8551_SS_TIMEOUT_12P5MS			1
#define SC8551_SS_TIMEOUT_25MS				2
#define SC8551_SS_TIMEOUT_50MS				3
#define SC8551_SS_TIMEOUT_100MS				4
#define SC8551_SS_TIMEOUT_400MS				5
#define SC8551_SS_TIMEOUT_1500MS			6
#define SC8551_SS_TIMEOUT_100000MS			7

#define SC8551_EN_REGULATION_MASK			0x10
#define SC8551_EN_REGULATION_SHIFT			4
#define SC8551_EN_REGULATION_ENABLE			1
#define SC8551_EN_REGULATION_DISABLE		0

#define SC8551_VOUT_OVP_DIS_MASK			0x08
#define SC8551_VOUT_OVP_DIS_SHIFT			3
#define SC8551_VOUT_OVP_ENABLE				1
#define SC8551_VOUT_OVP_DISABLE				0

#define SC8551_IBUS_UCP_RISE_TH_MASK		0x04
#define SC8551_IBUS_UCP_RISE_TH_SHIFT		2
#define SC8551_IBUS_UCP_RISE_150MA			0
#define SC8551_IBUS_UCP_RISE_250MA			1

#define SC8551_SET_IBAT_SNS_RES_MASK		0x02
#define SC8551_SET_IBAT_SNS_RES_SHIFT		1
#define SC8551_SET_IBAT_SNS_RES_2MHM		0
#define SC8551_SET_IBAT_SNS_RES_5MHM		1


#define SC8551_VAC_PD_EN_MASK				0x01
#define SC8551_VAC_PD_EN_SHIFT				0
#define SC8551_VAC_PD_ENABLE				1
#define SC8551_VAC_PD_DISABLE				0

/* Register 2Ch */
#define SC8551_REG_2C						0x2C
#define SC8551_IBAT_REG_MASK				0xC0
#define SC8551_IBAT_REG_SHIFT				6
#define SC8551_IBAT_REG_200MA				0
#define SC8551_IBAT_REG_300MA				1
#define SC8551_IBAT_REG_400MA				2
#define SC8551_IBAT_REG_500MA				3
#define SC8551_VBAT_REG_MASK				0x30
#define SC8551_VBAT_REG_SHIFT				4
#define SC8551_VBAT_REG_50MV				0
#define SC8551_VBAT_REG_100MV				1
#define SC8551_VBAT_REG_150MV				2
#define SC8551_VBAT_REG_200MV				3

#define SC8551_VBAT_REG_ACTIVE_STAT_MASK	0x08
#define SC8551_IBAT_REG_ACTIVE_STAT_MASK	0x04
#define SC8551_VDROP_OVP_ACTIVE_STAT_MASK	0x02
#define SC8551_VOUT_OVP_ACTIVE_STAT_MASK	0x01


#define SC8551_REG_2D						0x2D
#define SC8551_VBAT_REG_ACTIVE_FLAG_MASK	0x80
#define SC8551_IBAT_REG_ACTIVE_FLAG_MASK	0x40
#define SC8551_VDROP_OVP_FLAG_MASK			0x20
#define SC8551_VOUT_OVP_FLAG_MASK			0x10
#define SC8551_VBAT_REG_ACTIVE_MASK_MASK	0x08
#define SC8551_IBAT_REG_ACTIVE_MASK_MASK	0x04
#define SC8551_VDROP_OVP_MASK_MASK			0x02
#define SC8551_VOUT_OVP_MASK_MASK			0x01


#define SC8551_REG_2E						0x2E
#define SC8551_IBUS_LOW_DG_MASK				0x08
#define SC8551_IBUS_LOW_DG_SHIFT			3
#define SC8551_IBUS_LOW_DG_10US				0
#define SC8551_IBUS_LOW_DG_5MS				1

#define SC8551_REG_2F						0x2F
#define SC8551_PMID2OUT_UVP_FLAG_MASK		0x08
#define SC8551_PMID2OUT_OVP_FLAG_MASK		0x04
#define SC8551_PMID2OUT_UVP_STAT_MASK		0x02
#define SC8551_PMID2OUT_OVP_STAT_MASK		0x01

#define SC8551_REG_30						0x30
#define SC8551_IBUS_REG_EN_MASK				0x80
#define SC8551_IBUS_REG_EN_SHIFT			7
#define SC8551_IBUS_REG_ENABLE				1
#define SC8551_IBUS_REG_DISABLE				0
#define SC8551_IBUS_REG_ACTIVE_STAT_MASK    0x40
#define SC8551_IBUS_REG_ACTIVE_FLAG_MASK    0x20
#define SC8551_IBUS_REG_ACTIVE_MASK_MASK	0x10
#define SC8551_IBUS_REG_ACTIVE_MASK_SHIFT	4
#define SC8551_IBUS_REG_ACTIVE_NOT_MASK		0
#define SC8551_IBUS_REG_ACTIVE_MASK			1
#define SC8551_IBUS_REG_MASK                0x0F
#define SC8551_IBUS_REG_SHIFT				0
#define SC8551_IBUS_REG_BASE				1000
#define SC8551_IBUS_REG_LSB					250

#define SC8551_REG_31						0x31
#define SC8551_CHARGE_MODE_MASK				0x01
#define SC8551_CHARGE_MODE_SHIFT			0
#define SC8551_CHARGE_MODE_2_1				0
#define SC8551_CHARGE_MODE_1_1				1

#define SC8551_REG_34						0x34

#define SC8551_REG_35						0x35
#define SC8551_VBUS_RANGE_DIS_MASK			0x40
#define SC8551_VBUS_RANGE_DIS_SHIFT		6
#define	SC8551_VBUS_RANGE_ENABLE			0
#define	SC8551_VBUS_RANGE_DISABLE			1

typedef enum {
	ADC_IBUS,
	ADC_VBUS,
	ADC_VAC,
	ADC_VOUT,
	ADC_VBAT,
	ADC_IBAT,
	ADC_TBUS,
	ADC_TBAT,
	ADC_TDIE,
	ADC_MAX_NUM,
}ADC_CH;

#define SC8551_ROLE_STDALONE		0
#define SC8551_ROLE_SLAVE		1
#define SC8551_ROLE_MASTER		2

enum {
	SC_NONE = 0,
	SC8551 = 8551,
	SC_MAX = 0xFFFF,
};

enum {
	SC8551_STDALONE,
	SC8551_SLAVE,
	SC8551_MASTER,
};

static int sc8551_mode_data[] = {
	[SC8551_STDALONE] = SC8551_STDALONE,
	[SC8551_MASTER] = SC8551_ROLE_MASTER,
	[SC8551_SLAVE] = SC8551_ROLE_SLAVE,
};

#define	BAT_OVP_ALARM		BIT(7)
#define	BAT_OCP_ALARM		BIT(6)
#define	BUS_OVP_ALARM		BIT(5)
#define	BUS_OCP_ALARM		BIT(4)
#define	BAT_UCP_ALARM		BIT(3)
#define	VBUS_INSERT		BIT(2)
#define	VBAT_INSERT		BIT(1)
#define	ADC_DONE		BIT(0)

#define	BAT_OVP_FAULT		BIT(7)
#define	BAT_OCP_FAULT		BIT(6)
#define	BUS_OVP_FAULT		BIT(5)
#define	BUS_OCP_FAULT		BIT(4)
#define	TBUS_TBAT_ALARM		BIT(3)
#define	TS_BAT_FAULT		BIT(2)
#define	TS_BUS_FAULT		BIT(1)
#define	TS_DIE_FAULT		BIT(0)

/*below used for comm with other module*/
#define	BAT_OVP_FAULT_SHIFT		0
#define	BAT_OCP_FAULT_SHIFT		1
#define	BUS_OVP_FAULT_SHIFT		2
#define	BUS_OCP_FAULT_SHIFT		3
#define	BAT_THERM_FAULT_SHIFT		4
#define	BUS_THERM_FAULT_SHIFT		5
#define	DIE_THERM_FAULT_SHIFT		6

#define	BAT_OVP_FAULT_MASK		(1 << BAT_OVP_FAULT_SHIFT)
#define	BAT_OCP_FAULT_MASK		(1 << BAT_OCP_FAULT_SHIFT)
#define	BUS_OVP_FAULT_MASK		(1 << BUS_OVP_FAULT_SHIFT)
#define	BUS_OCP_FAULT_MASK		(1 << BUS_OCP_FAULT_SHIFT)
#define	BAT_THERM_FAULT_MASK		(1 << BAT_THERM_FAULT_SHIFT)
#define	BUS_THERM_FAULT_MASK		(1 << BUS_THERM_FAULT_SHIFT)
#define	DIE_THERM_FAULT_MASK		(1 << DIE_THERM_FAULT_SHIFT)

#define	BAT_OVP_ALARM_SHIFT		0
#define	BAT_OCP_ALARM_SHIFT		1
#define	BUS_OVP_ALARM_SHIFT		2
#define	BUS_OCP_ALARM_SHIFT		3
#define	BAT_THERM_ALARM_SHIFT		4
#define	BUS_THERM_ALARM_SHIFT		5
#define	DIE_THERM_ALARM_SHIFT		6
#define	BAT_UCP_ALARM_SHIFT		7

#define	BAT_OVP_ALARM_MASK		(1 << BAT_OVP_ALARM_SHIFT)
#define	BAT_OCP_ALARM_MASK		(1 << BAT_OCP_ALARM_SHIFT)
#define	BUS_OVP_ALARM_MASK		(1 << BUS_OVP_ALARM_SHIFT)
#define	BUS_OCP_ALARM_MASK		(1 << BUS_OCP_ALARM_SHIFT)
#define	BAT_THERM_ALARM_MASK		(1 << BAT_THERM_ALARM_SHIFT)
#define	BUS_THERM_ALARM_MASK		(1 << BUS_THERM_ALARM_SHIFT)
#define	DIE_THERM_ALARM_MASK		(1 << DIE_THERM_ALARM_SHIFT)
#define	BAT_UCP_ALARM_MASK		(1 << BAT_UCP_ALARM_SHIFT)

#define	VBAT_REG_STATUS_SHIFT		0
#define	IBAT_REG_STATUS_SHIFT		1

#define	VBAT_REG_STATUS_MASK		(1 << VBAT_REG_STATUS_SHIFT)
#define	IBAT_REG_STATUS_MASK		(1 << VBAT_REG_STATUS_SHIFT)

//add ipc log start
#if IS_ENABLED(CONFIG_FACTORY_BUILD)
	#if IS_ENABLED(CONFIG_DEBUG_OBJECTS)
		#define IPC_CHARGER_DEBUG_LOG
	#endif
#endif

#ifdef IPC_CHARGER_DEBUG_LOG
extern void *charger_ipc_log_context;

#define sc8551_err(fmt,...) \
	printk(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#undef pr_err
#define pr_err(_fmt, ...) \
	{ \
		if(!charger_ipc_log_context){   \
			printk(KERN_ERR pr_fmt(_fmt), ##__VA_ARGS__);    \
		}else{                                             \
			ipc_log_string(charger_ipc_log_context, "SC8551: %s %d "_fmt, __func__, __LINE__, ##__VA_ARGS__); \
		}\
	}
#define sc_err(_fmt, ...) \
	{ \
		if(!charger_ipc_log_context){   \
			printk(KERN_ERR pr_fmt(_fmt), ##__VA_ARGS__);    \
		}else{                                             \
			ipc_log_string(charger_ipc_log_context, "SC8551: %s %d "_fmt, __func__, __LINE__, ##__VA_ARGS__); \
		}\
	}

#define sc_info(_fmt, ...) \
	{ \
		if(!charger_ipc_log_context){   \
			printk(KERN_INFO pr_fmt(_fmt), ##__VA_ARGS__);    \
		}else{                                             \
			ipc_log_string(charger_ipc_log_context, "SC8551: %s %d "_fmt, __func__, __LINE__, ##__VA_ARGS__); \
		}\
	}

#else
#define sc8551_err(fmt,...)
//add ipc log end
#define sc_err(fmt, ...)                        \
do {											\
	if (sc->mode == SC8551_ROLE_MASTER)						\
		printk(KERN_ERR "[sc8551-MASTER]:%s:" fmt, __func__, ##__VA_ARGS__);	\
	else if (sc->mode == SC8551_ROLE_SLAVE)					\
		printk(KERN_ERR "[sc8551-SLAVE]:%s:" fmt, __func__, ##__VA_ARGS__);	\
	else										\
		printk(KERN_ERR "[sc8551-STANDALONE]:%s:" fmt, __func__, ##__VA_ARGS__);\
} while(0);

#define sc_info(fmt, ...)								\
do {											\
	if (sc->mode == SC8551_ROLE_MASTER)						\
		printk(KERN_INFO "[sc8551-MASTER]:%s:" fmt, __func__, ##__VA_ARGS__);	\
	else if (sc->mode == SC8551_ROLE_SLAVE)					\
		printk(KERN_INFO "[sc8551-SLAVE]:%s:" fmt, __func__, ##__VA_ARGS__);	\
	else										\
		printk(KERN_INFO "[sc8551-STANDALONE]:%s:" fmt, __func__, ##__VA_ARGS__);\
} while(0);
#endif
#define sc_dbg(fmt, ...)								\
do {											\
	if (sc->mode == SC8551_ROLE_MASTER)						\
		printk(KERN_DEBUG "[sc8551-MASTER]:%s:" fmt, __func__, ##__VA_ARGS__);	\
	else if (sc->mode == SC8551_ROLE_SLAVE)					\
		printk(KERN_DEBUG "[sc8551-SLAVE]:%s:" fmt, __func__, ##__VA_ARGS__);	\
	else										\
		printk(KERN_DEBUG "[sc8551-STANDALONE]:%s:" fmt, __func__, ##__VA_ARGS__);\
} while(0);

struct sc8551_cfg {
	bool bat_ovp_disable;
	bool bat_ocp_disable;
	bool bat_ovp_alm_disable;
	bool bat_ocp_alm_disable;

	int bat_ovp_th;
	int bat_ovp_alm_th;
	int bat_ocp_th;
	int bat_ocp_alm_th;

	bool bus_ovp_alm_disable;
	bool bus_ocp_disable;
	bool bus_ocp_alm_disable;

	int bus_ovp_th;
	int bus_ovp_alm_th;
	int bus_ocp_th;
	int bus_ocp_alm_th;

	bool bat_ucp_alm_disable;

	int bat_ucp_alm_th;
	int ac_ovp_th;

	bool bat_therm_disable;
	bool bus_therm_disable;
	bool die_therm_disable;

	int bat_therm_th; /*in %*/
	int bus_therm_th; /*in %*/
	int die_therm_th; /*in degC*/

	int sense_r_mohm;
};

struct sc8551 {
	struct device *dev;
	struct i2c_client *client;

	int part_no;
	int revision;
	int chip_vendor;

	int mode;

	struct mutex data_lock;
	struct mutex i2c_rw_lock;
	struct mutex charging_disable_lock;
	struct mutex irq_complete;

	bool irq_waiting;
	bool irq_disabled;
	bool resume_completed;

	bool batt_present;
	bool vbus_present;

	bool usb_present;
	bool charge_enabled;	/* Register bit status */

	bool is_sc8551;
	int  vbus_error;

	/* ADC reading */
	int vbat_volt;
	int vbus_volt;
	int vout_volt;
	int vac_volt;

	int ibat_curr;
	int ibus_curr;

	int bat_temp;
	int bus_temp;
	int die_temp;

	/* alarm/fault status */
	bool bat_ovp_fault;
	bool bat_ocp_fault;
	bool bus_ovp_fault;
	bool bus_ocp_fault;

	bool bat_ovp_alarm;
	bool bat_ocp_alarm;
	bool bus_ovp_alarm;
	bool bus_ocp_alarm;

	bool bat_ucp_alarm;

	bool bat_therm_alarm;
	bool bus_therm_alarm;
	bool die_therm_alarm;

	bool bat_therm_fault;
	bool bus_therm_fault;
	bool die_therm_fault;

	bool therm_shutdown_flag;
	bool therm_shutdown_stat;

	bool vbat_reg;
	bool ibat_reg;

	int  prev_alarm;
	int  prev_fault;

	int chg_ma;
	int chg_mv;

	int charge_state;

	struct sc8551_cfg *cfg;

	int skip_writes;
	int skip_reads;

	struct sc8551_platform_data *platform_data;

	struct delayed_work monitor_work;

	struct dentry *debug_root;
#ifdef CONFIG_LC_CP_POLICY_MODULE
	struct chargerpump_dev *master_cp_chg;
	struct chargerpump_dev *slave_cp_chg;
#endif
};

/************************************************************************/
static int __sc8551_read_byte(struct sc8551 *sc, u8 reg, u8 *data)
{
	s32 ret;
	int cnt = 3;

	while (cnt--) {
		ret = i2c_smbus_read_byte_data(sc->client, reg);
		if (ret < 0) {
			sc_err("i2c read fail: can't read from reg 0x%02X\n", reg);
		} else {
			*data = (u8) ret;
			return 0;
		}
		udelay(200);
	}

	return ret;
}

static int __sc8551_write_byte(struct sc8551 *sc, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(sc->client, reg, val);
	if (ret < 0) {
		sc_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
			val, reg, ret);
		return ret;
	}
	return 0;
}

static int sc8551_read_byte(struct sc8551 *sc, u8 reg, u8 *data)
{
	int ret;

	if (sc->skip_reads) {
		*data = 0;
		return 0;
	}

	mutex_lock(&sc->i2c_rw_lock);
	ret = __sc8551_read_byte(sc, reg, data);
	mutex_unlock(&sc->i2c_rw_lock);

	return ret;
}

static int sc8551_write_byte(struct sc8551 *sc, u8 reg, u8 data)
{
	int ret;

	if (sc->skip_writes)
		return 0;

	mutex_lock(&sc->i2c_rw_lock);
	ret = __sc8551_write_byte(sc, reg, data);
	mutex_unlock(&sc->i2c_rw_lock);

	return ret;
}

static int sc8551_update_bits(struct sc8551*sc, u8 reg,
					u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	if (sc->skip_reads || sc->skip_writes)
		return 0;

	mutex_lock(&sc->i2c_rw_lock);
	ret = __sc8551_read_byte(sc, reg, &tmp);
	if (ret) {
		sc_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	ret = __sc8551_write_byte(sc, reg, tmp);
	if (ret)
		sc_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&sc->i2c_rw_lock);
	return ret;
}

/*********************************************************************/

static int sc8551_enable_charge(struct sc8551 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SC8551_CHG_ENABLE;
	else
		val = SC8551_CHG_DISABLE;

	val <<= SC8551_CHG_EN_SHIFT;

	sc_err("sc8551 charger %s\n", enable == false ? "disable" : "enable");
	ret = sc8551_update_bits(sc, SC8551_REG_0C,
				SC8551_CHG_EN_MASK, val);

	return ret;
}

static int sc8551_check_charge_enabled(struct sc8551 *sc, bool *enabled)
{
	int ret;
	u8 val;

	ret = sc8551_read_byte(sc, SC8551_REG_0C, &val);
	sc_info(">>>reg [0x0c] = 0x%02x\n", val);
	if (!ret)
		*enabled = !!(val & SC8551_CHG_EN_MASK);
	return ret;
}
#if 0
static void sc8551_dump_reg(struct sc8551 *sc)
{
	int ret;
	u8 val;
	u8 addr;

	for (addr = 0x00; addr <= 0x36; addr++) {
		ret = sc8551_read_byte(sc, addr, &val);
		if (!ret)
			sc_info("Reg[%02X] = 0x%02X\n", addr, val);
	}
}
#endif

static int sc8551_enable_wdt(struct sc8551 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SC8551_WATCHDOG_ENABLE;
	else
		val = SC8551_WATCHDOG_DISABLE;

	val <<= SC8551_WATCHDOG_DIS_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_0B,
				SC8551_WATCHDOG_DIS_MASK, val);
	return ret;
}

/* define unused */
#if 0
static int sc8551_set_wdt(struct sc8551 *sc, int ms)
{
	int ret;
	u8 val;

	if (ms == 500)
		val = SC8551_WATCHDOG_0P5S;
	else if (ms == 1000)
		val = SC8551_WATCHDOG_1S;
	else if (ms == 5000)
		val = SC8551_WATCHDOG_5S;
	else if (ms == 30000)
		val = SC8551_WATCHDOG_30S;
	else
		val = SC8551_WATCHDOG_30S;

	val <<= SC8551_WATCHDOG_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_0B,
				SC8551_WATCHDOG_MASK, val);
	return ret;
}
#endif

static int sc8551_set_reg_reset(struct sc8551 *sc)
{
	int ret;
	u8 val = 1;

	sc_err("mode : %d  reg reset\n", sc->mode);

	val = SC8551_REG_RST_ENABLE;

	val <<= SC8551_REG_RST_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_0B,
				SC8551_REG_RST_MASK, val);
	return ret;
}

static int sc8551_enable_batovp(struct sc8551 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SC8551_BAT_OVP_ENABLE;
	else
		val = SC8551_BAT_OVP_DISABLE;

	val <<= SC8551_BAT_OVP_DIS_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_00,
				SC8551_BAT_OVP_DIS_MASK, val);
	return ret;
}

static int sc8551_set_batovp_th(struct sc8551 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SC8551_BAT_OVP_BASE)
		threshold = SC8551_BAT_OVP_BASE;

	val = (threshold - SC8551_BAT_OVP_BASE) / SC8551_BAT_OVP_LSB;

	val <<= SC8551_BAT_OVP_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_00,
				SC8551_BAT_OVP_MASK, val);
	return ret;
}

static int sc8551_enable_batovp_alarm(struct sc8551 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SC8551_BAT_OVP_ALM_ENABLE;
	else
		val = SC8551_BAT_OVP_ALM_DISABLE;

	val <<= SC8551_BAT_OVP_ALM_DIS_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_01,
				SC8551_BAT_OVP_ALM_DIS_MASK, val);
	return ret;
}

static int sc8551_set_batovp_alarm_th(struct sc8551 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SC8551_BAT_OVP_ALM_BASE)
		threshold = SC8551_BAT_OVP_ALM_BASE;

	val = (threshold - SC8551_BAT_OVP_ALM_BASE) / SC8551_BAT_OVP_ALM_LSB;

	val <<= SC8551_BAT_OVP_ALM_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_01,
				SC8551_BAT_OVP_ALM_MASK, val);
	return ret;
}

static int sc8551_enable_batocp(struct sc8551 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SC8551_BAT_OCP_ENABLE;
	else
		val = SC8551_BAT_OCP_DISABLE;

	val <<= SC8551_BAT_OCP_DIS_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_02,
				SC8551_BAT_OCP_DIS_MASK, val);
	return ret;
}

static int sc8551_set_batocp_th(struct sc8551 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SC8551_BAT_OCP_BASE)
		threshold = SC8551_BAT_OCP_BASE;

	val = (threshold - SC8551_BAT_OCP_BASE) / SC8551_BAT_OCP_LSB;

	val <<= SC8551_BAT_OCP_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_02,
				SC8551_BAT_OCP_MASK, val);
	return ret;
}

static int sc8551_enable_batocp_alarm(struct sc8551 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SC8551_BAT_OCP_ALM_ENABLE;
	else
		val = SC8551_BAT_OCP_ALM_DISABLE;

	val <<= SC8551_BAT_OCP_ALM_DIS_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_03,
				SC8551_BAT_OCP_ALM_DIS_MASK, val);
	return ret;
}

static int sc8551_set_batocp_alarm_th(struct sc8551 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SC8551_BAT_OCP_ALM_BASE)
		threshold = SC8551_BAT_OCP_ALM_BASE;

	val = (threshold - SC8551_BAT_OCP_ALM_BASE) / SC8551_BAT_OCP_ALM_LSB;

	val <<= SC8551_BAT_OCP_ALM_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_03,
				SC8551_BAT_OCP_ALM_MASK, val);
	return ret;
}

static int sc8551_set_busovp_th(struct sc8551 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SC8551_BUS_OVP_BASE)
		threshold = SC8551_BUS_OVP_BASE;

	val = (threshold - SC8551_BUS_OVP_BASE) / SC8551_BUS_OVP_LSB;

	val <<= SC8551_BUS_OVP_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_06,
				SC8551_BUS_OVP_MASK, val);
	return ret;
}

static int sc8551_enable_busovp_alarm(struct sc8551 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SC8551_BUS_OVP_ALM_ENABLE;
	else
		val = SC8551_BUS_OVP_ALM_DISABLE;

	val <<= SC8551_BUS_OVP_ALM_DIS_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_07,
				SC8551_BUS_OVP_ALM_DIS_MASK, val);
	return ret;
}

static int sc8551_set_busovp_alarm_th(struct sc8551 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SC8551_BUS_OVP_ALM_BASE)
		threshold = SC8551_BUS_OVP_ALM_BASE;

	val = (threshold - SC8551_BUS_OVP_ALM_BASE) / SC8551_BUS_OVP_ALM_LSB;

	val <<= SC8551_BUS_OVP_ALM_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_07,
				SC8551_BUS_OVP_ALM_MASK, val);
	return ret;
}

static int sc8551_enable_busocp(struct sc8551 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SC8551_BUS_OCP_ENABLE;
	else
		val = SC8551_BUS_OCP_DISABLE;

	val <<= SC8551_BUS_OCP_DIS_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_08,
				SC8551_BUS_OCP_DIS_MASK, val);
	return ret;
}

static int sc8551_set_busocp_th(struct sc8551 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SC8551_BUS_OCP_BASE)
		threshold = SC8551_BUS_OCP_BASE;

	val = (threshold - SC8551_BUS_OCP_BASE) / SC8551_BUS_OCP_LSB;

	val <<= SC8551_BUS_OCP_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_08,
				SC8551_BUS_OCP_MASK, val);
	return ret;
}

static int sc8551_enable_busocp_alarm(struct sc8551 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SC8551_BUS_OCP_ALM_ENABLE;
	else
		val = SC8551_BUS_OCP_ALM_DISABLE;

	val <<= SC8551_BUS_OCP_ALM_DIS_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_09,
				SC8551_BUS_OCP_ALM_DIS_MASK, val);
	return ret;
}

static int sc8551_set_busocp_alarm_th(struct sc8551 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SC8551_BUS_OCP_ALM_BASE)
		threshold = SC8551_BUS_OCP_ALM_BASE;

	val = (threshold - SC8551_BUS_OCP_ALM_BASE) / SC8551_BUS_OCP_ALM_LSB;

	val <<= SC8551_BUS_OCP_ALM_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_09,
				SC8551_BUS_OCP_ALM_MASK, val);
	return ret;
}

static int sc8551_enable_batucp_alarm(struct sc8551 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SC8551_BAT_UCP_ALM_ENABLE;
	else
		val = SC8551_BAT_UCP_ALM_DISABLE;

	val <<= SC8551_BAT_UCP_ALM_DIS_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_04,
				SC8551_BAT_UCP_ALM_DIS_MASK, val);
	return ret;
}

static int sc8551_set_batucp_alarm_th(struct sc8551 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SC8551_BAT_UCP_ALM_BASE)
		threshold = SC8551_BAT_UCP_ALM_BASE;

	val = (threshold - SC8551_BAT_UCP_ALM_BASE) / SC8551_BAT_UCP_ALM_LSB;

	val <<= SC8551_BAT_UCP_ALM_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_04,
				SC8551_BAT_UCP_ALM_MASK, val);
	return ret;
}

static int sc8551_set_acovp_th(struct sc8551 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SC8551_AC_OVP_BASE)
		threshold = SC8551_AC_OVP_BASE;

	if (threshold == SC8551_AC_OVP_6P5V)
		val = 0x07;
	else
		val = (threshold - SC8551_AC_OVP_BASE) /  SC8551_AC_OVP_LSB;

	val <<= SC8551_AC_OVP_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_05,
				SC8551_AC_OVP_MASK, val);

	return ret;

}

static int sc8551_set_vdrop_th(struct sc8551 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold == 300)
		val = SC8551_VDROP_THRESHOLD_300MV;
	else
		val = SC8551_VDROP_THRESHOLD_400MV;

	val <<= SC8551_VDROP_THRESHOLD_SET_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_05,
				SC8551_VDROP_THRESHOLD_SET_MASK,
				val);

	return ret;
}

static int sc8551_set_vdrop_deglitch(struct sc8551 *sc, int us)
{
	int ret;
	u8 val;

	if (us == 8)
		val = SC8551_VDROP_DEGLITCH_8US;
	else
		val = SC8551_VDROP_DEGLITCH_5MS;

	val <<= SC8551_VDROP_DEGLITCH_SET_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_05,
				SC8551_VDROP_DEGLITCH_SET_MASK,
				val);
	return ret;
}

static int sc8551_enable_bat_therm(struct sc8551 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SC8551_TSBAT_ENABLE;
	else
		val = SC8551_TSBAT_DISABLE;

	val <<= SC8551_TSBAT_DIS_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_0C,
				SC8551_TSBAT_DIS_MASK, val);
	return ret;
}

/*
 * the input threshold is the raw value that would write to register directly.
 */
static int sc8551_set_bat_therm_th(struct sc8551 *sc, u8 threshold)
{
	int ret;

	ret = sc8551_write_byte(sc, SC8551_REG_29, threshold);
	return ret;
}

static int sc8551_enable_bus_therm(struct sc8551 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SC8551_TSBUS_ENABLE;
	else
		val = SC8551_TSBUS_DISABLE;

	val <<= SC8551_TSBUS_DIS_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_0C,
				SC8551_TSBUS_DIS_MASK, val);
	return ret;
}

/*
 * the input threshold is the raw value that would write to register directly.
 */
static int sc8551_set_bus_therm_th(struct sc8551 *sc, u8 threshold)
{
	int ret;

	ret = sc8551_write_byte(sc, SC8551_REG_28, threshold);
	return ret;
}

/*
 * please be noted that the unit here is degC
 */
static int sc8551_set_die_therm_th(struct sc8551 *sc, u8 threshold)
{
	int ret;
	u8 val;

	/*BE careful, LSB is here is 1/LSB, so we use multiply here*/
	val = (threshold - SC8551_TDIE_ALM_BASE) * 10/SC8551_TDIE_ALM_LSB;
	val <<= SC8551_TDIE_ALM_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_2A,
				SC8551_TDIE_ALM_MASK, val);
	return ret;
}

static int sc8551_enable_adc(struct sc8551 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SC8551_ADC_ENABLE;
	else
		val = SC8551_ADC_DISABLE;

	val <<= SC8551_ADC_EN_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_14,
				SC8551_ADC_EN_MASK, val);
	return ret;
}

static int sc8551_set_adc_scanrate(struct sc8551 *sc, bool oneshot)
{
	int ret;
	u8 val;

	if (oneshot)
		val = SC8551_ADC_RATE_ONESHOT;
	else
		val = SC8551_ADC_RATE_CONTINOUS;

	val <<= SC8551_ADC_RATE_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_14,
				SC8551_ADC_EN_MASK, val);
	return ret;
}

static inline int sc_to_sc8551_adc(enum sc_adc_channel chan)
{
	switch (chan) {
	case ADC_GET_VBUS:
		return ADC_VBUS;
	case ADC_GET_VBAT:
		return ADC_VBAT;
	case ADC_GET_IBUS:
		return ADC_IBUS;
	case ADC_GET_IBAT:
		return ADC_IBAT;
	case ADC_GET_TDIE:
		return ADC_TDIE;
	default:
		break;
	}
	return ADC_MAX_NUM;
}

#define ADC_REG_BASE SC8551_REG_16
static int sc8551_get_adc_data(struct sc8551 *sc, int channel,  int *result)
{
	int ret;
	u8 val_l, val_h;
	u16 val;

	if(channel < 0 || channel >= ADC_MAX_NUM) return 0;

	ret = sc8551_read_byte(sc, ADC_REG_BASE + (channel << 1), &val_h);
	ret = sc8551_read_byte(sc, ADC_REG_BASE + (channel << 1) + 1, &val_l);

	if (ret < 0)
		return ret;
	val = (val_h << 8) | val_l;

	if (sc->chip_vendor == SC8551) {
		if (channel == ADC_IBUS) {		val = val * 15625/10000;}
		else if (channel == ADC_VBUS)		val = val * 375/100;
		else if (channel == ADC_VAC)		val = val * 5;
		else if (channel == ADC_VOUT)		val = val * 125 / 100;
		else if (channel == ADC_VBAT)		val = val * 12575/10000;
		else if (channel == ADC_IBAT)		val = val * 3125/1000 ;
		else if (channel == ADC_TBUS)		val = val * 9766/100000;
		else if (channel == ADC_TBAT)		val = val * 9766/100000;
		else if (channel == ADC_TDIE)		val = val * 5/10;
	}
	*result = val;
	return 0;
}

static int sc8551_set_adc_scan(struct sc8551 *sc, int channel, bool enable)
{
	int ret;
	u8 reg;
	u8 mask;
	u8 shift;
	u8 val;

	if (channel > ADC_MAX_NUM)
		return -EINVAL;

	if (channel == ADC_IBUS) {
		reg = SC8551_REG_14;
		shift = SC8551_IBUS_ADC_DIS_SHIFT;
		mask = SC8551_IBUS_ADC_DIS_MASK;
	} else {
		reg = SC8551_REG_15;
		shift = 8 - channel;
		mask = 1 << shift;
	}

	if (enable)
		val = 0 << shift;
	else
		val = 1 << shift;

	ret = sc8551_update_bits(sc, reg, mask, val);

	return ret;
}

static int sc8551_set_alarm_int_mask(struct sc8551 *sc, u8 mask)
{
	int ret;
	u8 val;

	ret = sc8551_read_byte(sc, SC8551_REG_0F, &val);
	if (ret)
		return ret;

	val |= mask;

	ret = sc8551_write_byte(sc, SC8551_REG_0F, val);

	return ret;
}

/* define unused */
#if 0
static int sc8551_clear_alarm_int_mask(struct sc8551 *sc, u8 mask)
{
	int ret;
	u8 val;

	ret = sc8551_read_byte(sc, SC8551_REG_0F, &val);
	if (ret)
		return ret;

	val &= ~mask;

	ret = sc8551_write_byte(sc, SC8551_REG_0F, val);

	return ret;
}

static int sc8551_set_fault_int_mask(struct sc8551 *sc, u8 mask)
{
	int ret;
	u8 val;

	ret = sc8551_read_byte(sc, SC8551_REG_12, &val);
	if (ret)
		return ret;

	val |= mask;

	ret = sc8551_write_byte(sc, SC8551_REG_12, val);

	return ret;
}

static int sc8551_clear_fault_int_mask(struct sc8551 *sc, u8 mask)
{
	int ret;
	u8 val;

	ret = sc8551_read_byte(sc, SC8551_REG_12, &val);
	if (ret)
		return ret;

	val &= ~mask;

	ret = sc8551_write_byte(sc, SC8551_REG_12, val);

	return ret;
}
#endif

static int sc8551_set_sense_resistor(struct sc8551 *sc, int r_mohm)
{
	int ret;
	u8 val;

	if (r_mohm == 2)
		val = SC8551_SET_IBAT_SNS_RES_2MHM;
	else if (r_mohm == 5)
		val = SC8551_SET_IBAT_SNS_RES_5MHM;
	else
		return -EINVAL;

	val <<= SC8551_SET_IBAT_SNS_RES_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_2B,
				SC8551_SET_IBAT_SNS_RES_MASK,
				val);
	return ret;
}

static int sc8551_enable_regulation(struct sc8551 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SC8551_EN_REGULATION_ENABLE;
	else
		val = SC8551_EN_REGULATION_DISABLE;

	val <<= SC8551_EN_REGULATION_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_2B,
				SC8551_EN_REGULATION_MASK,
				val);

	return ret;

}

static int sc8551_set_ss_timeout(struct sc8551 *sc, int timeout)
{
	int ret;
	u8 val;

	switch (timeout) {
	case 0:
		val = SC8551_SS_TIMEOUT_DISABLE;
		break;
	case 12:
		val = SC8551_SS_TIMEOUT_12P5MS;
		break;
	case 25:
		val = SC8551_SS_TIMEOUT_25MS;
		break;
	case 50:
		val = SC8551_SS_TIMEOUT_50MS;
		break;
	case 100:
		val = SC8551_SS_TIMEOUT_100MS;
		break;
	case 400:
		val = SC8551_SS_TIMEOUT_400MS;
		break;
	case 1500:
		val = SC8551_SS_TIMEOUT_1500MS;
		break;
	case 100000:
		val = SC8551_SS_TIMEOUT_100000MS;
		break;
	default:
		val = SC8551_SS_TIMEOUT_DISABLE;
		break;
	}

	val <<= SC8551_SS_TIMEOUT_SET_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_2B,
				SC8551_SS_TIMEOUT_SET_MASK,
				val);

	return ret;
}

static int sc8551_set_ibat_reg_th(struct sc8551 *sc, int th_ma)
{
	int ret;
	u8 val;

	if (th_ma == 200)
		val = SC8551_IBAT_REG_200MA;
	else if (th_ma == 300)
		val = SC8551_IBAT_REG_300MA;
	else if (th_ma == 400)
		val = SC8551_IBAT_REG_400MA;
	else if (th_ma == 500)
		val = SC8551_IBAT_REG_500MA;
	else
		val = SC8551_IBAT_REG_500MA;

	val <<= SC8551_IBAT_REG_SHIFT;
	ret = sc8551_update_bits(sc, SC8551_REG_2C,
				SC8551_IBAT_REG_MASK,
				val);

	return ret;

}

static int sc8551_set_vbat_reg_th(struct sc8551 *sc, int th_mv)
{
	int ret;
	u8 val;

	if (th_mv == 50)
		val = SC8551_VBAT_REG_50MV;
	else if (th_mv == 100)
		val = SC8551_VBAT_REG_100MV;
	else if (th_mv == 150)
		val = SC8551_VBAT_REG_150MV;
	else
		val = SC8551_VBAT_REG_200MV;

	val <<= SC8551_VBAT_REG_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_2C,
				SC8551_VBAT_REG_MASK,
				val);

	return ret;
}

#if 0
static int sc8551_get_work_mode(struct sc8551 *sc, int *mode)
{
	int ret;
	u8 val;

	ret = sc8551_read_byte(sc, SC8551_REG_0C, &val);

	if (ret) {
		sc_err("Failed to read operation mode register\n");
		return ret;
	}

	val = (val & SC8551_MS_MASK) >> SC8551_MS_SHIFT;
	if (val == SC8551_MS_MASTER)
		*mode = SC8551_ROLE_MASTER;
	else if (val == SC8551_MS_SLAVE)
		*mode = SC8551_ROLE_SLAVE;
	else
		*mode = SC8551_ROLE_STDALONE;

	sc_info("work mode:%s\n", *mode == SC8551_ROLE_STDALONE ? "Standalone" :
			(*mode == SC8551_ROLE_SLAVE ? "Slave" : "Master"));
	return ret;
}
#endif
__maybe_unused
static int sc8551_check_vbus_error_status(struct sc8551 *sc)
{
	int ret;
	u8 data;

	ret = sc8551_read_byte(sc, SC8551_REG_0A, &data);
	if(ret == 0){
		sc_err("vbus error >>>>%02x\n", data);
		sc->vbus_error = data;
	}

	return ret;
}

static int sc8551_detect_device(struct sc8551 *sc)
{
	int ret;
	u8 data;

	ret = sc8551_read_byte(sc, SC8551_REG_13, &data);
	if (ret == 0) {
		sc->part_no = (data & SC8551_DEV_ID_MASK);
		sc->part_no >>= SC8551_DEV_ID_SHIFT;
		if (data == SC8551_DEVICE_ID || data == SC8551A_DEVICE_ID)
			sc->chip_vendor = SC8551;
	}

	pr_err("sc8551_detect_device:PART_INFO:0x%x", data);
	return ret;
}

static int sc8551_parse_dt(struct sc8551 *sc, struct device *dev)
{
	int ret;
	struct device_node *np = dev->of_node;

	sc->cfg = devm_kzalloc(dev, sizeof(struct sc8551_cfg),
					GFP_KERNEL);

	if (!sc->cfg)
		return -ENOMEM;

	sc->cfg->bat_ovp_disable = of_property_read_bool(np,
			"sc,sc8551,bat-ovp-disable");
	sc->cfg->bat_ocp_disable = of_property_read_bool(np,
			"sc,sc8551,bat-ocp-disable");
	sc->cfg->bat_ovp_alm_disable = of_property_read_bool(np,
			"sc,sc8551,bat-ovp-alarm-disable");
	sc->cfg->bat_ocp_alm_disable = of_property_read_bool(np,
			"sc,sc8551,bat-ocp-alarm-disable");
	sc->cfg->bus_ocp_disable = of_property_read_bool(np,
			"sc,sc8551,bus-ocp-disable");
	sc->cfg->bus_ovp_alm_disable = of_property_read_bool(np,
			"sc,sc8551,bus-ovp-alarm-disable");
	sc->cfg->bus_ocp_alm_disable = of_property_read_bool(np,
			"sc,sc8551,bus-ocp-alarm-disable");
	sc->cfg->bat_ucp_alm_disable = of_property_read_bool(np,
			"sc,sc8551,bat-ucp-alarm-disable");
	sc->cfg->bat_therm_disable = of_property_read_bool(np,
			"sc,sc8551,bat-therm-disable");
	sc->cfg->bus_therm_disable = of_property_read_bool(np,
			"sc,sc8551,bus-therm-disable");

	ret = of_property_read_u32(np, "sc,sc8551,bat-ovp-threshold",
			&sc->cfg->bat_ovp_th);
	if (ret) {
		sc_err("failed to read bat-ovp-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "sc,sc8551,bat-ovp-alarm-threshold",
			&sc->cfg->bat_ovp_alm_th);
	if (ret) {
		sc_err("failed to read bat-ovp-alarm-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "sc,sc8551,bat-ocp-threshold",
			&sc->cfg->bat_ocp_th);
	if (ret) {
		sc_err("failed to read bat-ocp-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "sc,sc8551,bat-ocp-alarm-threshold",
			&sc->cfg->bat_ocp_alm_th);
	if (ret) {
		sc_err("failed to read bat-ocp-alarm-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "sc,sc8551,bus-ovp-threshold",
			&sc->cfg->bus_ovp_th);
	if (ret) {
		sc_err("failed to read bus-ovp-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "sc,sc8551,bus-ovp-alarm-threshold",
			&sc->cfg->bus_ovp_alm_th);
	if (ret) {
		sc_err("failed to read bus-ovp-alarm-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "sc,sc8551,bus-ocp-threshold",
			&sc->cfg->bus_ocp_th);
	if (ret) {
		sc_err("failed to read bus-ocp-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "sc,sc8551,bus-ocp-alarm-threshold",
			&sc->cfg->bus_ocp_alm_th);
	if (ret) {
		sc_err("failed to read bus-ocp-alarm-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "sc,sc8551,bat-ucp-alarm-threshold",
			&sc->cfg->bat_ucp_alm_th);
	if (ret) {
		sc_err("failed to read bat-ucp-alarm-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "sc,sc8551,bat-therm-threshold",
			&sc->cfg->bat_therm_th);
	if (ret) {
		sc_err("failed to read bat-therm-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "sc,sc8551,bus-therm-threshold",
			&sc->cfg->bus_therm_th);
	if (ret) {
		sc_err("failed to read bus-therm-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "sc,sc8551,die-therm-threshold",
			&sc->cfg->die_therm_th);
	if (ret) {
		sc_err("failed to read die-therm-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8551,ac-ovp-threshold",
			&sc->cfg->ac_ovp_th);
	if (ret) {
		sc_err("failed to read ac-ovp-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8551,sense-resistor-mohm",
			&sc->cfg->sense_r_mohm);
	if (ret) {
		sc_err("failed to read sense-resistor-mohm\n");
		return ret;
	}


	return 0;
}


static int sc8551_init_protection(struct sc8551 *sc)
{
	int ret;

	ret = sc8551_enable_batovp(sc, !sc->cfg->bat_ovp_disable);
	sc_info("%s bat ovp %s\n",
		sc->cfg->bat_ovp_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = sc8551_enable_batocp(sc, !sc->cfg->bat_ocp_disable);
	sc_info("%s bat ocp %s\n",
		sc->cfg->bat_ocp_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = sc8551_enable_batovp_alarm(sc, !sc->cfg->bat_ovp_alm_disable);
	sc_info("%s bat ovp alarm %s\n",
		sc->cfg->bat_ovp_alm_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = sc8551_enable_batocp_alarm(sc, !sc->cfg->bat_ocp_alm_disable);
	sc_info("%s bat ocp alarm %s\n",
		sc->cfg->bat_ocp_alm_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = sc8551_enable_batucp_alarm(sc, !sc->cfg->bat_ucp_alm_disable);
	sc_info("%s bat ocp alarm %s\n",
		sc->cfg->bat_ucp_alm_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = sc8551_enable_busovp_alarm(sc, !sc->cfg->bus_ovp_alm_disable);
	sc_info("%s bus ovp alarm %s\n",
		sc->cfg->bus_ovp_alm_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = sc8551_enable_busocp(sc, !sc->cfg->bus_ocp_disable);
	sc_info("%s bus ocp %s\n",
		sc->cfg->bus_ocp_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = sc8551_enable_busocp_alarm(sc, !sc->cfg->bus_ocp_alm_disable);
	sc_info("%s bus ocp alarm %s\n",
		sc->cfg->bus_ocp_alm_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = sc8551_enable_bat_therm(sc, !sc->cfg->bat_therm_disable);
	sc_info("%s bat therm %s\n",
		sc->cfg->bat_therm_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = sc8551_enable_bus_therm(sc, !sc->cfg->bus_therm_disable);
	sc_info("%s bus therm %s\n",
		sc->cfg->bus_therm_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = sc8551_set_batovp_th(sc, sc->cfg->bat_ovp_th);
	sc_info("set bat ovp th %d %s\n", sc->cfg->bat_ovp_th,
		!ret ? "successfully" : "failed");

	ret = sc8551_set_batovp_alarm_th(sc, sc->cfg->bat_ovp_alm_th);
	sc_info("set bat ovp alarm threshold %d %s\n", sc->cfg->bat_ovp_alm_th,
		!ret ? "successfully" : "failed");

	ret = sc8551_set_batocp_th(sc, sc->cfg->bat_ocp_th);
	sc_info("set bat ocp threshold %d %s\n", sc->cfg->bat_ocp_th,
		!ret ? "successfully" : "failed");

	ret = sc8551_set_batocp_alarm_th(sc, sc->cfg->bat_ocp_alm_th);
	sc_info("set bat ocp alarm threshold %d %s\n", sc->cfg->bat_ocp_alm_th,
		!ret ? "successfully" : "failed");

	ret = sc8551_set_busovp_th(sc, sc->cfg->bus_ovp_th);
	sc_info("set bus ovp threshold %d %s\n", sc->cfg->bus_ovp_th,
		!ret ? "successfully" : "failed");

	ret = sc8551_set_busovp_alarm_th(sc, sc->cfg->bus_ovp_alm_th);
	sc_info("set bus ovp alarm threshold %d %s\n", sc->cfg->bus_ovp_alm_th,
		!ret ? "successfully" : "failed");

	ret = sc8551_set_busocp_th(sc, sc->cfg->bus_ocp_th);
	sc_info("set bus ocp threshold %d %s\n", sc->cfg->bus_ocp_th,
		!ret ? "successfully" : "failed");

	ret = sc8551_set_busocp_alarm_th(sc, sc->cfg->bus_ocp_alm_th);
	sc_info("set bus ocp alarm th %d %s\n", sc->cfg->bus_ocp_alm_th,
		!ret ? "successfully" : "failed");

	ret = sc8551_set_batucp_alarm_th(sc, sc->cfg->bat_ucp_alm_th);
	sc_info("set bat ucp threshold %d %s\n", sc->cfg->bat_ucp_alm_th,
		!ret ? "successfully" : "failed");

	ret = sc8551_set_bat_therm_th(sc, sc->cfg->bat_therm_th);
	sc_info("set die therm threshold %d %s\n", sc->cfg->bat_therm_th,
		!ret ? "successfully" : "failed");
	ret = sc8551_set_bus_therm_th(sc, sc->cfg->bus_therm_th);
	sc_info("set bus therm threshold %d %s\n", sc->cfg->bus_therm_th,
		!ret ? "successfully" : "failed");
	ret = sc8551_set_die_therm_th(sc, sc->cfg->die_therm_th);
	sc_info("set die therm threshold %d %s\n", sc->cfg->die_therm_th,
		!ret ? "successfully" : "failed");

	ret = sc8551_set_acovp_th(sc, sc->cfg->ac_ovp_th);
	sc_info("set ac ovp threshold %d %s\n", sc->cfg->ac_ovp_th,
		!ret ? "successfully" : "failed");

	return 0;
}

static int sc8551_init_adc(struct sc8551 *sc)
{

	sc8551_set_adc_scanrate(sc, false);
	sc8551_set_adc_scan(sc, ADC_IBUS, true);
	sc8551_set_adc_scan(sc, ADC_VBUS, true);
	sc8551_set_adc_scan(sc, ADC_VOUT, true);
	sc8551_set_adc_scan(sc, ADC_VBAT, true);
	sc8551_set_adc_scan(sc, ADC_IBAT, true);
	sc8551_set_adc_scan(sc, ADC_TBUS, true);
	sc8551_set_adc_scan(sc, ADC_TBAT, true);
	sc8551_set_adc_scan(sc, ADC_TDIE, true);
	sc8551_set_adc_scan(sc, ADC_VAC, true);

	/* improve adc accuracy */
	sc8551_write_byte(sc, SC8551_REG_34, 0x01);

	sc8551_enable_adc(sc, true);

	return 0;
}

static int sc8551_init_int_src(struct sc8551 *sc)
{
	int ret;
	/*TODO:be careful ts bus and ts bat alarm bit mask is in
	 *	fault mask register, so you need call
	 *	sc8551_set_fault_int_mask for tsbus and tsbat alarm
	 */
	ret = sc8551_set_alarm_int_mask(sc, ADC_DONE
		/*			| BAT_UCP_ALARM */
					| BAT_OVP_ALARM);
	if (ret) {
		sc_err("failed to set alarm mask:%d\n", ret);
		return ret;
	}
#if 0
	ret = sc8551_set_fault_int_mask(sc, TS_BUS_FAULT);
	if (ret) {
		sc_err("failed to set fault mask:%d\n", ret);
		return ret;
	}
#endif
	return ret;
}

static int sc8551_init_regulation(struct sc8551 *sc)
{
	sc8551_set_ibat_reg_th(sc, 300);
	sc8551_set_vbat_reg_th(sc, 100);

	sc8551_set_vdrop_deglitch(sc, 5000);
	sc8551_set_vdrop_th(sc, 400);

	sc8551_enable_regulation(sc, false);

	if(sc->is_sc8551)
	{
		sc8551_write_byte(sc, SC8551_REG_2E, 0x08);
		sc8551_write_byte(sc, SC8551_REG_34, 0x01);
	}
	return 0;
}

static int sc8551_disable_vbus_range(struct sc8551 *sc, bool disable)
{
	int ret;
	u8 val;
	ret = sc8551_read_byte(sc, SC8551_REG_35, &val);
	sc_info("sc8551_disable_vbus_range:read0x35:0x%x", val);

	if (disable)
		val = SC8551_VBUS_RANGE_DISABLE;
	else
		val = SC8551_VBUS_RANGE_ENABLE;

	val <<= SC8551_VBUS_RANGE_DIS_SHIFT;

	ret = sc8551_update_bits(sc, SC8551_REG_35,
				SC8551_VBUS_RANGE_DIS_MASK, val);

	sc8551_read_byte(sc, SC8551_REG_35, &val);
	sc_info("sc8551_disable_vbus_range:read0x35:0x%x", val);
	return ret;
}
static int sc8551_init_device(struct sc8551 *sc)
{
	sc_info("sc8551_init_device");
	sc8551_set_reg_reset(sc);
	sc8551_enable_wdt(sc, false);

	sc8551_set_ss_timeout(sc, 100000);
	sc8551_set_sense_resistor(sc, sc->cfg->sense_r_mohm);

	sc8551_init_protection(sc);
	sc8551_init_adc(sc);
	sc8551_init_int_src(sc);

	sc8551_init_regulation(sc);

	if (sc->mode == SC8551_ROLE_SLAVE){
		sc8551_disable_vbus_range(sc, true);
	}

	//sc8551_dump_reg(sc);
	return 0;
}

__maybe_unused
static int sc8551_set_present(struct sc8551 *sc, bool present)
{
	sc->usb_present = present;

	if (present)
		sc8551_init_device(sc);
	return 0;
}

static ssize_t sc8551_show_registers(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sc8551 *sc = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[300];
	int len;
	int idx = 0;
	int ret;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sc8551");
	for (addr = 0x0; addr <= 0x31; addr++) {
		ret = sc8551_read_byte(sc, addr, &val);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,
					"Reg[%.2X] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t sc8551_store_register(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sc8551 *sc = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg <= 0x31)
		sc8551_write_byte(sc, (unsigned char)reg, (unsigned char)val);

	return count;
}


static DEVICE_ATTR(registers, 0660, sc8551_show_registers, sc8551_store_register);
static void sc8551_check_alarm_status(struct sc8551 *sc);
static void sc8551_check_fault_status(struct sc8551 *sc);
static void sc8551_create_device_node(struct device *dev)
{
	device_create_file(dev, &dev_attr_registers);
}

__maybe_unused
static void sc8551_check_alarm_status(struct sc8551 *sc)
{
	int ret;
	u8 flag = 0;
	u8 stat = 0;

	mutex_lock(&sc->data_lock);

	ret = sc8551_read_byte(sc, SC8551_REG_08, &flag);
	if (!ret && (flag & SC8551_IBUS_UCP_FALL_FLAG_MASK))
		sc_dbg("UCP_FLAG =0x%02X\n",
			!!(flag & SC8551_IBUS_UCP_FALL_FLAG_MASK));

	ret = sc8551_read_byte(sc, SC8551_REG_2D, &flag);
	if (!ret && (flag & SC8551_VDROP_OVP_FLAG_MASK))
		sc_dbg("VDROP_OVP_FLAG =0x%02X\n",
			!!(flag & SC8551_VDROP_OVP_FLAG_MASK));

	/*read to clear alarm flag*/
	ret = sc8551_read_byte(sc, SC8551_REG_0E, &flag);
	if (!ret && flag)
		sc_dbg("INT_FLAG =0x%02X\n", flag);

	ret = sc8551_read_byte(sc, SC8551_REG_0D, &stat);
	if (!ret && stat != sc->prev_alarm) {
		sc_dbg("INT_STAT = 0X%02x\n", stat);
		sc->prev_alarm = stat;
		sc->bat_ovp_alarm = !!(stat & BAT_OVP_ALARM);
		sc->bat_ocp_alarm = !!(stat & BAT_OCP_ALARM);
		sc->bus_ovp_alarm = !!(stat & BUS_OVP_ALARM);
		sc->bus_ocp_alarm = !!(stat & BUS_OCP_ALARM);
		sc->batt_present  = !!(stat & VBAT_INSERT);
		sc->vbus_present  = !!(stat & VBUS_INSERT);
		sc->bat_ucp_alarm = !!(stat & BAT_UCP_ALARM);
	}


	ret = sc8551_read_byte(sc, SC8551_REG_08, &stat);
	if (!ret && (stat & 0x50))
		sc_err("Reg[05]BUS_UCPOVP = 0x%02X\n", stat);

	ret = sc8551_read_byte(sc, SC8551_REG_0A, &stat);
	if (!ret && (stat & 0x02))
		sc_err("Reg[0A]CONV_OCP = 0x%02X\n", stat);

	mutex_unlock(&sc->data_lock);
}

__maybe_unused
static void sc8551_check_fault_status(struct sc8551 *sc)
{
	int ret;
	u8 flag = 0;
	u8 stat = 0;
	bool changed = false;

	mutex_lock(&sc->data_lock);

	ret = sc8551_read_byte(sc, SC8551_REG_10, &stat);
	if (!ret && stat)
		sc_err("FAULT_STAT = 0x%02X\n", stat);

	ret = sc8551_read_byte(sc, SC8551_REG_11, &flag);
	if (!ret && flag)
		sc_err("FAULT_FLAG = 0x%02X\n", flag);

	if (!ret && flag != sc->prev_fault) {
		changed = true;
		sc->prev_fault = flag;
		sc->bat_ovp_fault = !!(flag & BAT_OVP_FAULT);
		sc->bat_ocp_fault = !!(flag & BAT_OCP_FAULT);
		sc->bus_ovp_fault = !!(flag & BUS_OVP_FAULT);
		sc->bus_ocp_fault = !!(flag & BUS_OCP_FAULT);
		sc->bat_therm_fault = !!(flag & TS_BAT_FAULT);
		sc->bus_therm_fault = !!(flag & TS_BUS_FAULT);

		sc->bat_therm_alarm = !!(flag & TBUS_TBAT_ALARM);
		sc->bus_therm_alarm = !!(flag & TBUS_TBAT_ALARM);
	}

	mutex_unlock(&sc->data_lock);
}

static int sc_sc8551_set_enable(struct chargerpump_dev *charger_pump, bool enable)
{
	struct sc8551 *sc = chargerpump_get_private(charger_pump);
	int ret;

	ret = sc8551_enable_charge(sc,enable);

	return ret;
}

static int sc_sc8551_get_is_enable(struct chargerpump_dev *charger_pump, bool *enable)
{
	struct sc8551 *sc = chargerpump_get_private(charger_pump);
	int ret;

	ret = sc8551_check_charge_enabled(sc, enable);

	return ret;
}

static int sc_sc8551_get_status(struct chargerpump_dev *charger_pump, uint32_t *status)
{
	// struct sc8551 *sc = chargerpump_get_private(charger_pump);
	int ret = 0;

	// ret = sc8551_get_status(sc, status);

	return ret;
}

static int sc_sc8551_get_adc_value(struct chargerpump_dev *charger_pump, enum sc_adc_channel ch, int *value)
{
	struct sc8551 *sc = chargerpump_get_private(charger_pump);
	int ret = 0;

	ret = sc8551_get_adc_data(sc, sc_to_sc8551_adc(ch), value);

	return ret;
}

static int sc_sc8551_set_enable_adc(struct chargerpump_dev *charger_pump, bool en)
{
	struct sc8551 *sc = chargerpump_get_private(charger_pump);
	int ret = 0;

	ret = sc8551_enable_adc(sc, en);

	return ret;
}

static struct chargerpump_ops sc_sc8551_chargerpump_ops = {
	.set_enable = sc_sc8551_set_enable,
	.get_status = sc_sc8551_get_status,
	.get_is_enable = sc_sc8551_get_is_enable,
	.get_adc_value = sc_sc8551_get_adc_value,
	.set_enable_adc = sc_sc8551_set_enable_adc,
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	// .set_cp_workmode = sc_sc8551_set_cp_workmode,
	// .get_cp_workmode = sc_sc8551_get_cp_workmode,
#endif
};

/*
 * interrupt does nothing, just info event chagne, other module could get info
 * through power supply interface
 */
static irqreturn_t sc8551_charger_interrupt(int irq, void *dev_id)
{
	struct sc8551 *sc = dev_id;

	sc_dbg("INT OCCURED\n");
#if 1
	mutex_lock(&sc->irq_complete);
	sc->irq_waiting = true;
	if (!sc->resume_completed) {
		dev_dbg(sc->dev, "IRQ triggered before device-resume\n");
		if (!sc->irq_disabled) {
			disable_irq_nosync(irq);
			sc->irq_disabled = true;
		}
		mutex_unlock(&sc->irq_complete);
		return IRQ_HANDLED;
	}
	sc->irq_waiting = false;
#if 0
	/* TODO */
	sc8551_dump_important_regs(sc);
	sc8551_check_alarm_status(sc);
	sc8551_check_fault_status(sc);

#endif
	mutex_unlock(&sc->irq_complete);
#endif

	return IRQ_HANDLED;
}

static void determine_initial_status(struct sc8551 *sc)
{
	if (sc->client->irq)
		sc8551_charger_interrupt(sc->client->irq, sc);
}

static struct of_device_id sc8551_charger_match_table[] = {
	{
		.compatible = "sc,sc8551-standalone",
		.data = &sc8551_mode_data[SC8551_STDALONE],
	},
	{
		.compatible = "sc,sc8551-master",
		.data = &sc8551_mode_data[SC8551_MASTER],
	},
	{
		.compatible = "sc,sc8551-slave",
		.data = &sc8551_mode_data[SC8551_SLAVE],
	},
	{},
};

static int sc8551_charger_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct sc8551 *sc;
	const struct of_device_id *match;
	struct device_node *node = client->dev.of_node;
	int ret;
	struct gpio_desc *irq_gpio;

	sc8551_err("%s: start\n", __func__);

	sc = devm_kzalloc(&client->dev, sizeof(struct sc8551), GFP_KERNEL);
	if (!sc) {
		return -ENOMEM;
		goto err_kzalloc;
	}

	sc->dev = &client->dev;
	sc->client = client;

	mutex_init(&sc->i2c_rw_lock);
	mutex_init(&sc->data_lock);
	mutex_init(&sc->charging_disable_lock);
	mutex_init(&sc->irq_complete);

	sc->resume_completed = true;
	sc->irq_waiting = false;
	sc->is_sc8551 = true;

	ret = sc8551_detect_device(sc);
	if (ret) {
		sc_err("No sc8551 device found!\n");
		sc8551_err("No sc8551 device found!\n");
		ret = -ENODEV;
		goto err_free;
	}

	i2c_set_clientdata(client, sc);
	sc8551_create_device_node(&(client->dev));

	match = of_match_node(sc8551_charger_match_table, node);
	if (match == NULL) {
		sc_err("device tree match not found!\n");
		sc8551_err("device tree match not found!\n");
		ret = -ENODEV;
		goto err_free;
	}

	sc->mode =  *(int *)match->data;
	ret = sc8551_parse_dt(sc, &client->dev);
	if (ret)
	{
		ret = -EIO;
		goto err_free;
	}

	ret = sc8551_init_device(sc);
	if (ret) {
		sc_err("Failed to init device\n");
		sc8551_err("Failed to init device\n");
		goto err_free;
	}

	irq_gpio = devm_gpiod_get(&client->dev, "irq", GPIOD_IN);
	if (IS_ERR(irq_gpio)) {
		ret = PTR_ERR(irq_gpio);
		dev_err(&client->dev, "Failed to get irq gpio");
		goto err_free;
	} else {
		client->irq = gpiod_to_irq(irq_gpio);
	}

	if (client->irq > 0) {
		ret = devm_request_threaded_irq(&client->dev, client->irq,
				NULL, sc8551_charger_interrupt,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"sc8551 charger irq", sc);
		if (ret < 0) {
			sc_err("request irq for irq=%d failed, ret =%d\n",
							client->irq, ret);
			sc8551_err("request irq for irq=%d failed, ret =%d\n",
							client->irq, ret);
			goto err_register_irq;
		}
		enable_irq_wake(client->irq);
	} else {
		sc_err("get irq for failed, ret =%d\n", client->irq);
		sc8551_err("get irq for failed, ret =%d\n", client->irq);
	}

	sc->master_cp_chg = chargerpump_register("sc8551-standalone",
							sc->dev, &sc_sc8551_chargerpump_ops, sc);
	if (IS_ERR_OR_NULL(sc->master_cp_chg)) {
		ret = PTR_ERR(sc->master_cp_chg);
		dev_err(sc->dev,"Fail to register master_cp_chg!\n");
		goto err_register_sc_charger;
	}

	device_init_wakeup(sc->dev, 1);

	determine_initial_status(sc);

	sc_err("sc8551 probe successfully, Part Num:%d\n!", sc->part_no);

	return 0;

err_register_sc_charger:
err_register_irq:
	mutex_destroy(&sc->irq_complete);
	mutex_destroy(&sc->charging_disable_lock);
	mutex_destroy(&sc->data_lock);
	mutex_destroy(&sc->i2c_rw_lock);
err_free:
	devm_kfree(&client->dev,sc);
err_kzalloc:
	dev_err(&client->dev,"sc8551 probe fail\n");
	return ret;
}

static inline bool is_device_suspended(struct sc8551 *sc)
{
	return !sc->resume_completed;
}

static int sc8551_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sc8551 *sc = i2c_get_clientdata(client);

	mutex_lock(&sc->irq_complete);
	sc->resume_completed = false;
	mutex_unlock(&sc->irq_complete);
	sc_err("Suspend successfully!");

	return 0;
}

static int sc8551_suspend_noirq(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sc8551 *sc = i2c_get_clientdata(client);

	if (sc->irq_waiting) {
		pr_err_ratelimited("Aborting suspend, an interrupt was detected while suspending\n");
		return -EBUSY;
	}
	return 0;
}

static int sc8551_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sc8551 *sc = i2c_get_clientdata(client);


	mutex_lock(&sc->irq_complete);
	sc->resume_completed = true;
	if (sc->irq_waiting) {
		sc->irq_disabled = false;
		enable_irq(client->irq);
		mutex_unlock(&sc->irq_complete);
		sc8551_charger_interrupt(client->irq, sc);
	} else {
		mutex_unlock(&sc->irq_complete);
	}
	sc_err("Resume successfully!");

	return 0;
}
static int sc8551_charger_remove(struct i2c_client *client)
{
	struct sc8551 *sc = i2c_get_clientdata(client);

	sc8551_enable_adc(sc, false);

	mutex_destroy(&sc->charging_disable_lock);
	mutex_destroy(&sc->data_lock);
	mutex_destroy(&sc->i2c_rw_lock);
	mutex_destroy(&sc->irq_complete);

	return 0;
}

static void sc8551_charger_shutdown(struct i2c_client *client)
{
	struct sc8551 *sc = i2c_get_clientdata(client);

	sc8551_enable_adc(sc, false);
}

static const struct dev_pm_ops sc8551_pm_ops = {
	.resume		= sc8551_resume,
	.suspend_noirq = sc8551_suspend_noirq,
	.suspend	= sc8551_suspend,
};

static const struct i2c_device_id sc8551_charger_id[] = {
	{"sc8551-standalone", SC8551_ROLE_STDALONE},
	{},
};

static struct i2c_driver sc8551_charger_driver = {
	.driver		= {
		.name	= "sc8551-charger",
		.owner	= THIS_MODULE,
		.of_match_table = sc8551_charger_match_table,
		.pm	= &sc8551_pm_ops,
	},
	.id_table	= sc8551_charger_id,

	.probe		= sc8551_charger_probe,
	.remove		= sc8551_charger_remove,
	.shutdown	= sc8551_charger_shutdown,
};

module_i2c_driver(sc8551_charger_driver);

MODULE_DESCRIPTION("SC SC8551 Charge Pump Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Aiden-yu@southchip.com");
