#include"stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"

/* 定义按键值 */

#define KEYVALUE 0xF0
#define INVAKEY  0x00

int main(int argc, char *argv[])
{
    int fd,ret;
    char *filename;
    unsigned char keyvalue;

    if (argc != 2)
    {
        printf("Error Usage!\r\n");
        return -1;
    }
    filename = argv[1];

    /* 打开 key 驱动 */
    fd = open(filename, O_RDWR);
    if(fd < 0)
    {
        printf("file %s open failed!!\r\n",argv[1]);
        return -1;
    }

    /* 循环读取按键值数据！ */
    while(1)
    {
        read(fd, &keyvalue,sizeof(keyvalue));
        if(keyvalue == KEYVALUE)
        {
            printf("KEY0 press, value = %#X\r\n",keyvalue);  /* 按下 */
        }
    }

    ret = close(fd);
    if(ret < 0)
    {
        printf("file %s close failed!!\r\n",argv[1]);
        return -1;
    }
    return 0;
}