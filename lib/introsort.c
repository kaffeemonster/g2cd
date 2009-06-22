/*
 * this is all quite PD, nothing magic:
 * search introsort
 * search quicksort
 * search heapsort
 * search insertsort
 * glue it all together
 *
 * Thanks go to Ralph Unden for a paper about intro sort with
 * concrete java demonstration code.
 */

#include "../config.h"
#include "other.h"
#include "my_bitops.h"

#define SIZE_THRESHOLD 4

/*
 * introsort_u32 - sort an array of u32 with intro sort
 *
 * This sort tries to eliminate duplicates, it does not remove
 * every possible dublicate, but some.
 *
 * a[]: array of u32 to sort
 * n: number of u32 in a
 *
 * return value: number of "valid" output elements
 *
 * Intro sort is a variant of quick sort, which tries to fall back
 * to another sorting algorithm, when it would hit the worst case
 * for quick sort (O(n^2)).
 * Rest is quite of the mill.
 * Additionaly we try to remove duplicates. This is not 100%, but at
 * least should find some to reduce the number of elements in the output.
 */

/*
 * Common helper
 */
static inline void exchange_u32(uint32_t *a, uint32_t *b)
{
	uint32_t t = *a;
	*a = *b;
	*b = t;
}

/*
 * heap sort as worst case fallback
 */
static void downheap_u32(uint32_t a[], size_t i, size_t n)
{
	uint32_t d = a[i - 1];
	size_t child;

	while(i <= n / 2)
	{
		child = 2 * i;
		if(child < n && a[child - 1] < a [child])
			child++;
		if(!(d < a[child - 1]))
			break;
		a[i - 1] = a[child - 1];
		i = child;
	}
	a[i - 1] = d ;
}

static void heapsort_u32(uint32_t a[], size_t n)
{
	size_t i;
	for(i = n / 2; i >= 1; i--)
		downheap_u32(a, i, n);
	for(i = n; i > 1; i--) {
		exchange_u32(a, a + i - 1);
		downheap_u32(a, 1, i - 1);
	}
}

/*
 * insertion sort which tries to removes duplicates
 */
static size_t insertionsort_u32_c(uint32_t a[], size_t n)
{
	size_t i, j, k;
	uint32_t t;

	/* n can not be 0 here */
	i = 0; k = 0;
	goto start_loop;
	do
	{
		/*
		 * Since i don't see any super intelligent way to remove
		 * all duplicates, this simple check should remove most,
		 * since our input array is rougly presorted.
		 */
		if(a[i] == a[k - 1] ||
		   a[i] == t)
			continue;
start_loop:
		j = k;
		t = a[i];
		while(j && t < a[j - 1])
		{
			a[j] = a[j - 1];
			j--;
		}
		a[j] = t;
		k++;
	} while(++i < n);
	return k;
}

/*
 * intro sort
 */
static uint32_t medianof3_u32(uint32_t a[], size_t n)
{
	uint32_t lo = *a, mid = a[n / 2 + 1], hi = a[n - 1];
	return mid < lo ? (hi < mid ? mid : (hi < lo ? hi : lo)) :
	                  (hi < mid ? (hi < lo ? lo : hi) : mid);
}

static size_t partition_u32(uint32_t a[], size_t j, uint32_t x)
{
	size_t i = 0;
	while(1)
	{
		while(a[i] < x)
			i++;
		j--;
		while(x < a[j])
			j--;
		if(!(i < j))
			return i;
		exchange_u32(a + i, a + j);
		i++;
	}
}

static void introsort_loop_u32(uint32_t a[], size_t n, unsigned depth_limit)
{
	/*
	 * only pre sort, where quick sort is most effective,
	 * final run will be something else
	 */
	while(n > SIZE_THRESHOLD)
	{
		size_t p;
		if(!depth_limit) {
			heapsort_u32(a, n); /* we prop. hit the worst case */
			return;
		}
		p = partition_u32(a, n, medianof3_u32(a, n));
		introsort_loop_u32(a + p, n - p, depth_limit - 1);
		n = p;
	}
}

size_t introsort_u32(uint32_t a[], size_t n)
{
	if(n < 2)
		return n;
	introsort_loop_u32(a, n, flsst(n) - 1 /* floor(ld(n)) */);
	return insertionsort_u32_c(a, n);
}

/*@unused@*/
static char const rcsid_is[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
