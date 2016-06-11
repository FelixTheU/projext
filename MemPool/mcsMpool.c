/****************************************************************************************
 * 文 件 名 : yv00bspmcs_msg_mem_pool.c
 * 项目名称 : YVGA1207001A
 * 模 块 名 : mcs模块
 * 功    能 : mcs消息内存池
 * 操作系统 : LINUX
 * 修改记录 : 无
 * 版    本 : Rev 0.1.0
 *- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * 设    计 : zhengsw      '2016-05-27
 * 编    码 : zhengsw      '2016-05-27
 * 修    改 :
 ****************************************************************************************
 *- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * 公司介绍及版权说明
 *
 *           (C)Copyright 2012 YView    Corporation All Rights Reserved.
 *- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 ***************************************************************************************/
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <sys/socket.h>
#include <pthread.h>		/* 扶持多线程使用, 加锁 */
#include <stdio.h>

#include "list.h"
#include "mcsMpool.h"
#include "ftool.h"

#define DEBUG_INFO(...) printf(__VA_ARGS__);

extern struct globe_config config;				/* 全局配置 */

#define FRESH_MEM_MARK							(0xcc)			/* 未使用的内存标记 */
#define USED_CELL_MEM_MARK						(void *)(-1)
#define MAX_POWER_TO_SUPPORT					MPOOL_MAX_POWER
/*
 * 是否对未使用的内存进行标记
 * 开启后性能下降明显,将会输给 malloc
 * 不开启时相比 malloc 性能约提升 26%
#define MARK_UNUSED_MEM
*/

/*
 * 每种尺寸的内存单元预分配数目
 */
static int g_NumPerCell[MPOOL_MAX_POWER + 1] = {
/* byte */		/* 1 */50, 		/* 2 */50, 		/* 4 */50, 		/* 8 */50,
/* byte */		/* 16 */50, 	/* 32 */50, 	/* 64 */2000, 	/* 128 */400,
/* byte */		/* 256 */2000, 	/* 512 */400,
				/* 1k */500, 	/* 2k */500, 	/* 4k */500, 	/* 8k */500,
				/* 16k */500,	/* 32k */1000,  /* 64k */3000
};

struct stMemBlock;
struct stAddressHitInfo;

/*
 * 地址范围
 */
typedef struct stAddressRange
{
	void *begin;							/* 起始地址 */
	void *end;								/* 终止地址 */
} ST_AddressRange;

/*
 * 地址击中测试
 */
#define ADDRESS_HIT(p, pBlock) 			(p >= pBlock->addrRange.begin && p <= pBlock->addrRange.end)
#define ADDRESS_LOWER(p, pBlock) 		(p < pBlock->addrRange.begin)
#define ADDRESS_HIGHER(p, pBlock)		(p > pBlock->addrRange.end)
#define ADDRESS_HIT2(p, pBlock) 		ADDRESS_LOWER(p, pBlock) ? -1 : \
										(ADDRESS_HIGHER(p, pBlock) ? 1 : 0)

/* block */
typedef struct stMemBlock
{
	ST_AddressRange addrRange;				/* 地址范围 */
	int lCellSize;							/* 单元尺寸 */
	int lCellNums;							/* 单元数目 */
	struct list_head ceilList;				/* cell链表  */
	struct list_head *pFreeList;			/* 空闲 cell 链表, 指向 ceilList 中的空闲单元 */
	struct list_head list;					/* 链接 ST_MemBlock(自身) */
	pthread_mutex_t		memBlockMutex;		/* 保护 pFreeList, 防止多线程对同一个内存块同时进行 malloc 和 free 操作 */
#ifdef DEBUG
	int lUsedNums;							/* 已使用的量 */
#endif /* DEBUG */
} ST_MemBlock;

/*
 * 地址所属 MemBlock 判定结构
 * 说明:	本来各个块的内存是顺次申请的,它们应该是递增的,
 * 		但不能这样假设,因为可能当分配到中间某部分内存
 * 		需要较少,导致malloc 使用了前面较小地址的内存,
 * 		或者因为发生了回收.
 * 补充:	已知 malloc 出来的小块内存和大块内存(1k 以上)地址是在不同的内存区段
 */
typedef struct stAddressHitInfo
{
	ST_AddressRange addrRange;				/* 地址范围 */
	ST_MemBlock		*pMemBlock;				/* 地址所属的 MemBlock */
} ST_AddressHitInfo;

/*
 * 实现用到的文件内数据
 */
typedef struct stImpDatas
{
	/* 所需尺寸与其供应内存块映射表
	 * 2^(n - 1) + 1 ~ 2^n 的尺寸由 memBlockMap[n]内存块分配
	 *  */
	struct stMemBlock *memBlockMap[MAX_POWER_TO_SUPPORT + 1];

	/* 地址击中测试映射表
	 * 给定一个地址对分查找该结构判定其来自哪一个内存块
	 *  */
	struct stAddressHitInfo addrHitMap[MAX_POWER_TO_SUPPORT + 1];
} ST_ImpDatas;

static ST_ImpDatas __IMP = {
		.memBlockMap = { 0 },
		.addrHitMap =
		{
				/* ST_AddressHitInfo */{
						/* ST_AddressRange */{NULL, NULL}
						, NULL}
		}};

/* 内存单元结点 */
typedef struct stCellNode
{
	void *pMem;							/* 内存单元地址 */
	struct list_head list;				/* 将结点链接起来 */
} ST_CellNode;

/*
 * 初始化 block 中的内存单元链表
 */
static
int InitMemBlockCellList(ST_MemBlock *pBlock)
{
	int lRet = 0;
	int i = 0;
	ST_CellNode *pFreeCellNode = NULL;

	for (i = 0; i < pBlock->lCellNums; i++)
	{
		pFreeCellNode = malloc(sizeof (ST_CellNode));
		GOTO_ON_ERR(NULL == pFreeCellNode, lRet, -errno, out);

		INIT_LIST_HEAD(&pFreeCellNode->list);
		pFreeCellNode->pMem = pBlock->addrRange.begin + i * pBlock->lCellSize;
		assert(ADDRESS_HIT(pFreeCellNode->pMem, pBlock));

		list_add_tail(&pFreeCellNode->list, &pBlock->ceilList);
	}

	/* 整个视作空闲 */
	pBlock->pFreeList = pBlock->ceilList.next;	/* 指向第一个空闲 */

#if 0
	DEBUG_INFO("block cell size: %d, cell num: %d, block addr:%p -> %p.\n",
			pBlock->lCellSize, pBlock->lCellNums, pBlock->addrRange.begin, pBlock->addrRange.end);
	DEBUG_INFO("first cell:addr:%p, mem:%p.\n",
			pBlock->ceilList.next, list_entry(pBlock->ceilList.next, ST_CellNode, list)->pMem);
	DEBUG_INFO("first cell:addr:%p, mem:%p.\n",
			pBlock->ceilList.prev, list_entry(pBlock->ceilList.prev, ST_CellNode, list)->pMem);
#endif
out:
	return lRet;
}

/*
 * 击中映射表排序用来的比较函数, 比较内存块范围首地址即可
 */
static
int SortHitAddrMap_Inner_AddrHitInfo_CMP(const void *left, const void *right)
{
#define AS_ADDR_HIT(p) ((ST_AddressHitInfo *)p)

	if (left == right)
	{
		return 0;
	}

	/* 不可直接做减法, 64位平台得到的 8 字节负数(地址差)会截断为整形的正数 */
	return (AS_ADDR_HIT(left)->addrRange.begin < AS_ADDR_HIT(right)->addrRange.begin)
			? -1 : 1;
}

/*
 * 排序击中映射表
 */
static inline
void SortHitAddrMap(ST_AddressHitInfo *pAddrHitMap, int len)
{
	qsort(pAddrHitMap, len, sizeof (ST_AddressHitInfo), SortHitAddrMap_Inner_AddrHitInfo_CMP);
}

/*
 * 内存池初始化
 * 按照配置分配内存,并建立相关记录结构
 */
int mcs_mpool_init()
{
	int lRet = 0;
	int lSts = 0;
	int lBlockSize = 0;
	int i = 0;
	void *pNewMem = NULL;
	ST_MemBlock *pNewMemBlock = NULL;
	unsigned long long tatalAllocedMemSize = 0;

	for (i = 0; i < MAX_POWER_TO_SUPPORT + 1; i++)
	{
		pNewMemBlock = (ST_MemBlock *)malloc(sizeof (ST_MemBlock));
		GOTO_ON_ERR(NULL == pNewMemBlock, lRet, -errno, out);

		INIT_LIST_HEAD(&pNewMemBlock->ceilList);
		INIT_LIST_HEAD(&pNewMemBlock->list);
		pNewMemBlock->pFreeList = NULL;
		pNewMemBlock->lCellSize = 1 << i;	/* 每个 cell 尺寸, 2次幂 */
		pNewMemBlock->lCellNums = g_NumPerCell[i];
		pthread_mutex_init(&pNewMemBlock->memBlockMutex, 0);

#ifdef DEBUG
		pNewMemBlock->lUsedNums = 0;
#endif /* DEBUG */

		/* 计算 block 大小, 为其分配内存 */
		lBlockSize = pNewMemBlock->lCellSize * pNewMemBlock->lCellNums;
		pNewMem = malloc(lBlockSize);
		GOTO_ON_ERR(NULL == pNewMem, lRet, -errno, out);
		tatalAllocedMemSize += lBlockSize;

#if MARK_UNUSED_MEM
		memset(pNewMem, FRESH_MEM_MARK, lBlockSize);		/* 新内存标记 */
#endif
		/* 设定好 block 有效地址范围 */
		pNewMemBlock->addrRange.begin = pNewMem;
		/* end 指向该块上的最后一个单元起始地址(非最终结尾) */
		pNewMemBlock->addrRange.end = pNewMem + lBlockSize - pNewMemBlock->lCellSize;

		/* 所有内存置为空闲 */
		lSts = InitMemBlockCellList(pNewMemBlock);
		GOTO_ON_ERR(lSts < 0, lRet, -errno, out);

		/* 记录到 hit map, 为 free时进行地址击中测试做准备 */
		__IMP.addrHitMap[i].addrRange = pNewMemBlock->addrRange;
		__IMP.addrHitMap[i].pMemBlock = pNewMemBlock;

		/* 记录到 memBlockMap, 方便分配内存时根据大小决定从哪个 block 上分配 */
		__IMP.memBlockMap[i] = pNewMemBlock;
	}

	/* 将 __IMP.addrHitMap 按照地址范围排序,
	 * 为 free 时使用二分查找一个地址是由哪个内存块分配的
	 * */
	SortHitAddrMap(__IMP.addrHitMap, MAX_POWER_TO_SUPPORT + 1);

#ifdef DEBUG
	DEBUG_INFO("内存池总大小 %llu(%.fKB, %.fMB) .\n",
			tatalAllocedMemSize,
			tatalAllocedMemSize / 1024.0,
			tatalAllocedMemSize / 1024.0 / 1024.0);
#endif /* DEBUG */
out:
	return lRet;
}

static inline
int log2Of(int num)
{
	int mvCnt = 0;
	while ((num >>= 1) != 0) mvCnt++;
	return mvCnt;
}

#ifdef DEBUG
static inline /* 内存块是否已经用尽 */
int Block_Used_Out(ST_MemBlock *pBlock)
{
	int res = ( pBlock->pFreeList == &pBlock->ceilList);
	if (res)
	{
		DEBUG_INFO("block 单元用尽, 专卖: %d.\n", pBlock->lCellSize);
	}

	/* 已经分到头了 */
	return res;
}

#	define BLOCK_USED_OUT(pBlock)	Block_Used_Out(pBlock)
#else
#	define BLOCK_USED_OUT(pBlock) 	( pBlock->pFreeList == &pBlock->ceilList)
#endif /* DEBUG */

void *mcs_mpool_malloc(int size)
{
	void *vRet = NULL;
	struct list_head *plhSuitCell = NULL;
	ST_CellNode *pSuitCell = NULL;
	ST_MemBlock *pSuitableBlock = NULL;
	int blockIdx = log2Of(size);

	GOTO_ON_ERR(size > (1 << MAX_POWER_TO_SUPPORT), vRet, NULL, out);

	if (size != 1 << blockIdx)
	{	// 并不是整数幂, 需要使用大一号的尺寸
		blockIdx++;
	}

	/* 快速决断供应 block */
	pSuitableBlock = __IMP.memBlockMap[blockIdx];
	while (blockIdx < MAX_POWER_TO_SUPPORT + 1
			&& BLOCK_USED_OUT(pSuitableBlock))
	{	/* 通常是不需要再往大了给的,直接映射过去 */
#ifdef DEBUG
		DEBUG_INFO("专卖 %d(%.fk)内存块已用尽, 找大一号内存块 %d(%.fk).\n"
				, pSuitableBlock->lCellSize, pSuitableBlock->lCellSize / 1024.0
				, __IMP.memBlockMap[blockIdx + 1]->lCellSize
				, __IMP.memBlockMap[1 + blockIdx]->lCellSize / 1024.0);
#endif /* DEBUG */
		pSuitableBlock = __IMP.memBlockMap[++blockIdx];
	}

	if (MAX_POWER_TO_SUPPORT + 1 == blockIdx)
	{	/* 内存块全部用尽
	 	 * 预先分配的内存已经用尽,并不做临时动态分配
	 	 * */
		errno = ENOMEM;

		DEBUG_INFO("内存池内存用尽, 分配失败.\n");
		vRet = NULL;
		goto out;
	}

	/* 获取第一个空闲单元 */
	pthread_mutex_lock(&pSuitableBlock->memBlockMutex);
	plhSuitCell = pSuitableBlock->pFreeList;
	pSuitCell = list_entry(plhSuitCell, ST_CellNode, list);
#ifdef DEBUG
	if (pSuitableBlock->addrRange.end == pSuitCell->pMem)
	{
		DEBUG_INFO("内存块 %d(%.fk)专卖, 最后一个地址: %p 已经分配.\n",
				pSuitableBlock->lCellSize, pSuitableBlock->lCellSize / 1024.0, vRet);
	}

	if (!ADDRESS_HIT(pSuitCell->pMem, pSuitableBlock))
	{
		volatile int breakHere; breakHere++;
	}

	assert(ADDRESS_HIT(pSuitCell->pMem, pSuitableBlock));
	if (pSuitCell->pMem == USED_CELL_MEM_MARK)
	{
		volatile int breakHere; breakHere++;
	}
#endif		/* DEBUG */

	vRet = pSuitCell->pMem;
	pSuitCell->pMem = USED_CELL_MEM_MARK;							/* 标记为已分配 */
	pSuitableBlock->pFreeList = pSuitableBlock->pFreeList->next;	/* 指向下一个可用的 */
#ifdef DEBUG
	pSuitableBlock->lUsedNums++;
#endif /* DEBUG */
	pthread_mutex_unlock(&pSuitableBlock->memBlockMutex);
	goto out;

out:
	return vRet;
}

static ST_MemBlock *AddressHitAt(void *p)
{ 	/* 二分查找 */
	void *vRet = NULL;
	ST_AddressHitInfo *pMidBlock = NULL;
	int i = 0;
	int j = (MAX_POWER_TO_SUPPORT + 1);
	int hitRes = 0;

#define MID(i, j) 			((i + j) / 2)
#define MID_BLOCK(i, j) 	(&__IMP.addrHitMap[MID(i, j)])

	while (i <= j)
	{
		pMidBlock = MID_BLOCK(i, j);
		hitRes = ADDRESS_HIT2(p, pMidBlock);

		if (0 == hitRes)
		{ /* hit it ! */
			vRet = pMidBlock->pMemBlock;
			goto out;
		}

		if (hitRes < 0)
		{ /* 去左边看看 */
			j = MID(i, j) - 1;
		}
		else
		{ /* 去右边看看 */
			assert(ADDRESS_HIGHER(p, pMidBlock));
			i = MID(i, j) + 1;
		}
	}

	goto out;
out:
	return (ST_MemBlock *) vRet;
}

/* 回收一个内存单元 */
void mcs_mpool_free(void *p)
{
#ifdef DEBUG	/* 不检查 NULL 指针 */
	if (NULL == p || p == USED_CELL_MEM_MARK)
	{
		DEBUG_INFO("企图释放空指针.\n");
		goto out;
	}
#endif

	/* 内存击中测试, 得到其所属内存块 */
	ST_MemBlock *pHitBlock = AddressHitAt(p);
	struct list_head *plhPreNode = NULL;
	ST_CellNode *pPreNode = NULL;


	if (NULL == pHitBlock)
	{
		DEBUG_INFO("地址: 0x%p不是由本内存池分配的.\n", p);
		goto out;
	}

	assert(ADDRESS_HIT(p, pHitBlock));

	pthread_mutex_lock(&pHitBlock->memBlockMutex);
	plhPreNode = pHitBlock->pFreeList->prev;				/* 上一个分配出去的单元 list_head */
	pPreNode = list_entry(plhPreNode, ST_CellNode, list);	/* 上一个分配出去的单元 结点 */

#ifdef DEBUG
	if (pPreNode->pMem != USED_CELL_MEM_MARK)
	{
		volatile int breakHere; breakHere++;
	}
	assert(pPreNode->pMem == USED_CELL_MEM_MARK);			/* 上一个结点一定是已经分出去了的 */
#endif	/* DEBUG */

#if MARK_UNUSED_MEM
	memset(p, FRESH_MEM_MARK, pHitBlock->lCellSize);		/* 返还的内存标记为未使用 */
#endif

	pPreNode->pMem = p;										/* 返还的内存记录到上一个分配出去的单元中 */
	pHitBlock->pFreeList = pHitBlock->pFreeList->prev;		/* 将上一个分配出去的单元加入到空闲链表 */
#ifdef DEBUG
	pHitBlock->lUsedNums--;
#endif /* DEBUG */
	pthread_mutex_unlock(&pHitBlock->memBlockMutex);
out:
	return ;
}

void FreeMemBlock(ST_MemBlock *pBlock)
{
	struct list_head *pos = NULL;
	struct list_head *posN = NULL;
	ST_CellNode *pCellNode = NULL;

	/* 1) 回收 block 内存块 */
	free(pBlock->addrRange.begin);

	/* 2) 回收 cellList 结构 */
	list_for_each_safe(pos, posN, &pBlock->ceilList)
	{
		pCellNode = list_entry(pos, ST_CellNode, list);
		free(pCellNode);
	}

	/* 3) 销毁锁 */
	pthread_mutex_destroy(&pBlock->memBlockMutex);
}

int mcs_mpool_fini()
{
	int lRet = 0;
	int i = 0;
	for (i = 0; i < MAX_POWER_TO_SUPPORT + 1; i++)
	{
		FreeMemBlock(__IMP.memBlockMap[i]);
		/* 回收 block 自身内存 */
		free(__IMP.memBlockMap[i]);
		__IMP.memBlockMap[i] = NULL;
	}

	return lRet;
}
