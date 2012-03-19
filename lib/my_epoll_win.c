/*
 * my_epoll_win.c
 * wrapper to get epoll AND poll on windows systems
 *
 * Copyright (c) 2010-2012 Jan Seiffert
 *
 * This file is part of g2cd.
 *
 * g2cd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3
 * of the License, or (at your option) any later version.
 *
 * g2cd is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with g2cd.
 * If not, see <http://www.gnu.org/licenses/>.
 *
 * $Id: $
 */

#include <windows.h>
#include <ws2tcpip.h>
#include <stdarg.h>

/*
 * this is all just a fluke...
 * here is nothing really working
 * This is just a sketch and some hooking.
 * g2cd starts up with this code, but does not work.
 *
 * This needs some _heavy_ work to get this going.
 *
 * The whole trick is to add another layer:
 * Every fd has a send-/recv buffer in userspace, IOCP
 * works with them. As soon as the async IOCP has done
 * something to these buffer, we signal the "syncronous"
 * poll like API. For this "all" operations on the fd
 * (maybe we do not want to give out the fd, but a pointer
 * hidden in an int, 64 bit? uaaarrgh...) have to be hooked
 * and redirected to work on the userspace buffer and kick
 * the IOCP in the background.
 *
 * scetch write:
 * if private buffer not full
 *  -> signal write ready
 * my_epoll_send
 *  -> copy data into private buffer, kick IOCP
 *  a) buffer full -> not write ready
 *  b) buffer not full -> signal write ready
 * IOCP returns
 *  -> since buffer not full again signal write ready
 *
 * scetch read:
 * if read ready signal
 *  -> IOCP read to private buffer
 * when IOCP arives
 *  -> signal read ready
 * my_epoll_read
 *  -> copy data from private buffer, kick next IOCP
 *
 *
 * or something like this...
 */

/*
 * Compat type foo
 */
	/* MinGW header do not define this */
typedef int  (PASCAL *poll_ptr_t)(my_pollfd *, unsigned long, int);
typedef BOOL (WINAPI *accept_ex_ptr_t)(SOCKET, SOCKET, PVOID, DWORD, DWORD, DWORD, LPDWORD, LPOVERLAPPED);
typedef void (WINAPI *get_accept_ex_sockaddr_ptr_t)(PVOID, DWORD, DWORD, DWORD, LPSOCKADDR *, LPINT, LPSOCKADDR *, LPINT);
typedef int  (PASCAL *recv_msg_ptr_t)(SOCKET,LPWSAMSG,LPDWORD,LPWSAOVERLAPPED,LPWSAOVERLAPPED_COMPLETION_ROUTINE);
typedef BOOL (WINAPI *connect_ex_ptr_t)(SOCKET, const struct sockaddr *, int, PVOID, DWORD, LPDWORD, LPOVERLAPPED);
#ifndef WSAID_ACCEPTEX
# define WSAID_ACCEPTEX {0xb5367df1,0xcbac,0x11cf,{0x95,0xca,0x00,0x80,0x5f,0x48,0xa1,0x92}}
#endif
#ifndef WSAID_GETACCEPTEXSOCKADDRS
# define WSAID_GETACCEPTEXSOCKADDRS {0xb5367df2,0xcbac,0x11cf,{0x95,0xca,0x00,0x80,0x5f,0x48,0xa1,0x92}}
#endif
#ifndef WSAID_CONNECTEX
# define WSAID_CONNECTEX {0x25a207b9,0xddf3,0x4660,{0x8e,0xe9,0x76,0xe5,0x8c,0x74,0x06,0x3e}}
#endif
#ifndef WSAID_WSARECVMSG
# define WSAID_WSARECVMSG {0xf689d7c8,0x6f1f,0x436b,{0x8a,0x53,0xe5,0x4f,0xe3,0x51,0xc3,0x22}}
#endif

#if defined(HAVE_IP6_PKTINFO)
# define STRUCT_SIZE (sizeof(struct in6_pktinfo))
#elif defined(HAVE_IP_PKTINFO)
# define STRUCT_SIZE (sizeof(struct in_pktinfo))
#elif HAVE_DECL_IP_RECVDSTADDR == 1
# ifdef HAVE_IPV6
#  define STRUCT_SIZE (sizeof(struct in6_addr))
# else
#  define STRUCT_SIZE (sizeof(struct in_addr))
# endif
#else
# define STRUCT_SIZE (256)
#endif

/* broken mingw header... */
#ifdef WSA_CMSG_SPACE
# define CMSG_SPACE(x) WSA_CMSG_SPACE(x)
#endif
#ifndef CMSG_SPACE
# define __CMSG_ALIGN(p) (((unsigned)(p) + sizeof(int) - 1) & (~(sizeof(int) - 1)))
# define CMSG_SPACE(len) (__CMSG_ALIGN(sizeof(my_cmsghdr)) + __CMSG_ALIGN(len))
#endif

/*
 *
 */
enum sock_type
{
	ST_TCP,
	ST_ACCEPT,
	ST_UDP,
} GCC_ATTR_PACKED;

struct win_buff
{
	size_t pos;
	size_t m_pos;
	size_t limit;
	size_t capacity;
	char data[4096];
};

struct some_fd_data
{
	SOCKET fd;
	OVERLAPPED r_o, w_o;
	WSABUF r_b, w_b;
	WSAMSG r_m, w_m;
	CRITICAL_SECTION lock;
	struct epoll_event e;
	union
	{
		struct
		{
			bool oneshot:1;
			bool added:1;
		} flags;
		int all;
	} u;
	enum sock_type st;
	struct sockaddr addr;
	struct win_buff r_buf, w_buf;
	char cbuf[CMSG_SPACE(STRUCT_SIZE)] GCC_ATTR_ALIGNED(sizeof(size_t));
};

/*
 * Protos
 */
	/* you better not delete this proto, or it won't work */
static int vfcntl(int fd, int cmd, va_list args);
static void my_epoll_init(void) GCC_ATTR_CONSTRUCT;
static int PASCAL poll_emul(my_pollfd *, unsigned long, int);

/*
 * Vars
 */
static bool extra_funcs_ready;
static poll_ptr_t poll_ptr = poll_emul;
static accept_ex_ptr_t accept_ex_ptr;
static get_accept_ex_sockaddr_ptr_t get_accept_ex_sockaddr_ptr;
static recv_msg_ptr_t recv_msg_ptr;

/*
 * Funcs
 *
 * the init stuff
 */
static void my_epoll_init(void)
{
	poll_ptr_t pp;

	pp = (poll_ptr_t) GetProcAddress(GetModuleHandle(TEXT("ws2_32")), "WSAPoll");
	if(pp)
		poll_ptr = pp;
}

static bool get_extra_funcs(void)
{
	static const GUID aeg = WSAID_ACCEPTEX;
	static const GUID gaesg = WSAID_GETACCEPTEXSOCKADDRS;
	static const GUID rmg = WSAID_WSARECVMSG;
	DWORD ret_bytes;
	SOCKET s;
	accept_ex_ptr_t aep;
	get_accept_ex_sockaddr_ptr_t gaesp;
	recv_msg_ptr_t rmp;
	int res;
	bool ret_val = false;

	if(extra_funcs_ready)
		return true;

	s = socket(AF_INET, SOCK_STREAM, 0);
	if(INVALID_SOCKET == s)
		return false;

	res = WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, (void *)(intptr_t)&rmg,
	               sizeof(rmg), &rmp, sizeof(rmp),
	               &ret_bytes, NULL, NULL);
	if(SOCKET_ERROR == res) {
		errno = s_errno;
		goto out;
	}
	recv_msg_ptr = rmp;
	res = WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, (void *)(intptr_t)&aeg,
	               sizeof(aeg), &aep, sizeof(aep),
	               &ret_bytes, NULL, NULL);
	if(SOCKET_ERROR == res) {
		errno = s_errno;
		goto out;
	}
	accept_ex_ptr = aep;
	res = WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, (void *)(intptr_t)&gaesg,
	               sizeof(gaesg), &gaesp, sizeof(gaesp),
	               &ret_bytes, NULL, NULL);
	if(SOCKET_ERROR == res) {
		errno = s_errno;
		goto out;
	}
	get_accept_ex_sockaddr_ptr = gaesp;
	extra_funcs_ready = ret_val = true;
out:
	closesocket(s);
	return ret_val;
}

/*
 * Some first BS code to get it going
 */

static int start_read_tcp(struct some_fd_data *d)
{
	int res;
	DWORD flags = 0;

	d->r_b.buf = buffer_start(d->r_buf);
	d->r_b.len = buffer_remaining(d->r_buf);
	/*
	 * init r_b,
	 * (re)init r_o
	 */
	res = WSARecv(d->fd, &d->r_b, 1, NULL, &flags, &d->r_o, NULL /* comp routine?? */);
	if(SOCKET_ERROR == res)
	{
		if(WSA_IO_PENDING == s_errno)
			return 0;
		return -1;
	}
	/* did we received bytes now? we said overlapped, so no result bytes... */
	return 0;
}

static int start_read_accept(struct some_fd_data *d)
{
	SOCKET newsocket;
	DWORD res_byte;
	BOOL res;
	/*
	 * init newsocket
	 * init r_buf,
	 * (re)init r_o
	 */
	res = accept_ex_ptr(d->fd, newsocket, buffer_start(d->r_buf), 0, sizeof(union combo_addr) + 16, sizeof(union combo_addr) + 16, &res_byte, &d->r_o);
	if(!res)
	{
		if(ERROR_IO_PENDING == s_errno)
			return 0;
		if(WSAECONNRESET == s_errno)
			return 0; /* try again */
		return -1;
	}
	/*
	 * shit, we received a connection,
	 * look at res_byte
	 */
	return 0;
}

static int start_read_udp(struct some_fd_data *d)
{
	int res;

	d->r_m.lpBuffers = &d->r_b;
	d->r_b.buf = buffer_start(d->r_buf);
	d->r_b.len = buffer_remaining(d->r_buf);
	d->r_m.dwBufferCount = 1;
	d->r_m.name = &d->addr;
	d->r_m.namelen = sizeof(d->addr);
	d->r_m.Control.buf = d->cbuf;
	d->r_m.Control.len = sizeof(d->cbuf);
	/* these flags are only OK because we never use them... */
	d->r_m.dwFlags = 0;
	/*
	 * init r_buf,
	 * (re)init r_o
	 */
	res = recv_msg_ptr(d->fd, &d->r_m, NULL, &d->r_o, NULL /* comp routine? */);
	if(SOCKET_ERROR == res)
	{
		if(WSA_IO_PENDING == s_errno)
			return 0;
		return -1;
	}
	/* did we received bytes now? we said overlapped, so no result bytes... */
	return 0;
}

static int start_read(struct some_fd_data *d)
{
	switch(d->st)
	{
	case ST_TCP:
		return start_read_tcp(d);
	case ST_UDP:
		return start_read_udp(d);
	case ST_ACCEPT:
		return start_read_accept(d);
	}
	return -1;
}

static int start_write_tcp(struct some_fd_data *d)
{
	/*
	 * If space in buffer -> mark ready
	 * Check if a send is underway
	 *  -> if not and buffer not empty -> start
	 */
	return 0;
}

static int start_write_accept(struct some_fd_data *d GCC_ATTR_UNUSED_PARAM)
{
	/* do nothing, we are not writing on an accept socket */
	return 0;
}

static int start_write_udp(struct some_fd_data *d)
{
	/* always signal ready... */
	return 0;
}

static int start_write(struct some_fd_data *d)
{
	switch(d->st)
	{
	case ST_TCP:
		return start_write_tcp(d);
	case ST_UDP:
		return start_write_udp(d);
	case ST_ACCEPT:
		return start_write_accept(d);
	}
	return -1;
}

int my_epoll_ctl(some_fd epfd, int op, some_fd fd, struct epoll_event *event)
{
	struct some_fd_data *d = (struct some_fd_data *)fd;
	HANDLE ep = (HANDLE)epfd, res;
	uint32_t nmask, omask;
	int ret_val = 0, t;

	if(!d || INVALID_SOCKET == (SOCKET)fd) {
		errno = EINVAL;
		return -1;
	}

	if(INVALID_HANDLE_VALUE == ep) {
		errno = EBADF;
		return -1;
	}

	if(!event) {
		errno = EFAULT;
		return -1;
	}

	/* check for sane op before accuaring the lock */
	if(epfd == fd ||
	   !(EPOLL_CTL_ADD == op || EPOLL_CTL_MOD == op || EPOLL_CTL_DEL == op)) {
		errno = EINVAL;
		return -1;
	}

	if((EPOLL_CTL_ADD == op || EPOLL_CTL_MOD == op) && event->events & EPOLLET) {
		/* we cannot emulate EPOLLET */
		logg_develd("edge triggered request: efd: %#x fd: %i e: %#x p: %p\n",
		            epfd, fd, event->events, event->data.ptr);
		errno = EINVAL;
		return -1;
	}

	EnterCriticalSection(&d->lock);
	switch(op)
	{
	case EPOLL_CTL_ADD:
		d->u.flags.oneshot = !!(event->events & EPOLLONESHOT);
		if(event->events & EPOLLIN)
		{
			t = start_read(d);
			if(t) {
				errno = s_errno;
				ret_val = -1;
				break;
			}
		}
		if(event->events & EPOLLOUT)
		{
			t = start_write(d);
			if(t) {
				errno = s_errno;
				ret_val = -1;
				break;
			}
		}
		d->e = *event;
		res = CreateIoCompletionPort((HANDLE)d->fd, ep, fd, 1);
		if(!res) {
			errno = GetLastError();
			ret_val = -1;
			break;
		}
		d->u.flags.added = true;
	case EPOLL_CTL_MOD:
		d->u.flags.oneshot = !!(event->events & EPOLLONESHOT);
		nmask = (d->e.events ^ event->events) & event->events;
		omask = (d->e.events ^ event->events) & d->e.events;
		if(nmask & EPOLLIN)
		{
			t = start_read(d);
			if(t) {
				errno = s_errno;
				ret_val = -1;
				break;
			}
		}
		else if(omask & EPOLLIN)
		{
			/* stop read */;
		}
		if(nmask & EPOLLOUT)
		{
			t = start_write(d);
			if(t) {
				errno = s_errno;
				ret_val = -1;
				break;
			}
		}
		else if(omask & EPOLLOUT)
		{
			/* stop write */;
		}
		d->e = *event;
		ret_val = -1;
		break;
	case EPOLL_CTL_DEL:
		d->u.flags.added = false;
		if(!PostQueuedCompletionStatus(ep, (DWORD)-1, fd, NULL)) {
			errno = GetLastError();
			ret_val = -1;
		}
		break;
	}
	LeaveCriticalSection(&d->lock);

	return ret_val;
}

int my_epoll_wait(some_fd epfd, struct epoll_event *events, int maxevents GCC_ATTR_UNUSED_PARAM, int timeout)
{
	HANDLE ep = (HANDLE)epfd;
	OVERLAPPED *ovl;
	DWORD res_bytes;
	ULONG_PTR ckey;

	if(INVALID_HANDLE_VALUE == ep) {
		errno = EBADF;
		return -1;
	}

	if(!events) {
		errno = EFAULT;
		return -1;
	}

	if(timeout < 0)
		timeout = INFINITE;

	if(!GetQueuedCompletionStatus(ep, &res_bytes, &ckey, &ovl, timeout))
	{
		DWORD err = GetLastError();
		if(!ovl)
		{
			/* process failes comleted i/o request */
			errno = err;
			return -1;
		}
		else
		{
			if(WAIT_TIMEOUT == err)
				return 0; /* timeout */
			else
			{
				/* bad call to GetQueuedCompletionStatus */
				errno = err;
				return -1;
			}
		}
	}
	else
	{
		/* OK, success, what now... */
	}

	return -1;
}

some_fd my_epoll_create(int size GCC_ATTR_UNUSED_PARAM)
{
	HANDLE res;

	if(!get_extra_funcs())
		return -1;

	res = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
	if(!res) {
		errno = GetLastError();
		return -1;
	}
	return (some_fd)res;
}

int my_epoll_close(some_fd epfd)
{
	return CloseHandle((HANDLE)epfd) ? 0 : -1;
}


/*
 * Hooks
 */
some_fd my_epoll_socket(int domain, int type, int protocol)
{
	struct some_fd_data *d;
	enum sock_type st;

	if(AF_INET == domain || AF_INET6 == domain)
	{
		if(SOCK_STREAM == type)
			st = ST_TCP;
		else if(SOCK_DGRAM == type)
			st = ST_UDP;
		else {
			set_s_errno(EINVAL);
			return -1;
		}
	} else {
		set_s_errno(EINVAL);
		return -1;
	}

	d = malloc(sizeof(*d));
	if(!d) {
		set_s_errno(ENOMEM);
		return -1;
	}
	memset(d, 0, offsetof(struct some_fd_data, addr));

	d->fd = socket(domain, type, protocol);
	if(INVALID_SOCKET == d->fd) {
		free(d);
		return -1;
	}
	d->st = st;
	InitializeCriticalSection(&d->lock);
	d->r_buf.capacity = sizeof(d->r_buf.data);
	buffer_clear(d->r_buf);
	d->r_buf.m_pos = 0;
	d->w_buf.capacity = sizeof(d->w_buf.data);
	buffer_clear(d->w_buf);
	d->w_buf.m_pos = 0;
	return (some_fd)d;
}

int my_epoll_closesocket(some_fd sockfd)
{
	struct some_fd_data *d = (struct some_fd_data *)sockfd;
	int ret_val = -1;
	if(d && INVALID_SOCKET != (SOCKET)sockfd) {
		ret_val = closesocket(d->fd);
		DeleteCriticalSection(&d->lock);
		free(d);
	} else
		set_s_errno(EINVAL);
	return ret_val;
}

some_fd my_epoll_accept(some_fd epfd GCC_ATTR_UNUSED_PARAM, some_fd sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
	struct some_fd_data *d = (struct some_fd_data *)sockfd;
	some_fd ret_val;

	if(!d || INVALID_SOCKET == (SOCKET)sockfd) {
		set_s_errno(EINVAL);
		return -1;
	}
	ret_val = accept(d->fd, addr, addrlen);
	return ret_val;
}

int my_epoll_connect(some_fd sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
	struct some_fd_data *d = (struct some_fd_data *)sockfd;

	if(!d || INVALID_SOCKET == (SOCKET)sockfd) {
		set_s_errno(EINVAL);
		return -1;
	}
	return connect(d->fd, addr, addrlen);
}

int my_epoll_fcntl(some_fd fd, int cmd, ...)
{
	struct some_fd_data *d = (struct some_fd_data *)fd;
	va_list args;
	int ret_val;

	if(!d || INVALID_SOCKET == (SOCKET)fd) {
		set_s_errno(EINVAL);
		return -1;
	}

	va_start(args, cmd);
	ret_val = vfcntl(d->fd, cmd, args);
	va_end(args);
	return ret_val;
}

int my_epoll_bind(some_fd sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
	struct some_fd_data *d = (struct some_fd_data *)sockfd;

	if(!d || INVALID_SOCKET == (SOCKET)sockfd) {
		set_s_errno(EINVAL);
		return -1;
	}
	return bind(d->fd, addr, addrlen);
}

int my_epoll_listen(some_fd sockfd, int backlog)
{
	struct some_fd_data *d = (struct some_fd_data *)sockfd;

	if(!d || INVALID_SOCKET == (SOCKET)sockfd) {
		set_s_errno(EINVAL);
		return -1;
	}
	d->st = ST_ACCEPT;
	return listen(d->fd, backlog);
}

int my_epoll_setsockopt(some_fd sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
	struct some_fd_data *d = (struct some_fd_data *)sockfd;

	if(!d || INVALID_SOCKET == (SOCKET)sockfd) {
		set_s_errno(EINVAL);
		return -1;
	}
	return setsockopt(d->fd, level, optname, optval, optlen);
}

ssize_t my_epoll_send(some_fd epfd GCC_ATTR_UNUSED_PARAM, some_fd sockfd, const void *buf, size_t len, int flags)
{
	struct some_fd_data *d = (struct some_fd_data *)sockfd;

	if(!d || INVALID_SOCKET == (SOCKET)sockfd) {
		set_s_errno(EINVAL);
		return -1;
	}
	return send(d->fd, buf, len, flags);
}

ssize_t my_epoll_sendto(some_fd sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
	struct some_fd_data *d = (struct some_fd_data *)sockfd;

	if(!d || INVALID_SOCKET == (SOCKET)sockfd) {
		set_s_errno(EINVAL);
		return -1;
	}
	return sendto(d->fd, buf, len, flags, dest_addr, addrlen);
}

#if 0
// TODO: only >= Vista
static ssize_t my_epoll_sendmsg(some_fd sockfd, my_msghdr *msg, int flags)
{
	struct some_fd_data *d = (struct some_fd_data *)sockfd;
	DWORD num_recv = 0;
	int res;

	if(!d || INVALID_SOCKET == (SOCKET)sockfd) {
		set_s_errno(EINVAL);
		return -1;
	}

	res = WSASendMsg(sockfd, msg, flags, &num_recv, NULL, NULL);
	if(SOCKET_ERROR == res)
		return -1;
	return num_recv;
}
#endif

ssize_t my_epoll_recv(some_fd epfd GCC_ATTR_UNUSED_PARAM, some_fd sockfd, void *buf, size_t len, int flags)
{
	struct some_fd_data *d = (struct some_fd_data *)sockfd;

	if(!d || INVALID_SOCKET == (SOCKET)sockfd) {
		set_s_errno(EINVAL);
		return -1;
	}
	return recv(d->fd, buf, len, flags);
}

ssize_t my_epoll_recvmsg(some_fd sockfd, my_msghdr *msg, int flags GCC_ATTR_UNUSED_PARAM)
{
	struct some_fd_data *d = (struct some_fd_data *)sockfd;
	DWORD num_recv = 0;
	int res;

	if(!d || INVALID_SOCKET == (SOCKET)sockfd) {
		set_s_errno(EINVAL);
		return -1;
	}

	res = recv_msg_ptr(d->fd, msg, &num_recv, NULL, NULL);
	if(SOCKET_ERROR == res)
		return -1;
	return num_recv;
}


/*
 * fcntl emul
 *
 */
int fcntl(int fd, int cmd, ...)
{
	va_list args;
	int ret_val;

	va_start(args, cmd);
	ret_val = vfcntl(fd, cmd, args);
	va_end(args);
	return ret_val;
}

static int vfcntl(int fd, int cmd, va_list args)
{
	int ret_val = 0;
	u_long arg;

	switch(cmd)
	{
	case F_GETFL:
		break;
	case F_SETFL:
		arg = va_arg(args, int);
		arg = arg & O_NONBLOCK ? 1 : 0;
		ioctlsocket(fd, FIONBIO, &arg);
		break;
	default:
		errno = ENOSYS;
		ret_val = -1;
	}
	return ret_val;
}

/*
 * Socket pair emul
 *
 */
int socketpair(int domain GCC_ATTR_UNUSED_PARAM, int type GCC_ATTR_UNUSED_PARAM, int protocol GCC_ATTR_UNUSED_PARAM, some_fd sv[2])
{
	union combo_addr addr;
	SOCKET listener;
	socklen_t addrlen;
	int yes = 1;

	listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(INVALID_SOCKET == listener) {
		errno = WSAGetLastError();
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.in.sin_family = AF_INET;
	addr.in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.in.sin_port = 0;
	addrlen = sizeof(addr);
	sv[0] = sv[1] = INVALID_SOCKET;
	if(-1 == setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (void *)&yes, sizeof(yes)))
		goto OUT_ERR;
	if(SOCKET_ERROR == bind(listener, casa(&addr), casalen(&addr)))
		goto OUT_ERR;
	if(SOCKET_ERROR == getsockname(listener, casa(&addr), &addrlen))
		goto OUT_ERR;
	if(SOCKET_ERROR == listen(listener, 1))
		goto OUT_ERR;
	sv[1] = my_epoll_socket(AF_INET, SOCK_STREAM, 0);
	if(INVALID_SOCKET == (SOCKET)sv[1])
		goto OUT_ERR;
	if(SOCKET_ERROR == my_epoll_connect(sv[1], casa(&addr), casalen(&addr)))
		goto OUT_ERR;
	sv[0] = accept(listener, NULL, NULL);
	if(INVALID_SOCKET == (SOCKET)sv[0])
		goto OUT_ERR;
	closesocket(listener);
	return 0;

OUT_ERR:
	yes = WSAGetLastError();
	closesocket(listener);
	closesocket(sv[0]);
	my_epoll_closesocket(sv[1]);
	errno = yes;
	return -1;
}

/*
 * Poll emul
 * At least this works to get the main code of the ground
 */
int PASCAL poll(my_pollfd *fds, unsigned long nfds, int timeout)
{
	return poll_ptr(fds, nfds, timeout);
}

static int PASCAL poll_emul(my_pollfd *fds, unsigned long nfds, int timeout)
{
	fd_set rfd, wfd;
	struct timeval t;
	unsigned max_fd, i;
	int res, res_b;

	if(unlikely(nfds > FD_SETSIZE)) {
		logg(LOGF_INFO, "can't handle %lu fds\n", nfds);
		errno = EINVAL;
		return -1;
	}
	if(timeout >= 0)
	{
		if(!nfds) {
			sleep(timeout);
			return 0;
		}
	} else if(unlikely(!nfds)) {
		sleep(INFINITE);
		return 0;
	}

	FD_ZERO(&rfd);
	FD_ZERO(&wfd);
	max_fd = 0;
	for(i = 0; i < nfds; i++)
	{
		fds[i].revents = 0;
		if(!(fds[i].events & (POLLIN|POLLOUT)))
			continue;
		if(fds[i].events & POLLIN)
			FD_SET(fds[i].fd, &rfd);
		if(fds[i].events & POLLOUT)
			FD_SET(fds[i].fd, &wfd);
		max_fd = max_fd >= fds[i].fd ? max_fd : fds[i].fd;
	}
	if(timeout >= 0) {
		t.tv_sec  = timeout / 1000;
		t.tv_usec = (timeout % 1000) * 1000;
	}

	res_b = res = select(max_fd + 1, &rfd, &wfd, NULL, timeout >= 0 ? &t : NULL);
	if(SOCKET_ERROR == res) {
		errno = WSAGetLastError();
		return -1;
	}
	for(i = 0; i < nfds && res; i++)
	{
		if(FD_ISSET(fds[i].fd, &rfd))
			fds[i].revents |= POLLIN;
		if(FD_ISSET(fds[i].fd, &wfd))
			fds[i].revents |= POLLOUT;
		if(fds[i].revents)
			res--;
	}
	return res_b;
}

/*@unused@*/
static char const rcsid_mew[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
