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
#define DTSLED_NAME     "gpiokey"    /* 名字 */
#define KEY0VALUE        0XF0        /* 按键值 */
#define INVAKEY           0x00           /* 无效的按键值 */

/* key 设备结构体 */
struct  key_dev
{
    dev_t  devid;               /*设备号*/
    struct cdev   cdev;         /* cdev */
    struct class  *class;        /* 类 */
    struct device *device;       /*设备 */
    int    major;                /*主设备号 */
    int    minor;                /*次设备号*/
    struct device_node *nd;       /*设备节点*/
    int key_gpio;       /* key 所使用的 GPIO  编号 */
    atomic_t keyvalue; /* 按键值 */
};

struct key_dev keydev;   //key设备


/*
 * @description : 初始化按键 IO，open 函数打开驱动的时候
 * 初始化按键所使用的 GPIO 引脚。
 * @param : 无
 * @return : 无
 */
static int keyio_init(void)
{
    keydev.nd = of_find_node_by_path("/gpiokey");
    if(keydev.nd == NULL)
    {
        return -EINVAL;
    }

    keydev.key_gpio = of_get_named_gpio(keydev.nd, "key-gpio",0);
    if(keydev.key_gpio < 0)
    {
        printk("can't get key\r\n");
        return -EINVAL;
    }
    printk("key_gpio=%d\r\n", keydev.key_gpio);

    /* 初始化 key 所使用的 IO */
    gpio_request(keydev.key_gpio,"key0");   /* 请求 IO */
    gpio_direction_input(keydev.key_gpio); /* 设置为输入 */
    return 0;
}

/*
 * @description : 打开设备
 * @param – inode : 传递给驱动的 inode
 * @param – filp : 设备文件，file 结构体有个叫做 private_data 的成员变量
 * 一般在 open 的时候将 private_data 指向设备结构体。
 * @return : 0 成功;其他 失败
 */
static int key_open(struct inode *inode,struct file *filp)
{
    int ret;
    filp->private_data = &keydev;   //设置私有数据
    
    ret = keyio_init();             /* 初始化按键 IO */
    if (ret < 0){
        return ret;
    }
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
static ssize_t key_read(struct file *filp,char __user *buf,size_t cnt,loff_t *offt)
{
    int ret = 0;
    unsigned char value;
    struct key_dev *dev = filp->private_data;

    if(gpio_get_value(dev->key_gpio) == 0)  /* key0 按下 */
    {
        while(!gpio_get_value (dev->key_gpio))    /* 等待按键释放 */
        {
            atomic_set(&dev->keyvalue,KEY0VALUE);
        }
    }
     else
    {
        atomic_set(&dev->keyvalue,INVAKEY);
    }   

    value = atomic_read(&dev->keyvalue);
    ret = copy_to_user(buf,&value,sizeof(value));

    return ret;
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
 static struct file_operations keydev_fops = {
     .owner     = THIS_MODULE,
     .open      = key_open,
     .read      = key_read,
     .release   = key_release,
 };

 /*
 * @description : 驱动入口函数
 * @param : 无
 * @return : 无
 */
 static int __init my_key_init(void)
 {
 /* 初始化原子变量 */
 atomic_set(&keydev.keyvalue,INVAKEY);


 /* 注册字符设备驱动 */
 /* 1、创建设备号 */
 if(keydev.major)
 {
    keydev.devid = MKDEV(keydev.major,0);
    register_chrdev_region(keydev.devid,DTSLED_CNT,DTSLED_NAME);
 }
 else
 {
    alloc_chrdev_region(&keydev.devid,0,DTSLED_CNT,DTSLED_NAME);
    keydev.major = MAJOR(keydev.devid); /* 获取分配号的主设备号 */
    keydev.minor = MINOR(keydev.devid); /* 获取分配号的次设备号 */
 }
 printk("keydev major=%d,minor=%d\r\n",keydev.major,keydev.minor);

 /* 2、初始化 cdev */
 keydev.cdev.owner = THIS_MODULE;
 cdev_init(&keydev.cdev, &keydev_fops);

 /* 3、添加一个 cdev */
 cdev_add(&keydev.cdev,keydev.devid,DTSLED_CNT);

 /* 4、创建类 */
 keydev.class = class_create(THIS_MODULE, DTSLED_NAME);
 if (IS_ERR(keydev.class))
 {
    return PTR_ERR(keydev.class);
 }

 /* 5、创建设备 */
 keydev.device = device_create(keydev.class, NULL, keydev.devid,NULL, DTSLED_NAME);
 if (IS_ERR(keydev.device)) 
 {
    return PTR_ERR(keydev.device);
 }
    return 0;
}

 /*
 * @description : 驱动出口函数
 * @param : 无
 * @return : 无
 */
 static void __exit key_exit(void)
 {

 /* 注销字符设备驱动 */
    cdev_del(&keydev.cdev);/* 删除 cdev */
    unregister_chrdev_region(keydev.devid, DTSLED_CNT);/*注销设备号*/

    device_destroy(keydev.class, keydev.devid);
    class_destroy(keydev.class);
 }

 module_init(my_key_init);
 module_exit(key_exit);
 MODULE_LICENSE("GPL");
 MODULE_AUTHOR("NICK");