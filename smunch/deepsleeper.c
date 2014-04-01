#include <stdio.h>
#include <linux/wait.h>
#include <linux/sched.h>

DECLARE_WAIT_QUEUE_HEAD(con);

void deepsleeper() 
{
	printf("In deep Sleeper \n");
	sleep_on(&con);
	printf("Waking up from Deep sleep\n");
}
