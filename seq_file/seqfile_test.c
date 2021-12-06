#include <linux/kernel.h> 
#include <linux/module.h> 
#include <linux/proc_fs.h>  /* proc fs */
#include <linux/seq_file.h> /* for seq_file */
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/completion.h>
#include <linux/version.h>
#include <linux/slab.h>   // kmalloc

#define PROC_NAME "seqfile_test"
#define dbg(format, args...) printk("[%s]:%d => " format "\n" , __FUNCTION__, __LINE__, ##args)  // ex. dbg("msg %d %d",n1, n2)
#define DBG() printk("[%s]:%d => \n", __FUNCTION__, __LINE__)

const int data_size = 3;
const char* data[] = {"string1", "string2", "string3"};
int ret;

/**
* This function is called at the beginning of a sequence.
* ie, when:
* 1.the /proc file is read (first time)
* 2.after the function stop (end of sequence)
*
*/ 
static void *seq_start(struct seq_file *s, loff_t *pos) 
{ 
    dbg("pos=%lld data_size=%d",*pos,data_size);
    
    if (*pos >= data_size) 
       return NULL;
    
    loff_t* spos = kmalloc(sizeof(loff_t), GFP_KERNEL);
    if(!spos)
       return NULL;

    *spos = *pos;
    return spos;
} 
/**
* This function is called after the beginning of a sequence.
* It's called untill the return is NULL (this ends the sequence).
*
*/ 
static void *seq_next(struct seq_file *s, void *v, loff_t *pos) 
{ 
    loff_t* spos = v;
    *pos = ++(*spos);

    dbg("pos=%lld data_size=%d",*pos,data_size);
    if(*pos >= data_size)
        return NULL;
    
    return spos;
} 
/**
* This function is called at the end of a sequence
*
*/ 
static void seq_stop(struct seq_file *s, void *v) 
{ 
    DBG();
    if(v)
      kfree(v);
    /* nothing to do, we use a static value in start() */ 
} 
/**
* This function is called for each "step" of a sequence
*
*/ 
static int seq_show(struct seq_file *s, void *v) 
{ 
    loff_t *spos = v;
    long long n = (long long) *spos;
    dbg("n=%lld", n);
    seq_printf(s, "data[%lld]:%s \n", n, data[n]);
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
    //return single_open(file, seq_show, NULL);
};
/**
* This structure gather "function" that manage the /proc file
*
*/ 
// detect linux version, use proc_ops instead of file_operations in later linux versions
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0) // for linux ver above 5.6.0
static struct proc_ops fops = { 
    .proc_open = open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = seq_release, 
    //.proc_release = single_release, 
};
#else
static struct file_operations fops = { 
    .owner = THIS_MODULE,
    .open = open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = seq_release, 
    //.release = single_release, 
};
#endif
/**
* This function is called when the module is loaded
*
*/ 
int init_seq(void) 
{ 
    ret = 0;
    struct proc_dir_entry *entry;
    entry = proc_create(PROC_NAME, 0, NULL, &fops);
    if(!entry)
        ret = -EFAULT;
    
    return ret;
} 
/**
* This function is called when the module is unloaded.
*
*/ 
void exit_seq(void) 
{ 
    remove_proc_entry(PROC_NAME, NULL);
} 
module_init(init_seq);
module_exit(exit_seq);
MODULE_DESCRIPTION("seqfile_test");
MODULE_LICENSE("GPL");
