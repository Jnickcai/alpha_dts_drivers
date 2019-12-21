#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
/*
*argc:应用程序参数个数
*argv[] :具体的参数内容 ，字符串形式
*./chrdevbaseAPP <filename> ,<1:2> 1表示读  2表示写
*/

int main(int argc, char *argv[])
{
    int ret = 0;
    int fd = 0;
    char *filename;
    char readbuf[100];
    char writebuf[100];
    static char usrdata[] = {"usr data!!!!!!\r\n"};
    filename = argv[1];

    if(argc !=3 )
    {
        printf ("Error usage!\r\n");
        return -1;
    }

    fd = open(filename,O_RDWR);
    if (atoi(argv[2])== 1)
    {
        if (fd < 0)
        {
            printf("Can't open file s%\r\n",filename);
            return -1;
        }
        /*read 函数*/
        ret = read(fd,readbuf,50);
        if (ret < 0)
        {
            printf("read file %s fail!!!!\r\n",filename);
        }
        else
        {
            printf("APP read data %s\r\n",readbuf);/* code */
        }
     }


    if (atoi(argv[2]) == 2)
    {
        memcpy(writebuf,usrdata,sizeof(usrdata));
        ret = write(fd,writebuf, 50);  /*writ 函数*/
        if (ret < 0)
         {
            printf("wirte file %s fail!!!!\r\n",filename);
         }
        else
         {
            // printf("APP writer succesful !!!!\r\n");
         }
    }
     
    /*close 函数*/
    ret = close(fd);
    if (ret < 0)
    {
        printf("close filee %s\r\n erro!!!!",filename);
    }
    return 0 ;    
}