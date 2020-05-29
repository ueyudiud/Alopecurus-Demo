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
 ** the environment key
 */
#define ALO_ENVIRONMENT "_ENV"

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
#if !defined(ALO_SEARCH_LPATH)

#if defined(_WIN32)

#define ALO_SEARCH_LPATH \
	"~\\mod\\?.alo;" \
	"~\\?\\mod.alo;" \
	".\\?.alo;" \
	".\\?\\mod.alo;"
#define ALO_SEARCH_CPATH \
	"~\\lib\\?.dll;" \
	".\\?.dll;"

#else

#define ALO_ROOT "/usr/local/"
#define ALO_LDIR ALO_ROOT "share/alo/"
#define ALO_CDIR ALO_ROOT "lib/alo/"

#define ALO_SEARCH_LPATH \
	ALO_LDIR "mod/?.alo;" \
	ALO_LDIR "?/mod.alo;" \
	"./?.alo;" \
	"./?/mod.alo;"
#define ALO_SEARCH_CPATH \
	ALO_CDIR "lib/?.so;" \
	"./?.so;"

#endif

#endif

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
 ** optional macro list:
 ** - ALO_HARDMEMTEST
 ** 	when enable this option, the VM will do strict GC and stack allocation.
 ** - ALO_DEBUG
 ** 	when enable this option, the VM will enable aloE_assert macro.
 */

#endif /* ACFG_H_ */
