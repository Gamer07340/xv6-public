#include "types.h"
#include "user.h"
#include "fcntl.h"

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 200
#define VGA_BASE 0xA0000

uchar *vga_mem;

void
draw_pixel(int x, int y, int color)
{
  if(x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT)
    vga_mem[y * SCREEN_WIDTH + x] = color;
}

void
draw_rect(int x, int y, int w, int h, int color)
{
  int i, j;
  for(j = 0; j < h; j++)
    for(i = 0; i < w; i++)
      draw_pixel(x + i, y + j, color);
}

void
draw_cursor(int x, int y)
{
  draw_pixel(x, y, 15);
  draw_pixel(x+1, y, 15);
  draw_pixel(x, y+1, 15);
  draw_pixel(x+1, y+1, 15);
}

int
main(int argc, char *argv[])
{
  int fd;
  char buf[3];
  int mx = SCREEN_WIDTH / 2;
  int my = SCREEN_HEIGHT / 2;

  printf(1, "Starting GUI Server...\n");

  if(setvideomode(0x13) < 0){
    printf(1, "Failed to set video mode\n");
    exit();
  }

  // Map VGA memory to a fixed address in user space
  vga_mem = (uchar*)0x60000000;
  if(mapvga((int)vga_mem) < 0){
    printf(1, "Failed to map VGA memory\n");
    exit();
  }

  fd = open("/dev/mouse", O_RDONLY);
  if(fd < 0){
    printf(1, "Failed to open mouse\n");
    // exit();
  }

  // Clear screen (blue background)
  draw_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 1);

  // Draw a window
  draw_rect(50, 50, 100, 80, 7); // Light gray
  draw_rect(52, 52, 96, 10, 9); // Title bar (blue)

  uchar saved_pixels[4];
  int saved_x = -1, saved_y = -1;

  // Initial draw
  if(mx >= 0 && mx < SCREEN_WIDTH - 1 && my >= 0 && my < SCREEN_HEIGHT - 1){
      saved_x = mx;
      saved_y = my;
      saved_pixels[0] = vga_mem[my * SCREEN_WIDTH + mx];
      saved_pixels[1] = vga_mem[my * SCREEN_WIDTH + mx + 1];
      saved_pixels[2] = vga_mem[(my + 1) * SCREEN_WIDTH + mx];
      saved_pixels[3] = vga_mem[(my + 1) * SCREEN_WIDTH + mx + 1];
      draw_cursor(mx, my);
  }

  while(1){
    if(fd >= 0){
      int n = read(fd, buf, 3);
      if(n == 3){
        int dx = buf[1];
        int dy = buf[2];
        
        if(buf[0] & 0x10) dx |= 0xFFFFFF00;
        if(buf[0] & 0x20) dy |= 0xFFFFFF00;
        
        dy = -dy;

        // Restore old background
        if(saved_x != -1){
            vga_mem[saved_y * SCREEN_WIDTH + saved_x] = saved_pixels[0];
            vga_mem[saved_y * SCREEN_WIDTH + saved_x + 1] = saved_pixels[1];
            vga_mem[(saved_y + 1) * SCREEN_WIDTH + saved_x] = saved_pixels[2];
            vga_mem[(saved_y + 1) * SCREEN_WIDTH + saved_x + 1] = saved_pixels[3];
        }

        mx += dx;
        my += dy;

        if(mx < 0) mx = 0;
        if(mx >= SCREEN_WIDTH - 1) mx = SCREEN_WIDTH - 2;
        if(my < 0) my = 0;
        if(my >= SCREEN_HEIGHT - 1) my = SCREEN_HEIGHT - 2;

        // Save new background
        saved_x = mx;
        saved_y = my;
        saved_pixels[0] = vga_mem[my * SCREEN_WIDTH + mx];
        saved_pixels[1] = vga_mem[my * SCREEN_WIDTH + mx + 1];
        saved_pixels[2] = vga_mem[(my + 1) * SCREEN_WIDTH + mx];
        saved_pixels[3] = vga_mem[(my + 1) * SCREEN_WIDTH + mx + 1];

        draw_cursor(mx, my);
        
        if(buf[0] & 1){ // Left click
             draw_rect(mx, my, 10, 10, 4); // Draw red box
             // Update saved pixels because we just drew over them
             saved_pixels[0] = vga_mem[my * SCREEN_WIDTH + mx];
             saved_pixels[1] = vga_mem[my * SCREEN_WIDTH + mx + 1];
             saved_pixels[2] = vga_mem[(my + 1) * SCREEN_WIDTH + mx];
             saved_pixels[3] = vga_mem[(my + 1) * SCREEN_WIDTH + mx + 1];
             draw_cursor(mx, my); // Redraw cursor on top
        }
      }
    }
    // sleep(1);
  }

  exit();
}
