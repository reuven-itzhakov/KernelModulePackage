#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>

#define DEVICE_NAME "my_module"
#define CLASS_NAME  "my_module_class"
#define MINOR_START 0
#define MINOR_COUNT 1

static dev_t my_dev;
static struct cdev my_cdev;
static struct class *my_class;
static struct device *my_device;

static const struct file_operations my_fops = {
    .owner = THIS_MODULE,
};

static int __init my_module_init(void)
{
    int ret;

    ret = alloc_chrdev_region(&my_dev, MINOR_START, MINOR_COUNT, DEVICE_NAME);
    if (ret < 0) {
        pr_err("%s: Failed to allocate device number (err=%d)\n", DEVICE_NAME, ret);
        return ret;
    }

    cdev_init(&my_cdev, &my_fops);
    ret = cdev_add(&my_cdev, my_dev, MINOR_COUNT);
    if (ret < 0) {
        pr_err("%s: Failed to add cdev (err=%d)\n", DEVICE_NAME, ret);
        unregister_chrdev_region(my_dev, MINOR_COUNT);
        return ret;
    }

    my_class = class_create(CLASS_NAME);
    if (IS_ERR(my_class)) {
        ret = PTR_ERR(my_class);
        pr_err("%s: Failed to create class (err=%d)\n", DEVICE_NAME, ret);
        cdev_del(&my_cdev);
        unregister_chrdev_region(my_dev, MINOR_COUNT);
        return ret;
    }

    my_device = device_create(my_class, NULL, my_dev, NULL, DEVICE_NAME);
    if (IS_ERR(my_device)) {
        ret = PTR_ERR(my_device);
        pr_err("%s: Failed to create device (err=%d)\n", DEVICE_NAME, ret);
        class_destroy(my_class);
        cdev_del(&my_cdev);
        unregister_chrdev_region(my_dev, MINOR_COUNT);
        return ret;
    }

    pr_info("%s: Device created at /dev/%s (Major = %d, Minor = %d)\n",
            DEVICE_NAME, DEVICE_NAME, MAJOR(my_dev), MINOR(my_dev));

    return 0;
}

static void __exit my_module_exit(void)
{
    device_destroy(my_class, my_dev);
    class_destroy(my_class);
    cdev_del(&my_cdev);
    unregister_chrdev_region(my_dev, MINOR_COUNT);

    pr_info("%s: Character device unregistered and /dev/%s removed\n",
            DEVICE_NAME, DEVICE_NAME);
}

module_init(my_module_init);
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Reuven");
MODULE_DESCRIPTION("Character device driver with automatic /dev file creation");