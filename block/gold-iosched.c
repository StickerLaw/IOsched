
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/printk.h>


struct gold_data {
	struct list_head bigger;
	struct list_head smaller;

	sector_t last_sector;
};

/*
static inline void gold_sort_in(struct gold_data *nd, struct request *rq)
{
	sector_t pos;
	struct request *req;

	rq->elevator_private[0] = ALGOT_PRI0_SORTED;
	pos = blk_rq_pos(rq);
	list_for_each_entry(req, &nd->sort_queue, queuelist)
	{
		if (blk_rq_pos(req) > pos)
			break;
	}
	list_add_tail(&rq->queuelist, &req->queuelist);
	nd->nsorted += 1;
	nd->dirty += 1;
}

static void gold_merged_requests(struct request_queue *q, struct request *rq,
				 struct request *next)
{
	void* ref = next->elevator_private[0];
	if (ref != ALGOT_PRI0_UNSORTED)
	{
		struct gold_data* nd = q->elevator->elevator_data;
		nd->nsorted -= 1;

		if (ref != ALGOT_PRI0_SORTED)
		{
			BUG_ON((uintptr_t)ref > ALGOT_CALC_MAX);
			nd->sorted[(uintptr_t)ref] = ALGOT_REF_MERGED;
		}
	}
	list_del_init(&next->queuelist);
}
*/
static void gold_add_request(struct request_queue *q, struct request *rq)
{
	struct gold_data *nd = q->elevator->elevator_data;
	struct request *big, *small, *req;
	sector_t pos, pos1, pos2;

	if (list_empty(&nd->bigger) && list_empty(&nd->smaller)) {
		list_add_tail(&rq->queuelist, &nd->bigger);
	}
	else if (!list_empty(&nd->bigger) && list_empty(&nd->smaller)) {

		// get the first element of bigger queue.
		big = list_entry_rq(nd->bigger.next);
		pos = blk_rq_pos(rq);
		pos1 = blk_rq_pos(big);

		// if rq is bigger than the first element of 'bigger' queue
		if ( pos > pos1) {
			list_for_each_entry(req, &nd->bigger, queuelist) {
				if (blk_rq_pos(req) > pos)
					break;
			}
			list_add_tail(&rq->queuelist, &req->queuelist);
		}
		else {
			// else insert to the smaller queue
			list_add_tail(&rq->queuelist, &nd->smaller);
		}
	}
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
	else {
		big = list_entry_rq(nd->bigger.next);
		small = list_entry_rq(nd->smaller.next);

		pos1 = blk_rq_pos(big);
		pos2 = blk_rq_pos(small);
		pos = blk_rq_pos(rq);

		if ( pos > pos1 ) {
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

/*
static int gold_dispatch(struct request_queue *q, int force)
{
	struct gold_data *nd = q->elevator->elevator_data;

	if (!list_empty(&nd->sort_queue) || !list_empty(&nd->wait_queue)) {
		struct request *rq;

		if (nd->dirty >= ALGOT_DIRTY_COUNT)
			gold_program(q, nd);
		rq = pick_opt(q, nd);

		elv_dispatch_sort(q, rq);
		return 1;
	}
	return 0;
}

static struct request *
gold_former_request(struct request_queue *q, struct request *rq)
{
	struct gold_data *nd = q->elevator->elevator_data;

	if (rq->queuelist.prev == &nd->sort_queue || rq->queuelist.prev == &nd->wait_queue)
		return NULL;
	return list_entry(rq->queuelist.prev, struct request, queuelist);
}

static struct request *
gold_latter_request(struct request_queue *q, struct request *rq)
{
	struct gold_data *nd = q->elevator->elevator_data;

	if (rq->queuelist.next == &nd->sort_queue || rq->queuelist.next == &nd->wait_queue)
		return NULL;
	return list_entry(rq->queuelist.next, struct request, queuelist);
}
*/

static void *gold_init_queue(struct request_queue *q)
{
	struct gold_data *nd;

	nd = kmalloc_node(sizeof(*nd), GFP_KERNEL, q->node);

	if (!nd) {
		printk(KERN_ALERT "failed to allocate memory for gold\n");
		return NULL;
	}

	INIT_LIST_HEAD(&nd->bigger);
	INIT_LIST_HEAD(&nd->smaller);

	nd->last_sector = 0;

	return nd;
}

static void gold_exit_queue(struct elevator_queue *e)
{
	struct gold_data *nd = e->elevator_data;

	kfree(nd);
}

static struct elevator_type elevator_gold = {
	.ops = {
	//	.elevator_merge_req_fn		= gold_merged_requests,
//		.elevator_dispatch_fn		= gold_dispatch,
		.elevator_add_req_fn		= gold_add_request,
//		.elevator_former_req_fn		= gold_former_request,
//		.elevator_latter_req_fn		= gold_latter_request,
		.elevator_init_fn		= gold_init_queue,
		.elevator_exit_fn		= gold_exit_queue,
	},
	.elevator_name = "gold",
	.elevator_owner = THIS_MODULE,
};

static int __init gold_init(void)
{
	elv_register(&elevator_gold);

	return 0;
}

static void __exit gold_exit(void)
{
	elv_unregister(&elevator_gold);
}

module_init(gold_init);
module_exit(gold_exit);


MODULE_AUTHOR("Runzhen Wang");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GOLD IO scheduler");
