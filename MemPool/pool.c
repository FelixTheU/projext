#include "mcsMpool.h"
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*#define USE_MPOOL*/

#ifdef USE_MPOOL
#	define MALLOC		mcs_mpool_malloc
#	define FREE			mcs_mpool_free
#else
#	define MALLOC		malloc
#	define FREE			free
#endif

#define MALLOC_TIMES_PER_TEST	2000
#define TEST_TIMES				1500

void doMallocTest()
{
	int i = 0;
	int size = 0;
	void *parr[MALLOC_TIMES_PER_TEST] =
	{ 0 };
	for (i = 0; i < MALLOC_TIMES_PER_TEST; i++)
	{
		size = rand() % (1 << 16) + 1;
		parr[i] = MALLOC(size);
		// printf("大小: %d(%.0fk) 分配在了:%p.\n", size, size / 1024.0, parr[i]);

		if (NULL == parr[i])
		{
			printf("大小: %d(%.0fk), 内存分配失败.\n", size, size / 1024.0);
		}
		else
		{
			memset(parr[i], 255, size);
		}
	}

	for (i = 0; i < MALLOC_TIMES_PER_TEST; i++)
	{
		FREE(parr[MALLOC_TIMES_PER_TEST - i - 1]);
	}
}

int main()
{
	int lSts = 0;
	int lRet = 0;
	int i = 0;

#ifdef USE_MPOOL
	lSts = mcs_mpool_init();
#endif

	time_t tBegin = time(NULL);

	for (; i < TEST_TIMES; i++)
	{
		doMallocTest();
	}
	time_t tEnd = time(NULL);
	printf("耗时: %ld.\n", tEnd - tBegin);

#ifdef USE_MPOOL
	mcs_mpool_fini();
#endif

	lRet = lSts;
	return lRet;
}
