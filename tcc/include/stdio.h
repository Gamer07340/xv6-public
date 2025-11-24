#ifndef _STDIO_H
#define _STDIO_H
#include <stdarg.h>
#include <stddef.h>
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef struct FILE FILE;
extern FILE *stdin, *stdout, *stderr;

FILE *fopen(const char *filename, const char *mode);
FILE *fdopen(int fd, const char *mode);
int fclose(FILE *stream);
int fprintf(FILE *stream, const char *format, ...);
int printf(const char *format, ...);
int sprintf(char *str, const char *format, ...);
int snprintf(char *str, int size, const char *format, ...);
int sscanf(const char *str, const char *format, ...);
int vfprintf(FILE *stream, const char *format, va_list ap);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);
int fputc(int c, FILE *stream);
int fputs(const char *s, FILE *stream);
int fgetc(FILE *stream);
int ungetc(int c, FILE *stream);
int fwrite(const void *ptr, int size, int nmemb, FILE *stream);
int fread(void *ptr, int size, int nmemb, FILE *stream);
int fflush(FILE *stream);
int remove(const char *filename);
int ferror(FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
#endif
