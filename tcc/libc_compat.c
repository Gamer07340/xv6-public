typedef unsigned int uint;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <setjmp.h>
#include <time.h>
#include <sys/time.h>

// Declare xv6 specific functions not in standard headers
int uptime(void);
int exec(char*, char**);

// --- stdio.h implementation ---

struct FILE {
    int fd;
    int error;
    int eof;
    int unget_char; // -1 if none
};

FILE _stdin = {0, 0, 0, -1};
FILE _stdout = {1, 0, 0, -1};
FILE _stderr = {2, 0, 0, -1};

FILE *stdin = &_stdin;
FILE *stdout = &_stdout;
FILE *stderr = &_stderr;

FILE *fopen(const char *filename, const char *mode) {
    int fd;
    int flags = 0;
    
    if (strchr(mode, 'w')) {
        flags = O_WRONLY | O_CREAT;
        if (strchr(mode, '+')) flags = O_RDWR | O_CREAT;
    } else {
        flags = O_RDONLY;
        if (strchr(mode, '+')) flags = O_RDWR;
    }

    // Handle 'w' truncation manually if O_TRUNC not supported (xv6 doesn't have it in fcntl.h usually, but let's check)
    // Actually xv6 open doesn't support O_TRUNC. We might need to unlink first?
    if (strchr(mode, 'w')) {
        unlink(filename);
    }

    fd = open(filename, flags);
    if (fd < 0) return NULL;

    FILE *f = (FILE *)malloc(sizeof(FILE));
    f->fd = fd;
    f->error = 0;
    f->eof = 0;
    f->unget_char = -1;
    return f;
}

int fclose(FILE *stream) {
    if (!stream) return -1;
    close(stream->fd);
    if (stream != stdin && stream != stdout && stream != stderr) {
        free(stream);
    }
    return 0;
}

int fputc(int c, FILE *stream) {
    char ch = c;
    if (write(stream->fd, &ch, 1) != 1) {
        stream->error = 1;
        return EOF;
    }
    return c;
}

int fgetc(FILE *stream) {
    unsigned char ch;
    if (stream->unget_char != -1) {
        int c = stream->unget_char;
        stream->unget_char = -1;
        return c;
    }
    int n = read(stream->fd, &ch, 1);
    if (n <= 0) {
        stream->eof = 1;
        return EOF;
    }
    return ch;
}

int ungetc(int c, FILE *stream) {
    if (c == EOF) return EOF;
    stream->unget_char = c;
    return c;
}

int fwrite(const void *ptr, int size, int nmemb, FILE *stream) {
    int total = size * nmemb;
    int n = write(stream->fd, ptr, total);
    if (n < 0) {
        stream->error = 1;
        return 0;
    }
    return n / size;
}

int fread(void *ptr, int size, int nmemb, FILE *stream) {
    int total = size * nmemb;
    // Handle ungetc
    char *p = (char *)ptr;
    int read_count = 0;
    
    if (stream->unget_char != -1 && total > 0) {
        *p++ = stream->unget_char;
        stream->unget_char = -1;
        read_count++;
        total--;
    }

    if (total > 0) {
        int n = read(stream->fd, p, total);
        if (n <= 0) {
            stream->eof = 1;
        } else {
            read_count += n;
        }
    }
    return read_count / size;
}

int fflush(FILE *stream) {
    return 0; // Unbuffered
}

int remove(const char *filename) {
    return unlink(filename);
}

int ferror(FILE *stream) {
    return stream->error;
}

// Simple printf implementation using xv6's printf for stdout/stderr
// For general FILE*, we need a proper implementation.
// But TCC mostly uses printf/fprintf.

// We can reuse xv6's printf logic but adapted for FILE* or buffer.
// xv6 printf is in printf.c. It writes to console (fd 1).
// We need to implement vfprintf.

static void putc_func(int c, void *arg) {
    fputc(c, (FILE *)arg);
}

static void printint(void (*put)(int, void*), void *arg, int xx, int base, int sgn) {
  static char digits[] = "0123456789ABCDEF";
  char buf[16];
  int i, neg;
  uint x;

  neg = 0;
  if(sgn && xx < 0){
    neg = 1;
    x = -xx;
  } else {
    x = xx;
  }

  i = 0;
  do{
    buf[i++] = digits[x % base];
  }while((x /= base) != 0);
  if(neg)
    buf[i++] = '-';

  while(--i >= 0)
    put(buf[i], arg);
}

// Based on xv6 printf.c
int vfprintf(FILE *stream, const char *fmt, va_list ap) {
  char *s;
  int c, i, state;

  state = 0;
  for(i = 0; fmt[i]; i++){
    c = fmt[i] & 0xff;
    if(state == 0){
      if(c == '%'){
        state = '%';
      } else {
        fputc(c, stream);
      }
    } else if(state == '%'){
      if(c == 'd'){
        printint(putc_func, stream, va_arg(ap, int), 10, 1);
      } else if(c == 'x' || c == 'p'){
        printint(putc_func, stream, va_arg(ap, int), 16, 0);
      } else if(c == 's'){
        if((s = va_arg(ap, char*)) == 0)
          s = "(null)";
        for(; *s; s++)
          fputc(*s, stream);
      } else if(c == '%'){
        fputc('%', stream);
      } else if(c == 'c'){ // Added %c support
        fputc(va_arg(ap, int), stream);
      } else if(c == 'l'){ // Ignore 'l' modifier
          continue; // Stay in % state? No, usually %ld. 
          // Simple hack: if next is d/x, handle it.
          // But xv6 printf is very simple. TCC might use %ld.
          // Let's just consume 'l' and look at next char
          // This loop structure makes it hard.
          // Let's just support %ld as %d for now (32-bit)
          state = 'l'; // New state
          continue; 
      }
      state = 0;
    } else if (state == 'l') {
        if (c == 'd') printint(putc_func, stream, va_arg(ap, int), 10, 1);
        else if (c == 'u') printint(putc_func, stream, va_arg(ap, int), 10, 0);
        else if (c == 'x') printint(putc_func, stream, va_arg(ap, int), 16, 0);
        state = 0;
    }
  }
  return 0; // Should return number of chars written
}

int fprintf(FILE *stream, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vfprintf(stream, format, ap);
    va_end(ap);
    return ret;
}

// xv6 printf is void, but we need int.
// We can just call vfprintf(stdout, ...)
#undef printf
int printf(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    // Use &_stdout directly instead of stdout pointer
    int ret = vfprintf(&_stdout, format, ap);
    va_end(ap);
    return ret;
}

struct snprint_buf {
    char *buf;
    int size;
    int pos;
};

static void putc_str(int c, void *arg) {
    struct snprint_buf *b = (struct snprint_buf *)arg;
    if (b->pos < b->size - 1) {
        b->buf[b->pos++] = c;
    }
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap) {
    struct snprint_buf b = {str, size, 0};
    // Re-implement vfprintf logic for string buffer or make vfprintf generic
    // For brevity, duplicating logic or using a generic printer would be better.
    // Let's duplicate for now to avoid complex refactoring of the simple function above.
    
    char *s;
    int c, i, state;

    state = 0;
    for(i = 0; format[i]; i++){
        c = format[i] & 0xff;
        if(state == 0){
            if(c == '%'){
                state = '%';
            } else {
                putc_str(c, &b);
            }
        } else if(state == '%'){
            if(c == 'd'){
                printint(putc_str, &b, va_arg(ap, int), 10, 1);
            } else if(c == 'x' || c == 'p'){
                printint(putc_str, &b, va_arg(ap, int), 16, 0);
            } else if(c == 's'){
                if((s = va_arg(ap, char*)) == 0)
                    s = "(null)";
                for(; *s; s++)
                    putc_str(*s, &b);
            } else if(c == '%'){
                putc_str('%', &b);
            } else if(c == 'c'){
                putc_str(va_arg(ap, int), &b);
            } else if (c == 'l') {
                state = 'l';
                continue;
            }
            state = 0;
        } else if (state == 'l') {
             if (c == 'd') printint(putc_str, &b, va_arg(ap, int), 10, 1);
             else if (c == 'u') printint(putc_str, &b, va_arg(ap, int), 10, 0);
             else if (c == 'x') printint(putc_str, &b, va_arg(ap, int), 16, 0);
             state = 0;
        }
    }
    if (size > 0) b.buf[b.pos] = 0;
    return b.pos;
}

int snprintf(char *str, int size, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(str, size, format, ap);
    va_end(ap);
    return ret;
}

int sprintf(char *str, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(str, 10000, format, ap); // Unsafe but simple
    va_end(ap);
    return ret;
}

// --- stdlib.h implementation ---

// malloc/free are in umalloc.c, linked automatically if we include umalloc.o

void *realloc(void *ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    void *newptr = malloc(size);
    if (!newptr) return NULL;
    // We don't know the size of the old block easily in xv6 malloc.
    // umalloc.c uses a header.
    // Header *hp = (Header*)ptr - 1;
    // uint oldsize = (hp->s.size - 1) * sizeof(Header);
    // This is internal detail of umalloc.c.
    // For safety, we might just copy a fixed amount or try to access the header if we include the struct definition.
    // Let's assume TCC doesn't heavily rely on realloc resizing efficiently, or we can peek at the header.
    // struct header { struct header *ptr; uint size; };
    // typedef struct header Header;
    // Header *hp = (Header*)ptr - 1;
    // uint oldsize = (hp->size - 1) * sizeof(Header);
    // memcpy(newptr, ptr, oldsize < size ? oldsize : size);
    
    // Since we can't easily include umalloc internal structs without copying them, 
    // and we want to avoid fragility, let's just copy a "safe" amount or modify umalloc to expose size.
    // Or just copy 'size' bytes and hope we don't segfault (bad idea).
    // Actually, TCC uses realloc for growing arrays. It's important.
    // Let's define the struct here matching umalloc.c
    
    typedef long Align;
    union header {
      struct {
        union header *ptr;
        uint size;
      } s;
      Align x;
    };
    typedef union header Header;
    
    Header *hp = (Header*)ptr - 1;
    uint oldsize = (hp->s.size - 1) * sizeof(Header);
    
    memcpy(newptr, ptr, oldsize < size ? oldsize : size);
    free(ptr);
    return newptr;
}

void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *p = malloc(total);
    if (p) memset(p, 0, total);
    return p;
}

long strtol(const char *nptr, char **endptr, int base) {
    // Minimal implementation
    long result = 0;
    int sign = 1;
    while (*nptr == ' ' || *nptr == '\t') nptr++;
    if (*nptr == '-') { sign = -1; nptr++; }
    else if (*nptr == '+') { nptr++; }
    
    if (base == 0) {
        if (*nptr == '0') {
            if (nptr[1] == 'x' || nptr[1] == 'X') { base = 16; nptr += 2; }
            else base = 8;
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (nptr[0] == '0' && (nptr[1] == 'x' || nptr[1] == 'X')) nptr += 2;
    }

    while (*nptr) {
        int v;
        if (*nptr >= '0' && *nptr <= '9') v = *nptr - '0';
        else if (*nptr >= 'a' && *nptr <= 'z') v = *nptr - 'a' + 10;
        else if (*nptr >= 'A' && *nptr <= 'Z') v = *nptr - 'A' + 10;
        else break;
        if (v >= base) break;
        result = result * base + v;
        nptr++;
    }
    if (endptr) *endptr = (char *)nptr;
    return result * sign;
}

unsigned long strtoul(const char *nptr, char **endptr, int base) {
    return (unsigned long)strtol(nptr, endptr, base);
}

double strtod(const char *nptr, char **endptr) {
    // Very minimal, TCC might need more
    double res = 0.0;
    double fact = 1.0;
    int sign = 1;
    while (*nptr == ' ' || *nptr == '\t') nptr++;
    if (*nptr == '-') { sign = -1; nptr++; }
    
    while (*nptr >= '0' && *nptr <= '9') {
        res = res * 10.0 + (*nptr - '0');
        nptr++;
    }
    if (*nptr == '.') {
        nptr++;
        while (*nptr >= '0' && *nptr <= '9') {
            fact /= 10.0;
            res += (*nptr - '0') * fact;
            nptr++;
        }
    }
    if (endptr) *endptr = (char *)nptr;
    return res * sign;
}

// --- string.h implementation ---

// memcpy, memset, memmove, strlen, strcpy, strcmp, strchr are in ulib.c
// We need: strcat, strrchr, strdup, strstr, strncmp, strncpy, memcmp

void *memcpy(void *dest, const void *src, size_t n) {
    return memmove(dest, src, n);
}

char *strcat(char *dest, const char *src) {
    char *p = dest + strlen(dest);
    strcpy(p, src);
    return dest;
}

char *strrchr(const char *s, int c) {
    const char *last = NULL;
    do {
        if (*s == c) last = s;
    } while (*s++);
    return (char *)last;
}

char *strdup(const char *s) {
    char *p = malloc(strlen(s) + 1);
    if (p) strcpy(p, s);
    return p;
}

char *strstr(const char *haystack, const char *needle) {
    int n = strlen(needle);
    while (*haystack) {
        if (strncmp(haystack, needle, n) == 0) return (char *)haystack;
        haystack++;
    }
    return NULL;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n > 0 && *s1 && *s2) {
        if (*s1 != *s2) return (*s1 - *s2);
        s1++; s2++; n--;
    }
    if (n == 0) return 0;
    return (*s1 - *s2);
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++)
        dest[i] = src[i];
    for ( ; i < n; i++)
        dest[i] = '\0';
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = s1, *p2 = s2;
    while (n--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++; p2++;
    }
    return 0;
}

// --- setjmp.h implementation ---

int setjmp(jmp_buf env) {
    // Use GCC builtin if available, or inline asm
    // __builtin_setjmp is not standard.
    // Inline asm for x86
    int ret;
    asm volatile(
        "movl %%ebx, 0(%1)\n\t"
        "movl %%esi, 4(%1)\n\t"
        "movl %%edi, 8(%1)\n\t"
        "movl %%ebp, 12(%1)\n\t"
        "movl %%esp, 16(%1)\n\t"
        "movl $1f, 20(%1)\n\t" // eip
        "movl $0, %0\n\t"
        "1:\n\t"
        : "=r"(ret) : "r"(env) : "memory"
    );
    return ret;
}

void longjmp(jmp_buf env, int val) {
    if (val == 0) val = 1;
    asm volatile(
        "movl 0(%1), %%ebx\n\t"
        "movl 4(%1), %%esi\n\t"
        "movl 8(%1), %%edi\n\t"
        "movl 12(%1), %%ebp\n\t"
        "movl 16(%1), %%esp\n\t"
        "movl 20(%1), %%ecx\n\t" // eip
        "movl %0, %%eax\n\t"
        "jmp *%%ecx\n\t"
        : : "r"(val), "r"(env) : "eax", "ecx"
    );
}

// --- sys/time.h ---
int gettimeofday(struct timeval *tv, void *tz) {
    if (tv) {
        tv->tv_sec = uptime() / 100; // uptime is in ticks (10ms?)
        tv->tv_usec = (uptime() % 100) * 10000;
    }
    return 0;
}

// --- time.h ---
time_t time(time_t *t) {
    int u = uptime() / 100; // seconds? uptime is ticks. 100 ticks/sec?
    if (t) *t = u;
    return u;
}

struct tm *localtime(const time_t *timep) {
    static struct tm t;
    // Dummy implementation
    t.tm_sec = *timep % 60;
    t.tm_min = (*timep / 60) % 60;
    t.tm_hour = (*timep / 3600) % 24;
    t.tm_mday = 1;
    t.tm_mon = 0;
    t.tm_year = 70;
    return &t;
}

// --- stdlib.h ---
char *getenv(const char *name) {
    return NULL;
}

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    // Simple bubble sort or insertion sort for now
    char *b = (char *)base;
    if (nmemb < 2) return;
    for (size_t i = 0; i < nmemb - 1; i++) {
        for (size_t j = 0; j < nmemb - i - 1; j++) {
            if (compar(b + j * size, b + (j + 1) * size) > 0) {
                // swap
                char tmp[size];
                memcpy(tmp, b + j * size, size);
                memcpy(b + j * size, b + (j + 1) * size, size);
                memcpy(b + (j + 1) * size, tmp, size);
            }
        }
    }
}

// --- stdio.h ---
int fputs(const char *s, FILE *stream) {
    while (*s) {
        if (fputc(*s++, stream) == EOF) return EOF;
    }
    return 0;
}

FILE *fdopen(int fd, const char *mode) {
    FILE *f = (FILE *)malloc(sizeof(FILE));
    if (!f) return NULL;
    f->fd = fd;
    f->error = 0;
    f->eof = 0;
    f->unget_char = -1;
    return f;
}

// --- unistd.h ---
char *getcwd(char *buf, size_t size) {
    if (size > 1) {
        strcpy(buf, "/");
        return buf;
    }
    return NULL;
}

// --- stdlib.h ---
long long strtoll(const char *nptr, char **endptr, int base) {
    return (long long)strtol(nptr, endptr, base);
}

unsigned long long strtoull(const char *nptr, char **endptr, int base) {
    return (unsigned long long)strtoul(nptr, endptr, base);
}

// --- stdio.h ---
int sscanf(const char *str, const char *format, ...) {
    // Minimal sscanf for version parsing "%d.%d.%d"
    va_list ap;
    va_start(ap, format);
    int count = 0;
    const char *p = str;
    const char *f = format;
    
    while (*f) {
        if (*f == '%') {
            f++;
            if (*f == 'd') {
                int *val = va_arg(ap, int*);
                char *end;
                *val = strtol(p, &end, 10);
                if (end == p) break;
                p = end;
                count++;
            } else {
                // Unsupported
                break;
            }
        } else {
            if (*p == *f) {
                p++;
            } else {
                break;
            }
        }
        f++;
    }
    va_end(ap);
    return count;
}

int fseek(FILE *stream, long offset, int whence) {
    if (lseek(stream->fd, offset, whence) < 0) return -1;
    stream->eof = 0;
    stream->unget_char = -1;
    return 0;
}

long ftell(FILE *stream) {
    return lseek(stream->fd, 0, SEEK_CUR);
}

int execvp(const char *file, char *const argv[]) {
    return exec((char *)file, (char **)argv);
}

// --- errno ---
int errno;
int *__errno_location(void) {
    return &errno;
}

// --- math/float ---
double ldexp(double x, int exp) {
    // Minimal implementation
    if (exp == 0) return x;
    if (exp > 0) {
        while (exp--) x *= 2.0;
    } else {
        while (exp++) x /= 2.0;
    }
    return x;
}

long double strtold(const char *nptr, char **endptr) {
    return (long double)strtod(nptr, endptr);
}

float strtof(const char *nptr, char **endptr) {
    return (float)strtod(nptr, endptr);
}

