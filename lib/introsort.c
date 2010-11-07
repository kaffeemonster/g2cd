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
 *
 * rewritten by me for uint32_t and C by me, so all errors are
 * mine - Jan
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
// TODO: this could be rewritten (Floyd??)
/*
 * this way we get fewer compares, but "walk" the array 3 times...
 * so the question is how this hits the caches, esp. since
 * this should only be the fallback on deep recursion, so small
 * array sizes
 */
static void downheap_u32(uint32_t a[], size_t i, size_t n)
{
	uint32_t d = a[i - 1];
	size_t child;

	while((child = i * 2) <= n)
	{
		if(child < n && a[child - 1] < a[child])
			child++;
		if(!(d < a[child - 1]))
			break;
		a[i - 1] = a[child - 1];
		i = child;
	}
	a[i - 1] = d;
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
		while(j && t < a[j - 1]) {
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
	size_t p, n_s, n_b;
	uint32_t *a_s, *a_b;

	/*
	 * only pre sort, where quick sort is most effective,
	 * final run will be something else
	 */
	if(n <= SIZE_THRESHOLD)
		return;

	/* throttle it? */
	if(unlikely(!depth_limit))
	{
		/*
		 * we left the "optimal" recursion depth, so we are prop. heading
		 * for the worst case, quickly switch to some other sorting
		 * alg. for this partition
		 */
		heapsort_u32(a, n);
		return;
	}

	p = partition_u32(a, n, medianof3_u32(a, n));
	/*
	 * recurse into the smaller partition first (costs _real_
	 * stack), 'cause it should be easier to sort, while the
	 * bigger partition uses the tail recursion eliminated path
	 */
	if(n - p < p) {
		a_s = a + p; n_s = n - p;
		a_b = a;     n_b = p;
	} else {
		a_s = a;     n_s = p;
		a_b = a + p; n_b = n - p;
	}
	introsort_loop_u32(a_s, n_s, depth_limit - 1);
	/*
	 * we could write a loop, but keep tail recursion for simplycity
	 * the compiler should be able to eliminate it
	 * this way we also do keep track of the depth limit
	 */
	introsort_loop_u32(a_b, n_b, depth_limit - 1);
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
