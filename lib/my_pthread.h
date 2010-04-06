/*
 * my_pthread.h
 * header-file to redirect pthread.h inclusion
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

#ifndef LIB_MY_PTHREAD_H
# define LIB_MY_PTHREAD_H

# ifndef WIN32
#  include <pthread.h>
# else
#  include <time.h>
#  include <windows.h>
#  include "other.h"
#  define LIB_MY_PTHREAD_EXTRN(x) x GCC_ATTR_VIS("hidden")

// TODO: write this stuff...

struct timespec {
	time_t	tv_sec;	/* seconds */
	long	tv_nsec;	/* nanoseconds */
};

typedef CRITICAL_SECTION pthread_mutex_t;

typedef struct {
	/* we support no fancy attr */
} pthread_mutexattr_t;

typedef struct {
} pthread_rwlock_t;

typedef struct {
	/* we support no fancy attr */
} pthread_rwlockattr_t;

typedef struct {
	size_t stacksize;
} pthread_attr_t;

typedef DWORD pthread_t;

typedef struct {
  int waiters_count; /* Count of the number of waiters. */
  CRITICAL_SECTION waiters_count_lock; /* Serialize access to waiters_count */
  int release_count; /* Number of threads to release via a pthread_cond_broadcast or a pthread_cond_signal */
  int wait_generation_count; /* Keeps track of the current "generation" so that we don't allow one thread to steal all the "releases" from the broadcast. */
  HANDLE event; /* A manual-reset event that's used to block and release waiting threads */
} pthread_cond_t;

typedef struct {
	/* we support no fancy attr */
} pthread_condattr_t;

typedef DWORD pthread_key_t;

/* simple mutexes */
LIB_MY_PTHREAD_EXTRN(int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *restrict attr));
LIB_MY_PTHREAD_EXTRN(int pthread_mutex_destroy(pthread_mutex_t *mutex));
LIB_MY_PTHREAD_EXTRN(int pthread_mutex_lock(pthread_mutex_t *mutex));
LIB_MY_PTHREAD_EXTRN(int pthread_mutex_trylock(pthread_mutex_t *mutex));
LIB_MY_PTHREAD_EXTRN(int pthread_mutex_unlock(pthread_mutex_t *mutex));

/* since WIN32 seems to miss user level spinlocks, no impl. */

/* rw locks */
LIB_MY_PTHREAD_EXTRN(int pthread_rwlock_init(pthread_rwlock_t *restrict rwlock, const pthread_rwlockattr_t *restrict attr));
LIB_MY_PTHREAD_EXTRN(int pthread_rwlock_destroy(pthread_rwlock_t *rwlock));
LIB_MY_PTHREAD_EXTRN(int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock));
LIB_MY_PTHREAD_EXTRN(int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock));
LIB_MY_PTHREAD_EXTRN(int pthread_rwlock_unlock(pthread_rwlock_t *rwlock));

/* threads */
LIB_MY_PTHREAD_EXTRN(int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg));
LIB_MY_PTHREAD_EXTRN(int pthread_join(pthread_t thread, void **retval));

/* thread attributes */
LIB_MY_PTHREAD_EXTRN(int pthread_attr_init(pthread_attr_t *attr));
LIB_MY_PTHREAD_EXTRN(int pthread_attr_destroy(pthread_attr_t *attr));
LIB_MY_PTHREAD_EXTRN(int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize));
LIB_MY_PTHREAD_EXTRN(int pthread_attr_getstacksize(pthread_attr_t *attr, size_t *stacksize));

/* conditionals */
LIB_MY_PTHREAD_EXTRN(int pthread_cond_init(pthread_cond_t *restrict cond, const pthread_condattr_t *restrict attr));
LIB_MY_PTHREAD_EXTRN(int pthread_cond_destroy(pthread_cond_t *cond));
LIB_MY_PTHREAD_EXTRN(int pthread_cond_broadcast(pthread_cond_t *cond));
LIB_MY_PTHREAD_EXTRN(int pthread_cond_signal(pthread_cond_t *cond));
LIB_MY_PTHREAD_EXTRN(int pthread_cond_timedwait(pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex, const struct timespec *restrict abstime));
LIB_MY_PTHREAD_EXTRN(int pthread_cond_wait(pthread_cond_t *restrict cond, pthreat_mutex_t *restrict mutex));

/* thread local storage */
LIB_MY_PTHREAD_EXTRN(int pthread_key_create(pthread_key_t *key, void (*destructor)(void*)));
#  define pthread_getspecific(key)	TlsGetValue((key))
#  define pthread_setspecific(key, val)	TlsSetValue((key), (val))

/*
 * Other stuff....
 */
typedef int pid_t;
typedef int gid_t;

/* small fixes */
#  define getpid  _getpid
#  define getppid() (4) /* choosen by a fair dice roll... */
#  define srandom  srand
#  define random   rand
#  define sleep(t) Sleep(t * 1000)
/* wrapper */
LIB_MY_PTHREAD_EXTRN(int getpagesize(void));
#define PRIO_PROCESS 0
LIB_MY_PTHREAD_EXTRN(int getpriority(int which, int who));
LIB_MY_PTHREAD_EXTRN(int setpriority(int which, int who, int prio));

# endif
#endif
