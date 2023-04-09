#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <asm-generic/gpio.h>
#include <linux/kdev_t.h>
#include <linux/kern_levels.h>
#include <linux/printk.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/ioport.h>

static void __iomem *CCM_CCGR1;
static void __iomem *SW_MUX_SNVS_TAMPER3;
static void __iomem *SW_PAD_SNVS_TAMPER3;
static void __iomem *GPIO5_GDIR;
static void __iomem *GPIO5_DR;

static int status = 0;
static struct kobject *led_kobj;

static void led_board_ctrl(int status)
{
	uint32_t val;

	if (status == 1) {
		// 打开LED
		val = readl(GPIO5_DR);
		val &= ~(1 << 3);
		writel(val, GPIO5_DR);
	} else {
		// 关闭LED
		val = readl(GPIO5_DR);
		val |= (1 << 3);
		writel(val, GPIO5_DR);
	}
}

static ssize_t led_status_show(struct kobject *kobj,
			       struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "The led status is = %d\n", status);
}

static ssize_t led_status_store(struct kobject *kobj,
				struct kobj_attribute *attr, const char *buf,
				size_t count)
{
	if (0 == memcmp(buf, "on", 2)) {
		led_board_ctrl(1);
		status = 1;
	} else if (0 == memcmp(buf, "off", 3)) {
		led_board_ctrl(0);
		status = 0;
	} else {
		printk(KERN_INFO "Not support cmd\n");
	}
	return count;
}

static struct kobj_attribute led_status_attr =
	__ATTR(status, 0660, led_status_show, led_status_store);

static int led_probe(struct platform_device *pdev)
{
	int ret;
	uint32_t val;
	struct resource *res;

	led_kobj = kobject_create_and_add("LED", NULL);
	if (!led_kobj)
		return -ENOMEM;
	ret = sysfs_create_file(led_kobj, &led_status_attr.attr);
	if (ret)
		return ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	CCM_CCGR1 = ioremap(res->start, (res->end - res->start) + 1);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	SW_MUX_SNVS_TAMPER3 = ioremap(res->start, (res->end - res->start) + 1);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	SW_PAD_SNVS_TAMPER3 = ioremap(res->start, (res->end - res->start) + 1);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	GPIO5_GDIR = ioremap(res->start, (res->end - res->start) + 1);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 4);
	GPIO5_DR = ioremap(res->start, (res->end - res->start) + 1);

	// 使能外设时钟
	val = readl(CCM_CCGR1);
	val &= ~(3 << 26);
	val |= (3 << 26);
	writel(val, CCM_CCGR1);

	// 设置IOMUXC引脚复用和引脚属性
	writel(5, SW_MUX_SNVS_TAMPER3);
	writel(0x10B0, SW_PAD_SNVS_TAMPER3);

	// 设置GPIO输出高电平，默认关闭LED
	val = readl(GPIO5_GDIR);
	val &= ~(1 << 3);
	val |= (1 << 3);
	writel(val, GPIO5_GDIR);
	val = readl(GPIO5_DR);
	val |= (1 << 3);
	writel(val, GPIO5_DR);

	return 0;
}
static int led_remove(struct platform_device *pdev)
{
	iounmap(CCM_CCGR1);
	iounmap(SW_MUX_SNVS_TAMPER3);
	iounmap(SW_PAD_SNVS_TAMPER3);
	iounmap(GPIO5_GDIR);
	iounmap(GPIO5_DR);

	sysfs_remove_file(led_kobj, &led_status_attr.attr);
	kobject_put(led_kobj);

	return 0;
}

static struct of_device_id led_match_table[] = {
	{
		.compatible = "imx6ull,led",
	},
};
static struct platform_device_id led_device_ids[] = {
	{
		.name = "led",
	},
};

static struct platform_driver led_driver= 
{ 
	.probe = led_probe, 
	.remove = led_remove, 
	.driver={ 
	.name = "led", 
	.of_match_table = led_match_table, 
	}, 
	.id_table = led_device_ids,
};

static __init int led_init(void)
{
	platform_driver_register(&led_driver);
	return 0;
}

static __exit void led_exit(void)
{
	platform_driver_unregister(&led_driver);
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhang <zhang916772719@gmail.com>");
MODULE_DESCRIPTION("LED");
MODULE_VERSION("1.0");