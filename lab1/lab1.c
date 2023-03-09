#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/string.h>
#include <linux/proc_fs.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yaroslav");
MODULE_DESCRIPTION("Character device driver");

static struct proc_dir_entry *entry;

static bool is_letter(char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

static char history[1024*16];
static size_t history_size;

static ssize_t fs_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{    
    size_t size = history_size + 1;

    if (*off > 0 || len < size) 
    {
        return 0;
    }

    if (copy_to_user(buf, history, size) != 0)
	{
		return -EFAULT;
	}

    *off = len;
    return size;
}

static ssize_t fs_write(struct file *f, const char __user *buf,  size_t len, loff_t *off)
{
    char string[len];
    if (copy_from_user(string, buf, len) != 0) {
        return -EFAULT;
    }


    long summ = 0;
    size_t i = 0;
    while (i < len)
    {
        if (is_letter(string[i]))
            summ++;
        i++;
    }

    history_size += sprintf(history + history_size, "%d\n", summ);    
    history[history_size] = '\0';

    return len;
}

static int my_dev_uevent(struct device *dev, struct kobj_uevent_env *env)
{
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

static struct file_operations mychdev_fops =
{
  .owner      = THIS_MODULE,
  .read       = fs_read,
  .write      = fs_write
};

static const struct proc_ops proc_fops = { 
	.proc_read 	= fs_read
};

static dev_t first;
static struct cdev c_dev; 
static struct class *cl;

static int __init ch_drv_init(void)
{
    history_size = 0;
    history[history_size] = '\0';

    if (alloc_chrdev_region(&first, 0, 1, "ch_dev") < 0)
    {
		return -1;
    }

    if ((cl = class_create(THIS_MODULE, "chardrv")) == NULL)
    {
		unregister_chrdev_region(first, 1);
		return -1;
    }

    cl->dev_uevent = my_dev_uevent;

    if (device_create(cl, NULL, first, NULL, "var5") == NULL)
    {
		class_destroy(cl);
        unregister_chrdev_region(first, 1);
        return -1;
    }
    
    cdev_init(&c_dev, &mychdev_fops);
    if (cdev_add(&c_dev, first, 1) == -1)
    {
        device_destroy(cl, first);
        class_destroy(cl);
        unregister_chrdev_region(first, 1);
        return -1;
    }

    entry = proc_create("var5", 0666, NULL, &proc_fops);
    if (entry == NULL) {
        device_destroy(cl, first);
        class_destroy(cl);
        unregister_chrdev_region(first, 1);
        cdev_del(&c_dev);
        return -1;
    }

    return 0;
}
 
static void __exit ch_drv_exit(void)
{
    cdev_del(&c_dev);
    device_destroy(cl, first);
    class_destroy(cl);
    unregister_chrdev_region(first, 1);
    proc_remove(entry);
}
 
module_init(ch_drv_init);
module_exit(ch_drv_exit);

