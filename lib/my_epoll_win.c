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

/* you better not delete this proto, or it won't work */
static void my_epoll_init(void) GCC_ATTR_CONSTRUCT;
static int PASCAL poll_emul(my_pollfd *, unsigned long, int);

static int (PASCAL *poll_ptr)(my_pollfd *, unsigned long, int) = poll_emul;

/*
 * this is all just a fluke...
 * here is nothing really working
 */
static void my_epoll_init(void)
{
	int (PASCAL *pp)(my_pollfd *, unsigned long, int);

	pp = (int (PASCAL *)(my_pollfd *, unsigned long, int))
		GetProcAddress(GetModuleHandle(TEXT("ws2_32")), "WSAPoll");
	if(pp)
		poll_ptr = pp;
}

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
	HANDLE res = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
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
