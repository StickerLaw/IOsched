#include <linux/linkage.h>
#include <linux/kernel.h>

extern unsigned long sum_of_services;
extern unsigned long sum_of_waits;
extern unsigned long num_of_requests;

void show_rqcounter(void){
	printk(KERN_ALERT "in show rqcounter\n");
	printk(KERN_ALERT "service %lu\n", sum_of_services);
	printk(KERN_ALERT "wait %lu\n", sum_of_waits);
	printk(KERN_ALERT "num  %lu\n", num_of_requests);
}
