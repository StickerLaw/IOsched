#include <stdio.h>
#include <errno.h>
#include <sys/syscall.h>
#include <asm/unistd.h>

_syscall0(void, init_rqcounter)

_syscall0(void, show_rqcounter)


int main(void) 
{
	//init_rqcounter();
	show_rqcounter();
return 0;
}
