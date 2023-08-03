#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint-gcc.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <string.h>

#define export __attribute__((visibility("default")))

struct Color
{
    int a;
    int r;
    int g;
    int b;
} typedef Color;

struct DisplayHandle
{
    int bits_per_pixel;
    int line_length;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    uint8_t *fbp;
    int fbfd;
    long int screensize;
} typedef DisplayHandle;

DisplayHandle *export Open(const char *device)
{
    DisplayHandle *handle = (DisplayHandle *)malloc(sizeof(DisplayHandle));
    handle->fbfd = open(device, O_RDWR);
    if (handle->fbfd == -1)
    {
        printf("Error: cannot open framebuffer device.\n");
        return 0;
    }
    if (ioctl(handle->fbfd, FBIOGET_FSCREENINFO, &handle->vinfo) == -1)
    {
        printf("Error: reading fixed information.\n");
        return 0;
    }
    if (ioctl(handle->fbfd, FBIOGET_VSCREENINFO, &handle->vinfo) == -1)
    {
        printf("Error: reading variable information.\n");
        return 0;
    }

    if (handle->vinfo.nonstd != 0)
    {
        printf("Error: Non std pixformat detected.\n");
        return 0;
    }

    handle->screensize = handle->vinfo.xres * handle->vinfo.yres * handle->vinfo.bits_per_pixel / 8;
    handle->fbp = (char *)mmap(0, handle->screensize, PROT_READ | PROT_WRITE, MAP_SHARED, handle->fbfd, 0);
    if ((int)handle->fbp == -1)
    {
        printf("Error: failed to map framebuffer device to memory.\n");
        return 0;
    }

    // handle->line_length = handle->finfo.line_length;

    // if (handle->line_length == 0)
    //{
    handle->line_length = (handle->vinfo.xres * handle->vinfo.bits_per_pixel) / 8;
    //}
    return handle;
}

void export Clear(DisplayHandle *handle)
{
    memset(handle->fbp, 0, handle->screensize);
}

void export DrawPixel(DisplayHandle *handle, int x, int y, Color color)
{
    uint64_t offset = ((((uint64_t)x + (uint64_t)handle->vinfo.xoffset - 1) * ((uint64_t)handle->vinfo.bits_per_pixel / 8l)) +
                       (((uint64_t)y + (uint64_t)handle->vinfo.yoffset) * (uint64_t)handle->line_length));
    if (offset >= handle->screensize)
    {
        return;
    }
    switch (handle->vinfo.bits_per_pixel)
    {
    case 8:
        *(uint8_t *)(handle->fbp + offset++) = color.r;
        break;
    case 16:
        uint16_t pixel = (color.r << 11) | (color.g << 5) | color.b;
        uint8_t *pixel8 = (uint8_t *)pixel;
        *(uint8_t *)(handle->fbp + offset++) = pixel8[0];
        *(uint8_t *)(handle->fbp + offset++) = pixel8[1];
        *(uint8_t *)(handle->fbp + offset++) = pixel8[2];
        break;
    case 24:
        *(uint8_t *)(handle->fbp + offset++) = color.b;
        *(uint8_t *)(handle->fbp + offset++) = color.g;
        *(uint8_t *)(handle->fbp + offset++) = color.r;
        break;
    case 32:
        *(uint8_t *)(handle->fbp + offset++) = color.b;
        *(uint8_t *)(handle->fbp + offset++) = color.g;
        *(uint8_t *)(handle->fbp + offset++) = color.r;
        *(uint8_t *)(handle->fbp + offset++) = color.a;
        break;
    default:
        fprintf(stderr, "Unsupported pixel format\n");
        break;
    }
}

void export DrawLine(DisplayHandle *handle, int x1, int y1, int x2, int y2, Color color)
{
    int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = (dx > dy ? dx : -dy) / 2, e2;
    while (1)
    {
        DrawPixel(handle, x1, y1, color);
        if (x1 == x2 && y1 == y2)
        {
            break;
        }
        e2 = err;
        if (e2 > -dx)
        {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dy)
        {
            err += dx;
            y1 += sy;
        }
    }
}

void export DrawRectangle(DisplayHandle *handle, int x1, int y1, int x2, int y2, Color color, int fill)
{
    if (fill)
    {
        int i, j;
        int bytes_per_pixel = handle->bits_per_pixel / 8;
        int bytes_per_row = (x2 - x1 + 1) * bytes_per_pixel;
        uint8_t *dst = handle->fbp + y1 * handle->line_length + x1 * bytes_per_pixel;
        for (i = y1; i <= y2; i++)
        {
            for (j = x1; j <= x2; j++)
            {
                DrawPixel(handle, j, i, color);
            }
            dst += handle->line_length;
        }
    }
    else
    {
        DrawLine(handle, x1, y1, x2, y1, color);
        DrawLine(handle, x2, y1, x2, y2, color);
        DrawLine(handle, x2, y2, x1, y2, color);
        DrawLine(handle, x1, y2, x1, y1, color);
    }
}

uint64_t Offset(DisplayHandle *handle, int x, int y)
{
    return ((((uint64_t)x + (uint64_t)handle->vinfo.xoffset - 1) * ((uint64_t)handle->vinfo.bits_per_pixel / 8l)) +
            (((uint64_t)y + (uint64_t)handle->vinfo.yoffset) * (uint64_t)handle->line_length));
}

void export DrawBitmap(DisplayHandle *handle, int x1, int y1, int x2, int y2, uint8_t *bitmap)
{
    for (int y = y1; y <= y2; y++)
    {
        uint64_t start = Offset(handle, x1, y);
        uint64_t end = Offset(handle, x2, y);
        if(end > handle->screensize)
        {
            continue;
        }
        memcpy(handle->fbp + start, bitmap + start, end - start);
    }
    // memcpy(handle->fbp, bitmap, handle->screensize);
}

uint32_t export GetBitsPerPixel(DisplayHandle *handle)
{
    return handle->vinfo.bits_per_pixel;
}

uint32_t export GetWidth(DisplayHandle *handle)
{
    return handle->vinfo.xres;
}

uint32_t export GetHeight(DisplayHandle *handle)
{
    return handle->vinfo.yres;
}

int main()
{
    int ii;
    DisplayHandle *d = Open("/dev/fb0");
    printf("width: %d\n", GetWidth(d));
    printf("height: %d\n", GetHeight(d));
    printf("bits per pixel: %d\n", GetBitsPerPixel(d));
    printf("len: %d\n", d->screensize);
    scanf("%d", &ii);
    Clear(d);
    DrawRectangle(d, 0, 0, 1280, 720, (Color){0, 255, 0, 0}, 1);
    DrawRectangle(d, 50, 50, 1280 - 50, 720 - 50, (Color){0, 0, 255, 0}, 1);
    DrawRectangle(d, 100, 100, 1280 - 100, 720 - 100, (Color){0, 0, 0, 255}, 1);
}