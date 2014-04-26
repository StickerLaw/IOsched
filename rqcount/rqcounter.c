#include <linux/linkage.h>
#include <linux/kernel.h>

extern unsigned long sum_of_services;
extern unsigned long sum_of_waits;
extern unsigned long num_of_requests;

void init_rqcounter(void){
	printk(KERN_ALERT "in init rqcounter\n");
	sum_of_services = 0;
	sum_of_waits = 0;
	num_of_requests = 0;

}
