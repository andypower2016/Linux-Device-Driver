#include <linux/init.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/limits.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/completion.h>
#include <linux/param.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/preempt.h>
#include <linux/smp.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/timekeeping.h> 

#define dbg(format, args...) printk("[%s]:%d => " format "\n" , __FUNCTION__, __LINE__, ##args)
/*struct context
{
    struct seq_file *seq;
    struct timer_list tlist;
    struct completion done;
} g_context;

static void timer_func(struct timer_list* t)
{
    struct context *ctx = container_of(t, struct context, tlist);
    unsigned long now = jiffies;
    seq_printf(ctx->seq, "jiffies in timer_func = %ld\n", now);
    complete(&ctx->done);
}

static void init_context(void)
{
     g_context.seq = NULL;
     init_completion(&g_context.done);
}*/


/*
static int show(struct seq_file *m, void *v)  // call-back function of seq_file
{
    
    if () 
    {
      ret = -ERESTARTSYS;
      goto out;
    }
    ret = 0;
out:
    return ret;
}*/

struct timer_list t;
spinlock_t g_spin_lock;

static void timer_func(struct timer_list *t)
{
     spin_lock(&g_spin_lock);
     //do_gettimeofday(&tval);
     //dbg("sec=%lld",tval.tv_sec);
     spin_unlock(&g_spin_lock);
}

static int __init init(void)
{
    spin_lock_init(&g_spin_lock);

    t.expires = 3*HZ;
    timer_setup(&t,timer_func,0);
    add_timer(&t);
    //do_gettimeofday(&tval);
    //dbg("sec=%lld",tval.tv_sec);
    return 0;
}
module_init(init);

static void __exit term(void)
{
    
}
module_exit(term);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("test_timer");
