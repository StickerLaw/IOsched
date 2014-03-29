#include <linux/linkage.h>
#include <linux/kernel.h>
#include <linux/pid_namespace.h>
#include <linux/syscalls.h>
#include <linux/sched.h>

// -1 is fail, 0 is success
int smunch(int pid, unsigned long  bit_pattern)
{
	struct task_struct *tsk;
	unsigned long flags;

	tsk = pid_task(find_vpid(pid), PIDTYPE_PID);

	lock_task_sighand(tsk, &flags);

	if (thread_group_empty(tsk) == 0) {
		printk(KERN_ALERT "This is a multi-threaded process. Exit.\n");
		unlock_task_sighand(tsk, &flags);
		return -1;
	}

	// if the process has EXIT_ZOMBIE or EXIT_DEAD
	if ((tsk->exit_state == EXIT_ZOMBIE) || (tsk->exit_state == EXIT_DEAD)) {
		// and bit_pattern contains SIGKILL
		if (sigismember(bit_pattern, SIGKILL) == 1) {
			printk(KERN_ALERT "Processs has EXIT_ZOMBIE||EXIT_DEAD,
			       and SIGKILL is set!\n");
			unlock_task_sighand(tsk, &flags);
			release_task(tsk);
			return 0;
		}
		//otherwise, smunch ignore the bit_pattern, and return 0
		printk(KERN_ALERT "Zombie, NO SIGKILL,  smunch does nothing.\n");
		unlock_task_sighand(tsk, &flags);
		return 0;
	}
	// if the process doesn't has EXIT_ZOMBIE or EXIT_DEAD,
	// smunch will just put the bit_pattern in the process, 
	tsk->signal->shared_pending.signal.sig[0] = bit_pattern;
	wake_up_process(tsk);
	unlock_task_sighand(tsk, &flags);
	return 0;
}


