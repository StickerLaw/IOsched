/*
 * CPSC822  PROJECT 3
 *
 *     IO Scheduler 
 *
 *      Team GOLD
 *
 */

#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/printk.h>


struct sstf_data {
	struct list_head bigger;
	struct list_head smaller;

	sector_t last_sector;
};

static void sstf_merged_requests(struct request_queue *q, struct request *rq,
				 struct request *next)
{
	list_del_init(&next->queuelist);
}

static void sstf_add_request(struct request_queue *q, struct request *rq)
{
	struct sstf_data *nd = q->elevator->elevator_data;
	struct request *big, *small, *req;
	sector_t pos, pos1, pos2;

	// first case : both the queue are empty
	if (list_empty(&nd->bigger) && list_empty(&nd->smaller)) {
		// if both are empty, insert to the bigger queue.
		list_add_tail(&rq->queuelist, &nd->bigger);
	}
	// second case : the smaller queue is empty.
	else if (!list_empty(&nd->bigger) && list_empty(&nd->smaller)) {

		// get the first element of 'bigger' queue.
		big = list_entry_rq(nd->bigger.next);
		pos = blk_rq_pos(rq);
		pos1 = blk_rq_pos(big);

		// if 'rq' is bigger than the first element of 'bigger' queue
		if ( pos > pos1) {
			// find a right position to insert
			list_for_each_entry(req, &nd->bigger, queuelist) {
				if (blk_rq_pos(req) > pos)
					break;
			}
			list_add_tail(&rq->queuelist, &req->queuelist);
		}
		else {
			// else insert to the 'smaller' queue
			list_add_tail(&rq->queuelist, &nd->smaller);
		}
	}
	// third case : the bigger queue is empty
	else if (list_empty(&nd->bigger) && !list_empty(&nd->smaller)) {

		small = list_entry_rq(nd->smaller.next);

		pos = blk_rq_pos(rq);
		pos2 = blk_rq_pos(small);

		if (pos < pos2) {
			list_for_each_entry(req, &nd->smaller, queuelist) {
				if (blk_rq_pos(req) < pos)
					break;
			}
			list_add_tail(&rq->queuelist, &req->queuelist);
		}
		else {
			list_add_tail(&rq->queuelist, &nd->bigger);
		}
	}
	// fourth case : none of them are empty.
	else {
		big = list_entry_rq(nd->bigger.next);
		small = list_entry_rq(nd->smaller.next);

		pos1 = blk_rq_pos(big);
		pos2 = blk_rq_pos(small);
		pos = blk_rq_pos(rq);

		// sector num is bigger than the 'bigger' queue
		if ( pos > pos1 ) {

			// find a right position to insert
			list_for_each_entry(req, &nd->smaller, queuelist) {
				if (blk_rq_pos(req) > pos)
					break;
			}
			list_add_tail(&rq->queuelist, &req->queuelist);
		}
		else {
			list_for_each_entry(req, &nd->smaller, queuelist) {
				if (blk_rq_pos(req) < pos)
					break;
			}
			list_add_tail(&rq->queuelist, &req->queuelist);
		}
	}
}


static int sstf_dispatch(struct request_queue *q, int force)
{
	struct sstf_data *nd = q->elevator->elevator_data;
	struct request *rq, *rq1, *rq2;
	sector_t delta1, delta2;

	if (list_empty(&nd->bigger) && list_empty(&nd->smaller)) {
		return 0;
	}
	// only the bigger queue is empty.
	else if (!list_empty(&nd->bigger) && list_empty(&nd->smaller)) {
		rq = list_entry_rq(nd->bigger.next);
		list_del_init(&rq->queuelist);
		nd->last_sector = rq_end_sector(rq);

		elv_dispatch_sort(q, rq);
		return 1;
	}
	else if (list_empty(&nd->bigger) && !list_empty(&nd->smaller)) {
		rq = list_entry_rq(nd->smaller.next);
		list_del_init(&rq->queuelist);
		nd->last_sector = rq_end_sector(rq);

		elv_dispatch_sort(q, rq);
		return 1;
	}
	// none of them are empty.
	else {
		// fetch the head of each queue.
		rq1 = list_entry_rq(nd->smaller.next);
		rq2 = list_entry_rq(nd->bigger.next);

		// find the nearest one
		delta1 = abs(nd->last_sector - rq_end_sector(rq1));
		delta2 = abs(nd->last_sector - rq_end_sector(rq2));

		if ( delta1 < delta2 ) {
			list_del_init(&rq1->queuelist);
			nd->last_sector = rq_end_sector(rq1);
			elv_dispatch_sort(q, rq1);
		}
		else {
			list_del_init(&rq2->queuelist);
			nd->last_sector = rq_end_sector(rq2);
			elv_dispatch_sort(q, rq2);
		}
		return 1;
	}
}

static struct request *
sstf_former_request(struct request_queue *q, struct request *rq)
{
	struct sstf_data *nd = q->elevator->elevator_data;

	if (rq->queuelist.prev == &nd->bigger || rq->queuelist.prev == &nd->smaller)
		return NULL;
	return list_entry_rq(rq->queuelist.prev);
}

static struct request *
sstf_latter_request(struct request_queue *q, struct request *rq)
{
	struct sstf_data *nd = q->elevator->elevator_data;

	if (rq->queuelist.next == &nd->bigger || rq->queuelist.next == &nd->smaller)
		return NULL;
	return list_entry_rq(rq->queuelist.next);
}


static void *sstf_init_queue(struct request_queue *q)
{
	struct sstf_data *nd;

	nd = kmalloc_node(sizeof(*nd), GFP_KERNEL, q->node);

	if (!nd) {
		printk(KERN_ALERT "Failed to allocate memory.\n");
		return NULL;
	}

	INIT_LIST_HEAD(&nd->bigger);
	INIT_LIST_HEAD(&nd->smaller);

	nd->last_sector = 0;

	return nd;
}

static void sstf_exit_queue(struct elevator_queue *e)
{
	struct sstf_data *nd = e->elevator_data;

	kfree(nd);
}

static struct elevator_type elevator_sstf = {
	.ops = {
		.elevator_merge_req_fn		= sstf_merged_requests,
		.elevator_dispatch_fn		= sstf_dispatch,
		.elevator_add_req_fn		= sstf_add_request,
		.elevator_former_req_fn		= sstf_former_request,
		.elevator_latter_req_fn		= sstf_latter_request,
		.elevator_init_fn		= sstf_init_queue,
		.elevator_exit_fn		= sstf_exit_queue,
	},
	.elevator_name = "sstf",
	.elevator_owner = THIS_MODULE,
};

static int __init sstf_init(void)
{
	elv_register(&elevator_sstf);

	return 0;
}

static void __exit sstf_exit(void)
{
	elv_unregister(&elevator_sstf);
}

module_init(sstf_init);
module_exit(sstf_exit);


MODULE_AUTHOR("Runzhen Wang, Yaolin Zhang, Guangyan Wang, Shuai Wei, Xi Li");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SSTF IO scheduler");
