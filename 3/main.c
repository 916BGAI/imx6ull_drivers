#include "linux/ioport.h"
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

struct led_dev {
	dev_t dev_id;
	int major;
	int minor;
	struct cdev *cdev;
	struct class *class;
	struct device *device;
};

static struct led_dev led;

static void __iomem *CCM_CCGR1;
static void __iomem *SW_MUX_SNVS_TAMPER3;
static void __iomem *SW_PAD_SNVS_TAMPER3;
static void __iomem *GPIO5_GDIR;
static void __iomem *GPIO5_DR;

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

static int led_open(struct inode *inode, struct file *file)
{
	uint32_t val;

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
	val &= ~(1 << 3); /* 清除以前的设置 */
	val |= (1 << 3); /* 设置为输出 */
	writel(val, GPIO5_GDIR);
	val = readl(GPIO5_DR);
	val |= (1 << 3);
	writel(val, GPIO5_DR);
	
	return 0;
}

static int led_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static int led_read(struct file *filp, char __user *buff, size_t count,
		    loff_t *offp)
{
	return 0;
}

static int led_write(struct file *filp, const char __user *buff, size_t count,
		     loff_t *offp)
{
    int ret;
    unsigned char data_buff[1];
    unsigned char led_status;

	ret = copy_from_user(data_buff, buff, 1);
    if (ret < 0) {
        printk("led write failed!\n");
        return -EFAULT;
    }

    // 控制LED
    led_status = data_buff[0];
    if (led_status == 0) {
        led_board_ctrl(0);
    } else if (led_status == 1){
        led_board_ctrl(1);
    }

	return 0;
}

static const struct file_operations led_ops = {
	.owner = THIS_MODULE,
	.open = led_open,
    .release = led_release,
	.read = led_read,
	.write = led_write,
};

static int led_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;

	if (led.major) {
		led.dev_id = MKDEV(led.major, 0);
		ret = register_chrdev_region(led.dev_id, 1, "LED");
	} else {
		ret = alloc_chrdev_region(&led.dev_id, 0, 1, "LED");
		led.major = MAJOR(led.dev_id);
		led.minor = MINOR(led.dev_id);
	}
	if (ret) {
		pr_err(KERN_WARNING "chrdev region failed!\r\n");
		goto err_dev_id;
	}

	led.cdev = cdev_alloc();
	if (!led.cdev) {
		pr_err("cdev alloc failed!\r\n");
		goto err_cdev;
	}
	led.cdev->owner = THIS_MODULE;
	led.cdev->ops = &led_ops;
	ret = cdev_add(led.cdev, led.dev_id, 1);
	if (ret) {
		pr_err("cdev add failed!\r\n");
		goto err_cdev;
	}

	led.class = class_create(THIS_MODULE, "LED");
	if (!led.class) {
		pr_err("led class failed!\r\n");
		goto err_class;
	}

	led.device = device_create(led.class, NULL, led.dev_id, NULL, "LED");
	if (IS_ERR(led.device)) {
		pr_err("device create failed!\r\n");
		goto err_device;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	CCM_CCGR1 = ioremap(res->start,(res->end - res->start)+1);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	SW_MUX_SNVS_TAMPER3 = ioremap(res->start,(res->end - res->start)+1);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	SW_PAD_SNVS_TAMPER3 = ioremap(res->start,(res->end - res->start)+1);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	GPIO5_GDIR = ioremap(res->start,(res->end - res->start)+1);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 4);
	GPIO5_DR = ioremap(res->start,(res->end - res->start)+1);

	return 0;

err_device:
	class_destroy(led.class);
err_class:
	cdev_del(led.cdev);
err_cdev:
	unregister_chrdev_region(led.dev_id, 1);
err_dev_id:
	return -1;
}
static int led_remove(struct platform_device *pdev)
{
	iounmap(CCM_CCGR1);
	iounmap(SW_MUX_SNVS_TAMPER3);
	iounmap(SW_PAD_SNVS_TAMPER3);
	iounmap(GPIO5_GDIR);
	iounmap(GPIO5_DR);

	cdev_del(led.cdev);
	unregister_chrdev_region(led.dev_id, 1);
	device_destroy(led.class, led.dev_id);
	class_destroy(led.class);

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