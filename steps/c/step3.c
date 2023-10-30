#include <linux/module.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/kthread.h>
#include <linux/sched/task.h>
#include <net/net_namespace.h>

static struct task_struct *listener_thread;

static int echo_listener(void *data)
{
	struct socket *sock = data;
	sock_release(sock);
	return 0;
}

static int __init echo_init(void)
{
	int ret;
	struct socket *sock;
	struct sockaddr_in addr;
	struct task_struct *t;

	ret = sock_create_kern(&init_net, AF_INET, SOCK_STREAM, IPPROTO_TCP,
			&sock);
	if (ret)
		return ret;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(8080);
	addr.sin_addr.s_addr = INADDR_ANY;
	ret = kernel_bind(sock, (struct sockaddr *)&addr, sizeof(addr));
	if (ret)
		goto err_sock;

	ret = kernel_listen(sock, SOMAXCONN);
	if (ret)
		goto err_sock;

	t = kthread_create(echo_listener, sock, "listener");
	if (IS_ERR(t)) {
		ret = PTR_ERR(t);
		goto err_sock;
	}

	listener_thread = t;
	get_task_struct(t);
	wake_up_process(t);

	return 0;

err_sock:
	sock_release(sock);
	return ret;
}

static void __exit echo_exit(void)
{
	kthread_stop(listener_thread);
	put_task_struct(listener_thread);
}

module_init(echo_init);
module_exit(echo_exit);

MODULE_LICENSE("GPL");
