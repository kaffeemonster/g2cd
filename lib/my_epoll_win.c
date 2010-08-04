/*
 * my_epoll_win.c
 * wrapper to get epoll AND poll on windows systems
 *
 * Copyright (c) 2010 Jan Seiffert
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
#ifndef WSAID_ACCEPTEX
# define WSAID_ACCEPTEX {0xb5367df1,0xcbac,0x11cf,{0x95,0xca,0x00,0x80,0x5f,0x48,0xa1,0x92}}
#endif
#ifndef WSAID_GETACCEPTEXSOCKADDRS
# define WSAID_GETACCEPTEXSOCKADDRS {0xb5367df2,0xcbac,0x11cf,{0x95,0xca,0x00,0x80,0x5f,0x48,0xa1,0x92}}
#endif
#if 0
#ifndef WSAID_CONNECTEX
# define WSAID_CONNECTEX {0x25a207b9,0xddf3,0x4660,{0x8e,0xe9,0x76,0xe5,0x8c,0x74,0x06,0x3e}}
#endif
typedef BOOL (WINAPI *ConnectExPtr)(SOCKET, const struct sockaddr *, int, PVOID, DWORD, LPDWORD, LPOVERLAPPED);
#endif

/*
 * Protos
 */
	/* you better not delete this proto, or it won't work */
static void my_epoll_init(void) GCC_ATTR_CONSTRUCT;
static int PASCAL poll_emul(my_pollfd *, unsigned long, int);

/*
 * Vars
 */
static bool extra_funcs_ready;
static poll_ptr_t poll_ptr = poll_emul;
static accept_ex_ptr_t accept_ex_ptr;
static get_accept_ex_sockaddr_ptr_t get_accept_ex_sockaddr_ptr;

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
	DWORD ret_bytes;
	SOCKET s;
	accept_ex_ptr_t aep;
	get_accept_ex_sockaddr_ptr_t gaesp;
	int res;
	bool ret_val = false;

	if(extra_funcs_ready)
		return true;

	s = socket(AF_INET, SOCK_STREAM, 0);
	if(INVALID_SOCKET == s)
		return false;

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
int my_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
{

	if(INVALID_HANDLE_VALUE == (HANDLE)epfd) {
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

	if(EPOLL_CTL_ADD == op)
	{
		HANDLE res = CreateIoCompletionPort((HANDLE)fd, (HANDLE)epfd, 0/* data */, 0);
		if(!res) {
			errno = GetLastError();
			return -1;
		}
	}
	else if(EPOLL_CTL_MOD == op)
	{
		return -1;
	}
	else /* DEL */
	{
		return -1;
	}

	return 0;
}

int my_epoll_wait(int epfd, struct epoll_event *events, int maxevents GCC_ATTR_UNUSED_PARAM, int timeout)
{
	OVERLAPPED *ovl;
	DWORD res_bytes;
	ULONG_PTR ckey;

	if(INVALID_HANDLE_VALUE == (HANDLE)epfd) {
		errno = EBADF;
		return -1;
	}

	if(!events) {
		errno = EFAULT;
		return -1;
	}

	if(timeout < 0)
		timeout = INFINITE;

	if(!GetQueuedCompletionStatus((HANDLE)epfd, &res_bytes, &ckey, &ovl, timeout)) {
		if(!ovl)
			return 0; /* timeout */
		errno = GetLastError();
		return -1;
	}

	return -1;
}

int my_epoll_create(int size GCC_ATTR_UNUSED_PARAM)
{
	HANDLE res;

	if(!get_extra_funcs())
		return -1;

	res = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if(!res) {
		errno = GetLastError();
		return -1;
	}
	return (int)res;
}

int my_epoll_close(int epfd)
{
	return CloseHandle((HANDLE)epfd) ? 0 : -1;
}

/*
 * Hooks
 */
int my_epoll_accept(int epfd GCC_ATTR_UNUSED_PARAM, int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
	return accept(sockfd, addr, addrlen);
}

int my_epoll_socket(int domain, int type, int protocol)
{
	return socket(domain, type, protocol);
}

ssize_t my_epoll_send(int sockfd, const void *buf, size_t len, int flags)
{
	return send(sockfd, buf, len, flags);
}

ssize_t my_epoll_recv(int sockfd, void *buf, size_t len, int flags)
{
	return recv(sockfd, buf, len, flags);
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
