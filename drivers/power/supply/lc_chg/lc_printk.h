#ifndef __LC_PRINTK__
#define __LC_PRINTK__

#define TAG                     "[LC_CHG][CM]"
#define lc_err(fmt, ...)        pr_err(TAG ":%s:" fmt, __func__, ##__VA_ARGS__)
#define lc_info(fmt, ...)       pr_info(TAG ":%s:" fmt, __func__, ##__VA_ARGS__)
#define lc_debug(fmt, ...)      pr_debug(TAG ":%s:" fmt, __func__, ##__VA_ARGS__)

#endif
