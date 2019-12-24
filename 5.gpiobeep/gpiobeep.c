#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define DTSLED_CNT      1           /* 设备号个数 */
#define DTSLED_NAME     "gpiobeep"    /* 名字 */
#define BEEPOFF          0           /* 关灯 */
#define BEEPON           1           /* 开灯 */

/* gpioled 设备结构体 */
struct  gpiobeep_dev
{
    dev_t  devid;               /*设备号*/
    struct cdev   cdev;         /* cdev */
    struct class  *class;        /* 类 */
    struct device *device;       /*设备 */
    int    major;                /*主设备号 */
    int    minor;                /*次设备号*/
    struct device_node *nd;       /*设备节点*/
    int beep_gpio; /* led  所使用的 GPIO  编号 */
};

struct gpiobeep_dev gpiobeep;   //beep设备


/*
 * @description : 打开设备
 * @param – inode : 传递给驱动的 inode
 * @param – filp : 设备文件，file 结构体有个叫做 private_data 的成员变量
 * 一般在 open 的时候将 private_data 指向设备结构体。
 * @return : 0 成功;其他 失败
 */
static int beep_open(struct inode *inode,struct file *filp)
{
    filp->private_data = &gpiobeep;   //设置私有数据
    return 0;
}

 /*
 * @description : 从设备读取数据
 * @param – filp : 要打开的设备文件(文件描述符)
 * @param - buf : 返回给用户空间的数据缓冲区
 * @param - cnt : 要读取的数据长度
 * @param – offt : 相对于文件首地址的偏移
 * @return : 读取的字节数，如果为负值，表示读取失败
 */
static ssize_t beep_read(struct file *filp,char __user *buf,size_t cnt,loff_t *offt)
{
    return 0;
}

/*
 * @description : 向设备写数据
 * @param - filp : 设备文件，表示打开的文件描述符
 * @param - buf : 要写给设备写入的数据
 * @param - cnt : 要写入的数据长度
 * @param – offt : 相对于文件首地址的偏移
 * @return : 写入的字节数，如果为负值，表示写入失败
 */
static ssize_t beep_write (struct file *filp,const char __user *buf,size_t cnt,loff_t *offt)
{
    int retvalue;
    unsigned char databuf[1];
    unsigned char beepstat;
    struct gpiobeep_dev *dev = filp->private_data;

    retvalue = copy_from_user(databuf,buf,cnt);
    if(retvalue < 0)
    {
        printk("kernel write failed!\r\n");
        return -EFAULT;
    }

    beepstat = databuf[0];  //获取状态值
    if(beepstat == BEEPON)
    {
        gpio_set_value(dev->beep_gpio,0);
    }else if(beepstat == BEEPOFF)
    {
        gpio_set_value(dev->beep_gpio,1);
    }
    return 0;
}

/*
 * @description : 关闭/释放设备
 * @param – filp : 要关闭的设备文件(文件描述符)
 * @return : 0 成功;其他 失败
 */
 static int beep_release(struct inode *inode, struct file *filp)
 {
    return 0;
 }

 /* 设备操作函数 */
 static struct file_operations gpiobeep_fops = {
     .owner     = THIS_MODULE,
     .open      = beep_open,
     .read      = beep_read,
     .write     = beep_write,
     .release   = beep_release,
 };

 /*
 * @description : 驱动入口函数
 * @param : 无
 * @return : 无
 */
 static int __init beep_init(void)
 {
    int ret;

    /* 设置 LED 所使用的 GPIO */
    /* 1、获取设备节点：gpioled */
    gpiobeep.nd = of_find_node_by_path("/gpiobeep");
    if(gpiobeep.nd == NULL)
    {
        printk("gpiobeep node not find!!!!\r\n");
        return -EINVAL;
    }
    else
    {
        printk ("gpiobeep node find!!!\r\n");
    }
     
     /* 2、 获取设备树中的 gpio 属性，得到 LED 所使用的 LED 编号 */
    gpiobeep.beep_gpio = of_get_named_gpio(gpiobeep.nd,"beep-gpio",0);
    if(gpiobeep.beep_gpio < 0)
    {
        printk("cat't get led-gpio");
        return -EINVAL;
    }
    printk("led-gpio num = %d\r\n", gpiobeep.beep_gpio);

    /* 3、设置 GPIO1_IO03 为输出，并且输出高电平，默认关闭 LED 灯 */
    ret = gpio_direction_output(gpiobeep.beep_gpio,1);
    if (ret < 0)
    {
        printk("can't set gpio!\r\n");
    }

 /* 注册字符设备驱动 */
 /* 1、创建设备号 */
 if(gpiobeep.major)
 {
    gpiobeep.devid = MKDEV(gpiobeep.major,0);
    register_chrdev_region(gpiobeep.devid,DTSLED_CNT,DTSLED_NAME);
 }
 else
 {
    alloc_chrdev_region(&gpiobeep.devid,0,DTSLED_CNT,DTSLED_NAME);
    gpiobeep.major = MAJOR(gpiobeep.devid); /* 获取分配号的主设备号 */
    gpiobeep.minor = MINOR(gpiobeep.devid); /* 获取分配号的次设备号 */
 }
 printk("gpiobeep major=%d,minor=%d\r\n",gpiobeep.major,gpiobeep.minor);

 /* 2、初始化 cdev */
 gpiobeep.cdev.owner = THIS_MODULE;
 cdev_init(&gpiobeep.cdev, &gpiobeep_fops);

 /* 3、添加一个 cdev */
 cdev_add(&gpiobeep.cdev,gpiobeep.devid,DTSLED_CNT);

 /* 4、创建类 */
 gpiobeep.class = class_create(THIS_MODULE, DTSLED_NAME);
 if (IS_ERR(gpiobeep.class))
 {
    return PTR_ERR(gpiobeep.class);
 }

 /* 5、创建设备 */
 gpiobeep.device = device_create(gpiobeep.class, NULL, gpiobeep.devid,NULL, DTSLED_NAME);
 if (IS_ERR(gpiobeep.device)) 
 {
    return PTR_ERR(gpiobeep.device);
 }
    return 0;
}

 /*
 * @description : 驱动出口函数
 * @param : 无
 * @return : 无
 */
 static void __exit beep_exit(void)
 {
 /* 注销设备后关闭LED*/
    gpio_set_value(gpiobeep.beep_gpio,1);

 /* 注销字符设备驱动 */
    cdev_del(&gpiobeep.cdev);/* 删除 cdev */
    unregister_chrdev_region(gpiobeep.devid, DTSLED_CNT);/*注销设备号*/

    device_destroy(gpiobeep.class, gpiobeep.devid);
    class_destroy(gpiobeep.class);
 }

 module_init(beep_init);
 module_exit(beep_exit);
 MODULE_LICENSE("GPL");
 MODULE_AUTHOR("NICK");