#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#define DEVICE_NAME "led-control"
#define CLASS_NAME "led"

static int major_number;
static struct class* led_class = NULL;
static struct device* led_device = NULL;

static int led_ctrl_dev_open(struct inode *, struct file *);
static int led_ctrl_dev_release(struct inode *, struct file *);
static ssize_t led_ctrl_dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t led_ctrl_dev_write(struct file *, const char *, size_t, loff_t *);

/* File operations structure */
static struct file_operations f_ops = {
    .open = led_ctrl_dev_open,
    .read = led_ctrl_dev_read,
    .write = led_ctrl_dev_write,
    .release = led_ctrl_dev_release,
};

static int __init led_ctrl_init(void) {
    printk(KERN_WARNING "%s: Initializing the LED Control Device\n", __func__);

    major_number = register_chrdev(0, DEVICE_NAME, &f_ops);
    if (major_number < 0) {
        printk(KERN_ALERT "%s: failed to register a major number\n", __func__);
        return major_number;
    }
    printk(KERN_WARNING "%s: registered correctly with major number %d\n", __func__, major_number);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)
    led_class = class_create(CLASS_NAME);
#else
    led_class = class_create(THIS_MODULE, CLASS_NAME);
#endif
    if (IS_ERR(led_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "%s: Failed to register device class\n", __func__);
        return PTR_ERR(led_class);
    }
    printk(KERN_WARNING "%s: device class registered correctly\n", __func__);

    led_device = device_create(led_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(led_device)) {
        class_destroy(led_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "%s: Failed to create the device\n", __func__);
        return PTR_ERR(led_device);
    }
    printk(KERN_WARNING "%s: device class created correctly\n", __func__);

    return 0;
}

static void __exit led_ctrl_exit(void) {
    device_destroy(led_class, MKDEV(major_number, 0));
    class_unregister(led_class);
    class_destroy(led_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    printk(KERN_WARNING "%s: Goodbye from the LED Control Device!\n", __func__);
}

static int led_ctrl_dev_open(struct inode *inodep, struct file *filep) {
    printk(KERN_WARNING "%s: Device has been opened\n", __func__);
    return 0;
}

static int led_ctrl_dev_release(struct inode *inodep, struct file *filep) {
    printk(KERN_WARNING "%s: Device successfully closed\n", __func__);
    return 0;
}

static ssize_t led_ctrl_dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    printk(KERN_WARNING "%s: Read from device\n", __func__);
    return 0;
}

static ssize_t led_ctrl_dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    printk(KERN_WARNING "%s: Write to device\n", __func__);
    return len;
}

module_init(led_ctrl_init);
module_exit(led_ctrl_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("vilmursss");
MODULE_DESCRIPTION("A simple Linux char driver for controlling LEDs in Raspberry Pi 3 output pins");
MODULE_VERSION("0.1");