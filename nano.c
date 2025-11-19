#include "types.h"
#include "user.h"
#include "ansi.h"
#include "fcntl.h"

#define MAX_BUF 1024
#define WIDTH 80
#define HEIGHT 24

char buffer[MAX_BUF];
int buf_len = 0;
int cursor_pos = 0; // Position in buffer
char *filename = 0;
char status_msg[80] = "";

void
draw_status_bar()
{
  cursor(0, HEIGHT);
  color(ANSI_BLACK, ANSI_WHITE);
  
  // Clear line first
  int i;
  for(i=0; i<WIDTH; i++) putstr(" ");
  
  cursor(0, HEIGHT);
  if(status_msg[0] != 0){
      putstr(status_msg);
  } else {
      putstr(" nano-xv6 | ^X: Exit | ^O: Save | Arrows: Move | File: ");
      if(filename) putstr(filename);
      else putstr("[No Name]");
  }
  
  color(ANSI_WHITE, ANSI_BLACK);
}

void
draw_buffer()
{
  // Clear screen area (rows 0 to HEIGHT-1)
  printf(1, "\x1b[2J");
  
  draw_status_bar();

  cursor(0, 0);
  color(ANSI_WHITE, ANSI_BLACK);
  
  int i;
  int x = 0, y = 0;
  int cursor_x = 0, cursor_y = 0;
  
  for(i=0; i<buf_len; i++){
    if(i == cursor_pos){
      cursor_x = x;
      cursor_y = y;
    }
    
    if(buffer[i] == '\n'){
      x = 0;
      y++;
    } else {
      setchar(x, y, buffer[i]);
      x++;
      if(x >= WIDTH){
        x = 0;
        y++;
      }
    }
  }
  
  // If cursor is at end of buffer
  if(cursor_pos == buf_len){
    cursor_x = x;
    cursor_y = y;
  }
  
  cursor(cursor_x, cursor_y);
}

void
load_file()
{
  int fd;
  
  if(!filename) return;
  
  fd = open(filename, O_RDONLY);
  if(fd < 0) return; // New file
  
  buf_len = read(fd, buffer, MAX_BUF);
  if(buf_len < 0) buf_len = 0;
  cursor_pos = buf_len; // Start at end
  
  close(fd);
}

void
save_file()
{
  int fd;
  
  if(!filename){
      strcpy(status_msg, "Error: No filename provided");
      return;
  }
  
  unlink(filename);
  fd = open(filename, O_CREATE | O_WRONLY);
  if(fd < 0){
      strcpy(status_msg, "Error: Cannot open file for writing");
      return;
  }
  
  if(write(fd, buffer, buf_len) != buf_len){
      strcpy(status_msg, "Error: Write failed");
  } else {
      strcpy(status_msg, "File Saved!");
  }
  
  close(fd);
}

void
move_left()
{
  if(cursor_pos > 0) cursor_pos--;
}

void
move_right()
{
  if(cursor_pos < buf_len) cursor_pos++;
}

void
move_up()
{
  // Find start of current line
  int i = cursor_pos - 1;
  int col = 0;
  while(i >= 0 && buffer[i] != '\n'){
    i--;
    col++;
  }
  
  // Now i is at the newline before current line (or -1)
  if(i < 0) return; // Already on first line
  
  // Find start of previous line
  int prev_line_end = i;
  i--;
  while(i >= 0 && buffer[i] != '\n'){
    i--;
  }
  
  // i is now at newline before previous line (or -1)
  int prev_line_start = i + 1;
  int prev_line_len = prev_line_end - prev_line_start;
  
  // Move to same column in previous line (or end of line if shorter)
  if(col > prev_line_len) col = prev_line_len;
  cursor_pos = prev_line_start + col;
}

void
move_down()
{
  // Find end of current line
  int i = cursor_pos;
  int col = 0;
  
  // Count column position
  int line_start = cursor_pos;
  while(line_start > 0 && buffer[line_start - 1] != '\n'){
    line_start--;
  }
  col = cursor_pos - line_start;
  
  // Find end of current line
  while(i < buf_len && buffer[i] != '\n'){
    i++;
  }
  
  if(i >= buf_len) return; // Already on last line
  
  // i is now at newline, move to start of next line
  i++;
  int next_line_start = i;
  
  // Find end of next line
  int next_line_end = i;
  while(next_line_end < buf_len && buffer[next_line_end] != '\n'){
    next_line_end++;
  }
  
  int next_line_len = next_line_end - next_line_start;
  if(col > next_line_len) col = next_line_len;
  cursor_pos = next_line_start + col;
}

void
insert_char(char c)
{
  if(buf_len >= MAX_BUF - 1) return;
  
  // Shift everything after cursor_pos to the right
  int i;
  for(i = buf_len; i > cursor_pos; i--){
    buffer[i] = buffer[i-1];
  }
  
  buffer[cursor_pos] = c;
  buf_len++;
  cursor_pos++;
}

void
delete_char()
{
  if(cursor_pos == 0) return;
  
  // Shift everything after cursor_pos to the left
  int i;
  for(i = cursor_pos - 1; i < buf_len - 1; i++){
    buffer[i] = buffer[i+1];
  }
  
  buf_len--;
  cursor_pos--;
}

int
main(int argc, char *argv[])
{
  int c;
  
  if(argc >= 2){
      filename = argv[1];
      load_file();
  }
  
  // Enable raw mode
  setconsolemode(1);
  
  // Initial draw
  draw_buffer();
  
  while(1){
    c = 0;
    read(0, &c, 1);
    
    if(c == 24){ // Ctrl-X
      break;
    } else if(c == 15){ // Ctrl-O
      save_file();
      draw_buffer();
    } else if(c == 0xE2){ // KEY_UP
      move_up();
      draw_buffer();
    } else if(c == 0xE3){ // KEY_DN
      move_down();
      draw_buffer();
    } else if(c == 0xE4){ // KEY_LF
      move_left();
      draw_buffer();
    } else if(c == 0xE5){ // KEY_RT
      move_right();
      draw_buffer();
    } else if(c == '\b' || c == 127){ // Backspace
      delete_char();
      status_msg[0] = 0;
      draw_buffer();
    } else if(c == '\r' || c == '\n'){
      insert_char('\n');
      status_msg[0] = 0;
      draw_buffer();
    } else if(c >= 32 && c <= 126){ // Printable
      insert_char(c);
      status_msg[0] = 0;
      draw_buffer();
    }
  }
  
  // Restore cooked mode
  setconsolemode(0);
  
  // Clear screen and exit
  printf(1, "\x1b[2J");
  cursor(0, 0);
  exit();
}
