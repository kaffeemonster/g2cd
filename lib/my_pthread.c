/*
 * my_pthread.c
 * cheapo pthread emulation
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

#ifdef WIN32
# define W32_NEED_DB
# define _WIN32_WINNT 0x0501
# include <windows.h>
# include <signal.h>
# include <errno.h>
# include <fcntl.h>
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
	if(!mutex)
		return EINVAL;
	InitializeCriticalSection(mutex);
	return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	if(!mutex)
		return EINVAL;
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
	if(!TryEnterCriticalSection(mutex))
		return EBUSY;
	return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
/*	if(!mutex)
		return 0;
	if(mutex->OwningThread != (HANDLE)GetCurrentThreadId()) {
		return EPERM;
	}*/
	LeaveCriticalSection(mutex);
	return 0;
}

/*
 * Spin locks
 */
int pthread_spin_init(pthread_spinlock_t *lock, int pshared GCC_ATTR_UNUSED_PARAM)
{
	if(!lock)
		return EINVAL;
	InitializeCriticalSectionAndSpinCount(lock, 0x7fffffff);
	return 0;
}

int pthread_spin_destroy(pthread_spinlock_t *lock)
{
	if(!lock)
		return EINVAL;
	DeleteCriticalSection(lock);
	return 0;
}

int pthread_spin_lock(pthread_spinlock_t *lock)
{
	EnterCriticalSection(lock);
	return 0;
}

int pthread_spin_trylock(pthread_spinlock_t *lock)
{
	if(!TryEnterCriticalSection(lock))
		return EBUSY;
	return 0;
}

int pthread_spin_unlock(pthread_spinlock_t *lock)
{
	LeaveCriticalSection(lock);
	return 0;
}


/*
 * RW locks
 * Thanks go out to the apache project for a peek into their apr rwlocks
 */
int pthread_rwlock_init(pthread_rwlock_t *restrict rwlock, const pthread_rwlockattr_t *restrict attr GCC_ATTR_UNUSED_PARAM)
{
	if(!rwlock)
		return EINVAL;

	rwlock->readers = 0;
	rwlock->read_event = CreateEvent(NULL, TRUE, FALSE, NULL);
	if(!rwlock->read_event)
		return ENOMEM;

	rwlock->write_mutex = CreateMutex(NULL, FALSE, NULL);
	if(!rwlock->write_mutex) {
		CloseHandle(rwlock->read_event);
		return ENOMEM;
	}
	return 0;
}

int pthread_rwlock_destroy(pthread_rwlock_t *rwlock)
{
	BOOL res = TRUE;
	if(!rwlock)
		return EINVAL;
	res = res && CloseHandle(rwlock->read_event);
	res = res && CloseHandle(rwlock->write_mutex);
	if(res)
		return 0;
	return EACCES;
}

int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock)
{
	DWORD res;

	/* for a trylock, change the wait time to 0 */
	res = WaitForSingleObject(rwlock->write_mutex, INFINITE);

	if(res == WAIT_FAILED || res == WAIT_TIMEOUT)
		return EAGAIN;

	InterlockedIncrement(&rwlock->readers);
	if(!ResetEvent(rwlock->read_event)) {
		ReleaseMutex(rwlock->write_mutex);
		return EAGAIN;
	}

	if(!ReleaseMutex(rwlock->write_mutex))
		return EACCES;

	return 0;
}

int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock)
{
	DWORD res;

	/* for a trylock, change the wait time  to 0 */
	res = WaitForSingleObject(rwlock->write_mutex, INFINITE);

	if(res == WAIT_FAILED || res == WAIT_TIMEOUT)
		return EAGAIN;

	/* wait for the readers to leave the section */
	if(*(volatile LONG *)&rwlock->readers)
	{
		res = WaitForSingleObject(rwlock->read_event, INFINITE);

		if(res == WAIT_FAILED || res == WAIT_TIMEOUT) {
			ReleaseMutex(rwlock->write_mutex);
			return EAGAIN;
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
		if(*(volatile LONG *)&rwlock->readers &&
		   !InterlockedDecrement(&rwlock->readers) &&
		   !SetEvent(rwlock->read_event))
			return EAGAIN;
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

	if(!idata)
		return ENOMEM;

	idata->start_routine = start_routine;
	idata->arg = arg;
	thread_h = CreateThread(NULL, attr ? attr->stacksize : 0, pthread_thread_start, idata, 0, (LPDWORD)thread);

	if(thread_h)
		CloseHandle(thread_h);
	else {
		free(idata);
		return EAGAIN;
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
	if(!GetExitCodeThread(thread_h, (LPDWORD)retval))
		ret_st = EINVAL;

	CloseHandle(thread_h);
	return ret_st;
}

/*
 * Thread attributes
 */
int pthread_attr_init(pthread_attr_t *attr)
{
	if(!attr)
		return EINVAL;
	attr->stacksize = 0;
	return 0;
}

int pthread_attr_destroy(pthread_attr_t *attr)
{
	if(!attr)
		return EINVAL;
	return 0;
}

int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize)
{
	if(!attr)
		return EINVAL;
	attr->stacksize = stacksize;
	return 0;
}

int pthread_attr_getstacksize(pthread_attr_t *attr, size_t *stacksize)
{
	if(!(attr && stacksize))
		return EINVAL;
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
	if(!cond)
		return EINVAL;
	cond->waiters_count         = 0;
	cond->wait_generation_count = 0;
	cond->release_count         = 0;
	/* Create a manual-reset event */
	cond->event = CreateEvent(NULL,  /* no security */
	                          TRUE,  /* manual-reset */
	                          FALSE, /* non-signaled initially */
	                          NULL); /* unnamed */
	if(!cond->event)
		return EACCES;
	InitializeCriticalSection(&cond->waiters_count_lock);
	return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond)
{
	if(!cond)
		return EINVAL;
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
	if(!key)
		return EINVAL;
	i = TlsAlloc();
	if(TLS_OUT_OF_INDEXES == i)
		return GetLastError();
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

#define FSUF_ACCDB ".accdb"
#define FSUF_MDB ".mdb"
#define FSUF_MAX FSUF_ACCDB
#define FSUF_NUM 2
// "Driver={Microsoft Access Driver (*.mdb, *.accdb)};DSN='';DBQ=C:\\Northwind 2007.accdb;"
#define CHUNK1 "Driver={Microsoft Access Driver ("
#define CHUNK2 ")};DSN='';DBQ="
#define DBREADONLY "READONLY=false"
#define mempcpy(dst, src, len) ((void *)((char *)memcpy(dst, src, len) + len))

static SQLHENV our_odbc_env;

#define odbc_error(x, y, z) odbc_error_int(x, y, z, __func__, __LINE__)
#define odbc_error_dbc(y, z) odbc_error(SQL_HANDLE_DBC, y, z)
#define odbc_error_stmt(y, z) odbc_error(SQL_HANDLE_STMT, y, z)
static noinline void odbc_error_int(SQLSMALLINT type, SQLHANDLE h, const char *t, const char *func, const unsigned int line)
{
	static char error_buf[1024 + 1];
	char sql_state[8];
	SQLINTEGER native_error;
	SQLSMALLINT rt_len;
	SQLRETURN res;

	res = SQLGetDiagRec(
		type,
		h,
		1,
		(SQLCHAR *)sql_state,
		&native_error, /* native error ptr */
		(SQLCHAR *)error_buf, /**/
		sizeof(error_buf),
		&rt_len
	);
	if(SQL_SUCCEEDED(res))
		logg_more(LOGF_WARN, __FILE__, func, line, 0, "%s Error (%s,%#010x): %s\n", t, sql_state, native_error, error_buf);
	else
		logg_more(LOGF_WARN, __FILE__, func, line, 0, "Error SQLGetDiagRec failed (%#06hx)\n", res);
}

static BOOLEAN try_one_connect(DBM *ret, const char *f, const char *try_suf, BOOLEAN driver_n, BOOLEAN new_db)
{
	char *con_str, *wptr;
	SQLSMALLINT cl_t;
	SQLRETURN res;
	size_t o_len, fnlen;

	o_len  = fnlen = strlen(f);
	o_len += (FSUF_NUM + 1) * (sizeof(FSUF_MAX) + 3) + str_size(CHUNK1) + str_size(CHUNK2) + +str_size(DBREADONLY) + 12;
	con_str = malloc(o_len);

	if(new_db && 0)
	{
		wptr = mempcpy(con_str, f, fnlen);
		strcpy(wptr, try_suf);
		logg_develd_old("d_str: \"%s\"\n", con_str);
		DeleteFile(con_str);
	}

	wptr = mempcpy(con_str, CHUNK1, str_size(CHUNK1));
	*wptr++ = '*';
	wptr = mempcpy(wptr, FSUF_MDB, str_size(FSUF_MDB));
	if(driver_n) {
		*wptr++ = ','; *wptr++ = ' '; *wptr++ = '*';
		wptr = mempcpy(wptr, FSUF_ACCDB, str_size(FSUF_ACCDB));
	}
	wptr = mempcpy(wptr, CHUNK2, str_size(CHUNK2));
	wptr = mempcpy(wptr, f, fnlen);
	wptr = mempcpy(wptr, try_suf, strlen(try_suf));
	*wptr++ = ';';
//	wptr = mempcpy(wptr, DBREADONLY, str_size(DBREADONLY));
//	*wptr++ = ';';
	*wptr++ = '\0';
	logg_develd_old("c_str: \"%s\"\n", con_str);

	res = SQLDriverConnect(
		ret->dbc, /* connection handle */
		NULL, /* window handle */
		(SQLCHAR *)con_str, /* in connection string */
		SQL_NTS, /* connection string length */
		NULL, /* out connection string buffer */
		0, /* size of the out connection string buffer */
		&cl_t, /* length of the out connection string */
		SQL_DRIVER_NOPROMPT
	);
	free(con_str);

	if(!SQL_SUCCEEDED(res)) {
		odbc_error_dbc(ret->dbc, "SQLDriverConnect");
		return FALSE;
	}
	return TRUE;
}

static noinline BOOLEAN init_db_environ(void)
{
	/* Allocate an enviroment handle */
	if(SQL_NULL_HANDLE == our_odbc_env)
	{
		SQLRETURN res = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &our_odbc_env);
		if(!SQL_SUCCEEDED(res))
			return FALSE;
		res = SQLSetEnvAttr(our_odbc_env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
		if(!SQL_SUCCEEDED(res))
			return FALSE;
	}
	return TRUE;
}

#define QUERY_CREAT "CREATE TABLE g2cd_data (mkey VARCHAR("str_it(MAX_KEY_LEN)") PRIMARY KEY, mvalue VARCHAR("str_it(MAX_VALUE_LEN2)"));"
DBM *dbm_open(const char *f, int flags, int mode GCC_ATTR_UNUSED_PARAM)
{
	char *rptr, *wptr;
	DBM *ret;
	SQLHSTMT stmt;
	SQLLEN len;
	SQLRETURN res;
	BOOLEAN new_db = O_TRUNC == (flags & O_TRUNC), is_f_orig = TRUE;

	if(!init_db_environ())
		return NULL;

	if('.' == f[0])
	{
		size_t t_len = 128, fnlen = strlen(f) + 1;
		wptr = NULL;

		do
		{
			free(wptr);
			wptr = malloc(t_len + fnlen);
			if(!wptr)
				return NULL;
			rptr = _getcwd(wptr, t_len);
			t_len *= 2;
		} while(!rptr);
		f = strcat(strcat(rptr, "\\"), f);
		is_f_orig = FALSE;
	}
	if((wptr = strchr(f, '/')))
	{
		size_t fnlen = strlen(f) + 1;
		rptr = malloc(fnlen);
		if(!rptr)
			return NULL;
		memcpy(rptr, f, fnlen);
		wptr = &rptr[wptr - f];
		if(!is_f_orig)
			free((void *)(intptr_t)f);
		f = rptr;
		is_f_orig = FALSE;
		do {
			*wptr++ = '\\';
		} while((wptr = strchr(wptr, '/')));
	}

	ret = calloc(1, sizeof(*ret));
	if(!ret)
		return NULL;

	/* Allocate a connection handle */
	res = SQLAllocHandle(SQL_HANDLE_DBC, our_odbc_env, &ret->dbc);
	if(!SQL_SUCCEEDED(res))
		goto out_free;

	if(!try_one_connect(ret, f, FSUF_ACCDB, TRUE, new_db)) {
		if(!try_one_connect(ret, f, FSUF_MDB, TRUE, new_db)) {
			if(!try_one_connect(ret, f, FSUF_MDB, FALSE, new_db))
				goto out_freeh;
		}
	}

	res = SQLSetConnectAttr(ret->dbc, SQL_ATTR_ACCESS_MODE, (SQLPOINTER)SQL_MODE_READ_WRITE, 0);
	if(!SQL_SUCCEEDED(res)) {
		logg_devel("SQLSetConnectAttr failed\n");
		goto out_freed;
	}

	res = SQLAllocHandle(SQL_HANDLE_STMT, ret->dbc, &stmt);
	if(!SQL_SUCCEEDED(res)) {
		logg_devel("SQLAllocHandle failed\n");
		goto out_freed;
	}

	res = SQLTables(stmt,
		NULL, /* Catalog */
		SQL_NTS,
		NULL, /* schema */
		SQL_NTS,
		(SQLCHAR *)(intptr_t)"g2cd_data", /* table name */
		SQL_NTS,
		(SQLCHAR *)(intptr_t)"TABLE", /* table type */
		SQL_NTS
	);
	if(!SQL_SUCCEEDED(res)) {
		odbc_error_stmt(stmt, "SQLTables\n");
		goto out_frees;
	}

	res = SQLRowCount(stmt, &len);
	if(!SQL_SUCCEEDED(res)) {
		logg_devel("SQLRowCount failed\n");
		goto out_frees;
	}

	if(-1 == len)
	{
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);

		res = SQLAllocHandle(SQL_HANDLE_STMT, ret->dbc, &stmt);
		if(!SQL_SUCCEEDED(res)) {
			logg_devel("SQLAllocHandle failed\n");
			goto out_frees;
		}

		res = SQLExecDirect(stmt, (SQLCHAR *)(intptr_t)QUERY_CREAT, str_size(QUERY_CREAT));
		if(!SQL_SUCCEEDED(res)) {
			char sql_state[8];
			SQLINTEGER native_error;
			SQLSMALLINT rt_len;

			res = SQLGetDiagRec(
				SQL_HANDLE_STMT,
				stmt,
				1,
				(SQLCHAR *)sql_state,
				&native_error, /* native error ptr */
				(SQLCHAR *)ret->error_buf, /**/
				sizeof(ret->error_buf),
				&rt_len
			);
			if(SQL_SUCCEEDED(res)) {
				if(strcmp("42S01", sql_state) != 0) {
						logg_posd(LOGF_WARN, "Error in SQLExecDirect (%s,%#010x): %s\n", sql_state, native_error, ret->error_buf);
					goto out_frees;
				}
			} else
				goto out_frees;
		}
	}
	SQLFreeHandle(SQL_HANDLE_STMT, stmt);

	if(O_RDONLY == (flags & (O_RDONLY|O_WRONLY|O_RDWR)))
		res = SQLSetConnectAttr(ret->dbc, SQL_ATTR_ACCESS_MODE, (SQLPOINTER)SQL_MODE_READ_ONLY, 0);

	if(!is_f_orig)
		free((void *)(intptr_t)f);

	return ret;
out_frees:
	SQLFreeHandle(SQL_HANDLE_STMT, stmt);
out_freed:
	SQLDisconnect(ret->dbc);
out_freeh:
	SQLFreeHandle(SQL_HANDLE_DBC, ret->dbc);
out_free:
	free(ret);
	if(!is_f_orig)
		free((void *)(intptr_t)f);
	return NULL;
}

void dbm_close(DBM *db)
{
	if(db->stmt != SQL_NULL_HSTMT)
		SQLFreeHandle(SQL_HANDLE_STMT, db->stmt);
	SQLDisconnect(db->dbc);
	SQLFreeHandle(SQL_HANDLE_DBC, db->dbc);
	db->dbc = SQL_NULL_HDBC;
	free(db);
}

#define QUERY_SELV "SELECT mvalue FROM g2cd_data WHERE mkey = ?;"
datum dbm_fetch(DBM *db, datum key)
{
	signed char receive_buf[MAX_VALUE_LEN * 2 + 1];
	datum value = {NULL, 0};
	char *wptr;
	SQLHSTMT stmt;
	SQLRETURN res;
	SQLLEN len;
	SQLLEN rlen = SQL_NULL_DATA;
	size_t i;

	if(db->stmt != SQL_NULL_HSTMT) {
		SQLFreeHandle(SQL_HANDLE_STMT, db->stmt);
		db->stmt = SQL_NULL_HSTMT;
	}

	res = SQLAllocHandle(SQL_HANDLE_STMT, db->dbc, &stmt);
	if(!SQL_SUCCEEDED(res)) {
		odbc_error_dbc(db->dbc, "SQLAllocHandle");
		goto out;
	}

	logg_develd("key: \"%s\"\n", key.dptr);
//HACK: we know the key are strings
	len = key.dsize-1;
	res = SQLBindParameter(
		stmt, /* statement handle */
		1, /* parameter number */
		SQL_PARAM_INPUT, /* param in/out type */
		SQL_C_CHAR, /* C data type */
		SQL_VARCHAR, /* SQL data type */
		MAX_KEY_LEN, /* column size */
		0, /* decimal digits */
		key.dptr, /* parameter value ptr */
		key.dsize-1, /* buffer len */
		&len /* strlen or indp */
	);
	if(!SQL_SUCCEEDED(res)) {
		odbc_error_stmt(stmt, "SQLBindParameter");
		goto out_freeh;
	}

	res = SQLBindCol(stmt, 1, SQL_C_CHAR, receive_buf, sizeof(receive_buf), &rlen);
	if(!SQL_SUCCEEDED(res)) {
		odbc_error_stmt(stmt, "SQLBindCol");
		goto out_freeh;
	}

	res = SQLExecDirect(stmt, (SQLCHAR *)(intptr_t)QUERY_SELV, str_size(QUERY_SELV));
	if(!SQL_SUCCEEDED(res)) {
		odbc_error_stmt(stmt, "SQLExecDirect");
		goto out_freeh;
	}

	res = SQLFetch(stmt);
	if(!SQL_SUCCEEDED(res)) {
		odbc_error_stmt(stmt, "SQLFetch");
		goto out_freeh;
	}

	if(SQL_NULL_DATA == rlen)
		goto out_freeh;

	value.dptr  = &db->val_store;
	value.dsize = (size_t)rlen / 2;
	for(i = 0, wptr = value.dptr; i < (size_t)rlen; i += 2)
	{
		signed char c, b;
		b  = receive_buf[i] - ('a' - 10);
		b  = b >= 0 ? b : b + 39;
		c  = b << 4;
		b  = receive_buf[i+1] - ('a' - 10);
		b  = b >= 0 ? b : b + 39;
		c |= b;
		wptr[i/2] = c;
	}

out_freeh:
	SQLFreeHandle(SQL_HANDLE_STMT, stmt);
out:
	return value;
}

#define QUERY_KEY "SELECT mkey FROM g2cd_data ORDER BY mkey;"
datum dbm_firstkey(DBM *db)
{
	datum value = {NULL, 0};
	SQLRETURN res;

	if(db->stmt != SQL_NULL_HSTMT)
		SQLFreeHandle(SQL_HANDLE_STMT, db->stmt);

	res = SQLAllocHandle(SQL_HANDLE_STMT, db->dbc, &db->stmt);
	if(!SQL_SUCCEEDED(res)) {
		odbc_error_dbc(db->dbc, "SQLAllocHandle");
		goto out;
	}

	res = SQLBindCol(db->stmt, 1, SQL_C_CHAR, db->key_store, sizeof(db->key_store), &db->rlen);
	if(!SQL_SUCCEEDED(res)) {
		odbc_error_stmt(db->stmt, "SQLBindCol");
		goto out_freeh;
	}

	res = SQLExecDirect(db->stmt, (SQLCHAR *)(intptr_t)QUERY_KEY, str_size(QUERY_KEY));
	if(!SQL_SUCCEEDED(res)) {
		odbc_error_stmt(db->stmt, "SQLExecDirect");
		goto out_freeh;
	}

	res = SQLFetch(db->stmt);
	if(SQL_NO_DATA == res)
		goto out_freeh;
	if(!SQL_SUCCEEDED(res)) {
		odbc_error_stmt(db->stmt, "SQLFetch");
		goto out_freeh;
	}

	if(SQL_NULL_DATA == db->rlen || -1 == db->rlen)
		goto out_freeh;

	value.dptr  = db->key_store;
	value.dsize = db->rlen;

out:
	return value;
out_freeh:
	SQLFreeHandle(SQL_HANDLE_STMT, db->stmt);
	db->stmt = SQL_NULL_HSTMT;
	goto out;
}

datum dbm_nextkey(DBM *db)
{
	datum value = {NULL, 0};
	SQLRETURN res;

	if(db->stmt == SQL_NULL_HSTMT)
		goto out;

	res = SQLFetch(db->stmt);
	if(SQL_NO_DATA == res)
		goto out_freeh;
	if(!SQL_SUCCEEDED(res)) {
		odbc_error_stmt(db->stmt, "SQLFetch");
		goto out_freeh;
	}

	if(SQL_NULL_DATA == db->rlen || -1 == db->rlen)
		goto out_freeh;

	value.dptr  = db->key_store;
	value.dsize = db->rlen;

out:
	return value;
out_freeh:
	SQLFreeHandle(SQL_HANDLE_STMT, db->stmt);
	db->stmt = SQL_NULL_HSTMT;
	goto out;
}

#define QUERY_UPD "UPDATE g2cd_data SET mvalue= ? WHERE mkey = ?;"
#define QUERY_INS "INSERT INTO g2cd_data VALUES(?, ?);"
int dbm_store(DBM *db, datum key, datum content, int store_mode)
{
	unsigned char send_buf[MAX_VALUE_LEN * 2 + 1];
	SQLHSTMT stmt;
	SQLRETURN res;
	SQLLEN klen, clen, len;
	int ret = -1;

	if(store_mode & ~(DBM_INSERT|DBM_REPLACE)) {
		errno = EINVAL;
		return ret;
	}
	if((store_mode & (DBM_INSERT|DBM_REPLACE)) == (DBM_INSERT|DBM_REPLACE)) {
		errno = EINVAL;
		return ret;
	}
	if(content.dsize > MAX_VALUE_LEN || key.dsize-1  > MAX_KEY_LEN) {
		errno = EINVAL;
		return ret;
	}

	if(db->stmt != SQL_NULL_HSTMT) {
		SQLFreeHandle(SQL_HANDLE_STMT, db->stmt);
		db->stmt = SQL_NULL_HSTMT;
	}

	res = SQLAllocHandle(SQL_HANDLE_STMT, db->dbc, &stmt);
	if(!SQL_SUCCEEDED(res))
		goto out;

	*to_base16(send_buf, content.dptr, content.dsize) = '\0';

	if(store_mode & DBM_REPLACE)
	{
		clen = content.dsize * 2;
		res = SQLBindParameter(
			stmt, /* statement handle */
			1, /* parameter number */
			SQL_PARAM_INPUT, /* param in/out type */
			SQL_C_CHAR, /* C data type */
			SQL_VARCHAR, /* SQL data type */
			MAX_VALUE_LEN, /* column size */
			0, /* decimal digits */
			send_buf, /* parameter value ptr */
			content.dsize * 2, /* buffer len */
			&clen /* strlen or indp */
		);
		if(!SQL_SUCCEEDED(res)) {
			logg_devel("SQLBindParameter failed\n");
			goto out_freeh;
		}

//HACK: we know the key are strings
		klen = key.dsize-1;
		res = SQLBindParameter(
			stmt, /* statement handle */
			2, /* parameter number */
			SQL_PARAM_INPUT, /* param in/out type */
			SQL_C_CHAR, /* C data type */
			SQL_VARCHAR, /* SQL data type */
			MAX_KEY_LEN, /* column size */
			0, /* decimal digits */
			key.dptr, /* parameter value ptr */
			key.dsize-1, /* buffer len */
			&klen /* strlen or indp */
		);
		if(!SQL_SUCCEEDED(res)) {
			logg_devel("SQLBindParameter failed\n");
			goto out_freeh;
		}

		res = SQLExecDirect(stmt, (SQLCHAR *)(intptr_t)QUERY_UPD, str_size(QUERY_UPD));
		/* when a update/insert/delete affected no data, we get SQL_NO_DATA */
		if(SQL_NO_DATA != res)
		{
			if(!SQL_SUCCEEDED(res)) {
				odbc_error_stmt(stmt, "SQLDriverConnect");
				goto out_freeh;
			}

			res = SQLRowCount(stmt, &len);
			if(!SQL_SUCCEEDED(res)) {
				logg_devel("SQLRowCount failed\n");
				goto out_freeh;
			}

			if(len != -1) {
				ret = 0;
				goto out_freeh;
			}
		}
		res = SQLFreeStmt(stmt, SQL_CLOSE);
		res = SQLFreeStmt(stmt, SQL_UNBIND);
		res = SQLFreeStmt(stmt, SQL_RESET_PARAMS);
	}

//HACK: we know the key are strings
	klen = key.dsize-1;
	res = SQLBindParameter(
		stmt, /* statement handle */
		1, /* parameter number */
		SQL_PARAM_INPUT, /* param in/out type */
		SQL_C_CHAR, /* C data type */
		SQL_VARCHAR, /* SQL data type */
		MAX_KEY_LEN, /* column size */
		0, /* decimal digits */
		key.dptr, /* parameter value ptr */
		key.dsize-1, /* buffer len */
		&klen /* strlen or indp */
	);
	if(!SQL_SUCCEEDED(res)) {
		logg_devel("SQLBindParameter failed\n");
		goto out_freeh;
	}

	clen = content.dsize * 2;
	res = SQLBindParameter(
		stmt, /* statement handle */
		2, /* parameter number */
		SQL_PARAM_INPUT, /* param in/out type */
		SQL_C_CHAR, /* C data type */
		SQL_VARCHAR, /* SQL data type */
		MAX_VALUE_LEN, /* column size */
		0, /* decimal digits */
		send_buf, /* parameter value ptr */
		content.dsize * 2, /* buffer len */
		&clen /* strlen or indp */
	);
	if(!SQL_SUCCEEDED(res)) {
		logg_devel("SQLBindParameter failed\n");
		goto out_freeh;
	}

	res = SQLExecDirect(stmt, (SQLCHAR *)(intptr_t)QUERY_INS, str_size(QUERY_INS));
	if(!SQL_SUCCEEDED(res)) {
		odbc_error_stmt(stmt, "SQLDriverConnect");
		goto out_freeh;
	}

	res = SQLRowCount(stmt, &len);
	if(!SQL_SUCCEEDED(res)) {
		logg_devel("SQLRowCount failed\n");
		goto out_freeh;
	}

	if(len != -1)
		ret = 0;

out_freeh:
	SQLFreeHandle(SQL_HANDLE_STMT, stmt);
out:
	return ret;
}

#define QUERY_DEL "DELETE FROM g2cd_data WHERE mkey = ?;"
int dbm_delete(DBM *db, datum key)
{
	SQLHSTMT stmt;
	SQLRETURN res;
	SQLLEN len;
	int ret = -1;

	if(db->stmt != SQL_NULL_HSTMT) {
		SQLFreeHandle(SQL_HANDLE_STMT, db->stmt);
		db->stmt = SQL_NULL_HSTMT;
	}

	res = SQLAllocHandle(SQL_HANDLE_STMT, db->dbc, &stmt);
	if(!SQL_SUCCEEDED(res))
		goto out;

//HACK: we know the key are strings
	len = key.dsize-1;
	res = SQLBindParameter(
		stmt, /* statement handle */
		1, /* parameter number */
		SQL_PARAM_INPUT, /* param in/out type */
		SQL_C_CHAR, /* C data type */
		SQL_VARCHAR, /* SQL data type */
		MAX_KEY_LEN, /* column size */
		0, /* decimal digits */
		key.dptr, /* parameter value ptr */
		key.dsize-1, /* buffer len */
		&len /* strlen or indp */
	);
	if(!SQL_SUCCEEDED(res))
		goto out_freeh;

	res = SQLExecDirect(stmt, (SQLCHAR *)(intptr_t)QUERY_DEL, str_size(QUERY_DEL));
	/* when a update/insert/delete affected no data, we get SQL_NO_DATA */
	if(!SQL_SUCCEEDED(res))
		goto out_freeh;

	res = SQLRowCount(stmt, &len);
	if(!SQL_SUCCEEDED(res))
		goto out_freeh;

	if(len != -1)
		ret = 0;

out_freeh:
	SQLFreeHandle(SQL_HANDLE_STMT, stmt);
out:
	return ret;
}

/*@unused@*/
static char const rcsid_mpth[] GCC_ATTR_USED_VAR = "$Id:$";
#endif
/* EOF */
