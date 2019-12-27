#include"stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include "linux/ioctl.h"

/* 命令值 */

#define CLOSE_CMD       (_IO(0xEF, 0x1))    /* 关闭定时器 */
#define OPEN_CMD        (_IO(0xEF, 0x2))    /* 打开定时器 */
#define SETPERIOD_CMD   (_IO(0xEF, 0x3))    /* 设置定时器周期命令 */

int main(int argc, char *argv[])
{
    int fd,ret;
    char *filename;
    unsigned int cmd;
    unsigned int arg;
    unsigned char str[100];

    if (argc != 2)
    {
        printf("Error Usage!\r\n");
        return -1;
    }
    filename = argv[1];

    /* 打开 timer 驱动 */
    fd = open(filename, O_RDWR);
    if(fd < 0)
    {
        printf("file %s open failed!!\r\n",argv[1]);
        return -1;
    }

    /* 循环读取按键值数据！ */
    while(1)
    {
        printf("Input CMD:");
        ret = scanf("%d", &cmd);
        if (ret != 1)
        {
            gets(str);
        }
        if (cmd == 1)
            cmd = CLOSE_CMD;
        else if (cmd == 2)
            cmd =OPEN_CMD;
        else if (cmd == 3)
        {
            cmd = SETPERIOD_CMD;    /* 设置周期值 */
            printf("Input Timer Period:");
            ret = scanf("%d",&arg);
            if (ret != 1)
            {
                gets(str);
            }
        }
        ioctl(fd ,cmd, arg);    /* 控制定时器的打开和关闭 */
    }
    ret = close(fd);
    if(ret < 0)
    {
        printf("file %s close failed!!\r\n",argv[1]);
        return -1;
    }
    return 0;
}