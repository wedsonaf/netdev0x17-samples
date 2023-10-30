#include <linux/module.h>

static int __init echo_init(void)
{
	return 0;
}

static void __exit echo_exit(void)
{
}

module_init(echo_init);
module_exit(echo_exit);

MODULE_LICENSE("GPL");
