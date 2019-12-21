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
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define LEDCHR_CNT  1       //设备号个数
#define LEDCHR_NAME "LED"   //名字
#define LEDOFF      0       //关灯
#define LEDON       1       //开灯

/*寄存器物理地址*/

#define CCM_CCGR1_BASS          (0x020c406c)
#define SW_MUX_GPIO1_IO03_BASE  (0x020E0068)
#define SW_PAD_GPIO1_IO03_BASE  (0x020E02F4)
#define GPIO1_DR_BASE           (0x0209c000)
#define GPIO1_GDIR_BASE         (0x0209c004)
/*
#define CCM_CCGR1_BASS          (0x020c406c)
#define SW_MUX_GPIO1_IO03_BASE  (0x020E0070)
#define SW_PAD_GPIO1_IO03_BASE  (0x020E02Fc)
#define GPIO1_DR_BASE           (0x020ac000)
#define GPIO1_GDIR_BASE         (0x020ac004)
*/
/*映射后的寄存器虚拟地址指针*/
static void __iomem *IMX6u_CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;


struct newchrled_dev
{
    dev_t devid;            // 获取到的设备号
    struct class *class;    // 类   
    struct cdev cdev;       // cdev
    struct device *device;  // 设备
    int major;              //主设备号
    int minor;              //次设备号
};

struct newchrled_dev newchrled; //led设备

void led_switch(u8 sta)     //打开  关闭LED
{
    u32 val =0;
    if (sta == LEDON)
    {
        val =  readl(GPIO1_DR);
        val &= ~(1<<3);
        writel(val,GPIO1_DR);
    }
    else if(sta == LEDOFF)
    {
        val = readl(GPIO1_DR);
        val |=(1<<3);
        writel(val,GPIO1_DR);
        printk("LED OFF\r\n");
    }
}


static int led_open (struct inode *inode,struct file *filp)
{
    filp->private_data = &newchrled; //设置私有数据 
    return 0;
}

static ssize_t led_read(struct file*filp,char __user *buf,size_t cnt,loff_t *offt)
{
    return 0;
}

static ssize_t led_write(struct file *filp,const char __user *buf,size_t cnt,loff_t *offt)
{
    int retvalue;
    unsigned char databuf[1];
    unsigned char ledstat;
    retvalue = copy_from_user(databuf,buf,cnt);
    if(retvalue < 0)
    {
        printk("kernel write failed!\r\n");
        return -EFAULT;
    }
    ledstat = databuf[0];       //获取LED状态值
    led_switch(ledstat);        //根据状态开关LED
    return 0;
}

static int led_release(struct inode *inode,struct file *filp)
{
    return 0;
}

/*设备操作函数*/
static struct file_operations newchrled_fops =
{
    .owner   = THIS_MODULE,
    .open    = led_open,
    .read    = led_read,
    .write   = led_write,
    .release = led_release,
};


static int __init Led_init(void)
{
    u32 val = 0;
    /* 初始化 LED */
    /* 1、寄存器地址映射 */
    IMX6u_CCM_CCGR1     = ioremap(CCM_CCGR1_BASS,4);
    SW_MUX_GPIO1_IO03   = ioremap(SW_MUX_GPIO1_IO03_BASE,4);
    SW_PAD_GPIO1_IO03   = ioremap(SW_PAD_GPIO1_IO03_BASE,4);
    GPIO1_DR            = ioremap(GPIO1_DR_BASE,4);
    GPIO1_GDIR          = ioremap(GPIO1_GDIR_BASE,4);

    /* 2、使能 GPIO1 时钟 */
    val = readl(IMX6u_CCM_CCGR1);
    val &= ~(3<<26);
    val |=  (3<<26);
    writel(val,IMX6u_CCM_CCGR1);

    /* 3、设置 GPIO1_IO03 的复用功能，将其复用为 GPIO1_IO03，最后设置 IO 属性。*/
    writel(5,SW_MUX_GPIO1_IO03);
    writel(0x10B0,SW_PAD_GPIO1_IO03);

    /* 4、设置 GPIO1_IO03 为输出功能 */
    val = readl(GPIO1_GDIR);
    val &= ~(1<<3);
    val |= (1<<3);
    writel(val,GPIO1_GDIR);

    /* 5、默认关闭 LED */
    val = readl(GPIO1_DR);
    val |= (1 << 3);
    writel(val,GPIO1_DR);

    /*  注册字符设备驱动 */
    /*1 创建设备号*/
    if (newchrled.major)    /* 定义了设备号 */
    {
        newchrled.devid = MKDEV(newchrled.major , 0);  //获取设备号
        register_chrdev_region(newchrled.devid,LEDCHR_CNT, LEDCHR_NAME); //已经有主设备号，通过函数 register_chrdev_region注册。
    }
    else        /*  没有定义设备号 */
    {
        alloc_chrdev_region(&newchrled.devid,0,LEDCHR_CNT,LEDCHR_NAME);  //自动分配设备号
        newchrled.major = MAJOR(newchrled.devid);
        newchrled.minor = MINOR(newchrled.devid);
    }
    printk("newcheled major=%d,minor=%d\r\n",newchrled.major,newchrled.minor);

    /* 2 、初始化 cdev */
    newchrled.cdev.owner = THIS_MODULE;
    cdev_init(&newchrled.cdev,&newchrled_fops);

    /* 3 、添加一个 cdev */
    cdev_add(&newchrled.cdev,newchrled.devid,LEDCHR_CNT);

    /* 4 、创建类 */
    newchrled.class = class_create(THIS_MODULE,LEDCHR_NAME);
    if (IS_ERR(newchrled.class)) 
    {
        return PTR_ERR(newchrled.class);
    }

    /* 5 、创建设备 */
    newchrled.device = device_create(newchrled.class,NULL,newchrled.devid,NULL,LEDCHR_NAME);
   if (IS_ERR(newchrled.device))
    {
        return PTR_ERR(newchrled.device);
    }
    return 0;
}


static void __exit Led_Exit(void)
{
    unsigned int val;
    /* 5、默认关闭 LED */
    val = readl(GPIO1_DR);
    val |= (1 << 3);
    writel(val,GPIO1_DR);

    /* 取消映射 */
    iounmap(IMX6u_CCM_CCGR1);
    iounmap(SW_MUX_GPIO1_IO03);
    iounmap(SW_PAD_GPIO1_IO03);
    iounmap(GPIO1_DR);
    iounmap(GPIO1_GDIR);

    /*  注销字符设备 */    
    cdev_del(&newchrled.cdev);
    unregister_chrdev_region(newchrled.devid,LEDCHR_CNT);  //注销设备

    device_destroy(newchrled.class,newchrled.devid); //删除设备
    class_destroy(newchrled.class); //删除类
}

module_init(Led_init);
module_exit(Led_Exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nick");
//MODULE_DESCRIPTION("hwconfig driver");