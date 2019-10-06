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

#define ALO_LIB_DIR "/lib"

/**
 ** when enable this option, the float number will not transfer to
 ** integer number while compiling and taking operation.
 */
#define ALO_STRICT_NUMTYPE false

#define ALO_HARDMEMTEST
#define ALO_DEBUG

#endif /* ACFG_H_ */
