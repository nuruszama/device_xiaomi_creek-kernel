// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 XiaoMi Inc.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif

#define AUDIO_PINCTRL_DRV_VERSION  "V1.0.0"

struct audio_pinctrl_dev
{
	struct device *dev;
	struct pinctrl *pinctrl;
	struct pinctrl_state *aud_default;
	struct pinctrl_state *aud_active;
};
typedef struct audio_pinctrl_dev audio_pinctrl_dev_t;

static DEFINE_MUTEX(audio_pinctrl_mutex);
static audio_pinctrl_dev_t *g_audio_pinctrl_dev = NULL;

static int audio_pinctrl_init(struct audio_pinctrl_dev *apd_priv)
{
	struct pinctrl *ctrl;
	struct pinctrl_state *state;
	char *state_name;
	int ret = 0;

	if (!apd_priv)
		return -EINVAL;

	ctrl = devm_pinctrl_get(apd_priv->dev);
	if (IS_ERR(ctrl)) {
		dev_err(apd_priv->dev, "not found pinctrl!\n");
		ret = PTR_ERR(apd_priv->pinctrl);
		apd_priv->pinctrl = NULL;
		return ret;
	}
	apd_priv->pinctrl = ctrl;

	state_name = "aud_default";
	state = pinctrl_lookup_state(ctrl, state_name);
	if (IS_ERR(state)) {
		ret = PTR_ERR(state);
		pr_err("lookup state: %s failed\n", state_name);
		apd_priv->aud_default = NULL;
	} else {
		pinctrl_select_state(apd_priv->pinctrl, state);
		apd_priv->aud_default = state;
	}

	state_name = "aud_active";
	state = pinctrl_lookup_state(ctrl, state_name);
	if (IS_ERR(state)) {
		ret = PTR_ERR(state);
		pr_err("lookup state: %s failed\n", state_name);
		apd_priv->aud_active = NULL;
	} else {
		apd_priv->aud_active = state;
	}

	return ret;
}

static int audio_pinctrl_switch(struct audio_pinctrl_dev *apd_priv, bool active)
{
	if (apd_priv == NULL) {
		pr_err("%s: apd_priv is null\n", __func__);
		return -EINVAL;
	}
	if (apd_priv->pinctrl == NULL) {
		dev_err(apd_priv->dev, "%s: pinctrl is null\n", __func__);
		return -EINVAL;
	}
	if (active) {
		pinctrl_select_state(apd_priv->pinctrl, apd_priv->aud_active);
	} else {
		pinctrl_select_state(apd_priv->pinctrl, apd_priv->aud_default);
	}
	pr_info("audio_pinctrl_switch done %d\n", active);
	return 0;
}

int audio_pinctrl_update(bool active)
{
	int ret = 0;

	if (g_audio_pinctrl_dev == NULL) {
		pr_err("%s: apd_priv is null\n", __func__);
		return -EINVAL;
	}
	pr_debug("audio_pinctrl_update %d\n", active);

	mutex_lock(&audio_pinctrl_mutex);
	ret = audio_pinctrl_switch(g_audio_pinctrl_dev, active);
	mutex_unlock(&audio_pinctrl_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(audio_pinctrl_update);

static int audio_pinctrl_dev_probe(struct platform_device *pdev)
{
	audio_pinctrl_dev_t *apd_priv;
	int ret = 0;

	pr_debug("audio_pinctrl version: %s\n", AUDIO_PINCTRL_DRV_VERSION);
	apd_priv = devm_kzalloc(&pdev->dev, sizeof(audio_pinctrl_dev_t), GFP_KERNEL);
	if (apd_priv == NULL) {
		pr_err("allocate memery failed\n");
		return -ENOMEM;
	}

	memset(apd_priv, 0, sizeof(audio_pinctrl_dev_t));
	apd_priv->dev = &pdev->dev;

	ret |= audio_pinctrl_init(apd_priv);
	if (ret) {
		pr_err("init fail\n");
		return ret;
	}

	platform_set_drvdata(pdev, apd_priv);
	g_audio_pinctrl_dev = apd_priv;

	return ret;
}

static int audio_pinctrl_dev_remove(struct platform_device *pdev)
{
	audio_pinctrl_dev_t *apd_priv = platform_get_drvdata(pdev);

	if (apd_priv) {
		devm_kfree(&pdev->dev, apd_priv);
	}

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id audio_pinctrl_of_match[] =
{
	{.compatible = "mi,audio-pinctrl"},
	{},
};
MODULE_DEVICE_TABLE(of, audio_pinctrl_of_match);
#endif

static struct platform_driver audio_pinctrl_dev_driver =
{
	.probe = audio_pinctrl_dev_probe,
	.remove = audio_pinctrl_dev_remove,
	.driver = {
		.name = "audio_pinctrl",
#ifdef CONFIG_OF
		.of_match_table = audio_pinctrl_of_match,
#endif
	}
};

#if !defined(module_platform_driver)
static int __init audio_pinctrl_dev_init(void)
{
	int ret;

	ret = platform_driver_register(&audio_pinctrl_dev_driver);
	if (ret)
		pr_err("register driver failed, ret: %d\n", ret);

	return ret;
}

static void __exit audio_pinctrl_dev_exit(void)
{
	platform_driver_unregister(&audio_pinctrl_dev_driver);
}

module_init(audio_pinctrl_dev_init);
module_exit(audio_pinctrl_dev_exit);
#else
module_platform_driver(audio_pinctrl_dev_driver);
#endif

MODULE_AUTHOR("AUDIO SW");
MODULE_DESCRIPTION("Platfrom Controller driver");
MODULE_VERSION(AUDIO_PINCTRL_DRV_VERSION);
MODULE_LICENSE("GPL");
