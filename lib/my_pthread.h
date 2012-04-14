/*
 * my_pthread.h
 * header-file to redirect pthread.h inclusion
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
 * $Id:$
 */

#ifndef LIB_MY_PTHREAD_H
# define LIB_MY_PTHREAD_H

/*
 * move almost all pthread_mutex stuff over a wrapper
 */
# define mutex_t	pthread_mutex_t
# define MUTEX_INITIALIZER	PTHREAD_MUTEX_INITIALIZER
# define mutex_init(da_mutex)	pthread_mutex_init (da_mutex, NULL)
# define mutex_destroy(da_mutex)	pthread_mutex_destroy (da_mutex)
# define mutex_lock(da_mutex)	pthread_mutex_lock (da_mutex)
# define mutex_trylock(da_mutex)	pthread_mutex_trylock (da_mutex)
# define mutex_unlock(da_mutex)	pthread_mutex_unlock (da_mutex)

/*
 * if we are on a SMP-System (also HT) take a spinlock for
 * short locks, else a mutex, since our only Processor shouldn't
 * active spin-wait for itself
 */
# if defined HAVE_SMP && (defined HAVE_SPINLOCKS || defined WIN32)
#  define shortlock_t	pthread_spinlock_t
#  define shortlock_init(da_lock)	pthread_spin_init (da_lock, PTHREAD_PROCESS_PRIVATE)
#  define shortlock_destroy(da_lock)	pthread_spin_destroy(da_lock)
#  define shortlock_lock(da_lock)	pthread_spin_lock(da_lock)
#  define shortlock_unlock(da_lock)	pthread_spin_unlock(da_lock)
# else
#  define shortlock_t	pthread_mutex_t
#  define shortlock_init(da_lock)	pthread_mutex_init (da_lock, NULL)
#  define shortlock_destroy(da_lock)	pthread_mutex_destroy(da_lock)
#  define shortlock_lock(da_lock)	pthread_mutex_lock(da_lock)
#  define shortlock_unlock(da_lock)	pthread_mutex_unlock(da_lock)
# endif /* HAVE_SMP */

# ifndef WIN32
#  include <pthread.h>
#  define set_s_errno(x) (errno = (x))
#  define s_errno errno
# else
#  ifndef _WIN32_WINNT
#   define _WIN32_WINNT 0x0501
#  endif
#  include <time.h>
#  include <windows.h>
#  include <io.h>
#  include "other.h"
#  define LIB_MY_PTHREAD_EXTRN(x) x GCC_ATTR_VIS("hidden")

// TODO: totaly untested...

#  define set_s_errno(x) (WSASetLastError(x))
#  define s_errno WSAGetLastError()

struct timespec {
	time_t	tv_sec;	/* seconds */
	long	tv_nsec;	/* nanoseconds */
};

typedef struct {
	/* dirty emul, no content */
} siginfo_t;

struct sigaction {
	void	(*sa_handler)(int);
	void	(*sa_sigaction)(int, siginfo_t *, void *);
	sigset_t	sa_mask;
	int	sa_flags;
	void	(*sa_restorer)(void);
};

typedef CRITICAL_SECTION pthread_mutex_t;

typedef CRITICAL_SECTION pthread_spinlock_t;

typedef struct {
	/* we support no fancy attr */
} pthread_mutexattr_t;

typedef struct {
	HANDLE	write_mutex;
	HANDLE	read_event;
	LONG	readers;
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

/* spin locks */
#define PTHREAD_PROCESS_PRIVATE 0
#define PTHREAD_PROCESS_SHARED 1
LIB_MY_PTHREAD_EXTRN(int pthread_spin_init(pthread_spinlock_t *lock, int pshared));
LIB_MY_PTHREAD_EXTRN(int pthread_spin_destroy(pthread_spinlock_t *lock));
LIB_MY_PTHREAD_EXTRN(int pthread_spin_lock(pthread_spinlock_t *lock));
LIB_MY_PTHREAD_EXTRN(int pthread_spin_trylock(pthread_spinlock_t *lock));
LIB_MY_PTHREAD_EXTRN(int pthread_spin_unlock(pthread_spinlock_t *lock));

/* rw locks */
LIB_MY_PTHREAD_EXTRN(int pthread_rwlock_init(pthread_rwlock_t *restrict rwlock, const pthread_rwlockattr_t *restrict attr));
LIB_MY_PTHREAD_EXTRN(int pthread_rwlock_destroy(pthread_rwlock_t *rwlock));
LIB_MY_PTHREAD_EXTRN(int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock));
LIB_MY_PTHREAD_EXTRN(int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock));
LIB_MY_PTHREAD_EXTRN(int pthread_rwlock_unlock(pthread_rwlock_t *rwlock));

/* threads */
LIB_MY_PTHREAD_EXTRN(int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg));
LIB_MY_PTHREAD_EXTRN(void pthread_exit(void *retval));
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
LIB_MY_PTHREAD_EXTRN(int pthread_cond_wait(pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex));

/* thread local storage */
LIB_MY_PTHREAD_EXTRN(int pthread_key_create(pthread_key_t *key, void (*destructor)(void*)));
LIB_MY_PTHREAD_EXTRN(int pthread_key_delete(pthread_key_t key));
#  define pthread_getspecific(key)	TlsGetValue((key))
#  define pthread_setspecific(key, val)	(TlsSetValue((key), (val)) ? 0 : -1)

/*
 * Other stuff....
 */
// typedef int pid_t;
// typedef int gid_t;

/* small fixes */
#  define getpid   _getpid
#  define getppid() (4) /* choosen by a fair dice roll... */
#  define pipe(x)  _pipe((x), 64 * 1024, _O_BINARY)
#  define srandom  srand
#  define random   rand
#  define sleep(t) Sleep(t * 1000)
#  define SHUT_WR  SD_SEND
#  define SHUT_RD  SD_RECEIVE
#  define SHUT_RDWR SD_BOTH
/* wrapper */
LIB_MY_PTHREAD_EXTRN(int getpagesize(void));
#  define PRIO_PROCESS 0
LIB_MY_PTHREAD_EXTRN(int getpriority(int which, int who));
LIB_MY_PTHREAD_EXTRN(int setpriority(int which, int who, int prio));
LIB_MY_PTHREAD_EXTRN(struct tm *localtime_r(const time_t *timep, struct tm *result));

#  define SA_SIGINFO (1<<0)
LIB_MY_PTHREAD_EXTRN(int sigemptyset(sigset_t *set));
LIB_MY_PTHREAD_EXTRN(int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact));

#  define ETIMEDOUT 7954
#  define ENOBUFS 7956
#  define EWOULDBLOCK WSAEWOULDBLOCK

#  ifdef W32_NEED_DB
#   include <sqlext.h>
#   define MAX_KEY_LEN 255
#   define MAX_VALUE_LEN 80
#   define MAX_VALUE_LEN2 160
typedef struct {
	void *dptr;
	size_t dsize;
} datum;
typedef struct {
	SQLHDBC dbc;
	SQLHSTMT stmt;
	SQLLEN rlen;
	char key_store[MAX_KEY_LEN+1];
	char val_store[MAX_VALUE_LEN];
	char error_buf[1024+1];
} DBM;
LIB_MY_PTHREAD_EXTRN(DBM *dbm_open(const char *f, int flags, int mode));
LIB_MY_PTHREAD_EXTRN(void dbm_close(DBM *));
LIB_MY_PTHREAD_EXTRN(datum dbm_fetch(DBM *, datum));
LIB_MY_PTHREAD_EXTRN(datum dbm_firstkey(DBM *));
LIB_MY_PTHREAD_EXTRN(datum dbm_nextkey(DBM *));
#   define DBM_INSERT 1
#   define DBM_REPLACE 2
LIB_MY_PTHREAD_EXTRN(int dbm_store(DBM *, datum, datum, int));
LIB_MY_PTHREAD_EXTRN(int dbm_delete(DBM *, datum));
#  endif

# endif
#endif
