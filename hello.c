#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>


static int hello_init(void)
{
	printk("hello world\n");	// prints message to the kernel log
	return 0;
}

static void hello_exit(void)
{
	printk("exit world new\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");

