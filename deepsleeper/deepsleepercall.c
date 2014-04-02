#include <stdio.h>
#include <sys/syscall.h>
#include<asm/unistd.h>
#include<sys/errno.h>

_syscall0(void, deepsleeper)

int main()
{
	printf(" \n Calling Deep-Sleepr System Call \n");
	deepsleepr();
	return 0;
}
