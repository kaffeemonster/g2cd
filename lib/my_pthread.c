/*
 * my_pthread.c
 * cheapo pthread emulation
 *
 * Copyright (c) 2010 Jan Seiffert
 *
 * This file is part of g2cd.
 *
 * g2cd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * g2cd is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with g2cd; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 *
 * $Id:$
 */

#ifdef WIN32
# include <windows.h>
# include <errno.h>
# include "other.h"
# include "my_pthread.h"

/*
 * Mutexes
 * Since we only need the simple binary lock, no bells and wistles,
 * recursion, foo, whatever, we can map to the simple CriticalSection
 */
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *restrict attr)
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
	RtlLeaveCriticalSection(mutex);
}

/*
 * RW locks
 */
int pthread_rwlock_init(pthread_rwlock_t *restrict rwlock, const pthread_rwlockattr_t *restrict attr)
{
}

int pthread_rwlock_destroy(pthread_rwlock_t *rwlock)
{
}

int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock)
{
}

int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock)
{
}

int pthread_rwlock_unlock(pthread_rwlock_t *rwlock)
{
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

int pthread_join(pthread_t thread, void **retval)
{
	int ret_st = 0;
	HANDLE thread_h = OpenThread(THREAD_ALL_ACCESS, FALSE, (DWORD)thread);

	WaitForSingleObject(thread_h, INFINITE);
	if(!GetExitCodeThread(thread_t, (LPDWORD)retval)) {
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
		 * is 1 MB. It would be nice to query the system, but since
		 * i don't know how...
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
int pthread_cond_init(pthread_cond_t *restrict cond, const pthread_condattr_t *restrict attr);
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
}

int pthread_cond_broadcast(pthread_cond_t *cond)
{
	EnterCriticalSection(&cond->waiters_count_lock);
	if(cond->waiters_count > 0) {
		SetEvent(cond->event); /* Release all the threads in this generation. */
		cond->release_count = cond->waiters_count; /* Start a new generation. */
		cond->wait_generation_count++;
	}
	LeaveCriticalSection (&cond->waiters_count_lock_);
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
}


int pthread_cond_timedwait(pthread_cond_t *restrict cond, pthreat_mutex_t *restrict mutex, const struct timespec *restrict abstime)
{
	int my_generation;
	bool last_waiter, release = true;
	DWORD ms;

// TODO: insert a wonder here...
	/*
	 * Make the dance to convert the system time (1601) into a timespec aka 1970
	 */

	/* Avoid race conditions. */
	EnterCriticalSection(&ccond->waiters_count_lock);
	/* Increment count of waiters. */
	cond->waiters_count++;
	/* Store current generation in our activation record. */
	my_generation = cond->wait_generation_count;
	LeaveCriticalSection(&cond->waiters_count_lock);
	LeaveCriticalSection(mutex);

	while(true)
	{
		bool wait_done;
		DWORD res;

		/* Wait until the event is signaled. */
		res = WaitForSingleObject(cond->event, ms);

		if(res == WAIT_FAILED) {
			errno = EACCES;
			release = false;
			break;
		}
		if(res == WAIT_TIMEOUT) {
// TODO: if we do not have a TLS errno, this will crash and burn...
			errno = EBUSY;
			release = false;
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
	last_waiter = cv->release_count == 0;
	LeaveCriticalSection(&cond->waiters_count_lock);
	if(last_waiter) /* We're the last waiter to be notified, so reset the manual event. */
		ResetEvent(cond->event);
	return 0;
}

int pthread_cond_wait(pthread_cond_t *restrict cond, pthreat_mutex_t *restrict mutex)
{
	int my_generation;
	bool last_waiter;

	/* Avoid race conditions. */
	EnterCriticalSection(&ccond->waiters_count_lock);
	/* Increment count of waiters. */
	cond->waiters_count++;
	/* Store current generation in our activation record. */
	my_generation = cond->wait_generation_count;
	LeaveCriticalSection(&cond->waiters_count_lock);
	LeaveCriticalSection(mutex);

	while(true)
	{
		bool wait_done;

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
	last_waiter = cv->release_count == 0;
	LeaveCriticalSection(&cond->waiters_count_lock);
	if(last_waiter) /* We're the last waiter to be notified, so reset the manual event. */
		ResetEvent(cond->event);
}

/*
 * TLS
 */
int pthread_key_create(pthread_key_t *key, void (*destructor)(void*))
{
	pthread_key_t key = TlsAlloc();
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
	return key;
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

/*@unused@*/
static char const rcsid_mpth[] GCC_ATTR_USED_VAR = "$Id:$";
#endif
/* EOF */
