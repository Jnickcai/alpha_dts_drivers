#include<linux/types.h>
#include<linux/kernel.h>
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
#include <linux/timer.h>
#include <linux/of_irq.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/platform_device.h>
#include <linux/fcntl.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>



#define KEYINPUT_CNT    1               /* 设备号个数 */
#define KEYINPUT_NAME   "keyinput"      /* 名字 */
#define KEY0VALUE       0x01            /* KEY0 按键值 */
#define INVAKEY         0xFF            /* 无效的按键值 */
#define KEY_NUM         1               /* 按键数量 */


/* 中断 IO 设备结构体 */
struct irq_keydesc
{
    int gpio;               /* gpio */
    int irqnum;             /* 中断号 */
    unsigned char value;    /* 按键对应的键值 */
    char name[10];          /* 名字 */
    irqreturn_t (*handler) (int,void *);    /* 中断服务函数 */
};

/* keyinput 设备结构体 */
struct keyinput_dev
{
    dev_t devid;            /* 设备号 */
    struct cdev cdev;       /* cdev */
    struct class *class;    /* 类 */
    struct device *device;  /* 设备 */
    int major;              /* 主设备号 */
    int minor;              /* 次设备号 */
    struct device_node *nd;  /* 设备节点 */
    struct timer_list timer;   /* 定义一个定时器  */
    struct irq_keydesc irqkeydesc[KEY_NUM]; /* 按键描述数组 */
    unsigned char curkeynum;                /* 当前的按键号 */
    struct input_dev *inputdev;             /* input  结构体 */
};


struct keyinput_dev keyinputdev;   /* key input 设备 */

/* @description : 中断服务函数，开启定时器，延时 10ms，
 * 定时器用于按键消抖。
 * @param - irq : 中断号
 * @param - dev_id : 设备结构。
 * @return : 中断执行结果
 */
static irqreturn_t key0_handler(int irq,void *dev_id)
{
    struct keyinput_dev *dev = (struct keyinput_dev *)dev_id;

    dev->curkeynum = 0;
    dev->timer.data = (volatile long)dev_id;
    mod_timer(&dev->timer,jiffies+msecs_to_jiffies(10));
    return IRQ_RETVAL(IRQ_HANDLED);
}

/* @description : 定时器服务函数，用于按键消抖，定时器到了以后
 * 再次读取按键值，如果按键还是处于按下状态就表示按键有效。
 * @param - arg : 设备结构变量
 * @return : 无
 */

void timer_function(unsigned long arg)
{
    unsigned char value;
    unsigned char num;
    struct irq_keydesc *keydesc;
    struct keyinput_dev *dev = (struct keyinput_dev *)arg;

    num = dev->curkeynum;
    keydesc = &dev->irqkeydesc[num];
    value = gpio_get_value(keydesc->gpio);      /* 读取 IO 值 */
    if(value == 0)
    {
        printk("test press\r\n");
        /*  上报按键值 */
        input_report_key(dev->inputdev, keydesc->value, 1);
        input_sync(dev->inputdev);
    }
    else
    {
        printk("test release\r\n");
        input_report_key(dev->inputdev, keydesc->value, 0);
        input_sync(dev->inputdev);
    }
    
}

/*
 * @description : 按键 IO 初始化
 * @param : 无
 * @return : 无
 */
static int keyio_init(void)
{
    unsigned char i = 0;
    char name[10];
    int ret = 0;

    keyinputdev.nd = of_find_node_by_path("/gpiokey");
    if (keyinputdev.nd == NULL)
    {
        printk("key node not find!\r\n");
        return -EINVAL;
    }

    /* 提取 GPIO */
    for(i = 0; i < KEY_NUM; i++)
    {
        keyinputdev.irqkeydesc[i].gpio = of_get_named_gpio(keyinputdev.nd,"key-gpio",i);
        if(keyinputdev.irqkeydesc[i].gpio < 0)
        {
            printk("can't get key%d\r\n",i);
        }
    }

    /* 初始化 key 所使用的 IO，并且设置成中断模式 */
    for (i = 0;i < KEY_NUM; i++)
    {
        memset(keyinputdev.irqkeydesc[i].name,0,sizeof(name));
        sprintf(keyinputdev.irqkeydesc[i].name,"KEY%d",i);
        gpio_request(keyinputdev.irqkeydesc[i].gpio,name);
        gpio_direction_input(keyinputdev.irqkeydesc[i].gpio);
        keyinputdev.irqkeydesc[i].irqnum = irq_of_parse_and_map(keyinputdev.nd,i);
    }

    /* 申请中断 */
    keyinputdev.irqkeydesc[0].handler = key0_handler;
    keyinputdev.irqkeydesc[0].value   = KEY_0;

    for (i = 0;i < KEY_NUM; i++)
    {
        ret = request_irq(keyinputdev.irqkeydesc[i].irqnum,keyinputdev.irqkeydesc[i].handler,IRQF_TRIGGER_FALLING|IRQF_TRIGGER_RISING,keyinputdev.irqkeydesc[i].name,&keyinputdev);
        if(ret < 0)
        {
            printk("irq %d request failed!\r\n",keyinputdev.irqkeydesc[i].irqnum);
            return -EFAULT;
        }
    } 

    /* 创建定时器 */
    init_timer(&keyinputdev.timer);
    keyinputdev.timer.function = timer_function;

    /*  申请 input_dev */
    keyinputdev.inputdev = input_allocate_device();
    keyinputdev.inputdev->name = KEYINPUT_NAME;

    keyinputdev.inputdev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP);
    input_set_capability(keyinputdev.inputdev, EV_KEY, KEY_0);

    /*  注册输入设备 */
    ret = input_register_device(keyinputdev.inputdev);
    if(ret)
    {
        printk("register input device failed!\r\n");
        return ret;
    }
    return 0;
}

 /*
 * @description : 驱动入口函数
 * @param : 无
 * @return : 无
 */
static int __init keyinput_init(void)
{
    keyio_init();
    return 0;
}


/*
 * @description : 驱动出口函数
 * @param : 无
 * @return : 无
 */
static void __exit keyinput_exit(void)
{
    unsigned i = 0;
    /* 删除定时器 */
    del_timer_sync(&keyinputdev.timer);

    /* 释放中断 */
    for(i = 0;i < KEY_NUM; i++)
    {
        free_irq(keyinputdev.irqkeydesc[i].irqnum,&keyinputdev);
    }

    /*  释放 input_dev */
    input_unregister_device(keyinputdev.inputdev);
    input_free_device(keyinputdev.inputdev);
}


module_init(keyinput_init);
module_exit(keyinput_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nick");







