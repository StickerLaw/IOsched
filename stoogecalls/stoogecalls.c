#include <linux/linkage.h>
#include <linux/kernel.h>
#include <linux/pid_namespace.h>
#include <linux/syscalls.h>
#include <linux/sched.h>

int init_sigcounter(int pid)
{
	struct sighand_struct *sighand;
	int i;
	unsigned long flags;
	struct task_struct *p;
	p = pid_task(find_vpid(pid), PIDTYPE_PID);
	lock_task_sighand(p, &flags);	
	sighand=p->sighand;

	for (i = 0; i < 64; i++) {
		
		sighand->sig_counter[i] = 1;
	}
	unlock_task_sighand(p, &flags);
	printk(KERN_ALERT "in init_sigcounter\n");
return 0;
}

int get_sigcounter(int signumber) 
{
	unsigned long flags;
	int result; 
	if (signumber < 0 || signumber > 63) {
		return -1;
	}
	lock_task_sighand(current, &flags);
	struct sighand_struct *sighand = current->sighand;
	result =  sighand->sig_counter[signumber];
	unlock_task_sighand(current, &flags);
	printk(KERN_ALERT "in get_sig()  current = %x, signumber = %d, result = %d\n",
	        current, signumber, result);
	return result;
}
