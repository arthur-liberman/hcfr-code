#ifndef EX1_H

/* 
 * Argyll Color Correction System
 *
 * Image Engineering EX1
 *
 * Author: Graeme W. Gill
 * Date:   4/4/2015
 *
 * Copyright 2001 - 2015, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 *
 * Based on spebos.h
 */

/* 
   If you make use of the instrument driver code here, please note
   that it is the author(s) of the code who take responsibility
   for its operation. Any problems or queries regarding driving
   instruments with the Argyll drivers, should be directed to
   the Argyll's author(s), and not to any other party.

   If there is some instrument feature or function that you
   would like supported here, it is recommended that you
   contact Argyll's author(s) first, rather than attempt to
   modify the software yourself, if you don't have firm knowledge
   of the instrument communicate protocols. There is a chance
   that an instrument could be damaged by an incautious command
   sequence, and the instrument companies generally cannot and
   will not support developers that they have not qualified
   and agreed to support.
 */

#include "inst.h"

#ifdef __cplusplus
	extern "C" {
#endif

/* Communication errors */
#define EX1_TIMEOUT				    0xFF02		/* Communication timeout */
#define EX1_COMS_FAIL				0xFF03		/* Communication failure */
#define EX1_UNKNOWN_MODEL			0xFF04		/* Not an ex1 */
#define EX1_SHORT_WRITE  			0xFF06		/* Write failure */
#define EX1_SHORT_READ  			0xFF07		/* Read failure */
#define EX1_LONG_READ  				0xFF08		/* Read more data than expected */
#define EX1_DATA_CHSUM_ERROR  		0xFF09		/* Checksum failed */
#define EX1_DATA_PARSE_ERROR  		0xFF0A		/* Read data parsing error */

/* Measurement errors */
#define EX1_RD_SENSORSATURATED	    0xFF0B		/* Sensor is saturated */


/* Internal software errors */
#define EX1_INTERNAL_ERROR		   0xE000		/* Internal software error */
#define EX1_NOT_IMP				   0xE001		/* Function not implemented */
#define EX1_MEMORY				   0xE002		/* Memory allocation fail */
#define EX1_INT_THREADFAILED       0xE003
#define EX1_INTTIME_RANGE          0xE004		/* Integration time is out of range */
#define EX1_DELTIME_RANGE          0xE005		/* Trigger Delaty time time is out of range */
#define EX1_STROBE_RANGE           0xE006		/* Strobe period time is out of range */
#define EX1_AVERAGE_RANGE          0xE007		/* Number to average is out of range */
#define EX1_BOXCAR_RANGE           0xE008		/* Boxcar filtering size out of ange */
#define EX1_INT_CAL_SAVE           0xE009		/* Saving calibration to file failed */
#define EX1_INT_CAL_RESTORE        0xE00A		/* Restoring calibration to file failed */
#define EX1_INT_CAL_TOUCH          0xE00B		/* Touching calibration to file failed */
#define EX1_INT_CIECONVFAIL        0xE00C		/* Creating spec. to XYZ conversion failed */

/* Configuration or operation errors */
#define EX1_NO_WL_CAL              0xD001		/* There is no wavelegth calibration info */
#define EX1_NO_IR_CAL              0xD002		/* There is no irradiance calibration info */

/* Real instrument error code */
#define EX1_OK						0
#define EX1_UNSUP_PROTOCOL			1
#define EX1_MES_UNKN    			2
#define EX1_MES_BAD_CHSUM			3
#define EX1_MES_TOO_LARGE			4
#define EX1_MES_PAYLD_LEN			5
#define EX1_MES_PAYLD_INV			6
#define EX1_DEV_NOT_RDY 			7
#define EX1_MES_UNK_CHSUM			8
#define EX1_DEV_UNX_RST 			9
#define EX1_TOO_MANY_BUSSES 		10
#define EX1_OUT_OF_MEM 				11
#define EX1_NO_INF	 				12
#define EX1_DEV_INT_ERR				13
#define EX1_DECRYPT_ERR				100
#define EX1_FIRM_LAYOUT				101
#define EX1_PACKET_SIZE				102
#define EX1_HW_REV_INCPT			103
#define EX1_FLASH_MAP   			104
#define EX1_DEFERRED	   			255

/* EX1 communication object */
struct _ex1 {
	INST_OBJ_BASE

	ORD8 *cbuf;					/* USB communication buffer */
	int  cbufsz;				/* Current cbuf size */			

	char *alias;				/* Alias string (OEM Model ?) */
	int hwrev;					/* Hardware rev number */
	int fwrev;					/* Hardware rev number */
	char *serno;				/* ASCII serial number */
	int slitw;					/* Slit width in microns */
	int fiberw;					/* Fiber width in microns */
	char *grating;				/* Grating description */
	char *filter;				/* Filter description */
	char *coating;				/* Coating description */

	inst_mode mode;				/* Currently instrument mode */

	double max_meastime;		/* Maximum integration time to use for measurement */
	double inttime;				/* Current integration time */
	int noaverage;				/* Current number being averaged */

	rspec_inf sconf;			/* Measurement & spectral configuration */

	/* Don't know what to do with stray light - not present in EX1 ? */
	unsigned int nstraylight;	/* Number in array */
	double *straylight;			/* Array of stray light coefficients. */

	unsigned int nemis_coef;	/* Number in array */
	double *emis_coef;			/* Emission calibration coefficients for raw */
	double emis_area;			/* Emission collection area */

	/* Adaptive emission black calibration */
	int idark_valid;			/* idark calibration factors valid */
	time_t iddate;				/* Date/time of last dark idark calibration */
	double idark_int_time[2];	/* Target dark integration times */

	/* Other state */
	inst_opt_type trig;			/* Reading trigger mode */
	int want_dcalib;			/* Want Dark Calibration at start */
	int noinitcalib;			/* Disable initial calibration if not essential */
	int lo_secs;				/* Seconds since last opened (from calibration file mod time) */ 

	xsp2cie *conv;				/* spectral to XYZ conversion */

}; typedef struct _ex1 ex1;

/* Constructor */
extern ex1 *new_ex1(icoms *icom, instType itype);

#ifdef __cplusplus
	}
#endif

#define EX1_H
#endif /* EX1_H */
