/*
 * arch/arm/mach-msm/lge/lge_handle_panic.c
 *
 * Copyright (C) 2012 LGE, Inc
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/io.h>
#include <asm/setup.h>
#include <linux/init.h>
#include <mach/subsystem_restart.h>
#include <mach/msm_iomap.h>
#include <mach/lge_handle_panic.h>
#include <mach/board_lge.h>
#include <mach/msm_memtypes.h>
#include <linux/memory_alloc.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_fdt.h>
#endif

#define PANIC_HANDLER_NAME        "panic-handler"

#define RESTART_REASON_ADDR       0x65C
#define DLOAD_MODE_ADDR           0x0
#define UEFI_RAM_DUMP_MAGIC_ADDR  0xC
#define RAM_CONSOLE_ADDR_ADDR     0x28
#define RAM_CONSOLE_SIZE_ADDR     0x2C
#define FB1_ADDR_ADDR             0x30

#define RESTART_REASON      (MSM_IMEM_BASE + RESTART_REASON_ADDR)
#define UEFI_RAM_DUMP_MAGIC \
		(MSM_IMEM_BASE + DLOAD_MODE_ADDR + UEFI_RAM_DUMP_MAGIC_ADDR)
#define RAM_CONSOLE_ADDR    (MSM_IMEM_BASE + RAM_CONSOLE_ADDR_ADDR)
#define RAM_CONSOLE_SIZE    (MSM_IMEM_BASE + RAM_CONSOLE_SIZE_ADDR)
#define FB1_ADDR            (MSM_IMEM_BASE + FB1_ADDR_ADDR)

static int dummy_arg;

static int subsys_crash_magic = 0x0;

int lge_set_magic_subsystem(const char *name, int type)
{
	const char *subsys_name[] = { "adsp", "mba", "modem", "wcnss" };
	int i = 0;

	if (!name)
		return -1;

	for (i = 0; i < ARRAY_SIZE(subsys_name); i++) {
		if (!strncmp(subsys_name[i], name, 40)) {
			subsys_crash_magic = LGE_RB_MAGIC | ((i+1) << 12)
				| type;
			break;
		}
	}

	return 0;
}
EXPORT_SYMBOL(lge_set_magic_subsystem);

void lge_skip_dload_by_sbl(int on)
{
	/* skip entering dload mode at sbl1 */
	__raw_writel(on ? 1 : 0, UEFI_RAM_DUMP_MAGIC);
}
EXPORT_SYMBOL(lge_skip_dload_by_sbl);

void lge_set_ram_console_addr(unsigned int addr, unsigned int size)
{
	__raw_writel(addr, RAM_CONSOLE_ADDR);
	__raw_writel(size, RAM_CONSOLE_SIZE);
}
EXPORT_SYMBOL(lge_set_ram_console_addr);

void lge_set_fb1_addr(unsigned int addr)
{
	__raw_writel(addr, FB1_ADDR);
}
EXPORT_SYMBOL(lge_set_fb1_addr);

void lge_set_restart_reason(unsigned int reason)
{
	if ((lge_get_laf_mode() == LGE_LAF_MODE_LAF)
			&& (reason != LAF_DLOAD_MODE))
		__raw_writel(LGE_RB_MAGIC | LGE_LAF_CRASH, RESTART_REASON);
	else
	__raw_writel(reason, RESTART_REASON);
}
EXPORT_SYMBOL(lge_set_restart_reason);

void lge_set_panic_reason(void)
{
	if (subsys_crash_magic == 0)
		lge_set_restart_reason(LGE_RB_MAGIC | LGE_ERR_KERN);
	else
		lge_set_restart_reason(subsys_crash_magic);
}
EXPORT_SYMBOL(lge_set_panic_reason);

static int gen_bug(const char *val, struct kernel_param *kp)
{
	BUG();
	return 0;
}
module_param_call(gen_bug, gen_bug, param_get_bool, &dummy_arg,
		S_IWUSR | S_IRUGO);

static int gen_panic(const char *val, struct kernel_param *kp)
{
	panic("generate test-panic");
	return 0;
}
module_param_call(gen_panic, gen_panic, param_get_bool, &dummy_arg,\
		S_IWUSR | S_IRUGO);

static int gen_adsp_panic(const char *val, struct kernel_param *kp)
{
	subsystem_restart("adsp");
	return 0;
}
module_param_call(gen_adsp_panic, gen_adsp_panic, param_get_bool, &dummy_arg,
		S_IWUSR | S_IRUGO);

static int gen_mba_panic(const char *val, struct kernel_param *kp)
{
	subsystem_restart("mba");
	return 0;
}
module_param_call(gen_mba_panic, gen_mba_panic, param_get_bool, &dummy_arg,
		S_IWUSR | S_IRUGO);

static int gen_modem_panic_type = 0;

int lge_get_modem_panic(void)
{
	return gen_modem_panic_type;
}

EXPORT_SYMBOL(lge_get_modem_panic);

static int gen_modem_panic(const char *val, struct kernel_param *kp)
{
	int ret = param_set_int(val, kp);
	if (ret) {
		pr_err("error setting value %d\n", ret);
		return ret;
	}
	pr_err("gen_modem_panic param to %d\n", gen_modem_panic_type);
	switch (gen_modem_panic_type) {
		case 2:
			subsys_modem_restart();
			break;
		default:
			subsystem_restart("modem");
			break;
	}
	return 0;
}
module_param_call(gen_modem_panic, gen_modem_panic, param_get_bool, &gen_modem_panic_type,
		S_IWUSR | S_IRUGO);

static int gen_wcnss_panic(const char *val, struct kernel_param *kp)
{
	subsystem_restart("wcnss");
	return 0;
}
module_param_call(gen_wcnss_panic, gen_wcnss_panic, param_get_bool, &dummy_arg,
		S_IWUSR | S_IRUGO);

#define WDT0_RST        0x04
#define WDT0_EN         0x08
#define WDT0_BARK_TIME  0x10
#define WDT0_BITE_TIME  0x14

extern void __iomem *wdt_timer_get_timer0_base(void);

static int gen_wdt_bark(const char *val, struct kernel_param *kp)
{
	static void __iomem *msm_tmr0_base;
	msm_tmr0_base = wdt_timer_get_timer0_base();

	__raw_writel(0, msm_tmr0_base + WDT0_EN);
	__raw_writel(1, msm_tmr0_base + WDT0_RST);
	__raw_writel(0x31F3, msm_tmr0_base + WDT0_BARK_TIME);
	__raw_writel(5 * 0x31F3, msm_tmr0_base + WDT0_BITE_TIME);
	__raw_writel(1, msm_tmr0_base + WDT0_EN);
	return 0;
}
module_param_call(gen_wdt_bark, gen_wdt_bark, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

static int gen_wdt_bite(const char *val, struct kernel_param *kp)
{
	static void __iomem *msm_tmr0_base;
	msm_tmr0_base = wdt_timer_get_timer0_base();
	__raw_writel(0, msm_tmr0_base + WDT0_EN);
	__raw_writel(1, msm_tmr0_base + WDT0_RST);
	__raw_writel(5 * 0x31F3, msm_tmr0_base + WDT0_BARK_TIME);
	__raw_writel(0x31F3, msm_tmr0_base + WDT0_BITE_TIME);
	__raw_writel(1, msm_tmr0_base + WDT0_EN);
	return 0;
}
module_param_call(gen_wdt_bite, gen_wdt_bite, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

static int gen_bus_hang(const char *val, struct kernel_param *kp)
{
	static void __iomem *reserved;

	reserved = ioremap(0xFF000000 - 0x10, 0x10);
	__raw_writel(1, reserved);

	return 0;
}
module_param_call(gen_bus_hang, gen_bus_hang, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

extern void msm_disable_wdog_debug(void);
static int gen_hw_reset(const char *val, struct kernel_param *kp)
{
	static void __iomem *reserved;

	msm_disable_wdog_debug();
	reserved = ioremap(0xFF000000 - 0x10, 0x10);
	__raw_writel(1, reserved);

	return 0;
}
module_param_call(gen_hw_reset, gen_hw_reset, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

static int __init lge_panic_handler_probe(struct platform_device *pdev)
{
#ifdef CONFIG_OF
	unsigned int phys = 0;
	size_t size = 0;

	if (of_property_read_u32(pdev->dev.of_node,
		"qcom,memory-reservation-size",
		&size) || !size) {
		printk("%s: no memory reservation for FB\n", __func__);
		return 0;
	}

	phys = (unsigned int)allocate_contiguous_ebi_nomap(size, SZ_1M);
	if (!phys) {
		printk("%s: unable to alloc fbmem size= %u\n", __func__, size);
		return 0;
	}

	printk("%s: reserved address : 0x%x\n", __func__, phys);
	lge_set_fb1_addr(phys);
#endif
	return 0;
}

static int __devexit lge_panic_handler_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id panic_handler_match_table[] = {
	{ .compatible = "lge,lge-handle-panic", },
	{ },
};
EXPORT_COMPAT("lge,lge-handle-panic");
#endif

static struct platform_driver panic_handler_driver __refdata = {
	.probe = lge_panic_handler_probe,
	.remove = __devexit_p(lge_panic_handler_remove),
	.driver = {
		.name = PANIC_HANDLER_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = panic_handler_match_table,
#endif
	},
};

static int __init lge_panic_handler_init(void)
{
	return platform_driver_register(&panic_handler_driver);
}

static void __exit lge_panic_handler_exit(void)
{
	platform_driver_unregister(&panic_handler_driver);
}

module_init(lge_panic_handler_init);
module_exit(lge_panic_handler_exit);

MODULE_DESCRIPTION("LGE panic handler driver");
MODULE_AUTHOR("SungEun Kim <cleaneye.kim@lge.com>");
MODULE_LICENSE("GPL");
