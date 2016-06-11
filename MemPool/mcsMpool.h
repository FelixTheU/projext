/****************************************************************************************
 * 文 件 名 : yv00bspmcs_msg_mem_pool.h
 * 项目名称 : YVGA1207001A
 * 模 块 名 : mcs模块
 * 功    能 : mcs 专用内存池(能分配的内存块大小有上限,比如 64k, 并且分配的大小可能超过请求数)
 * 				如请求 30k,实际分配 32K.
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
 *               (C)Copyright 2012 YView    Corporation All Rights Reserved.
 *- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 ***************************************************************************************/
#ifndef YV00BSPMCS_MSG_MEM_POOL_H
#define YV00BSPMCS_MSG_MEM_POOL_H

#ifdef __cplusplus /* If this is a C++ compiler, use C linkage */
extern "C" {
#endif

#define MPOOL_MAX_POWER					(16)	/* 最大支持到 2^16 的 ceil 分配, 64k */

/*
 * 内存池初始化
 */
int mcs_mpool_init();

/*
 * 内存池销毁
 */
int mcs_mpool_fini();

/*
 * 使用内存池进行内存分配
 */
void *mcs_mpool_malloc(int size);

/*
 * 回收从内存池分配的内存
 */
void mcs_mpool_free(void *p);

#ifdef __cplusplus
}
#endif
#endif /* YV00BSPMCS_MSG_MEM_POOL_H */
