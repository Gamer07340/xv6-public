#ifndef ANSI_H
#define ANSI_H

// Colors
#define ANSI_BLACK 0
#define ANSI_RED 1
#define ANSI_GREEN 2
#define ANSI_YELLOW 3
#define ANSI_BLUE 4
#define ANSI_MAGENTA 5
#define ANSI_CYAN 6
#define ANSI_WHITE 7

// Function prototypes
void cursor(int x, int y);
void color(int fg, int bg);
void setchar(int x, int y, char c);
void putstr(char *s);
void reset(void);
#endif // ANSI_H
