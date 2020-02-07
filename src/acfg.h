/*
 * acfg.h
 *
 * configuration file.
 *
 *  Created on: 2019年7月20日
 *      Author: ueyudiud
 */

#ifndef ACFG_H_
#define ACFG_H_

#define ALO_INT_FORMAT "%"PRId64
#define ALO_FLT_FORMAT "%.8g"

/* file separator */
#define ALO_PATH_SEP ";"
/* mark environment path */
#define ALO_PATH_ENV "~"

/**
 ** the home of Alopecurus.
 */
#define ALO_PATH "ALO_HOME"

/**
 ** library search path.
 */
#define ALO_LIB_DIR "/lib"

/* max stack size of each thread */
#define ALO_MAXSTACKSIZE 100000

/**
 ** the extra space for each thread.
 */
#define ALO_THREAD_EXTRASPACE sizeof(void*)

/**
 ** when enable this option, the float number will not transfer to
 ** integer number while compiling and taking operation.
 */
#define ALO_STRICT_NUMTYPE false

/**
 ** optional macro list:
 ** - ALO_HARDMEMTEST
 ** 	when enable this option, the VM will do strict GC and stack allocation.
 ** - ALO_DEBUG
 ** 	when enable this option, the VM will enable aloE_assert macro.
 ** - ALO_OPEN_THREAD(T,from)
 ** 	called macro after thread is constructed.
 ** 		T		the thread been constructed.
 ** 	 	from	the source thread, NULL if constructed by function 'alo_newstate'.
 ** - ALO_CLOSE_THREAD(T)
 ** 	called macro before thread is destroyed.
 ** 		T		the thread been destroyed
 */

#endif /* ACFG_H_ */
