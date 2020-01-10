#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "sys/ioctl.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include <poll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#include <linux/input.h>

/* 定义一个 input_event 变量，存放输入事件信息 */
static struct input_event inputevent;

/*
 * @description : main 主程序
 * @param - argc : argv 数组元素个数
 * @param - argv : 具体参数
 * @return : 0 成功;其他 失败
 */
int main(int argc, char *argv[])
{
    int fd;
    int err = 0;
    char *filename;

    filename = argv[1];
    if(argc != 2)
    {
        printf("ERRO Usage!\r\n");
        return -1;
    }
    
    fd = open(filename, O_RDWR);
    if (fd < 0)
    {
        printf("Can't open file %s\r\n", filename);
        return -1;
    }

    while (1)
    {
        err = read(fd, &inputevent,sizeof(inputevent));
        if (err > 0)
        {
            switch (inputevent.type)
            {
                case EV_KEY:
                    if(inputevent.code < BTN_MISC)
                    {
                        printf("key1 %d %s\r\n",inputevent.code,inputevent.value ? "press" : "release");
                    }
                    else
                    {
                        printf("button %d %s\r\n", inputevent.code,inputevent.value ? "press" : "release");
                    }
                    break;
                case EV_REL:
                    break;
                case EV_ABS:
                    break;
                case EV_MSC:
                    break;
                case EV_SW:
                    break;
            }
        }
        else
        {
            printf("读取数据失败\r\n");
        }
        
    }
    return 0;
}

