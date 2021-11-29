#include <linux/kernel.h> /* We're doing kernel work */
#include <linux/module.h> /* Specifically, a module */
#include <linux/proc_fs.h> /* Necessary because we use proc fs */
#include <linux/seq_file.h> /* for seq_file */
#include <linux/time.h>
#include <linux/timer.h>
＃include <linux/sched.h>

#define PROC_NAME "iter"
#define dbg(fmt,args...) printk("[%s]:%d => "fmt,__FUNCTION__,__LINE__,##args)
#define DBG() printk("[%s]:%d => \n",__FUNCTION__,__LINE__)
const int data_size = 3;
static char data[3] = {"string1", "string2", "string3"};
const unsigned long delay = 3 * HZ;
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
    //unsigned long n;
    DBG();
    int n = (int)v;
    unsigned long now = jiffies;
    seq_printf(s, "jiffies before delay = %ld\n", now);
    schedule_timeout(delay);
    now = jiffies;
    seq_printf(s, "jiffies after delay = %ld\n", now);
    seq_printf(s, "data[%d]:%s  \n", n, data[n]);
    return 0;
} 
/**
* This structure gather "function" to manage the sequence
*
*/ 
static struct seq_operations seq_ops = { 
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
static struct file_operations file_ops = { 
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
