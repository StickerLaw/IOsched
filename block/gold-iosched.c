
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/printk.h>


struct algot_data {
    struct list_head wait_queue;
    struct list_head sort_queue;

    struct request **sorted;// reference array sorted in c-scan order
    sector_t *cost_matrix;  // algot computation matrix

    sector_t rw_head;
    unsigned int nsorted;   // number of requests in sort_queue
    unsigned int opt_s;     // current start index
    unsigned int opt_e;     // current end index
    unsigned int opt_ns;    // current table width for subscription

    int  dirty;             // dirty flag for cost_matrix & sorted
    bool vloc;              // vmalloc flag for cost_matrix
};


static inline void algot_sort_in(struct algot_data *nd, struct request *rq)
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

static void algot_merged_requests(struct request_queue *q, struct request *rq,
                 struct request *next)
{
    void* ref = next->elevator_private[0];
    if (ref != ALGOT_PRI0_UNSORTED)
    {
        struct algot_data* nd = q->elevator->elevator_data;
        nd->nsorted -= 1;

        if (ref != ALGOT_PRI0_SORTED)
        {
            BUG_ON((uintptr_t)ref > ALGOT_CALC_MAX);
            nd->sorted[(uintptr_t)ref] = ALGOT_REF_MERGED;
        }
    }
    list_del_init(&next->queuelist);
}

static void algot_add_request(struct request_queue *q, struct request *rq)
{
    struct algot_data *nd = q->elevator->elevator_data;

    while (!list_empty(&nd->wait_queue) && nd->nsorted < ALGOT_CALC_MAX)
    {
        struct request *req = list_entry_rq(nd->wait_queue.next);
        list_del_init(&req->queuelist);
        algot_sort_in(nd, req);
    }

    if (list_empty(&nd->wait_queue) && nd->nsorted < ALGOT_CALC_MAX)
        algot_sort_in(nd, rq);
    else
    {
        rq->elevator_private[0] = ALGOT_PRI0_UNSORTED;
        list_add_tail(&rq->queuelist, &nd->wait_queue);
    }
}

#define MIN(a,b) (a<b)?a:b
#define MATRIX(matrix, width, i, j)  (matrix[(i)*(width) + (j)])

static inline sector_t
sect_dist(struct request **sorted, unsigned long i, unsigned long j)
{
    if (sorted[j] == NULL)
    {
        printk(KERN_ALERT "%lu %lu\n", i, j);
    }
    return abs(blk_rq_pos(sorted[i])-blk_rq_pos(sorted[j]));
}



static int algot_dispatch(struct request_queue *q, int force)
{
    struct algot_data *nd = q->elevator->elevator_data;

    if (!list_empty(&nd->sort_queue) || !list_empty(&nd->wait_queue)) {
        struct request *rq;

        if (nd->dirty >= ALGOT_DIRTY_COUNT)
            algot_program(q, nd);
        rq = pick_opt(q, nd);

        elv_dispatch_sort(q, rq);
        return 1;
    }
    return 0;
}

static struct request *
algot_former_request(struct request_queue *q, struct request *rq)
{
    struct algot_data *nd = q->elevator->elevator_data;

    if (rq->queuelist.prev == &nd->sort_queue || rq->queuelist.prev == &nd->wait_queue)
        return NULL;
    return list_entry(rq->queuelist.prev, struct request, queuelist);
}

static struct request *
algot_latter_request(struct request_queue *q, struct request *rq)
{
    struct algot_data *nd = q->elevator->elevator_data;

    if (rq->queuelist.next == &nd->sort_queue || rq->queuelist.next == &nd->wait_queue)
        return NULL;
    return list_entry(rq->queuelist.next, struct request, queuelist);
}

static void *algot_init_queue(struct request_queue *q)
{
    struct algot_data *nd;
    unsigned long ms = ALGOT_CALC_MAX;

    nd = kmalloc_node(sizeof(*nd), GFP_KERNEL, q->node);
    nd->nsorted = 0;
    nd->rw_head = 0;
    nd->dirty = ALGOT_DIRTY_COUNT-1;
    INIT_LIST_HEAD(&nd->wait_queue);
    INIT_LIST_HEAD(&nd->sort_queue);

    nd->vloc = false;
    nd->cost_matrix = kmalloc_node(sizeof(sector_t)*ms*ms, GFP_KERNEL, q->node);
    if (nd->cost_matrix == NULL)
    {
        nd->cost_matrix = vmalloc_node(sizeof(sector_t)*ms*ms, q->node);
        nd->vloc = true;
    }
    nd->sorted = kmalloc_node(sizeof(void*)*ms, GFP_KERNEL, q->node);

    if (!nd || !nd->sorted || !nd->cost_matrix)
    {
        printk(KERN_ALERT "failed to allocate memory for algot\n");
        return NULL;
    }

    return nd;
}

static void algot_exit_queue(struct elevator_queue *e)
{
    struct algot_data *nd = e->elevator_data;

    BUG_ON(!list_empty(&nd->sort_queue));
    if (nd->vloc)
        vfree(nd->cost_matrix);
    else
        kfree(nd->cost_matrix);
    kfree(nd->sorted);
    kfree(nd);
}

static struct elevator_type elevator_algot = {
    .ops = {
        .elevator_merge_req_fn        = algot_merged_requests,
        .elevator_dispatch_fn        = algot_dispatch,
        .elevator_add_req_fn        = algot_add_request,
        .elevator_former_req_fn        = algot_former_request,
        .elevator_latter_req_fn        = algot_latter_request,
        .elevator_init_fn        = algot_init_queue,
        .elevator_exit_fn        = algot_exit_queue,
    },
    .elevator_name = "algot",
    .elevator_owner = THIS_MODULE,
};

static int __init algot_init(void)
{
    elv_register(&elevator_algot);

    return 0;
}

static void __exit algot_exit(void)
{
    elv_unregister(&elevator_algot);
}

module_init(algot_init);
module_exit(algot_exit);


MODULE_AUTHOR("Xia Yang, Navin Soni, Neeraj Jain, Yuefeng Zhou, Yingxia Chen");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AlgoT IO scheduler");
