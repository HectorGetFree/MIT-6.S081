#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

int
main(int argc, char *argv[])
{
  
  // your code here.  you should write the secret to fd 2 using write
  // (e.g., write(2, secret, 8)
  char* begin = sbrk(PGSIZE * 17);
  begin += PGSIZE * 16;
  write(2, begin+32, 8);
  exit(1);
}
