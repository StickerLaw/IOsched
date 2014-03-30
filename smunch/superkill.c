#include<stdio.h>

_syscall1(int, smunch, int, pid, unsigned long, bitpattern);

int main(int argc, char** argv)
{
	int ret;
	unsigned long bitpattern = 0;
	int pid = atoi(argv[1]);
	bitpattern |= 256; // setting only kill bit . BitWise Or operation.
	ret = smunch(pid,bitpattern);
	printf("SMUNCH RETURNED %d \n", ret);
	return 0;
}
