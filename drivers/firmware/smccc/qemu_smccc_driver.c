#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/arm-smccc.h>

#define DEVICE_NAME "qemu_smccc"

struct arm_smccc_args {
	int func_id;
    int a1;
    int a2;
    int a3;
    struct arm_smccc_res res;
};

#define IOCTL_SMC_CALL _IOWR('s', 1, struct arm_smccc_args)

static void smccc_remove(struct platform_device *pdev);
static int smccc_probe(struct platform_device *pdev);

static __initconst struct of_device_id smccc_of_match[] = {
    { .compatible = "qemu,smccc" },
    {},
};

MODULE_DEVICE_TABLE(of, smccc_of_match);

static struct platform_driver smccc_plat_driver = {
    .probe  = smccc_probe,
    .remove = smccc_remove,
    .driver = {
        .name           = "my_smc_cc",
        .of_match_table = smccc_of_match,
    },
};

static dev_t smc_dev;
static struct cdev smc_cdev;
static struct class *smc_class;

static long smc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct arm_smccc_args args;
    if (cmd != IOCTL_SMC_CALL)
        return -ENOTTY;
    if (copy_from_user(&args, (void __user *)arg, sizeof(args)))
        return -EFAULT;
    arm_smccc_smc(args.func_id, args.a1, args.a2, args.a3,
                  0, 0, 0, 0, &args.res);
    if (copy_to_user((void __user *)arg, &args, sizeof(args)))
        return -EFAULT;
    return 0;
}

static const struct file_operations smc_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = smc_ioctl,
    /* 필요 시 .open/.release 등 추가 */
};

static int smccc_probe(struct platform_device *pdev)
{
    int ret;

    /* (1) 디바이스 번호 할당 */
    ret = alloc_chrdev_region(&smc_dev, 0, 1, DEVICE_NAME);
    if (ret)
        return ret;

    /* (2) cdev 초기화 및 등록 */
    cdev_init(&smc_cdev, &smc_fops);
    smc_cdev.owner = THIS_MODULE;
    ret = cdev_add(&smc_cdev, smc_dev, 1);
    if (ret)
        goto unregister_chrdev;

    /* (3) /dev 노드 자동 생성 */
    smc_class = class_create(DEVICE_NAME);
    if (IS_ERR(smc_class)) {
        ret = PTR_ERR(smc_class);
        goto del_cdev;
    }
    device_create(smc_class, NULL, smc_dev, NULL, DEVICE_NAME);

    dev_info(&pdev->dev, "SMCCC char device registered\n");
    return 0;

del_cdev:
    cdev_del(&smc_cdev);
unregister_chrdev:
    unregister_chrdev_region(smc_dev, 1);
    return ret;
}

static void smccc_remove(struct platform_device *pdev)
{
    device_destroy(smc_class, smc_dev);
    class_destroy(smc_class);
    cdev_del(&smc_cdev);
    unregister_chrdev_region(smc_dev, 1);
}

module_platform_driver(smccc_plat_driver);