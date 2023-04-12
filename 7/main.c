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
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/export.h>
#include <linux/i2c.h>

static struct kobject *at24c02_kobj;
static char recv[16] = {0};
struct i2c_client *at24c02_dev;

static ssize_t at24c02_show(struct kobject *kobj, struct kobj_attribute *attr,
			    char *buf)
{
	int ret;
	ret = i2c_smbus_write_byte(at24c02_dev, 0x02);
	if (!ret) {
		printk(KERN_ERR "write addr failed!\n");
	}
	recv[0] = i2c_smbus_read_byte(at24c02_dev);
	return sprintf(buf, "read data = 0x%x\n", recv[0]);
}

static ssize_t at24c02_store(struct kobject *kobj, struct kobj_attribute *attr,
			     const char *buf, size_t count)
{
	int ret;
	char data = 0x2f;
	ret = i2c_smbus_write_i2c_block_data(at24c02_dev, 0x02, 1, &data);
	if (ret < 0) {
		printk(KERN_ERR "write data failed!\n");
	}
	return count;
}

static struct kobj_attribute at24c02_attr =
	__ATTR(at24c02, 0660, at24c02_show, at24c02_store);

static int at24c02_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret;

	at24c02_kobj = kobject_create_and_add("AT24C02", NULL);
	if (!at24c02_kobj)
		return -ENOMEM;
	ret = sysfs_create_file(at24c02_kobj, &at24c02_attr.attr);
	if (ret)
		return ret;

	at24c02_dev = client;

	return 0;
}

static int at24c02_remove(struct i2c_client *client)
{
	
	sysfs_remove_file(at24c02_kobj, &at24c02_attr.attr);
	kobject_put(at24c02_kobj);

	return 0;
}

static const struct of_device_id at24c02_of_match[] = {
	{ .compatible = "imx6ull,at24c02" },
	{},
};

static const struct i2c_device_id at24c02_id[] = {
	{ "imx6ull,at24c02", 0 },
	{ }
};

MODULE_DEVICE_TABLE(of, at24c02_of_match);

static struct i2c_driver at24c02_driver = {
	.driver = {
	.name = "test,at24c0x",
	.owner = THIS_MODULE, 
	.of_match_table = at24c02_of_match,
	},
	.probe = at24c02_probe,
	.remove = at24c02_remove,
	.id_table = at24c02_id,
};

module_i2c_driver(at24c02_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhang <zhang916772719@gmail.com>");
MODULE_DESCRIPTION("LED");
MODULE_VERSION("1.0");