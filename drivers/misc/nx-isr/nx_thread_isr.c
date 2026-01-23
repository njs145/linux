// nx_isr.c
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>

struct nx_isr_dev {
    int irq;
    atomic64_t cnt;
    struct miscdevice misc;
};

static irqreturn_t nx_isr_top(int irq, void *dev_id)
{
    /* 최소만 하고 스레드로 넘김 */
    return IRQ_WAKE_THREAD;
}

static irqreturn_t nx_isr_thread(int irq, void *dev_id)
{
    struct nx_isr_dev *d = dev_id;

    atomic64_inc(&d->cnt);

    /* 여기서는 sleep 가능 */
    // msleep(5);

    printk("[%s]nx_isr(thread): irq=%d cnt=%lld in_irq=%d in_softirq=%d\n", __FUNCTION__,
            irq, (long long)atomic64_read(&d->cnt),
            in_irq(), in_softirq());

    return IRQ_HANDLED;
}

static ssize_t nx_isr_read(struct file *f, char __user *ubuf, size_t len, loff_t *ppos)
{
    struct miscdevice *m = f->private_data;              // misc가 넣어줌
    struct nx_isr_dev *d = container_of(m, struct nx_isr_dev, misc);
    char buf[32];
    int n;
    long long v;

    if (*ppos != 0)
        return 0; // EOF

    v = (long long)atomic64_read(&d->cnt);
    n = scnprintf(buf, sizeof(buf), "%lld\n", v);

    if (len < n)
        return -EINVAL;
    if (copy_to_user(ubuf, buf, n))
        return -EFAULT;

    *ppos = n;
    return n;
}

static const struct file_operations nx_isr_fops = {
    .owner = THIS_MODULE,
    .read  = nx_isr_read,
    .llseek = noop_llseek,
};

static int nx_isr_probe(struct platform_device *pdev)
{
    struct nx_isr_dev *d;
    int ret;

    d = devm_kzalloc(&pdev->dev, sizeof(*d), GFP_KERNEL);
    if (!d)
        return -ENOMEM;

    atomic64_set(&d->cnt, 0);

    d->irq = platform_get_irq(pdev, 0);
    if (d->irq < 0)
        return d->irq;

    ret = devm_request_threaded_irq(&pdev->dev,
                                d->irq,
                                nx_isr_top,       /* top-half */
                                nx_isr_thread,    /* thread_fn */
                                IRQF_SHARED,
                                "nx_isr", d);

    if (ret) 
    {
        dev_err(&pdev->dev, "request_threaded_irq(%d) failed: %d\n", d->irq, ret);
        return ret;
    }

    d->misc.minor = MISC_DYNAMIC_MINOR;
    d->misc.name  = "nx_isr";
    d->misc.fops  = &nx_isr_fops;

    ret = misc_register(&d->misc);
    if (ret)
        return ret;

    platform_set_drvdata(pdev, d);
    dev_info(&pdev->dev, "ready: /dev/nx_isr (irq=%d)\n", d->irq);
    return 0;
}

static void nx_isr_remove(struct platform_device *pdev)
{
    struct nx_isr_dev *d = platform_get_drvdata(pdev);
    misc_deregister(&d->misc);
}

static const struct of_device_id nx_isr_of_match[] = {
    { .compatible = "nexell,nx-isr", },
    { }
};
MODULE_DEVICE_TABLE(of, nx_isr_of_match);

static struct platform_driver nx_isr_driver = {
    .probe  = nx_isr_probe,
    .remove = nx_isr_remove,
    .driver = {
        .name = "nx_isr",
        .of_match_table = nx_isr_of_match,
    },
};
module_platform_driver(nx_isr_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Minimal misc+platform shared IRQ counter (QEMU virt)");