
#ifndef IA64_H
# define IA64_H

# define SOULL (sizeof(unsigned long long))
# define SOULLM1 (SOULL - 1)

static inline unsigned long long psad1(unsigned long long r2, unsigned long long r3)
{
	unsigned long long res;
	asm("psad1	%0=%r1, %r2" : "=r" (res) : "rO" (r2), "rO" (r3));
	return res;
}

static inline unsigned long long mux1_shuf(unsigned long long r2)
{
	unsigned long long res;
	asm("mux1	%0=%r1, @shuf" : "=r" (res) : "r" (r2));
	return res;
}

static inline unsigned long long mix1l(unsigned long long r2, unsigned long long r3)
{
	unsigned long long res;
	asm("mix1.l	%0=%r1, %r2" : "=r" (res) : "rO" (r2), "rO" (r3));
	return res;
}

static inline unsigned long long mix1r(unsigned long long r2, unsigned long long r3)
{
	unsigned long long res;
	asm("mix1.r	%0=%r1, %r2" : "=r" (res) : "rO" (r2), "rO" (r3));
	return res;
}

static inline unsigned long long unpack1l(unsigned long long r2, unsigned long long r3)
{
	unsigned long long res;
	asm("unpack1.l	%0=%r1, %r2" : "=r" (res) : "rO" (r2), "rO" (r3));
	return res;
}

static inline unsigned long long unpack1h(unsigned long long r2, unsigned long long r3)
{
	unsigned long long res;
	asm("unpack1.h	%0=%r1, %r2" : "=r" (res) : "rO" (r2), "rO" (r3));
	return res;
}

static inline unsigned long long unpack4l(unsigned long long r2, unsigned long long r3)
{
	unsigned long long res;
	asm("unpack4.l	%0=%r1, %r2" : "=r" (res) : "rO" (r2), "rO" (r3));
	return res;
}

static inline unsigned long long unpack4h(unsigned long long r2, unsigned long long r3)
{
	unsigned long long res;
	asm("unpack4.h	%0=%r1, %r2" : "=r" (res) : "rO" (r2), "rO" (r3));
	return res;
}

static inline unsigned long long pmpy2r(unsigned long long r2, unsigned long long r3)
{
	unsigned long long res;
	asm("pmpy2.r	%0=%r1, %r2" : "=r" (res) : "%rO" (r2), "rO" (r3));
	return res;
}

static inline unsigned long long pmpy2l(unsigned long long r2, unsigned long long r3)
{
	unsigned long long res;
	asm("pmpy2.l	%0=%r1, %r2" : "=r" (res) : "%rO" (r2), "rO" (r3));
	return res;
}

static inline unsigned long long pcmp1eq(unsigned long long r2, unsigned long long r3)
{
	unsigned long long res;
	asm("pcmp1.eq	%0=%r1, %r2" : "=r" (res) : "%rO" (r2), "rO" (r3));
	return res;
}

static inline unsigned long long pcmp1gt(long long r2, long long r3)
{
	unsigned long long res;
	asm("pcmp1.gt	%0=%r1, %r2" : "=r" (res) : "%rO" (r2), "rO" (r3));
	return res;
}

static inline unsigned long long czx1(unsigned long long a)
{
	unsigned long long res;
	if(!HOST_IS_BIGENDIAN)
		asm("czx1.r	%0=%1" : "=r" (res) : "r" (a));
	else
		asm("czx1.l	%0=%1" : "=r" (res) : "r" (a));
	return res;
}

static inline unsigned long long czx2(unsigned long long a)
{
	unsigned long long res;
	if(!HOST_IS_BIGENDIAN)
		asm("czx2.r	%0=%1" : "=r" (res) : "r" (a));
	else
		asm("czx2.l	%0=%1" : "=r" (res) : "r" (a));
	return res;
}

#endif
