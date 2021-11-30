#include <linux/kernel.h> 
#include <linux/module.h> 
#include <linux/proc_fs.h>  /* proc fs */
#include <linux/seq_file.h> /* for seq_file */
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/completion.h>

#define PROC_NAME "seq_file_test"
#define dbg(format, args...) printk("[%s]:%d => " format "\n" , __FUNCTION__, __LINE__, ##args)  // ex. dbg("msg %d %d",n1, n2)
#define DBG() printk("[%s]:%d => \n", __FUNCTION__, __LINE__)
const int data_size = 3;
static char data[3] = {"string1", "string2", "string3"};
const unsigned long delay = (10 * HZ / MSEC_PER_SEC); // 10ms

struct context
{
    seq_file *seq;
    struct timer_list t;
    completion done;
} g_context;

static void timer_func(struct timer_list* t)
{
    struct context *ctx = container_of(t, struct context, t);
    unsigned long now = jiffies;
    seq_printf(ctx->seq, "jiffies in timer_func = %ld\n", now);
    complete(&ctx->done);
}

static void init_context(void)
{
     g_context.seq = NULL;
     init_completion(&g_contex.done);
}
/**
* This function is called at the beginning of a sequence.
* ie, when:
* 1.the /proc file is read (first time)
* 2.after the function stop (end of sequence)
*
*/ 
static void *seq_start(struct seq_file *s, loff_t *pos) 
{ 
    DBG();
    if (*pos >= data_size) 
       return NULL;
    
    return (void *)((unsigned long) *pos);
} 
/**
* This function is called after the beginning of a sequence.
* It's called untill the return is NULL (this ends the sequence).
*
*/ 
static void *seq_next(struct seq_file *s, void *v, loff_t *pos) 
{ 
    DBG();
    ++(*pos);
    return seq_start(s, pos);
} 
/**
* This function is called at the end of a sequence
*
*/ 
static void seq_stop(struct seq_file *s, void *v) 
{ 
    DBG();
    /* nothing to do, we use a static value in start() */ 
} 
/**
* This function is called for each "step" of a sequence
*
*/ 
static int seq_show(struct seq_file *s, void *v) 
{ 
    DBG();
    int n = (int)v;
    seq_printf(s, "data[%d]:%s  \n", n, data[n]);
    
    unsigned long now = jiffies;
    seq_printf(s, "jiffies before delay = %ld\n", now);
    schedule_timeout(delay);
    now = jiffies;
    seq_printf(s, "jiffies after delay = %ld\n", now);
    
    if(!g_contex.seq)
    {
        now = jiffies;
        seq_printf(s, "jiffies at timer init = %ld\n", now);
        g_contex.t.expire = now + delay;  // delay 10ms from now
        g_contex.seq = s;
        g_context.t.data = 0;
        timer_setup(&g_context.t, timer_func, 0);
        add_timer(&g_contex.t);
        if (wait_for_completion_interruptible(&g_contex.done))  // wait until timer expires
        {
           seq_printf(s, "completion error! \n");
        }
    }
    return 0;
} 
/**
* This structure gather "function" to manage the sequence
*
*/ 
static struct seq_operations seq_ops = 
{ 
    .start = seq_start,
    .next = seq_next,
    .stop = seq_stop,
    .show = seq_show,
};
/**
* This function is called when the /proc file is open.
*
*/ 
static int open(struct inode *inode, struct file *file) 
{ 
    DBG();
    return seq_open(file, &seq_ops);
};
/**
* This structure gather "function" that manage the /proc file
*
*/ 
static struct file_operations file_ops = 
{ 
    .owner = THIS_MODULE,
    .open = open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = seq_release, 
};
/**
* This function is called when the module is loaded
*
*/ 
int init_module(void) 
{ 
    struct proc_dir_entry *entry;
    entry = create_proc_entry(PROC_NAME, 0, NULL, &file_ops);
    
    init_context();
    return 0;
} 
/**
* This function is called when the module is unloaded.
*
*/ 
void cleanup_module(void) 
{ 
    remove_proc_entry(PROC_NAME, NULL);
} 
MODULE_DESCRIPTION("seqfile_test");
MODULE_LICENSE("GPL");
