
#ifndef DEV_H

/*
 * Abstract base class for all devices handled here.
 */

/* 
 * Argyll Color Management System
 *
 * Author: Graeme W. Gill
 * Date:   17/8/2016
 *
 * Copyright 2016 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 *
 */

#include "icoms.h"			/* libinst Includes this functionality */
#include "conv.h"

#ifdef __cplusplus
	extern "C" {
#endif

/* Device base object. */
#define DEV_OBJ_BASE															\
	a1log *log;			/* Pointer to debug & error logging class */			\
	devType  dtype;		/* Device type determined by driver */					\
	icoms *icom;		/* Device coms object */								\
	int gotcoms;		/* Coms established flag */                             \
	int inited;			/* Device open and initialized flag */              	\

/* The base object type */
struct _dev {
	DEV_OBJ_BASE
}; typedef struct _dev dev;

#ifdef __cplusplus
	}
#endif

#define DEV_H
#endif /* DEV_H */
