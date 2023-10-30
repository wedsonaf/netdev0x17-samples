#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/sched/task.h>

static struct task_struct *listener_thread;

static int echo_listener(void *data)
{
	return 0;
}

static int __init echo_init(void)
{
	struct task_struct *t =
		kthread_create(echo_listener, NULL, "listener");
	if (IS_ERR(t))
		return PTR_ERR(t);

	listener_thread = t;
	get_task_struct(t);
	wake_up_process(t);

	return 0;
}

static void __exit echo_exit(void)
{
	kthread_stop(listener_thread);
	put_task_struct(listener_thread);
}

module_init(echo_init);
module_exit(echo_exit);

MODULE_LICENSE("GPL");
