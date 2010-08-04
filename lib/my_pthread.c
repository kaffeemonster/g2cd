/*
 * my_pthread.c
 * cheapo pthread emulation
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
 * $Id:$
 */

#ifdef WIN32
# define _WIN32_WINNT 0x0501
# include <windows.h>
# include <signal.h>
# include <errno.h>
# include "other.h"
# include "my_pthread.h"
# include "combo_addr.h"
# include "log_facility.h"

#define NINETHY70_OFF (0x19db1ded53e8000ULL)

/*
 * Mutexes
 * Since we only need the simple binary lock, no bells and wistles,
 * recursion, foo, whatever, we can map to the simple CriticalSection
 */
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *restrict attr GCC_ATTR_UNUSED_PARAM)
{
	if(!mutex) {
		errno = EINVAL;
		return -1;
	}
	InitializeCriticalSection(mutex);
	return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	if(!mutex) {
		errno = EINVAL;
		return -1;
	}
	DeleteCriticalSection(mutex);
	return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
	EnterCriticalSection(mutex);
	return 0;
}

int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	if(!TryEnterCriticalSection(mutex)) {
		errno = EBUSY;
		return -1;
	}
	return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	if(!mutex)
		return 0;
	if(mutex->OwningThread != (HANDLE)GetCurrentThreadId()) {
		errno = EPERM;
		return -1;
	}
	LeaveCriticalSection(mutex);
	return 0;
}

/*
 * RW locks
 * Thanks go out to the apache project for a peek into their apr rwlocks
 */
int pthread_rwlock_init(pthread_rwlock_t *restrict rwlock, const pthread_rwlockattr_t *restrict attr GCC_ATTR_UNUSED_PARAM)
{
	if(!rwlock) {
		errno = EINVAL;
		return -1;
	}

	rwlock->readers = 0;
	rwlock->read_event = CreateEvent(NULL, TRUE, FALSE, NULL);
	if(!rwlock->read_event) {
		errno = ENOMEM;
		return -1;
	}
	rwlock->write_mutex = CreateMutex(NULL, FALSE, NULL);
	if(!rwlock->write_mutex) {
		CloseHandle(rwlock->read_event);
		errno = ENOMEM;
		return -1;
	}
	return 0;
}

int pthread_rwlock_destroy(pthread_rwlock_t *rwlock)
{
	BOOL res = TRUE;
	if(!rwlock) {
		errno = EINVAL;
		return -1;
	}
	res = res && CloseHandle(rwlock->read_event);
	res = res && CloseHandle(rwlock->write_mutex);
	if(res)
		return 0;
	errno = EACCES;
	return -1;
}

int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock)
{
	DWORD res;

	/* for a trylock, change the wait time to 0 */
	res = WaitForSingleObject(rwlock->write_mutex, INFINITE);

	if(res == WAIT_FAILED || res == WAIT_TIMEOUT) {
		errno = EAGAIN;
		return -1;
	}

	InterlockedIncrement(&rwlock->readers);
	if(!ResetEvent(rwlock->read_event)) {
		ReleaseMutex(rwlock->write_mutex);
		errno = EAGAIN;
		return -1;
	}

	if(!ReleaseMutex(rwlock->write_mutex)) {
		errno = EACCES;
		return -1;
	}

	return 0;
}

int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock)
{
	DWORD res;

	/* for a trylock, change the wait time  to 0 */
	res = WaitForSingleObject(rwlock->write_mutex, INFINITE);

	if(res == WAIT_FAILED || res == WAIT_TIMEOUT) {
		errno = EAGAIN;
		return -1;
	}

	/* wait for the readers to leave the section */
	if(rwlock->readers)
	{
		res = WaitForSingleObject(rwlock->read_event, INFINITE);

		if(res == WAIT_FAILED || res == WAIT_TIMEOUT) {
			ReleaseMutex(rwlock->write_mutex);
			errno = EAGAIN;
			return -1;
		}
	}

	return 0;
}

int pthread_rwlock_unlock(pthread_rwlock_t *rwlock)
{
	DWORD res = 0;

	/* do we own the write lock? */
	if(!ReleaseMutex(rwlock->write_mutex))
		res = GetLastError();

	if(res == ERROR_NOT_OWNER)
	{
		/* Nope, we must have a read lock */
		if(rwlock->readers &&
		   !InterlockedDecrement(&rwlock->readers) &&
		   !SetEvent(rwlock->read_event)) {
			errno = EAGAIN;
			return -1;
		}
	}
	return 0;
}

/*
 * Threads
 */
struct pthread_thread_init
{
	void *(*start_routine)(void*);
	void *arg;
};

static DWORD CALLBACK pthread_thread_start(LPVOID data)
{
	struct pthread_thread_init init = *(struct pthread_thread_init*)data;
	free(data);
	return (DWORD)init.start_routine(init.arg);
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg)
{
	HANDLE thread_h;
	struct pthread_thread_init *idata = malloc(sizeof(struct pthread_thread_init));

	if(!idata) {
		errno = ENOMEM;
		return -1;
	}
	idata->start_routine = start_routine;
	idata->arg = arg;
	thread_h = CreateThread(NULL, attr ? attr->stacksize : 0, pthread_thread_start, idata, 0, (LPDWORD)thread);

	if(thread_h)
		CloseHandle(thread_h);
	else {
		free(idata);
		errno = EAGAIN;
		return -1;
	}
	return 0;
}

void pthread_exit(void *retval)
{
	ExitThread((DWORD)retval);
}

int pthread_join(pthread_t thread, void **retval)
{
	int ret_st = 0;
	HANDLE thread_h = OpenThread(THREAD_ALL_ACCESS, FALSE, (DWORD)thread);

	WaitForSingleObject(thread_h, INFINITE);
	if(!GetExitCodeThread(thread_h, (LPDWORD)retval)) {
		errno = EINVAL;
		ret_st = -1;
	}

	CloseHandle(thread_h);
	return ret_st;
}

/*
 * Thread attributes
 */
int pthread_attr_init(pthread_attr_t *attr)
{
	if(!attr) {
		errno = EINVAL;
		return -1;
	}
	attr->stacksize = 0;
	return 0;
}

int pthread_attr_destroy(pthread_attr_t *attr)
{
	if(!attr) {
		errno = EINVAL;
		return -1;
	}
	return 0;
}

int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize)
{
	if(!attr) {
		errno = EINVAL;
		return -1;
	}
	attr->stacksize = stacksize;
	return 0;
}

int pthread_attr_getstacksize(pthread_attr_t *attr, size_t *stacksize)
{
	if(!(attr && stacksize)) {
		errno = EINVAL;
		return -1;
	}
	if(0 == attr->stacksize)
	{
		/*
		 * 0 means auto mode, the system sets the default stacksize.
		 * The default stack size is taken from the program header and
		 * can be set on linking.
		 * Since we do not mess with it, we assume the default, which
		 * is 1 MB. It would be nice to query the system at runtime,
		 * but since i don't know how...
		 */
		*stacksize = 1 * 1024 * 1024;
	}
	else
		*stacksize = attr->stacksize;
	return 0;
}

/*
 * Conditionals
 * I say thanks to http://www.cs.wustl.edu/~schmidt/win32-cv-1.html,
 * conditionals are complicated...
 * We would not need all the features, since we only have one waiter.
 */
int pthread_cond_init(pthread_cond_t *restrict cond, const pthread_condattr_t *restrict attr GCC_ATTR_UNUSED_PARAM)
{
	if(!cond) {
		errno = EINVAL;
		return -1;
	}
	cond->waiters_count         = 0;
	cond->wait_generation_count = 0;
	cond->release_count         = 0;
	/* Create a manual-reset event */
	cond->event = CreateEvent(NULL,  /* no security */
	                          TRUE,  /* manual-reset */
	                          FALSE, /* non-signaled initially */
	                          NULL); /* unnamed */
	if(!cond->event) {
		errno = EACCES;
		return -1;
	}
	InitializeCriticalSection(&cond->waiters_count_lock);
	return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond)
{
	if(!cond) {
		errno = EINVAL;
		return -1;
	}
	CloseHandle(cond->event);
	cond->event = NULL;
	DeleteCriticalSection(&cond->waiters_count_lock);
	return 0;
}

int pthread_cond_broadcast(pthread_cond_t *cond)
{
	EnterCriticalSection(&cond->waiters_count_lock);
	if(cond->waiters_count > 0) {
		SetEvent(cond->event); /* Release all the threads in this generation. */
		cond->release_count = cond->waiters_count; /* Start a new generation. */
		cond->wait_generation_count++;
	}
	LeaveCriticalSection (&cond->waiters_count_lock);
	return 0;
}

int pthread_cond_signal(pthread_cond_t *cond)
{
	EnterCriticalSection(&cond->waiters_count_lock);
	if(cond->waiters_count > cond->release_count) {
		SetEvent(cond->event); /* Signal the manual-reset event. */
		cond->release_count++;
		cond->wait_generation_count++;
	}
	LeaveCriticalSection (&cond->waiters_count_lock);
	return 0;
}


int pthread_cond_timedwait(pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex, const struct timespec *restrict abstime)
{
	int my_generation;
	int ret_val = 0;
	BOOL last_waiter, release = TRUE;
	DWORD ms;

	/* Avoid race conditions. */
	EnterCriticalSection(&cond->waiters_count_lock);
	/* Increment count of waiters. */
	cond->waiters_count++;
	/* Store current generation in our activation record. */
	my_generation = cond->wait_generation_count;
	LeaveCriticalSection(&cond->waiters_count_lock);
	LeaveCriticalSection(mutex);

	while(TRUE)
	{
		BOOL wait_done;
		DWORD res;

		if(abstime)
		{
			union {
				FILETIME ft;
				ULARGE_INTEGER ui;
			} u;
			struct timespec ts;
			/*
			 * Make the dance to convert the system time (since 1601) into
			 * a timespec (aka since 1970)
			 */
			GetSystemTimeAsFileTime(&u.ft);
			u.ui.QuadPart -= NINETHY70_OFF;
			ts.tv_sec  =  u.ui.QuadPart / 10000000;
			ts.tv_nsec = (u.ui.QuadPart % 10000000) * 100;
			if(abstime->tv_sec >= ts.tv_sec || abstime->tv_nsec > ts.tv_nsec)
			{
				long nsec = (long)abstime->tv_nsec - (long)ts.tv_nsec;
				ms   = (abstime->tv_sec - ts.tv_sec) * 1000;
				ms  += nsec / 1000000;
				/*
				 * we could substract some kind of overhead but pfff,
				 * thats good enough
				 */
			}
			/* the absolute time already passed, only do a once off test */
			else
				ms = 0;
		}
		else
			ms = INFINITE;

		/* Wait until the event is signaled. */
		res = WaitForSingleObject(cond->event, ms);

		if(res == WAIT_FAILED) {
			ret_val = EACCES;
			release = FALSE;
			break;
		}
		if(res == WAIT_TIMEOUT) {
			ret_val = ETIMEDOUT;
			release = FALSE;
			break;
		}
		EnterCriticalSection(&cond->waiters_count_lock);
		/*
		 * Exit the loop when the <cv->event_> is signaled and
		 * there are still waiting threads from this <wait_generation>
		 * that haven't been released from this wait yet.
		 */
		wait_done = cond->release_count > 0 &&
		            cond->wait_generation_count != my_generation;
		LeaveCriticalSection(&cond->waiters_count_lock);
		if(wait_done)
			break;
	}

	EnterCriticalSection(mutex);
	EnterCriticalSection(&cond->waiters_count_lock);
	cond->waiters_count--;
	if(release)
		cond->release_count--;
	last_waiter = cond->release_count == 0;
	LeaveCriticalSection(&cond->waiters_count_lock);
	if(last_waiter) /* We're the last waiter to be notified, so reset the manual event. */
		ResetEvent(cond->event);
	return ret_val;
}

int pthread_cond_wait(pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex)
{
	int my_generation;
	BOOL last_waiter;

	/* Avoid race conditions. */
	EnterCriticalSection(&cond->waiters_count_lock);
	/* Increment count of waiters. */
	cond->waiters_count++;
	/* Store current generation in our activation record. */
	my_generation = cond->wait_generation_count;
	LeaveCriticalSection(&cond->waiters_count_lock);
	LeaveCriticalSection(mutex);

	while(TRUE)
	{
		BOOL wait_done;

		/* Wait until the event is signaled. */
		WaitForSingleObject(cond->event, INFINITE);

		EnterCriticalSection(&cond->waiters_count_lock);
		/*
		 * Exit the loop when the <cv->event_> is signaled and
		 * there are still waiting threads from this <wait_generation>
		 * that haven't been released from this wait yet.
		 */
		wait_done = cond->release_count > 0 &&
		            cond->wait_generation_count != my_generation;
		LeaveCriticalSection(&cond->waiters_count_lock);
		if (wait_done)
			break;
	}

	EnterCriticalSection(mutex);
	EnterCriticalSection(&cond->waiters_count_lock);
	cond->waiters_count--;
	cond->release_count--;
	last_waiter = cond->release_count == 0;
	LeaveCriticalSection(&cond->waiters_count_lock);
	if(last_waiter) /* We're the last waiter to be notified, so reset the manual event. */
		ResetEvent(cond->event);
	return 0;
}

/*
 * TLS
 */
int pthread_key_create(pthread_key_t *key, void (*destructor)(void*))
{
	DWORD i;
	if(!key) {
		errno = EINVAL;
		return -1;
	}
	i = TlsAlloc();
	if(TLS_OUT_OF_INDEXES == i) {
		errno = GetLastError();
		return -1;
	}
	*key = i;
	if(destructor)
	{
		/*
		 * running the destructor function on Windows would
		 * involve "voodoo", and is to complicated for us.
		 * Luckily we do not really need it.
		 * All destructor which g2cd use are to make the
		 * valgrind output cleaner, not because we acually
		 * leak something. Our threads live from start till
		 * end, no fancy "create a thread for a work item".
		 */
	}
	return 0;
}

int pthread_key_delete(pthread_key_t key)
{
	return TlsFree(key) ? 0 : -1;
}

/*
 * And now to something completly different
 */
int getpagesize(void)
{
	static int pagesize = 0;
	/* a little bit racy, but pfff.... */
	if(pagesize == 0) {
		SYSTEM_INFO system_info;
		GetSystemInfo(&system_info);
		pagesize = system_info.dwPageSize;
	}
	return pagesize;
}

int getpriority(int which, int who)
{
	int myprio;

	if(which != PRIO_PROCESS || who != 0) {
		/* we only support the simpliest use */
		errno = EINVAL;
		return -1;
	}

	myprio = GetPriorityClass(GetCurrentProcess());
	if (myprio == 0) {
		errno = EACCES;
		return -1;
	}
	errno = 0;

	switch(myprio)
	{
	case ABOVE_NORMAL_PRIORITY_CLASS:
		return -1;
	case BELOW_NORMAL_PRIORITY_CLASS:
		return 1;
	case HIGH_PRIORITY_CLASS:
		return -10;
	case IDLE_PRIORITY_CLASS:
		return 20;
	case NORMAL_PRIORITY_CLASS:
		return 0;
	case REALTIME_PRIORITY_CLASS:
		return -20;
	default:
		return 0;
	}
}

int setpriority(int which, int who, int prio)
{
	int myprio;

	if(which != PRIO_PROCESS || who != 0) {
		/* we only support the simpliest use */
		errno = EINVAL;
		return -1;
	}
	/*
	 * Windows priorities go from 0 (lowest) to 32 highest.
	 * 8 is normal == 0 on unix.
	 * we could really map the prio, but since we do not
	 * want to rely on such details...
	 */
	if(prio <= -20)
		myprio = REALTIME_PRIORITY_CLASS;
	else if(prio <= -10)
		myprio = HIGH_PRIORITY_CLASS;
	else if(prio <= -1)
		myprio = ABOVE_NORMAL_PRIORITY_CLASS;
	else if(prio == 0)
		myprio = NORMAL_PRIORITY_CLASS;
	else if(prio < 20)
		myprio = BELOW_NORMAL_PRIORITY_CLASS;
	else
		myprio = IDLE_PRIORITY_CLASS;

	if(!SetPriorityClass(GetCurrentProcess(), myprio)) {
		errno = EACCES;
		return -1;
	}
	return 0;
}

#if 0
/* mingw suplies one... */
int gettimeofday(struct timeval *tv, struct timezone *tz)
{
	union {
		FILETIME ft;
		ULARGE_INTEGER ui;
	} u;

	if(unlikely(!tv)) {
		errno = EACCES;
		return -1;
	}
	if(unlikey(tz)) {
		errno = ENOSYS;
		return -1;
	}
	/* for our case, we simply ignore the timezone */
	GetSystemTimeAsFileTime(&u.ft);
	u.ui.QuadPart -= NINETHY70_OFF;
	tv->tv_sec = u.ui.QuadPart / 10000000;
	tv->tv_usec = (u.ui.QuadPart % 10000000) / 10;
	/*
	 * this function is NOT precise, SystemTime is in UTC,
	 * we want unix epoch, which is off by the leap seconds.
	 * But it's good enough for us, and fast.
	 */
	return 0;
}
#endif

struct tm *localtime_r(const time_t *timep, struct tm *result)
{
	union {
		FILETIME ft;
		ULARGE_INTEGER ui;
	} u;
	FILETIME lft;
	SYSTEMTIME st;

	if(!timep || !result) {
		errno = EINVAL;
		return NULL;
	}

	u.ui.QuadPart = (long long)*timep * 10000000;
	u.ui.QuadPart += NINETHY70_OFF;
	if(!FileTimeToLocalFileTime(&u.ft, &lft)) {
		errno = ERANGE;
		return NULL;
	}
	if(!FileTimeToSystemTime(&lft, &st)) {
		errno = ERANGE;
		return NULL;
	}
	result->tm_sec   = st.wSecond;
	result->tm_min   = st.wMinute;
	result->tm_hour  = st.wHour;
	result->tm_mday  = st.wDay;
	result->tm_mon   = st.wMonth - 1;
	result->tm_year  = st.wYear - 1900;
	result->tm_wday  = st.wDayOfWeek;
// TODO: Do we need to calc this? can we get that from from api?
	/* day in the year */
	result->tm_yday  = 0;
	result->tm_isdst = 0;
	return result;
}

int fcntl(int fd, int cmd, ...)
{
	va_list args;
	int ret_val = 0;
	u_long arg;

	va_start(args, cmd);
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
	va_end(args);
	return ret_val;
}

int sigemptyset(sigset_t *set)
{
	*set = 0;
	return 0;
}

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
	void (*o_hand)(int);
	void (*n_hand)(int);

	if(act)
	{
		if(act->sa_flags & SA_SIGINFO) {
			logg(LOGF_INFO,"Warning: setting up a sigaction as normal signal, this is WRONG!\n");
			n_hand = (void (*)(int)) act->sa_sigaction;
		}
		else
			n_hand = act->sa_handler;
	} else
		n_hand = SIG_DFL;
	o_hand = signal(signum, n_hand);
	if(SIG_ERR == o_hand)
		return -1;
	if(oldact)
		oldact->sa_handler = o_hand;
	return 0;
}

int socketpair(int domain GCC_ATTR_UNUSED_PARAM, int type GCC_ATTR_UNUSED_PARAM, int protocol GCC_ATTR_UNUSED_PARAM, int sv[2])
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
	sv[0] = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, 0);
	if(INVALID_SOCKET == (SOCKET)sv[0])
		goto OUT_ERR;
	if(SOCKET_ERROR == connect(sv[0], casa(&addr), casalen(&addr)))
		goto OUT_ERR;
	sv[1] = accept(listener, NULL, NULL);
	if(INVALID_SOCKET == (SOCKET)sv[1])
		goto OUT_ERR;
	closesocket(listener);
	return 0;

OUT_ERR:
	yes = WSAGetLastError();
	closesocket(listener);
	closesocket(sv[0]);
	closesocket(sv[1]);
	errno = yes;
	return -1;
}

/*@unused@*/
static char const rcsid_mpth[] GCC_ATTR_USED_VAR = "$Id:$";
#endif
/* EOF */
