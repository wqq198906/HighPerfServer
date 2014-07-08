/*
 * JEEP: Johan Efficient Event Platform
 *
 * jeep_network.c
 *	
 *  Created on: 2011-9-30
 *      Author: feng jianhua (johan fong)
 *        Mail: jianhuafeng@sohu-inc.com 56683216@qq.com
 *
 *  �޸ļ�¼��
 *  ��1��2011-11-14: �Ż����ģ���֪ͨ���̣�
 *  		��worker����ظ�ѡ��free conn��Ϊserverֱ��ѡ��free worker��
 *  		serverֱ֪ͨ��дһ�����ֽڣ����֪ͨЧ��
 *  ��2��2011-11-26��conn_read�Ż�
 *  		֧�ִ�������������绷��
 */

#include "jeep_network.h"
#include "jeep_log.h"
#include "jeep_akg.h"

static pthread_mutex_t start_lock;//start_worker_numͬ��;cond wait����ͬ��
static pthread_cond_t start_cond;
static uint16 start_worker_num = 0;
static uint64 conn_count = 0;

static void safe_close(int fd) {
	int ret = close(fd);
	while (ret != 0) {
		if (errno != EINTR || errno == EBADF)
			break;
		ret = close(fd);
	}
}

static int open_server_socket(const char *ip, short port, int backlog) {
	int fd = -1;
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		fprintf(stderr, "cannot open server socket, errno: %d %m\n", errno);
		return -1;
	}

	unsigned long non_blocking = 1;
	if (ioctl(fd, FIONBIO, &non_blocking) != 0) {
		fprintf(stderr, "cannot set nonblocking, errno: %d %m\n", errno);
		safe_close(fd);
		return -1;
	}
	int flag_reuseaddr = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*) &flag_reuseaddr, sizeof(flag_reuseaddr));

	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	memset(&addr, 0, addrlen);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = ip == NULL ? INADDR_ANY : inet_addr(ip);
	addr.sin_port = htons(port);

	if (bind(fd, (const struct sockaddr *) &addr, addrlen) != 0) {
		fprintf(stderr, "cannot bind, port: %d errno: %d %m\n", port, errno);
		safe_close(fd);
		return -1;
	}

	if (listen(fd, backlog) != 0) {
		fprintf(stderr, "cannot listen, port: %d errno: %d %m\n", port, errno);
		safe_close(fd);
		return -1;
	}

	return fd;
}

static int accept_client(int fd, struct sockaddr_in *s_in) {
	int cfd = -1;
	do {
		socklen_t len = sizeof(struct sockaddr_in);
		cfd = accept(fd, (struct sockaddr *) s_in, &len);
		if (cfd < 0) {
			if (errno == EINTR)
				continue;
			fprintf(stderr, "cannot accpet, errno: %d %m\n", errno);
			break;
		}
		unsigned long non_blocking = 1;
		if (ioctl(cfd, FIONBIO, &non_blocking) != 0) {
			fprintf(stderr, "cannot set nonblocking, client_fd: %d errno: %d %m\n", cfd, errno);
			safe_close(cfd);
			cfd = -1;
		}
		return cfd;
	} while (-1);
	return -1;
}

static void accept_action(int fd, short event, void *arg) {
	SERVER *s = (SERVER*) arg;
	struct sockaddr_in s_in;
	int cfd = accept_client(fd, &s_in);
	if (cfd == -1) {
		slog_err_t_w(s->qlog, "client accept error, errno: %d %m", errno);
		return;
	}

	int retry = 0;
	do {
		WORKER* w = &s->workers->array[(conn_count++) % s->workers->size];
		CONN *citem;
		//johan: conns is a one-way queue; its out-way is owned by the server and its in-way is owned by one worker.
		GET_FREE_CONN(w->conns, citem);
		if (citem == NULL)
			continue;
		citem->fd = cfd;
		citem->cip = *(uint32 *) &s_in.sin_addr;
		citem->cport = (uint16) s_in.sin_port;
		//notify worker to enable cfd's read and write
		write(w->notifed_wfd, (char *) &citem->ind, 4);

		if (AKG_FUNC[g_akg_connected_id] != NULL)
			AKG_FUNC[g_akg_connected_id](citem);
		return;
	} while (++retry < s->workers->size / 2);//half retry times is enough

	slog_err_t_w(s->qlog, "workers are too busy!");
	safe_close(cfd);
}

static void receive_notify_action(int fd, short event, void *arg) {
	WORKER *w = (WORKER*) arg;
	uint32 ind;
	if (read(fd, &ind, 4) != 4) {
		slog_err_t_w(w->qlog, "notify_pipe read error, errno: %d %m", errno);
		return;
	}
	bufferevent_setfd(w->conns->list[ind].bufev, w->conns->list[ind].fd);
	bufferevent_enable(w->conns->list[ind].bufev, EV_READ | EV_WRITE);
}

static inline void close_conn(CONN *c, uint16 akg_id) {
	c->err_type = akg_id;
	c->in_buf_len = 0;
	if (NULL != AKG_FUNC[akg_id])
		AKG_FUNC[akg_id](c);
	bufferevent_disable(c->bufev, EV_READ | EV_WRITE);
	safe_close(c->fd);
	PUT_FREE_CONN(c->owner->conns, c);
}

void close_err_conn(CONN *c) {
	close_conn(c, g_akg_closed_id);
}

static void on_conn_read(struct bufferevent *be, void *arg) {
	CONN *c = (CONN*) arg;
	while (1) {
		int fsize;
		while (1) {
			fsize = CONN_BUF_LEN - c->in_buf_len;
			if (0 == fsize)
				break;
			int len = bufferevent_read(be, c->in_buf + c->in_buf_len, fsize);
			if (len <= 0)
				break;
			c->in_buf_len += len;
		}

		while (1) {
			if (c->in_buf_len < SIZE_OF_HEAD) {
				//һ��Э���������ͷ��û���꣬�����ѭ�������ߵȴ���һ��libeventʱ�����ѭ����
				if (0 == fsize)
					break;
				else
					return;
			}
			HEAD *h = (HEAD*) c->in_buf;
			if (h->pkglen > MAX_PKG_LEN || h->pkglen < SIZE_OF_HEAD) {
				//��������Ϸ�
				slog_err_t(c->owner->qlog, "pkg error, length: %u pkglen: %u", c->in_buf_len, h->pkglen);
				close_err_conn(c);
				return;
			}
			if (c->in_buf_len < h->pkglen) {
				if (0 == fsize)
					break;
				else
					return;
			}

			//ִ��Э��ָ��
			if (h->akg_id < g_akg_connected_id && AKG_FUNC[h->akg_id]) {
				if(AKG_FUNC[h->akg_id](c)){
					return;
				}
			} else {
				slog_warn_t(c->owner->qlog, "MAX_PKG_TYPE:%d", MAX_PKG_TYPE);
				slog_warn_t(c->owner->qlog, "AKG_FUNC[%d]:%p", h->akg_id, AKG_FUNC[h->akg_id]);
				slog_warn_t(c->owner->qlog, "g_akg_connected_id: %u", g_akg_connected_id);
				slog_warn_t(c->owner->qlog, "invalid akg_id: %u len %u", h->akg_id, h->pkglen);
				close_err_conn(c);
				return;
			}

			//������һ��Э��������߼�����
			c->in_buf_len -= h->pkglen;
			if (c->in_buf_len == 0) {
				if (0 == fsize)
					break;
				else
					return;
			}
			memmove(c->in_buf, c->in_buf + h->pkglen, c->in_buf_len);
		}

	}
}

static void on_conn_err(struct bufferevent *be, short event, void *arg) {
	CONN *c = (CONN*) arg;
	uint16 id = -1;
	if (event & EVBUFFER_TIMEOUT) {
		id = g_akg_timeout_id;
	} else if (event & EVBUFFER_EOF) {
		id = g_akg_closed_id;
	} else if (event & EVBUFFER_ERROR) {
		id = g_akg_error_id;
	}
	close_conn(c, id);
}

static void *start_worker(void *arg) {
	WORKER* w = (WORKER*) arg;
	pthread_mutex_lock(&start_lock);
	++start_worker_num;
	pthread_cond_signal(&start_cond);
	pthread_mutex_unlock(&start_lock);
	event_base_loop(w->base, 0);
	fprintf(stderr, "start_worker error, thread_id: %lu\n", w->thread_id);
	return NULL;
}

SERVER* init_server(int port, uint16 workernum, uint32 connnum, int read_timeout, int write_timeout) {
	LOG_QUEUE *lq = create_log_queue();
	if (lq == NULL)
		return NULL;

	SERVER *s = (SERVER*) malloc(sizeof(SERVER));
	if (s == NULL) {
		fprintf(stderr, "init_server malloc error, errno: %d %m\n", errno);
		return NULL;
	}
	memset(s, 0, sizeof(SERVER));
	s->qlog = lq;
	s->port = port;
	s->base = event_init();
	if (s->base == NULL) {
		fprintf(stderr, "init_server event base error, errno: %m\n");
		return NULL;
	}
	s->workers = init_workers(workernum, connnum, read_timeout, write_timeout);
	if (s->workers == NULL)
		return NULL;
	return s;
}

WORKER_ARRAY* init_workers(uint16 workernum, uint32 connnum, int read_timeout, int write_timeout) {
	WORKER_ARRAY *workers = NULL;
	uint32 len = sizeof(WORKER_ARRAY) + sizeof(WORKER) * workernum;
	workers = (WORKER_ARRAY*) malloc(len);
	if (workers == NULL) {
		fprintf(stderr, "init_workers malloc error, errno: %d %m\n", errno);
		return NULL;
	}
	memset(workers, 0, len);
	workers->size = workernum;
	for (int i = 0; i < workernum; i++) {
		int fds[2];
		if (pipe(fds)) {
			fprintf(stderr, "init_workers pipe error, errno: %d %m\n", errno);
			return NULL;
		}
		WORKER *w = &workers->array[i];
		LOG_QUEUE *lq = create_log_queue();
		if (lq == NULL) {
			fprintf(stderr, "init_workers qlog error, errno: %d %m\n", errno);
			return NULL;
		}
		w->qlog = lq;
		w->ind = i;
		w->notified_rfd = fds[0];
		w->notifed_wfd = fds[1];

		w->base = event_init();
		if (w->base == NULL) {
			fprintf(stderr, "init_workers event base error, errno: %d %m\n", errno);
			return NULL;
		}

		event_set(&w->notify_event, w->notified_rfd, EV_READ | EV_PERSIST, receive_notify_action, w);
		event_base_set(w->base, &w->notify_event);
		if (event_add(&w->notify_event, 0) == -1) {
			fprintf(stderr, "init_workers add event error, errno: %d %m\n", errno);
			return NULL;
		}

		CONN_LIST *lst = init_conn_list(connnum);
		if (lst == NULL) {
			fprintf(stderr, "init_workers conn_list error, errno: %d %m\n", errno);
			event_base_free(w->base);
			free(workers);
			return NULL;
		}
		w->conns = lst;
		CONN *p = lst->head;
		while (p != NULL) {
			p->bufev = bufferevent_new(-1, on_conn_read, NULL, on_conn_err, p);
			bufferevent_base_set(w->base, p->bufev);
			bufferevent_settimeout(p->bufev, read_timeout, write_timeout);
			p->owner = w;
			p = p->next;
		}
	}
	return workers;
}

int start_server(SERVER* server) {
	pthread_mutex_init(&start_lock, NULL);
	pthread_cond_init(&start_cond, NULL);
	if (start_workers(server->workers) == -1) {
		fprintf(stderr, "start workers error, errno: %m\n");
		return -1;
	}
	server->server_fd = open_server_socket(NULL, server->port, 1024);
	if (server->server_fd < 0) {
		fprintf(stderr, "open server socket error, errno: %d %m\n", errno);
		return -1;
	}
	event_set(&server->listen_event, server->server_fd, EV_READ | EV_PERSIST, accept_action, server);
	event_base_set(server->base, &server->listen_event);
	if (event_add(&server->listen_event, 0) == -1) {
		fprintf(stderr, "start server add listen event error, errno: %d %m\n", errno);
		return -1;
	}
	for (int i = 0; i < server->workers->size; i++)
		server->workers->array[i].sfd = server->server_fd;

	fprintf(stdout, "--------------start server ok--------------\n");
	fflush(stdout);
	event_base_loop(server->base, 0);
	fprintf(stderr, "start server loop error, errno: %d %m\n", errno);
	return 0;
}

int start_workers(WORKER_ARRAY* workers) {
	for (int i = 0; i < workers->size; i++) {
		WORKER *w = &workers->array[i];
		if (pthread_create(&w->thread_id, NULL, start_worker, w) != 0) {
			fprintf(stderr, "start_workers create thread error, errno: %d %m\n", errno);
			return -1;
		}
	}
	while (start_worker_num < workers->size) {
		pthread_mutex_lock(&start_lock);
		pthread_cond_wait(&start_cond, &start_lock);
		pthread_mutex_unlock(&start_lock);
	}
	return 0;
}
