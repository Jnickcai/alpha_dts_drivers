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
#include <linux/semaphore.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define DTSLED_CNT      1           /* 设备号个数 */
#define DTSLED_NAME     "timer"    /* 名字 */
#define CLOSE_CMD       (_IO(0xEF, 0x1))    /* 关闭定时器 */
#define OPEN_CMD        (_IO(0xEF, 0x2))    /* 打开定时器 */
#define SETPERIOD_CMD   (_IO(0xEF, 0x3))    /* 设置定时器周期命令 */
#define LEDON           1       /* 开灯 */
#define LEDOFF          0       /* 关灯 */

/* timer 设备结构体 */
struct  timer_dev
{
    dev_t  devid;               /*设备号*/
    struct cdev   cdev;         /* cdev */
    struct class  *class;       /* 类 */
    struct device *device;      /*设备 */
    int    major;               /*主设备号 */
    int    minor;               /*次设备号*/
    struct device_node *nd;     /*设备节点*/
    int led_gpio;       /* led 所使用的 GPIO  编号 */
    int timeperiod;     /* 定时周期,单位为 ms */
    struct timer_list timer;    /* 定义一个定时器  */
    spinlock_t lock;            /* 定义自旋锁 */
};

struct timer_dev timerdev;   //timer设备


/*
 * @description : 初始化按键 IO，open 函数打开驱动的时候
 * 初始化按键所使用的 GPIO 引脚。
 * @param : 无
 * @return : 无
 */
static int ledio_init(void)
{
    int ret = 0;

    timerdev.nd = of_find_node_by_path("/gpioled");
    if(timerdev.nd == NULL)
    {
        return -EINVAL;
    }

    timerdev.led_gpio = of_get_named_gpio(timerdev.nd, "led-gpio",0);
    if(timerdev.led_gpio < 0)
    {
        printk("can't get led\r\n");
        return -EINVAL;
    }
    printk("led_gpio=%d\r\n", timerdev.led_gpio);

    /* 初始化 led 所使用的 IO */
    gpio_request(timerdev.led_gpio,"led");   /* 请求 IO */
    ret = gpio_direction_output(timerdev.led_gpio,1); /* 设置为输入 */
    if(ret < 0)
    {
        printk("can't set gpio!\r\n");
    }
    return 0;
}

/*
 * @description : 打开设备
 * @param – inode : 传递给驱动的 inode
 * @param – filp : 设备文件，file 结构体有个叫做 private_data 的成员变量
 * 一般在 open 的时候将 private_data 指向设备结构体。
 * @return : 0 成功;其他 失败
 */
static int timer_open(struct inode *inode,struct file *filp)
{
    int ret;
    filp->private_data = &timerdev;   //设置私有数据
    
    ret = ledio_init();             /* 初始化按键 IO */
    if (ret < 0){
        return ret;
    }
    return 0;
}


 /*
 * @description : ioctl 函数，
 * @param – filp : 要打开的设备文件(文件描述符)
 * @param - cmd : 应用程序发送过来的命令
 * @param - arg : 参数
 * @return : 0 成功;其他 失败
 */
static long timer_unlocked_ioctl(struct file *filp,unsigned int cmd,unsigned long arg)
{
    struct timer_dev *dev = (struct timer_dev *)filp->private_data;
    int timerperiod;
    unsigned long flags;

    switch (cmd)
    {
        case CLOSE_CMD:             /* 关闭定时器 */
            del_timer_sync(&dev->timer);
            break;
        case OPEN_CMD:              /* 打开定时器 */
            spin_lock_irqsave(&dev->lock, flags);
            timerperiod = dev->timeperiod;
            spin_unlock_irqrestore(&dev->lock,flags);
            mod_timer(&dev->timer,jiffies + msecs_to_jiffies(timerperiod));
            break;
        case SETPERIOD_CMD:         /* 设置定时器周期 */
            spin_lock_irqsave(&dev->lock, flags);
            dev->timeperiod = arg;
            spin_unlock_irqrestore(&dev->lock,flags);
            mod_timer(&dev->timer,jiffies+msecs_to_jiffies(arg));
            break;
        default:
            printk("not the CMD!!!!!!");
            break;
    }
    return 0;
}



/*
 * @description : 关闭/释放设备
 * @param – filp : 要关闭的设备文件(文件描述符)
 * @return : 0 成功;其他 失败
 */
 static int key_release(struct inode *inode, struct file *filp)
 {
    return 0;
 }

 /* 设备操作函数 */
 static struct file_operations timerdev_fops = {
     .owner     = THIS_MODULE,
     .open      = timer_open,
     .unlocked_ioctl  = timer_unlocked_ioctl,
     .release   = key_release,
 };

/* 定时器回调函数 */

void timer_function(unsigned long arg)
{
    struct timer_dev *dev = (struct timer_dev *)arg;
    static int sta = 1;
    int timerperiod;
    unsigned long flags;

    sta = !sta;     /* 每次都取反，实现 LED 灯反转 */
    gpio_set_value(dev->led_gpio,sta);

    /* 重启定时器 */
    spin_lock_irqsave(&dev->lock, flags);
    timerperiod = dev->timeperiod;
    spin_unlock_irqrestore(&dev->lock,flags);
    mod_timer(&dev->timer,jiffies+msecs_to_jiffies(dev->timeperiod));
    }


 /*
 * @description : 驱动入口函数
 * @param : 无
 * @return : 无
 */
 static int __init timer_init(void)
 {
 /* 初始化自旋锁 */
 spin_lock_init(&timerdev.lock);


 /* 注册字符设备驱动 */
 /* 1、创建设备号 */
 if(timerdev.major)
 {
    timerdev.devid = MKDEV(timerdev.major,0);
    register_chrdev_region(timerdev.devid,DTSLED_CNT,DTSLED_NAME);
 }
 else
 {
    alloc_chrdev_region(&timerdev.devid,0,DTSLED_CNT,DTSLED_NAME);
    timerdev.major = MAJOR(timerdev.devid); /* 获取分配号的主设备号 */
    timerdev.minor = MINOR(timerdev.devid); /* 获取分配号的次设备号 */
 }
 printk("timerdev major=%d,minor=%d\r\n",timerdev.major,timerdev.minor);

 /* 2、初始化 cdev */
 timerdev.cdev.owner = THIS_MODULE;
 cdev_init(&timerdev.cdev, &timerdev_fops);

 /* 3、添加一个 cdev */
 cdev_add(&timerdev.cdev,timerdev.devid,DTSLED_CNT);

 /* 4、创建类 */
 timerdev.class = class_create(THIS_MODULE, DTSLED_NAME);
 if (IS_ERR(timerdev.class))
 {
    return PTR_ERR(timerdev.class);
 }

 /* 5、创建设备 */
 timerdev.device = device_create(timerdev.class, NULL, timerdev.devid,NULL, DTSLED_NAME);
 if (IS_ERR(timerdev.device)) 
 {
    return PTR_ERR(timerdev.device);
 }

 /* 6、初始化 timer，设置定时器处理函数,还未设置周期，所有不会激活定时器 */
 init_timer(&timerdev.timer);
 timerdev.timer.function = timer_function;
 timerdev.timer.data = (unsigned long)&timerdev;

 timerdev.timeperiod = 500;  //初始化定时器为500ms
 return 0;
}

 /*
 * @description : 驱动出口函数
 * @param : 无
 * @return : 无
 */
 static void __exit timer_exit(void)
 {

 /* 注销字符设备驱动 */
    cdev_del(&timerdev.cdev);/* 删除 cdev */
    unregister_chrdev_region(timerdev.devid, DTSLED_CNT);/*注销设备号*/

    device_destroy(timerdev.class, timerdev.devid);
    class_destroy(timerdev.class);
 }

 module_init(timer_init);
 module_exit(timer_exit);
 MODULE_LICENSE("GPL");
 MODULE_AUTHOR("NICK");