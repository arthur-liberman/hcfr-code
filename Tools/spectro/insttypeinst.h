
/* Add instrument instance headers here */

/* 
 * Copyright 2001  - 2013 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

#ifdef __cplusplus
	extern "C" {
#endif


#ifdef ENABLE_SERIAL
# include "dtp22.h"
# include "dtp41.h"
# include "dtp51.h"
# include "ss.h"
#endif

#ifdef ENABLE_FAST_SERIAL
# include "specbos.h"
# include "kleink10.h"
# include "smcube.h"
#endif

#ifdef ENABLE_USB
# include "dtp20.h"
# include "dtp92.h"
# include "i1disp.h"
# include "i1d3.h"
# include "i1pro.h"
# include "i1pro3.h"
# include "munki.h"
# include "spyd2.h"
# include "spydX.h"
# include "huey.h"
# include "ex1.h"
# include "hcfr.h"
# include "colorhug.h"
#endif


