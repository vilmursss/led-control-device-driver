#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/io.h>

// Module defines
#define DEVICE_NAME "led-control"
#define CLASS_NAME "led"
#define ERROR_MSG_SIZE 256

// I/O defines
#define GPIO_BASE 0x3F200000
#define GPIO_SET_OFFSET 0x1C
#define GPIO_CLR_OFFSET 0x28
#define GPIO_PIN_21 21
#define GPIO_PIN_20 20
#define GPIO_PIN_16 16
#define GPIO_MAPPED_REGION_SIZE 0xB0

// Module variables
static int major_number;
static struct class* led_class = NULL;
static struct device* led_device = NULL;
static char last_error[ERROR_MSG_SIZE] = {0};
volatile unsigned int *gpio;

// Local functions
static void set_last_error(const char *fmt, ...);
static void gpio_set(int pin);
static void gpio_clear(int pin);
static void gpio_blink(int pin, int duration_ms);
static void set_gpio_direction_out(int pin);
static void handle_input(const char *input);
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

static int __init gpio_init(void) {
    gpio = (volatile unsigned int *) ioremap(
        GPIO_BASE, GPIO_MAPPED_REGION_SIZE);

    if (!gpio) {
        printk(KERN_ERR "Failed to map GPIO memory\n");
        return -ENOMEM;
    }
    return 0;
}

static int __init led_ctrl_init(void) {
    printk(KERN_INFO "%s: Initializing the LED Control Device\n", __func__);

    major_number = register_chrdev(0, DEVICE_NAME, &f_ops);
    if (major_number < 0) {
        printk(KERN_ALERT "%s: failed to register a major number\n", __func__);
        return major_number;
    }

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

    led_device = device_create(led_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(led_device)) {
        class_destroy(led_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "%s: Failed to create the device\n", __func__);
        return PTR_ERR(led_device);
    }

     int ret = gpio_init();
     if (ret) {
        return ret;
     }

    // Set LEDs to be output pins
    set_gpio_direction_out(GPIO_PIN_21);
    set_gpio_direction_out(GPIO_PIN_20);
    set_gpio_direction_out(GPIO_PIN_16);

    printk(KERN_INFO "%s: Device created successfully\n", __func__);

    return 0;
}

static void __exit gpio_exit(void) {
    iounmap(gpio);
}

static void __exit led_ctrl_exit(void) {
    // Turn LEDs off
    gpio_clear(GPIO_PIN_21);
    gpio_clear(GPIO_PIN_20);
    gpio_clear(GPIO_PIN_16);
    
    gpio_exit();

    device_destroy(led_class, MKDEV(major_number, 0));
    class_unregister(led_class);
    class_destroy(led_class);
    unregister_chrdev(major_number, DEVICE_NAME);

    printk(KERN_INFO "%s: Goodbye from the LED Control Device!\n", __func__);
}

static void set_last_error(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vsnprintf(last_error, ERROR_MSG_SIZE, fmt, args);
    va_end(args);
}

static void gpio_set(int pin) {
    // Dividing by 4 converts the byte offset to a word offset
    // (since each register is 4 bytes).
    iowrite32(1 << pin, gpio + GPIO_SET_OFFSET / 4);
}

static void gpio_clear(int pin) {
    // Dividing by 4 converts the byte offset to a word offset
    // (since each register is 4 bytes).
    iowrite32(1 << pin, gpio + GPIO_CLR_OFFSET / 4);
}

static void gpio_blink(int pin, int duration_ms) {
    int i;
    for (i = 0; i < duration_ms / 100; i++) {
        gpio_set(pin);
        msleep(50);
        gpio_clear(pin);
        msleep(50);
    }
}

static void set_gpio_direction_out(int pin) {
    // Get the register index (GPFSEL)
    int reg = pin / 10;

    // Calcualte the bit shift for the specific pin
    int shift = (pin % 10) * 3;

    // Read current GPFSEL value
    unsigned int value = ioread32(gpio + reg);

    // Clear the 3 bits corresponding to the PIN's function
    value &= ~(7 << shift);

    // Set the 3 bits to '001' to configure the pin as an ouput
    value |= (1 << shift);

    // Write modified value back
    iowrite32(value, gpio + reg);
}

static void handle_input(const char *input) {
    int pin;
    char action[10];

    // Parse the input string
    if (sscanf(input, "%d:%9s", &pin, action) != 2) {
        set_last_error("Invalid input format\n");
        return;
    }

    // Perform the action
    if (strcmp(action, "on") == 0) {
        gpio_set(pin);
    } else if (strcmp(action, "off") == 0) {
        gpio_clear(pin);
    } else if (strcmp(action, "blink") == 0) {
        gpio_blink(pin, 5000); // Blink for 5 second
    } else {
        set_last_error("Unknown action: %s\n", action);
    }
}

static int led_ctrl_dev_open(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "LED Control device opened\n");
    return 0;
}

static int led_ctrl_dev_release(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "LED Control device closed\n");
    return 0;
}

static ssize_t led_ctrl_dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    int error_len;

    error_len = strlen(last_error);

    if (*offset >= error_len) {
        return 0;
    }

    if (len > error_len - *offset) {
        len = error_len - *offset;
    }

    if (copy_to_user(buffer, last_error + *offset, len)) {
        return -EFAULT;
    }

    *offset += len;

    return len;
}

static ssize_t led_ctrl_dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    char input[256] = {0};

    if (len > 255) {
        len = 255;
    }

    if (copy_from_user(input, buffer, len)) {
        return -EFAULT;
    }

    handle_input(input);

    return len;
}

module_init(led_ctrl_init);
module_exit(led_ctrl_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("vilmursss");
MODULE_DESCRIPTION("A simple Linux char driver for controlling LEDs in Raspberry Pi 3 output pins");
MODULE_VERSION("0.1");