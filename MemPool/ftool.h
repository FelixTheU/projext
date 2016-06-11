#ifndef FTOOL_H
#define FTOOL_H

#ifdef DEBUG
#	include <stdio.h>
#	define DEBUG_INFO(...)			printf(__VA_ARGS__)
#else
#	define DEBUG_INFO(...)
#endif /* DEBUG */

#define TRACE_ERR_F(fmt, ...)				\
  printf(fmt "line:%d, at func:%s, errno:%d, errStr:%s.\n",	\
	 __VA_ARGS__, __LINE__, __FUNCTION__, errno, strerror(errno))

#define TRACE_ERR()			TRACE_ERR_F("%s", "")

#define TRACE_ERR_S(str)		TRACE_ERR_F("%s", str)

#define GOTO_ON_ERR(cond, errVar, err, to, ...) 	\
  if (cond) {					\
	  errVar = err;				\
    TRACE_ERR();				\
    goto to;}

#define STATIC_ASSERT(condition) ((void)sizeof(char[1 - 2*!(condition)]))

#define DIMEN_OF(array)			(sizeof(array) / sizeof(array[0]))

/* 说明性占位宏 */
#define IMPLEMENT_USE_ONLY

#endif /* FTOOL_H */
