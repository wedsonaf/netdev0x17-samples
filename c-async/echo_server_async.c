#include <linux/module.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/kthread.h>
#include <linux/sched/task.h>
#include <linux/workqueue.h>
#include <net/net_namespace.h>

static struct task_struct *listener_thread;
static LIST_HEAD(connections);
static DEFINE_MUTEX(conn_mutex);

struct connection {
	struct socket *sock;
	bool is_reading;
	char *next_write;
	int pending_write;
	struct work_struct work;
	struct wait_queue_entry wq_entry;
	struct list_head links;
	char buf[512];
};

void cleanup_conn(struct connection *conn)
{
	mutex_lock(&conn_mutex);
	if (conn->links.next == &conn->links) {
		/* Has already been removed. */
		mutex_unlock(&conn_mutex);
		return;
	}
	list_del_init(&conn->links);
	mutex_unlock(&conn_mutex);

	/* Prevent notifications. */
	remove_wait_queue(&conn->sock->wq.wait, &conn->wq_entry);

	/* If there's work pending, cancel it. */
	cancel_work(&conn->work);

	sock_release(conn->sock);
	kfree(conn);
}

void echo_work(struct work_struct *work)
{
	struct connection *conn = container_of(work, struct connection, work);
	struct kvec iov;
	int ret;

	for (;;) {
		struct msghdr msg = {};

		if (conn->is_reading) {
			iov.iov_base = conn->buf;
			iov.iov_len = sizeof(conn->buf);
			ret = kernel_recvmsg(conn->sock, &msg, &iov, 1,
					sizeof(conn->buf), MSG_DONTWAIT);
			if (ret <= 0) {
				if (ret != -EAGAIN)
					cleanup_conn(conn);
				return;
			}
			conn->is_reading = false;
			conn->pending_write = ret;
			conn->next_write = conn->buf;
		} else {
			msg.msg_flags = MSG_DONTWAIT;
			iov.iov_base = conn->next_write;
			iov.iov_len = conn->pending_write;
			ret = kernel_sendmsg(conn->sock, &msg, &iov, 1,
					conn->pending_write);
			if (ret <= 0) {
				if (ret != -EAGAIN)
					cleanup_conn(conn);
				return;
			}

			conn->pending_write -= ret;
			conn->next_write += ret;
			conn->is_reading = conn->pending_write == 0;
		}
	}
}

static int wake_callback(struct wait_queue_entry *entry, unsigned mode,
		int flags, void *key)
{
	struct connection *conn =
		container_of(entry, struct connection, wq_entry);

	/* TODO: Check mask. */
	queue_work(system_wq, &conn->work);

	return 1;
}

static int echo_listener(void *data)
{
	struct socket *sock = data;
	int ret = 0;

	while (!kthread_should_stop()) {
		struct socket *newsock;
		struct connection *conn;

		ret = kernel_accept(sock, &newsock, 0);
		if (ret)
			continue;

		conn = kmalloc(sizeof(*conn), GFP_KERNEL);
		if (!conn) {
			sock_release(newsock);
			continue;
		}

		conn->sock = newsock;
		conn->is_reading = true;

		INIT_WORK(&conn->work, echo_work);
		init_waitqueue_func_entry(&conn->wq_entry, wake_callback);
		add_wait_queue(&conn->sock->wq.wait, &conn->wq_entry);

		mutex_lock(&conn_mutex);
		list_add(&conn->links, &connections);
		mutex_unlock(&conn_mutex);

		/* Initial iteration. */
		queue_work(system_wq, &conn->work);
	}

	sock_release(sock);

	return ret;
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
	addr.sin_port = htons(8081);
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

	mutex_lock(&conn_mutex);
	while (!list_empty(&connections)) {
		struct connection *conn = container_of(connections.next,
				struct connection, links);
		list_del_init(&conn->links);
		mutex_unlock(&conn_mutex);

		/* Prevent notifications. */
		remove_wait_queue(&conn->sock->wq.wait, &conn->wq_entry);

		/* Cancel pending work and wait for inflight ones to finish. */
		cancel_work_sync(&conn->work);
		sock_release(conn->sock);
		kfree(conn);

		mutex_lock(&conn_mutex);
	}
	mutex_unlock(&conn_mutex);
}

module_init(echo_init);
module_exit(echo_exit);

MODULE_LICENSE("GPL");
