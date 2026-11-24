#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include "my_ioctl.h"
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/of.h>
#include <linux/version.h>

#define DEVICE_NAME "my_module"
#define CLASS_NAME  "my_module_class"
#define MINOR_START 0
#define MINOR_COUNT 1
#define BUF_SIZE 4096

struct my_device_data {
    struct class *my_class;
    struct device *my_device;
    struct cdev cdev;
    int total_opens;
    int total_closes;
    char buffer[BUF_SIZE];
    size_t head;
    size_t tail;
};

static dev_t my_dev;
static struct my_device_data my_dev_data;

static int my_open(struct inode *inode, struct file *filp){
    struct my_device_data *dev_data;

    dev_data = container_of(inode->i_cdev, struct my_device_data, cdev);
    filp->private_data = dev_data;
    dev_data->total_opens++;

    pr_info("%s: Device opened %d times\n", DEVICE_NAME, dev_data->total_opens);
    return 0;
}

static int my_release(struct inode *inode, struct file *filp){
    struct my_device_data *dev_data;

    dev_data = (struct my_device_data *)filp->private_data;
    dev_data->total_closes++;

    pr_info("%s: Device closed %d times\n", DEVICE_NAME, dev_data->total_closes);
    return 0;
}

static size_t circ_buf_available(struct my_device_data *dev_data){
    if(dev_data->head >= dev_data->tail){
        return dev_data->head - dev_data->tail;
    }
    return BUF_SIZE - dev_data->tail + dev_data->head;
}

static size_t circ_buf_free_space(struct my_device_data *dev_data){
    return BUF_SIZE - 1 - circ_buf_available(dev_data);
}

static ssize_t my_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos){
    struct my_device_data *dev_data = (struct my_device_data *)filp->private_data;
    size_t available;
    size_t to_read;
    size_t first_chunk;
    size_t second_chunk;

    available = circ_buf_available(dev_data);
    if(available == 0){
        pr_info("%s: Read request, buffer is empty\n", DEVICE_NAME);
        return 0;
    }

    to_read = count < available ? count : available;

    first_chunk = BUF_SIZE - dev_data->tail;
    if(first_chunk > to_read){
        first_chunk = to_read;
    }

    if(copy_to_user(buf, &dev_data->buffer[dev_data->tail], first_chunk) != 0){
        return -EFAULT;
    }

    second_chunk = to_read - first_chunk;
    if(second_chunk > 0){
        if(copy_to_user(buf + first_chunk, &dev_data->buffer[0], second_chunk) != 0){
            return -EFAULT;
        }
    }

    dev_data->tail = (dev_data->tail + to_read) % BUF_SIZE;
    *ppos += to_read;

    pr_info("%s: Read %zu bytes from device\n", DEVICE_NAME, to_read);
    return to_read;
}

static ssize_t my_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos){
    struct my_device_data *dev_data = filp->private_data;
    size_t free_space;
    size_t to_write;
    size_t first_chunk;
    size_t second_chunk;

    if(count == 0){
        return 0;
    }

    free_space = circ_buf_free_space(dev_data);
    if(free_space == 0){
        pr_info("%s: Write request, buffer is full\n", DEVICE_NAME);
        return -ENOSPC;
    }

    to_write = count < free_space ? count : free_space;

    first_chunk = BUF_SIZE - dev_data->head;
    if(first_chunk > to_write){
        first_chunk = to_write;
    }

    if(copy_from_user(&dev_data->buffer[dev_data->head], buf, first_chunk) != 0){
        return -EFAULT;
    }

    second_chunk = to_write - first_chunk;
    if(second_chunk > 0){
        if(copy_from_user(&dev_data->buffer[0], buf + first_chunk, second_chunk) != 0){
            return -EFAULT;
        }
    }

    dev_data->head = (dev_data->head + to_write) % BUF_SIZE;
    *ppos += to_write;

    pr_info("%s: Wrote %zu bytes to device\n", DEVICE_NAME, to_write);
    return to_write;
}

static long my_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){
    struct my_device_data *dev_data = filp->private_data;
    struct my_driver_stats stats;

    if (_IOC_TYPE(cmd) != MY_IOC_MAGIC){
        return -ENOTTY;
    }

    switch (cmd){
    case MY_IOC_GET_STATS:
        stats.total_opens = dev_data->total_opens;
        stats.total_closes = dev_data->total_closes;

        if (copy_to_user((struct my_driver_stats __user *)arg, &stats, sizeof(stats)) != 0){
            return -EFAULT;
        }
        pr_info("%s: Retrieved stats via ioctl (opens=%d, closes=%d)\n",
                DEVICE_NAME, stats.total_opens, stats.total_closes);
        break;

    case MY_IOC_CLEAR_STATS:
        dev_data->total_opens = 0;
        dev_data->total_closes = 0;
        pr_info("%s: Cleared stats via ioctl\n", DEVICE_NAME);
        break;

    default:
        return -ENOTTY;
    }

    return 0;
}

static const struct file_operations my_fops = {
    .owner = THIS_MODULE,
    .open = my_open,
    .release = my_release,
    .read = my_read,
    .write = my_write,
    .unlocked_ioctl = my_ioctl,
};

static void print_dt_properties(struct platform_device *pdev, const char *func_name){
    struct device_node *np = pdev->dev.of_node;
    u32 addr_cells = 0;
    u32 size_cells = 0;
    int parent_addr_cells;
    int parent_size_cells;
    bool bool_prop;

    if (!np) {
        pr_warn("%s (%s): No device tree node associated with device\n", DEVICE_NAME, func_name);
        return;
    }

    if (of_property_read_u32(np, "#address-cells", &addr_cells) == 0) {
        pr_info("%s (%s): node #address-cells = %u\n", DEVICE_NAME, func_name, addr_cells);
    } else {
        pr_info("%s (%s): #address-cells property not found on node\n", DEVICE_NAME, func_name);
    }

    if (of_property_read_u32(np, "#size-cells", &size_cells) == 0) {
        pr_info("%s (%s): node #size-cells = %u\n", DEVICE_NAME, func_name, size_cells);
    } else {
        pr_info("%s (%s): #size-cells property not found on node\n", DEVICE_NAME, func_name);
    }

    parent_addr_cells = of_n_addr_cells(np);
    parent_size_cells = of_n_size_cells(np);
    pr_info("%s (%s): of_n_addr_cells = %d, of_n_size_cells = %d\n", DEVICE_NAME, func_name, parent_addr_cells, parent_size_cells);

    bool_prop = (of_get_property(np, "my-bool-prop", NULL) != NULL);
    pr_info("%s (%s): my-bool-prop = %s\n", DEVICE_NAME, func_name, bool_prop ? "true" : "false");
}

static int my_probe(struct platform_device *pdev){
    int ret;

    pr_info("%s: probe function called\n", DEVICE_NAME);
    print_dt_properties(pdev, "probe");

    my_dev_data.head = 0;
    my_dev_data.tail = 0;
    my_dev_data.total_opens = 0;
    my_dev_data.total_closes = 0;

    ret = alloc_chrdev_region(&my_dev, MINOR_START, MINOR_COUNT, DEVICE_NAME);
    if (ret < 0) {
        pr_err("%s: Failed to allocate device number (err=%d)\n", DEVICE_NAME, ret);
        goto alloc_chrdev_region_failed;
    }

    cdev_init(&my_dev_data.cdev, &my_fops);
    ret = cdev_add(&my_dev_data.cdev, my_dev, MINOR_COUNT);
    if (ret < 0) {
        pr_err("%s: Failed to add cdev (err=%d)\n", DEVICE_NAME, ret);
        goto cdev_add_failed;
    }

    my_dev_data.my_class = class_create(CLASS_NAME);
    if (IS_ERR(my_dev_data.my_class)) {
        ret = PTR_ERR(my_dev_data.my_class);
        pr_err("%s: Failed to create class (err=%d)\n", DEVICE_NAME, ret);
        goto class_create_failed;
    }

    my_dev_data.my_device = device_create(my_dev_data.my_class, &pdev->dev, my_dev, NULL, DEVICE_NAME);
    if (IS_ERR(my_dev_data.my_device)) {
        ret = PTR_ERR(my_dev_data.my_device);
        pr_err("%s: Failed to create device (err=%d)\n", DEVICE_NAME, ret);
        goto device_create_failed;
    }

    platform_set_drvdata(pdev, &my_dev_data);

    pr_info("%s: Device created at /dev/%s (Major = %d, Minor = %d)\n",
            DEVICE_NAME, DEVICE_NAME, MAJOR(my_dev), MINOR(my_dev));

    return 0;

device_create_failed:
    class_destroy(my_dev_data.my_class);
class_create_failed:
    cdev_del(&my_dev_data.cdev);
cdev_add_failed:
    unregister_chrdev_region(my_dev, MINOR_COUNT);
alloc_chrdev_region_failed:
    return ret;
}

static void my_remove(struct platform_device *pdev){
    struct my_device_data *dev_data = platform_get_drvdata(pdev);

    pr_info("%s: remove function called\n", DEVICE_NAME);
    print_dt_properties(pdev, "remove");

    device_destroy(dev_data->my_class, my_dev);
    class_destroy(dev_data->my_class);
    cdev_del(&dev_data->cdev);
    unregister_chrdev_region(my_dev, MINOR_COUNT);

    pr_info("%s: Character device unregistered and /dev/%s removed\n",
            DEVICE_NAME, DEVICE_NAME);
}

static const struct of_device_id my_dt_ids[] = {
    { .compatible = "logtel,my-module" },
    { }
};
MODULE_DEVICE_TABLE(of, my_dt_ids);

static struct platform_driver my_platform_driver = {
    .probe  = my_probe,
    .remove = my_remove,
    .driver = {
        .name           = DEVICE_NAME,
        .of_match_table = of_match_ptr(my_dt_ids),
    },
};

module_platform_driver(my_platform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Reuven");
MODULE_DESCRIPTION("Platform and Character device driver reading Device Tree properties");