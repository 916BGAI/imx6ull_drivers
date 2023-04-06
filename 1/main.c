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

#define CCM_CCGR1_BASE (0X020C406C)
#define SW_MUX_SNVS_TAMPER3_BASE (0X02290014)
#define SW_PAD_SNVS_TAMPER3_BASE (0X02290058)
#define GPIO5_GDIR_BASE (0X020AC004)
#define GPIO5_DR_BASE (0X020AC000)

static void __iomem *CCM_CCGR1;
static void __iomem *SW_MUX_SNVS_TAMPER3;
static void __iomem *SW_PAD_SNVS_TAMPER3;
static void __iomem *GPIO5_GDIR;
static void __iomem *GPIO5_DR;

struct led_dev {
	dev_t dev_id;
	int major;
	int minor;
	struct cdev *cdev;
	struct class *class;
	struct device *device;
};

static struct led_dev led;

static int led_board_init(void)
{
	uint32_t val;

	CCM_CCGR1 = ioremap(CCM_CCGR1_BASE, 4);
	SW_MUX_SNVS_TAMPER3 = ioremap(SW_MUX_SNVS_TAMPER3_BASE, 4);
	SW_PAD_SNVS_TAMPER3 = ioremap(SW_PAD_SNVS_TAMPER3_BASE, 4);
	GPIO5_GDIR = ioremap(GPIO5_GDIR_BASE, 4);
	GPIO5_DR = ioremap(GPIO5_DR_BASE, 4);

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

static void led_board_deinit(void)
{
    iounmap(CCM_CCGR1);
    iounmap(SW_MUX_SNVS_TAMPER3);
    iounmap(SW_PAD_SNVS_TAMPER3);
    iounmap(GPIO5_GDIR);
    iounmap(GPIO5_DR);
}

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

static __init int led_init(void)
{
	int ret;

    ret = led_board_init();
	if (ret) {
		printk(KERN_WARNING "led_board_init failed!\r\n");
		return -1;
	}

	if (led.major) {
		led.dev_id = MKDEV(led.major, 0);
		ret = register_chrdev_region(led.dev_id, 1, "LED");
	} else {
		ret = alloc_chrdev_region(&led.dev_id, 0, 1, "LED");
		led.major = MAJOR(led.dev_id);
		led.minor = MINOR(led.dev_id);
	}
	if (ret) {
		pr_err("chrdev region failed!\r\n");
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

static __exit void led_exit(void)
{
    led_board_deinit();
	cdev_del(led.cdev);
	unregister_chrdev_region(led.dev_id, 1);
	device_destroy(led.class, led.dev_id);
	class_destroy(led.class);
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhang <zhang916772719@gmail.com>");
MODULE_DESCRIPTION("LED");
MODULE_VERSION("1.0");
