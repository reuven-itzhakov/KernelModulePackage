#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>

#define DEVICE_NAME "my_module"
#define MINOR_START 0
#define MINOR_COUNT 1

static dev_t my_dev;

static struct cdev my_cdev;

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

    pr_info("%s: Registered character device! Major = %d, Minor = %d\n",
            DEVICE_NAME, MAJOR(my_dev), MINOR(my_dev));

    return 0;
}

static void __exit my_module_exit(void)
{
    cdev_del(&my_cdev);

    unregister_chrdev_region(my_dev, MINOR_COUNT);

    pr_info("%s: Character device unregistered\n", DEVICE_NAME);
}

module_init(my_module_init);
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Reuven");
MODULE_DESCRIPTION("Character device driver registration exercise");