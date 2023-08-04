#include <linux/fs.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/miscdevice.h>

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define DEVICE_NAME "arcane"
#define STARTING_UID 1000
#define N_UIDS 128

static int karcane_open(struct inode *, struct file *);
static int karcane_release(struct inode *, struct file *);
static ssize_t karcane_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t karcane_write(struct file *, const char __user *, size_t,
                            loff_t *);
static long karcane_ioctl(struct file *, unsigned int cmd, unsigned long arg);
static int karcane_mmap(struct file *, struct vm_area_struct *);

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = karcane_read,
    .write = karcane_write,
    .open = karcane_open,
    .unlocked_ioctl = karcane_ioctl,
    .mmap = karcane_mmap,
    .release = karcane_release
};

static struct miscdevice miscdev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DEVICE_NAME,
    .fops = &fops,
    .mode = 0666
};

const static phys_addr_t g_user_pages_phys = 0x13370000;

static int __init karcane_init(void) {
    int res = 0;

    res = misc_register(&miscdev);
    if (res) {
        pr_err("Failed registering dev\n");
        goto err;
    }

    if (!request_mem_region(g_user_pages_phys, N_UIDS * 0x1000, DEVICE_NAME"_mem")) {
        pr_err("Failed requesting memory region\n");
        res = -ENOMEM;
        goto err_deregister;
    }

    pr_info("Init done\n");
    return 0;

err_deregister:
    misc_deregister(&miscdev);
err:
    return res;
}

static void __exit karcane_exit(void) {
    misc_deregister(&miscdev);
    release_mem_region(g_user_pages_phys, N_UIDS * 0x1000);
}

static int karcane_open(struct inode *inode, struct file *file) { return 0; }

static int karcane_release(struct inode *inode, struct file *file) { return 0; }

static ssize_t karcane_read(struct file *filp, char __user *buffer,
                           size_t length, loff_t *offset) {
    return -EINVAL;
}

static ssize_t karcane_write(struct file *filp, const char __user *buff,
                            size_t len, loff_t *off) {
    return -EINVAL;
}

static long karcane_ioctl(struct file *filp, unsigned int cmd,
                         unsigned long arg) {
    return -EINVAL;
}

static const struct vm_operations_struct my_vm_ops = {
#ifdef CONFIG_HAVE_IOREMAP_PROT
    .access = generic_access_phys
#endif
};

phys_addr_t get_addr_from_uid(uid_t uid) {
    return g_user_pages_phys + ((uid - STARTING_UID) * 0x1000);
}

static int karcane_mmap(struct file *file, struct vm_area_struct *vma) {
    size_t size;
    phys_addr_t phys;
    uid_t uid;
    int ret = 0;

    size = vma->vm_end - vma->vm_start;

    uid = current_uid().val;

    if ((uid < STARTING_UID) ||
        (uid >= STARTING_UID + N_UIDS)) {
        ret = -EINVAL;
        goto exit;
    }

    phys = get_addr_from_uid(uid);

    if (size != PAGE_SIZE) {
        ret = -EINVAL;
        goto exit;
    }

    if (vma->vm_pgoff != 0) {
        ret = -EINVAL;
        goto exit;
    }

    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
    vma->vm_ops = &my_vm_ops;

    /* Remap-pfn-range will mark the range VM_IO */
    if (remap_pfn_range(vma, vma->vm_start, phys >> PAGE_SHIFT, size,
                        vma->vm_page_prot)) {
        ret = -EAGAIN;
        goto exit;
    }

exit:
    return ret;
}

module_init(karcane_init);
module_exit(karcane_exit);

MODULE_AUTHOR("Ajeje Brazorf");
MODULE_DESCRIPTION("ArcaneLink Linux device driver");
MODULE_LICENSE("GPL");
