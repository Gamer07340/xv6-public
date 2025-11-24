#include "syscall.h"

.globl fork
fork:
  movl $1, %eax
  int $64
  ret

.globl exit
exit:
  movl $2, %eax
  int $64
  ret

.globl wait
wait:
  movl $3, %eax
  int $64
  ret

.globl pipe
pipe:
  movl $4, %eax
  int $64
  ret

.globl read
read:
  movl $5, %eax
  int $64
  ret

.globl kill
kill:
  movl $6, %eax
  int $64
  ret

.globl exec
exec:
  movl $7, %eax
  int $64
  ret

.globl fstat
fstat:
  movl $8, %eax
  int $64
  ret

.globl chdir
chdir:
  movl $9, %eax
  int $64
  ret

.globl dup
dup:
  movl $10, %eax
  int $64
  ret

.globl getpid
getpid:
  movl $11, %eax
  int $64
  ret

.globl sbrk
sbrk:
  movl $12, %eax
  int $64
  ret

.globl sleep
sleep:
  movl $13, %eax
  int $64
  ret

.globl uptime
uptime:
  movl $14, %eax
  int $64
  ret

.globl open
open:
  movl $15, %eax
  int $64
  ret

.globl write
write:
  movl $16, %eax
  int $64
  ret

.globl mknod
mknod:
  movl $17, %eax
  int $64
  ret

.globl unlink
unlink:
  movl $18, %eax
  int $64
  ret

.globl link
link:
  movl $19, %eax
  int $64
  ret

.globl mkdir
mkdir:
  movl $20, %eax
  int $64
  ret

.globl close
close:
  movl $21, %eax
  int $64
  ret

.globl crash
crash:
  movl $22, %eax
  int $64
  ret

.globl ps
ps:
  movl $23, %eax
  int $64
  ret

.globl setconsolemode
setconsolemode:
  movl $24, %eax
  int $64
  ret

.globl mount
mount:
  movl $25, %eax
  int $64
  ret

.globl umount
umount:
  movl $26, %eax
  int $64
  ret

.globl lseek
lseek:
  movl $27, %eax
  int $64
  ret
