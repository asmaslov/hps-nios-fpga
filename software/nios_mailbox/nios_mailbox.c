#include "nios_mailbox.h"

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/sched/signal.h>

#define NAME_BUF_SIZE  32

static struct list_head g_dev_list;
static struct semaphore g_dev_list_sem;
static int g_dev_index;

static int nios_mailbox_probe(struct platform_device *pdev);
static int nios_mailbox_remove(struct platform_device *pdev);
static ssize_t nios_mailbox_read(struct file *file, char *buffer, size_t len, loff_t *offset);
static ssize_t nios_mailbox_write(struct file *file, const char *buffer, size_t len, loff_t *offset);
static loff_t nios_mailbox_lseek(struct file *file, loff_t offset, int orig);
static long nios_mailbox_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

struct nios_mailbox_dev {
  char name[NAME_BUF_SIZE];
  struct list_head dev_list;
  struct miscdevice miscdev;
  void __iomem *regs;
  u32 data[2];
  struct siginfo sinfo;
  pid_t pid;
  struct task_struct *task;
  spinlock_t lock;
};

static struct of_device_id nios_mailbox_dt_ids[] = {
    {
        .compatible = "altr,mailbox-1.0"
    },
    { /* end of table */ }
};

MODULE_DEVICE_TABLE(of, nios_mailbox_dt_ids);

static struct platform_driver nios_mailbox_platform = {
    .probe = nios_mailbox_probe,
    .remove = nios_mailbox_remove,
    .driver = {
        .name = "nios_mailbox",
        .owner = THIS_MODULE,
        .of_match_table = nios_mailbox_dt_ids
    }
};

static const struct file_operations nios_mailbox_fops = {
    .owner = THIS_MODULE,
    .read = nios_mailbox_read,
    .write = nios_mailbox_write,
    .llseek = nios_mailbox_lseek,
    .unlocked_ioctl = nios_mailbox_ioctl
};

static irq_handler_t nios_mailbox_isr(int irq, void *pdev)
{
  struct nios_mailbox_dev *dev = (struct nios_mailbox_dev*)platform_get_drvdata(pdev);
  spin_lock(&dev->lock);
  //NOTE: Order is important! CMD register should be read after PTR register
  dev->data[1] = ioread32(dev->regs + ALTERA_AVALON_MAILBOX_SIMPLE_PTR_OFST * sizeof(u32));
  dev->data[0] = ioread32(dev->regs + ALTERA_AVALON_MAILBOX_SIMPLE_CMD_OFST * sizeof(u32));
  spin_unlock(&dev->lock);
  pr_info("NIOS Mailbox new mail!\n");
  if(dev->task)
  {
    send_sig_info(dev->sinfo.si_signo, &dev->sinfo, dev->task);
  }
  return (irq_handler_t)IRQ_HANDLED;
}

static int nios_mailbox_init(void)
{
  int ret_val = 0;
  pr_info("Initializing the NIOS Mailbox module\n");
  INIT_LIST_HEAD(&g_dev_list);
  sema_init(&g_dev_list_sem, 1);
  g_dev_index = 0;
  ret_val = platform_driver_register(&nios_mailbox_platform);
  if(ret_val != 0)
  {
    pr_err("platform_driver_register returned %d\n", ret_val);
    return ret_val;
  }
  pr_info("NIOS Mailbox module successfully initialized!\n");
  return 0;
}

static int nios_mailbox_probe(struct platform_device *pdev)
{
  int ret_val = -EBUSY;
  struct nios_mailbox_dev *dev;
  int irq_num;
  struct clk *clk;
  unsigned long clk_rate;
  struct resource *r = 0;

  if(down_interruptible(&g_dev_list_sem))
  {
    return -ERESTARTSYS;
  }
  pr_info("nios_mailbox_probe enter\n");
  r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
  if(r == NULL)
  {
    pr_err("IORESOURCE_MEM (register space) does not exist\n");
    goto bad_exit_return;
  }
  pr_info("r->start = 0x%08lx\n", (long unsigned int)r->start);
  pr_info("r->end = 0x%08lx\n", (long unsigned int)r->end);
  pr_info("r->name = %s\n", r->name);
  clk = clk_get(&pdev->dev, NULL);
  if (IS_ERR(clk))
  {
    goto bad_exit_return;
  }
  else
  {
    clk_rate = clk_get_rate(clk);
    pr_info("clk = %lu HZ\n", clk_rate);
  }
  dev = devm_kzalloc(&pdev->dev, sizeof(struct nios_mailbox_dev), GFP_KERNEL);
  scnprintf(dev->name, NAME_BUF_SIZE, "nios_mailbox_%d", g_dev_index);
  pr_info("name = %s\n", dev->name);
  INIT_LIST_HEAD(&dev->dev_list);
  dev->regs = devm_ioremap_resource(&pdev->dev, r);
  if(IS_ERR(dev->regs))
  {
    goto bad_ioremap;
  }
  irq_num = platform_get_irq(pdev, 0);
  if(irq_num >= 0)
  {
    pr_info("NIOS Mailbox IRQ %d about to be registered!\n", irq_num);
    ret_val = request_irq(irq_num, (irq_handler_t)nios_mailbox_isr, 0, "nios_mailbox", pdev);
    if(ret_val)
    {
      dev_err(&pdev->dev, "request IRQ %d failed", irq_num);
      goto bad_exit_return;
    }
    spin_lock_init(&dev->lock);
    iowrite32(ALTERA_AVALON_MAILBOX_SIMPLE_INTR_PEN_MSK, dev->regs +
              ALTERA_AVALON_MAILBOX_SIMPLE_INTR_OFST * sizeof(u32));
    memset(&dev->sinfo, 0, sizeof(struct siginfo));
    dev->sinfo.si_signo = NIOS_MAILBOX_REALTIME_SIGNO;
  }
  dev->miscdev.minor = MISC_DYNAMIC_MINOR;
  dev->miscdev.name = dev->name;
  dev->miscdev.fops = &nios_mailbox_fops;
  ret_val = misc_register(&dev->miscdev);
  if(ret_val != 0)
  {
    pr_err("Couldn't register misc device\n");
    goto bad_exit_return;
  }
  list_add(&dev->dev_list, &g_dev_list);
  g_dev_index++;
  platform_set_drvdata(pdev, (void*)dev);
  pr_info("nios_mailbox_probe exit\n");
  up(&g_dev_list_sem);
  return 0;

bad_ioremap:
  ret_val = PTR_ERR(dev->regs);

bad_exit_return:
  pr_err("nios_mailbox_probe bad exit\n");
  up(&g_dev_list_sem);
  return ret_val;
}

static ssize_t nios_mailbox_read(struct file *file, char *buffer, size_t len, loff_t *offset)
{
  int success = 0;
  unsigned long size = 0;

  struct nios_mailbox_dev *dev = container_of(file->private_data, struct nios_mailbox_dev, miscdev);
  if(*offset == 0)
  {
    size = sizeof(dev->data);
    spin_lock(&dev->lock);
    success = copy_to_user(buffer, dev->data, size);
    spin_unlock(&dev->lock);
    if(success != 0)
    {
      pr_err("Failed to return NIOS Mailbox data to userspace\n");
      return -EFAULT;
    }
    *offset += size;
    return size;
  }
  else
  {
    return 0;
  }
}

static ssize_t nios_mailbox_write(struct file *file, const char *buffer, size_t len, loff_t *offset)
{
  int success = 0;

  struct nios_mailbox_dev *dev = container_of(file->private_data, struct nios_mailbox_dev, miscdev);
  spin_lock(&dev->lock);
  success = copy_from_user(dev->data, buffer, sizeof(dev->data));
  spin_unlock(&dev->lock);
  if(success != 0)
  {
    pr_err("Failed to read NIOS Mailbox data from userspace\n");
    return -EFAULT;
  }
  iowrite32(dev->data[1], dev->regs + ALTERA_AVALON_MAILBOX_SIMPLE_PTR_OFST * sizeof(u32));
  iowrite32(dev->data[0], dev->regs + ALTERA_AVALON_MAILBOX_SIMPLE_CMD_OFST * sizeof(u32));
  return len;
}

static loff_t nios_mailbox_lseek(struct file *file, loff_t offset, int orig)
{
  loff_t new_pos = 0;

  struct nios_mailbox_dev *dev = container_of(file->private_data, struct nios_mailbox_dev, miscdev);
  switch(orig)
  {
    case SEEK_SET:
      new_pos = offset;
      break;
    case SEEK_CUR:
      new_pos = file->f_pos + offset;
      break;
    case SEEK_END:
      new_pos = sizeof(dev->data) - offset;
      break;
  }
  if(new_pos > sizeof(dev->data))
  {
    new_pos = sizeof(dev->data);
  }
  if(new_pos < 0)
  {
    new_pos = 0;
  }
  file->f_pos = new_pos;
  return new_pos;
}

static long nios_mailbox_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
  int success = 0;

  struct nios_mailbox_dev *dev = container_of(file->private_data, struct nios_mailbox_dev, miscdev);
  if((_IOC_TYPE(cmd) != IOC_MAGIC))
  {
    return -ENOTTY;
  }
  switch(cmd) {
    case IOCTL_SET_PID:
      spin_lock(&dev->lock);
      success = copy_from_user(&dev->pid, (void *)arg, sizeof(dev->pid));
      spin_unlock(&dev->lock);
      if(success != 0)
      {
        pr_err("Failed to read NIOS Mailbox pid from userspace\n");
        return -EFAULT;
      }
      pr_info("NIOS Mailbox got pid %d\n", dev->pid);
      spin_lock(&dev->lock);
      dev->task = pid_task(find_vpid(dev->pid), PIDTYPE_PID);
      spin_unlock(&dev->lock);
      if(dev->task == NULL)
      {
        pr_err("Cannot find task for pid from user program\n");
        break;
      }
      pr_info("NIOS Mailbox set task %p (pid %d)\n", dev->task, dev->task->pid);
      break;
  }
  return 0;
}

static int nios_mailbox_remove(struct platform_device *pdev)
{
  int irq_num;
  struct nios_mailbox_dev *dev = (struct nios_mailbox_dev*)platform_get_drvdata(pdev);

  if(down_interruptible(&g_dev_list_sem))
  {
    return -ERESTARTSYS;
  }
  pr_info("nios_mailbox_remove enter\n");
  list_del_init(&dev->dev_list);
  misc_deregister(&dev->miscdev);
  irq_num = platform_get_irq(pdev, 0);
  if(irq_num >= 0)
  {
    pr_info("NIOS Mailbox IRQ %d about to be freed!\n", irq_num);
    free_irq(irq_num, NULL);
  }
  devm_kfree(&pdev->dev, dev);
  pr_info("nios_mailbox_remove exit\n");
  up(&g_dev_list_sem);
  return 0;
}

static void nios_mailbox_exit(void)
{
  pr_info("NIOS Mailbox module exit\n");
  platform_driver_unregister(&nios_mailbox_platform);
  pr_info("NIOS Mailbox module successfully unregistered\n");
}

module_init(nios_mailbox_init);
module_exit(nios_mailbox_exit);

// Define information about this kernel module
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dr.Maze");
MODULE_DESCRIPTION("NIOS mailbox driver");
MODULE_VERSION("0.9");
