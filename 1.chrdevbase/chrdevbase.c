/*
模块编译
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>

#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#define CHRDEVBASE_MAJOR 200            //主设备号
#define CHRDEVBASE_NAME "chrdevbase"    //名字

static char readbuf[100]; //读缓冲
static char writerbuf[100];//写缓冲
static char kerneldata[] = {"kernel data!"};

static int chrdevbase_open(struct inode *inode,struct file *filp)
{
        //printk("chrdevbase_open\r\n") ;
        return  0;   
};

static int chrdevbase_release(struct inode *inode,struct  file *filp)
{
    //printk ("chrdevbase closed!!!\r\n");
    return 0;
}

static ssize_t chrdevbase_read(struct file *filp,__user char *buf,size_t count,loff_t *ppos)
{
    int ret;
    //printk ("chrdevbase readed!!!\r\n");
    memcpy(readbuf,kerneldata,sizeof(kerneldata));
    ret = copy_to_user(buf,readbuf,count);
    if (ret == 0)
    {

    }
    else
    {

    }
    return 0;
}

static ssize_t chrdevbase_write(struct file *file,const char __user *buf,size_t cnt, loff_t *offt)
{
    int ret = 0;
    //printk ("chrdevbase writed!!!\r\n");
    ret = copy_from_user(writerbuf,buf,cnt);
    if(ret == 0)
    {
        printk ("kernel recevdata:%s\r\n",writerbuf);
    }
    else
    {
        printk ("kernel writer failed!!!!!\r\n");/* code */
    }
    
    return 0;
}

static struct file_operations chrdevbase_fops =
{
    .owner = THIS_MODULE,
    .open = chrdevbase_open,
    .release =chrdevbase_release,
    .read = chrdevbase_read,
    .write = chrdevbase_write,
};



static int __init chrdevbase_init(void)
{
    int ret = 0; //注册设备的返回值
    printk("chrdevbase init\r\n");
    /*注册字符设备*/
    ret = register_chrdev(CHRDEVBASE_MAJOR,CHRDEVBASE_NAME,&chrdevbase_fops);
    if (ret < 0)
       {
           printk ("chrdevbase register erro!!!!!");
       } 
    
    return 0;
}

static void __exit chrdevbase_exit(void)
{
    printk("chrdevbase exit\r\n");   
    unregister_chrdev(CHRDEVBASE_MAJOR,CHRDEVBASE_NAME);
}


module_init(chrdevbase_init); /*驱动入口*/
module_exit(chrdevbase_exit); /*驱动出口*/

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nick_cai");
