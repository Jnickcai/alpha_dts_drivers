
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/fb.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>



#define x0 0
#define y0 272                    //定义全局变量x0,y0:坐标轴中心（x0,y0)
#define RED 	0XFF0000
#define GREEN 	0X00FF00
#define BLUE  	0X0000FF
#define WHITE 	0xFFFFFF 
#define BLACK 	0xFFFFFF 



static int fd_framebuff;
static struct fb_var_screeninfo var;
static unsigned int line_width ,pixel_width;
static int screen_size;
static unsigned char *fb_base;

/* 背景颜色索引 */
unsigned int backcolor[5] = {
BLUE, 		GREEN, 		RED, 	WHITE, 	BLACK
};   

void lcd_put_pixel(int x,int y,unsigned int color)
{
	unsigned char  *pen_8 = fb_base + y*line_width + x*pixel_width;
	unsigned short *pen_16,color_16;
	unsigned int *pen_32;
	
	unsigned int red,green,blue;

	pen_16 = (unsigned short *)pen_8;
	pen_32 = (unsigned int *)pen_8;

	switch (var.bits_per_pixel)
	{
		case 8:
		{
			*pen_8 = color;
			break;
		}
		
		case 16:
		{
			//RGB565
			red   = (color >> 16) & 0xff;
			green = (color >> 8)  & 0xff;
			blue  = (color >>0)  & 0xff;
			color_16 = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);
			*pen_16 = (unsigned short)color;
			break;
		}
		
		case 32:
		{
			*pen_32 = color;
			break;
		}

		default:
		{
			printf("can't surport %dbpp\n", var.bits_per_pixel);
			break;
		}
		
	}
	
}



void Middle_point_draw_circle(int x1, int y1, int r,unsigned int color) 
{
	int d0, x = 0, y = r;//d0是判别式的值
	d0 = 1.25 - r;   //判别式的初始值，1.25可以改为1
	while (x < y) 
	{
		if (d0 >= 0) 
		{
			d0 = d0 + 2 * (x - y) + 5;            //d0一定要先比x,y更新
			x += 1;                //因为d0表达式中的x,y是上一个点
			y -= 1;
			lcd_put_pixel(((x + x1) + x0), 272-(y0 - (y + y1)), color);         //(x,y)
			lcd_put_pixel(((-x + x1) + x0), 272-(y0 - (y + y1)), color);        //(-x,y)
			lcd_put_pixel(((y + x1) + x0), 272-(y0 - (x + y1)), color);         //(y,x)
			lcd_put_pixel(((-y + x1) + x0), 272-(y0 - (x + y1)), color);        //(-y,x)
			lcd_put_pixel(((x + x1) + x0), 272-(y0 - (-y + y1)), color);        //(x,-y)
			lcd_put_pixel(((-x + x1) + x0),272- (y0 - (-y + y1)), color);       //(-x,-y)
			lcd_put_pixel(((y + x1) + x0), 272-(y0 - (-x + y1)), color);        //(y,-y)
			lcd_put_pixel(((-y + x1) + x0), 272-(y0 - (-x + y1)), color);       //(-y,-x)
		}
		else 
		{
			d0 = d0 + 2 * x + 3;
			x += 1;
			y = y;
			lcd_put_pixel(((x + x1) + x0), 272-(y0 - (y + y1)), color);         //(x,y)
			lcd_put_pixel(((-x + x1) + x0),272- (y0 - (y + y1)), color);        //(-x,y)
			lcd_put_pixel(((y + x1) + x0), 272-(y0 - (x + y1)), color);         //(y,x)
			lcd_put_pixel(((-y + x1) + x0), 272-(y0 - (x + y1)), color);        //(-y,x)
			lcd_put_pixel(((x + x1) + x0), 272-(y0 - (-y + y1)), color);        //(x,-y)
			lcd_put_pixel(((-x + x1) + x0), 272-(y0 - (-y + y1)), color);       //(-x,-y)
			lcd_put_pixel(((y + x1) + x0), 272-(y0 - (-x + y1)), color);        //(y,-y)
			lcd_put_pixel(((-y + x1) + x0), 272-(y0 - (-x + y1)), color);       //(-y,-x)
		}
	}
}

void lcd_fill(unsigned    short fx0, unsigned short fy0, 
                 unsigned short x1, unsigned short y1, unsigned int color)
{ 
    unsigned short x, y;

	if(fx0 < 0) fx0 = 0;
	if(fy0 < 0) fy0 = 0;
	
    for(y = fy0; y <= y1; y++)
    {
        for(x = fx0; x <= x1; x++)
			lcd_put_pixel(x, y, color);
    }
}

int main(int argv , char argc)
{	
	int i,j,k;
	fd_framebuff = open("/dev/fb0", O_RDWR);
	
	if(fd_framebuff < 0) //文件打开失败
	{
		printf("can't open /dev/fb0\n");
		return -1;
	}

	if(ioctl(fd_framebuff,FBIOGET_VSCREENINFO, &var)) 		//获取LED屏幕参数
	{
		printf(" can't get fb_var_screeninfo \n");
		return -1;
	}

	line_width  = var.xres * var.bits_per_pixel/8;//LCD x方向的像素点
	pixel_width = var.bits_per_pixel/8;
	screen_size = var.xres* var.yres * var.bits_per_pixel /8 ;
	fb_base 	= (unsigned char *)mmap(NULL,screen_size,PROT_READ|PROT_WRITE,MAP_SHARED,fd_framebuff,0);


	printf("var.xres ==%d，var.yres == %d，var.bits_per_pixel = %d , screen_size == %d \n",var.xres,var.yres ,var.bits_per_pixel,screen_size);
	if(fb_base == (unsigned char *)-1)
	{
		printf(" can't mmp \n");
		return -1;
	}
	//memset(fb_base, 0xFF0000, screen_size);
	for(i=0;i<5;i++)
	{
		lcd_fill(0,0,100,100,backcolor[i]);
		Middle_point_draw_circle(100, 100, 25,backcolor[i]);
		sleep(1);
	}
	munmap(fb_base , screen_size);
	close(fd_framebuff);

	return 0;	

	
}




































