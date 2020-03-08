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

/**
 ** the home of Alopecurus.
 */
#define ALO_PATH "ALO_HOME"

/**
 ** path configuration information:
 ** ';' is file separator.
 ** '?' is file name for replacement.
 ** '~' is environment path.
 */

/* library search path. */
#define ALO_SEARCH_LPATH \
	"~\\mod\\?.alo;" \
	"~\\?\\mod.alo;" \
	".\\?.alo;" \
	".\\?\\mod.alo;"
#define ALO_SEARCH_CPATH \
	"~\\lib\\?.dll;" \
	".\\?.dll;"

/* max stack size of each thread */
#define ALO_MAXSTACKSIZE 100000

/* max capacity of memory buffer */
#define ALO_MBUF_LIMIT 10000000

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
 ** when enable this option, the runtime debugger and related tools
 ** will be available.
 */
#define ALO_RUNTIME_DEBUG true

/**
 ** optional macro list:
 ** - ALO_HARDMEMTEST
 ** 	when enable this option, the VM will do strict GC and stack allocation.
 ** - ALO_DEBUG
 ** 	when enable this option, the VM will enable aloE_assert macro.
 ** - ALO_BUILD_TO_DL
 ** 	if the project is going to create a dynamic library, enable this option.
 ** - ALO_OPEN_THREAD(T,from)
 ** 	called macro after thread is constructed.
 ** 		T		the thread been constructed.
 ** 	 	from	the source thread, NULL if constructed by function 'alo_newstate'.
 ** - ALO_CLOSE_THREAD(T)
 ** 	called macro before thread is destroyed.
 ** 		T		the thread been destroyed
 */

#endif /* ACFG_H_ */
