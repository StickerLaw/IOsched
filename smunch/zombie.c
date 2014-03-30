#include <stdio.h>
#include <stdlib.h>

int main() {

	int pid;
	switch(pid = fork()) {
		case 0:
			printf(" \n This is Child \n");
			exit(0);
			break;
		default:
			printf("\n In Parent .. Running for 60 seconds\n");
			sleep(60);
			break;
		}
return 0;
}
