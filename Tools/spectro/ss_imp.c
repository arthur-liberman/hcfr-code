
#ifndef SS_IMP_H

/* 
 * Argyll Color Correction System
 *
 * Gretag Spectrolino and Spectroscan related
 * defines and declarations - implementation.
 *
 * Author: Graeme W. Gill
 * Date:   14/7/2005
 *
 * Copyright 2005 - 2007 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 *
 * This is an alternative driver to spm/gretag.
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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#ifndef SALONEINSTLIB
#include "copyright.h"
#include "aconfig.h"
#include "numlib.h"
#else /* SALONEINSTLIB */
#include "sa_config.h"
#include "numsup.h"
#endif /* SALONEINSTLIB */
#include "xspect.h"
#include "insttypes.h"
#include "icoms.h"
#include "conv.h"
#include "ss.h"

/* ------------------------------------------- */
/* Serialisation for different types functions */

/* QUERY: */

/* These add characters to the current send buffer, */
/* And set the status appropriately */

/* 4 bits to hex character conversion table */
static char b2h[16] = {
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

/* Check that there is enough space to write size more characters */
#define CHWSPACE(size)                          \
	if (p->snerr == ss_et_NoError               \
	  && (p->sbufe - p->sbuf) < (size)) {       \
		p->snerr = ss_et_SendBufferFull;        \
	}                                           \
	if (p->snerr != ss_et_NoError)              \
		return;

/* Reset send buffer, and init with start character */
void ss_init_send(ss *p) {
	p->snerr = ss_et_NoError;
	p->sbuf = p->_sbuf;
	CHWSPACE(1);
	p->sbuf[0] = ';';
	p->sbuf += 1;
}

/* Reset send buffer, and add an Spectrolino Request enum */
void ss_add_soreq(ss *p, int rq) {
	ss_init_send(p);
	CHWSPACE(2);
	p->sbuf[0] = b2h[(rq >> 4) & 0xf];
	p->sbuf[1] = b2h[(rq >> 0) & 0xf];
	p->sbuf += 2;
}

/* Reset send buffer, and add an SpectroScan Request enum */
void ss_add_ssreq(ss *p, int rq) {
	ss_init_send(p);
	CHWSPACE(4);
	p->sbuf[0] = b2h[((int)ss_ReqPFX >> 4) & 0xf];	/* Prefix */
	p->sbuf[1] = b2h[((int)ss_ReqPFX >> 0) & 0xf];	/* Prefix */
	p->sbuf[2] = b2h[(rq >> 4) & 0xf];
	p->sbuf[3] = b2h[(rq >> 0) & 0xf];
	p->sbuf += 4;
}

/* Add an int/enum/char into one byte type */
void ss_add_1(ss *p, int c) {
	CHWSPACE(2);
	p->sbuf[0] = b2h[(c >> 4) & 0xf];
	p->sbuf[1] = b2h[(c >> 0) & 0xf];
	p->sbuf += 2;
}

/* Add an int/enum into two byte type */
void ss_add_2(ss *p, int s) {
	CHWSPACE(4);
	p->sbuf[0] = b2h[(s >> 4) & 0xf];	/* LSB */
	p->sbuf[1] = b2h[(s >> 0) & 0xf];
	p->sbuf[2] = b2h[(s >> 12) & 0xf];	/* MSB */
	p->sbuf[3] = b2h[(s >> 8) & 0xf];
	p->sbuf += 4;
}

/* Add an int/enum into four byte type */
void ss_add_4(ss *p, int i) {
	CHWSPACE(8);
	p->sbuf[0] = b2h[(i >> 4) & 0xf];	/* LSB */
	p->sbuf[1] = b2h[(i >> 0) & 0xf];
	p->sbuf[2] = b2h[(i >> 12) & 0xf];
	p->sbuf[3] = b2h[(i >> 8) & 0xf];
	p->sbuf[4] = b2h[(i >> 20) & 0xf];
	p->sbuf[5] = b2h[(i >> 16) & 0xf];
	p->sbuf[6] = b2h[(i >> 28) & 0xf];	/* MSB */
	p->sbuf[7] = b2h[(i >> 24) & 0xf];
	p->sbuf += 8;
}

/* Add a double into four byte type */
void ss_add_double(ss *p, double d) {
	unsigned int id;

	id = doubletoIEEE754(d);

	ss_add_4(p, id);
}

/* Add an  ASCII string into the send buffer. */
/* The string will be padded with nul's up to len. */
void ss_add_string(ss *p, char *t, int len) {
	int i;

	CHWSPACE(2 * len);
	for (i = 0; i < len; i++) {
		p->sbuf[2 * i + 0] = b2h[(t[i] >> 4) & 0xf];
		p->sbuf[2 * i + 1] = b2h[(t[i] >> 0) & 0xf];
		if (t[i] == '\000')
			break;
	}
	for (; i < len; i++) {
		p->sbuf[2 * i + 0] = '0';
		p->sbuf[2 * i + 1] = '0';
	}
	p->sbuf += 2 * len;
}

/* - - - - - - - - - - - - - - - - - - - - - */
/* ANSWER: */

/* Check that there is not already an error, */
/* and that there is enough space to read size more characters. */
/* Return nz if not. */
static int chrspace(ss *p, int size) {

	if (p->snerr != ss_et_NoError)		/* Problem assembling request */
		return 1;

	if ((p->rbufe - p->rbuf) < (size)) {
		p->snerr = ss_et_RecBufferEmpty;
		return 1;
	}
	{
		char *rr = p->rbuf, *re = p->rbuf + size;
		while (rr < re && *rr != '\000')
			rr++;
		if (rr < re) {
			p->snerr = ss_et_RecBufferEmpty;
			return 1;
		}
	}
	return 0;
}

/* Check that the read buffer is fully consumed. */
static void chended(ss *p) {
	if (p->snerr == ss_et_NoError	/* Don't overrite any existing error */
	 && p->rbufe != p->rbuf) {
		p->snerr = ss_et_BadAnsFormat;
	}
}
	
/* Convert an ASCII Hex character to an integer. */
static int h2b(ss *p, char c) {
	if (c >= '0' && c <= '9')
		return (c-(int)'0');
	if (c >= 'A' && c <= 'F')
		return (10 + c-(int)'A');
	if (c >= 'a' && c <= 'f')
		return (10 + c-(int)'a');
	if (p->snerr == ss_et_NoError)   /* Don't overrite any existing error */
		p->snerr = ss_et_BadHexEncoding;
	return 0;
}

/* Return the first enum from the recieve buffer without removing it. */
int ss_peek_ans(ss *p) {
	int rv;

	if (chrspace(p, 2))
		return 0;
	rv = (h2b(p, p->rbuf[0]) << 4)
	   | (h2b(p, p->rbuf[1]) << 0);
	
	return rv;
}

/* Remove a Spectrolino answer enum from the revieve buffer, */
/* and check it is correct.  */
void ss_sub_soans(ss *p, int cv) {
	int rv;

	if (chrspace(p, 2))
		return;
	rv = (h2b(p, p->rbuf[0]) << 4)
	   | (h2b(p, p->rbuf[1]) << 0);
	p->rbuf += 2;
	if (rv != cv) {
		if (p->snerr == ss_et_NoError)   /* Don't overrite any existing error */
			p->snerr = ss_et_BadAnsFormat;
		return;
	}
	return;
}

/* Remove a SpectroScan Prefix and answer enum from the revieve buffer, */
/* and check it is correct.  */
void ss_sub_ssans(ss *p, int cv) {
	int rv;

	if (chrspace(p, 4))
		return;
	if (p->rbuf[0] != 'D'
	 || p->rbuf[1] != '1') {
		if (p->snerr == ss_et_NoError)   /* Don't overrite any existing error */
			p->snerr = ss_et_BadAnsFormat;
		return;
	}
	rv = (h2b(p, p->rbuf[2]) << 4)
	   | (h2b(p, p->rbuf[3]) << 0);
	p->rbuf += 4;
	if (rv != cv) {
		if (p->snerr == ss_et_NoError)   /* Don't overrite any existing error */
			p->snerr = ss_et_BadAnsFormat;
		return;
	}
	return;
}

/* Remove an int/enum/char into one byte type */
int ss_sub_1(ss *p) {
	int rv;

	if (chrspace(p, 2))
		return 0;
	rv = (h2b(p, p->rbuf[0]) << 4)
	   | (h2b(p, p->rbuf[1]) << 0);
	p->rbuf += 2;
	
	return rv;
}

/* Remove an int/enum into two byte type */
int ss_sub_2(ss *p) {
	int rv;

	if (chrspace(p, 4))
		return 0;
	rv = (h2b(p, p->rbuf[0]) << 4)
	   | (h2b(p, p->rbuf[1]) << 0)
	   | (h2b(p, p->rbuf[2]) << 12)
	   | (h2b(p, p->rbuf[3]) << 8);
	p->rbuf += 4;
	
	return rv;
}

/* Remove an int/enum into four byte type */
int ss_sub_4(ss *p) {
	int rv;

	if (chrspace(p, 8))
		return 0;
	rv = (h2b(p, p->rbuf[0]) << 4)
	   | (h2b(p, p->rbuf[1]) << 0)
	   | (h2b(p, p->rbuf[2]) << 12)
	   | (h2b(p, p->rbuf[3]) << 8)
	   | (h2b(p, p->rbuf[4]) << 20)
	   | (h2b(p, p->rbuf[5]) << 16)
	   | (h2b(p, p->rbuf[6]) << 28)
	   | (h2b(p, p->rbuf[7]) << 24);
	p->rbuf += 8;
	
	return rv;
}

/* Remove a double into four byte type */
double ss_sub_double(ss *p) {
	unsigned int ip;
	double op;

	ip = (unsigned int)ss_sub_4(p);

	op = IEEE754todouble(ip);

	return op;
}

/* Remove an ASCII string from the receive buffer. */
/* The string will be terminated with a nul, so a buffer */
/* of len+1 should be provided to return the string in. */
void ss_sub_string(ss *p, char *t, int len) {
	int i;

	if (chrspace(p, 2 * len))
		return;
	for (i = 0; i < len; i++) {
		t[i] = (char)((h2b(p, p->rbuf[2 * i + 0]) << 4)
		            | (h2b(p, p->rbuf[2 * i + 1]) << 0));
	}
	t[i] = '\000';
	p->rbuf += 2 * len;
}

/* - - - - - - - - - - - - - - - - - - - - - */
/* ERROR CODES: */

/* Convert an ss error into an inst_error */
inst_code ss_inst_err(ss *p) {
	ss_et ec = p->snerr;

	switch(ec) {
		case ss_et_NoError:
			return inst_ok;

		case ss_et_WhiteMeasOK:
		case ss_et_ResetDone:
		case ss_et_EmissionCalOK:
			return inst_notify | ec;

		case ss_et_WhiteMeasWarn:
			return inst_warning | ec;

		case ss_et_SendTimeout:
		case ss_et_SerialFail:
			return inst_coms_fail | ec;

		case ss_et_StopButNoStart:
		case ss_et_IllegalCharInRec:
		case ss_et_IncorrectRecLen:
		case ss_et_IllegalRecType:
		case ss_et_NoTagField:
		case ss_et_ConvError:
		case ss_et_RecBufferEmpty:
		case ss_et_BadAnsFormat:
		case ss_et_BadHexEncoding:
		case ss_et_RecBufferOverun:
		case ss_et_OutOfRange:
		case ss_et_NotAnSROrBoolean:
			return inst_protocol_error | ec;

		case ss_et_UserAbort:
			return inst_user_abort | ec;
		case ss_et_UserTerm:
			return inst_user_term | ec;
		case ss_et_UserTrig:
			return inst_user_trig | ec;
		case ss_et_UserCmnd:
			return inst_user_cmnd | ec;

		case ss_et_RemOverFlow:
		case ss_et_MeasDisabled:
		case ss_et_MeasurementError:
		case ss_et_DorlOutOfRange:
		case ss_et_ReflectanceOutOfRange:
		case ss_et_Color1OutOfRange:
		case ss_et_Color2OutOfRange:
		case ss_et_Color3OutOfRange:
			return inst_misread | ec;

		case ss_et_NoValidCommand:
		case ss_et_NoUserAccess:
			return inst_unsupported | ec;

		case ss_et_NotReady:
		case ss_et_NoAccess:
			return inst_other_error | ec;
			break;

		case ss_et_SendBufferFull:
			return inst_internal_error | ec;

		case ss_et_NoValidMeas:
		case ss_et_OnlyEmission:
		case ss_et_DensCalError:
		case ss_et_NoTransmTable:
		case ss_et_InvalidForEmission:
		case ss_et_NotInTransmMode:
		case ss_et_NotInReflectMode:
		case ss_et_NoValidDStd:
		case ss_et_NoValidWhite:
		case ss_et_NoValidIllum:
		case ss_et_NoValidObserver:
		case ss_et_NoValidMaxLambda:
		case ss_et_NoValidSpect:
		case ss_et_NoValidColSysOrIndex:
		case ss_et_NoValidChar:
		case ss_et_NoValidValOrRef:
		case ss_et_DeviceIsOffline:
		case ss_et_NoDeviceFound:
			return inst_wrong_config | ec;

		case ss_et_BackupError:
		case ss_et_ProgramROMError:
		case ss_et_CheckSumWrong:
		case ss_et_MemoryError:
		case ss_et_FullMemory:
		case ss_et_EPROMFailure:
		case ss_et_MemoryFailure:
		case ss_et_PowerFailure:
		case ss_et_LampFailure:
		case ss_et_HardwareFailure:
		case ss_et_DriveError:
		case ss_et_FilterOutOfPos:
		case ss_et_ProgrammingError:
			return inst_hardware_fail | ec;

	}
	return inst_other_error | ec;
}

/* Incorporate a error into the snerr value */
void ss_incorp_err(ss *p, ss_et se) {
	if (p->snerr != ss_et_NoError)		/* Don't overrite any existing error */
		return;
	if (se == ss_set_NoError)
		return;

	p->snerr = se;
}

/* Incororate Remote Error Set values into snerr value */
/* Since ss_res is a bit mask, we just prioritize the errors. */
void ss_incorp_remerrset(ss *p, ss_res es) {
	int i, ii;
	if (p->snerr != ss_et_NoError)      /* Don't overrite any existing error */
		return;
	if (es == ss_res_NoError)
		return;

	for (i = ss_et_NoValidDStd, ii = 0x0001; ii < 0x10000; i++, ii <<= 1) {
		if ((ii & es) != 0) {
			break;
		}
	}
	p->snerr = i;
}

/* Incorporate a scan error into the snerr value */
void ss_incorp_scanerr(ss *p, ss_set se) {
	if (p->snerr != ss_et_NoError)		/* Don't overrite any existing error */
		return;
	if (se == ss_set_NoError)
		return;

	p->snerr = se + ss_et_DeviceIsOffline - 1;
}

/* Incorporate a device communication error into the snerr value */
void ss_incorp_comerr(ss *p, ss_cet se) {
	if (p->snerr != ss_et_NoError)		/* Don't overrite any existing error */
		return;
	if (se == ss_cet_NoError)
		return;

	p->snerr = se + ss_et_StopButNoStart - 1;
}

/* - - - - - - - - - - - - - - - - - - - - - */
/* EXECUTION: */

/* Interpret an icoms error into a SS error */
int icoms2ss_err(int se) {
	if (se & ICOM_USERM) {
		se &= ICOM_USERM;
		if (se == ICOM_USER)
			return ss_et_UserAbort;
		else if (se == ICOM_TERM)
			return ss_et_UserTerm;
		else if (se == ICOM_TRIG)
			return ss_et_UserTrig;
		else if (se == ICOM_CMND)
			return ss_et_UserCmnd;
	}
	if (se != ICOM_OK)
		return ss_et_SerialFail;
	return ss_et_NoError;
}

/* Terminate the send buffer, and then do a */
/* send/receieve to the device. */
/* Leave any error in p->snerr */
void ss_command(ss *p, double tmo) {
	int se;

	CHWSPACE(3);
	p->sbuf[0] = '\r';
	p->sbuf[1] = '\n';
	p->sbuf[2] = '\00';					/* write_read terminates on nul */

	p->rbuf = p->_rbuf;				/* Reset read pointer */
	if ((se = p->icom->write_read(p->icom, p->_sbuf, p->_rbuf, SS_MAX_RD_SIZE, '\n', 1, tmo)) != 0) {
		p->snerr = icoms2ss_err(se);
		return;
	}

	/* Figure out receive size, and clean up termination. */
	p->rbufe = p->_rbuf + strlen(p->_rbuf);
	if ((p->rbufe - p->rbuf) >= 1 && p->rbufe[-1] == '\n') {
		--p->rbufe;
		*p->rbufe = '\000';
	}
	if ((p->rbufe - p->rbuf) >= 1 && p->rbufe[-1] == '\r') {
		--p->rbufe;
		*p->rbufe = '\000';
	}

	/* Check receive format and look for a COM error */
	if ((p->rbufe - p->rbuf) < 1 || p->rbuf[0] != ':') {
		p->snerr = ss_et_BadAnsFormat;
		return;
	}
	p->rbuf++;

	/* See if this is a Spectroscan COM error */
	if ((p->rbufe - p->rbuf) >= 2
	  && p->rbuf[0] == 'D'
	  && p->rbuf[1] == '1') {

		if ((p->rbufe - p->rbuf) >= 4
		  && p->rbuf[0] == 'A'
		  && p->rbuf[1] == '0') {	/* COMM Error */
			p->rbuf += 4;
			ss_incorp_comerr(p, (ss_cet)ss_sub_1(p));
			return;
		}
	}

	/* See if it is a Spectrolino COMM error */
	if ((p->rbufe - p->rbuf) >= 2
	  && p->rbuf[0] == '2'
	  && p->rbuf[1] == '6') {	/* COMM Error */
		p->rbuf += 2;
		ss_incorp_comerr(p, (ss_cet)ss_sub_1(p));
		return;
	}
	return;
}

/* =================================================================== */
/* Complete Spectrolino instructions. */
/* [This is less elegant than Neil Okamoto's */
/*  table approach, but is more flexible, */
/*  and it does the job.] */

/* - - - - - - - - - - - - - - - - - - - - */
/* Device Initialisation and configuration */

/* Reset instrument */
inst_code so_do_ResetStatusDownload(
ss *p,
ss_smt sm		/* Init all or all except communications */
) {
	ss_add_soreq(p, ss_ResetStatusDownload);
	ss_add_1(p, 0x01);
	ss_add_1(p, 0x04);
	ss_add_1(p, sm);
	ss_command(p, IT_TMO);
	ss_sub_soans(p, ss_DownloadError);
	ss_incorp_remerrset(p, ss_sub_2(p));
	chended(p);
	return ss_inst_err(p);
}

/* Load various parameters, such as: */
/* comm flow control, baud rate, speaker, */
/* reflective/tranmission/emmission mode, */
/* custom filter on/off */
inst_code so_do_MeasControlDownload(
ss *p,
ss_ctt ct			/* Control */
) {
	ss_add_soreq(p, ss_MeasControlDownload);
	ss_add_1(p, ct);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_DownloadError);
	ss_incorp_remerrset(p, ss_sub_2(p));
	chended(p);
	
	return ss_inst_err(p);
}

/* Query various current parameters, such as: */
/* comm flow control, baud rate, speaker, */
/* reflective/tranmission/emmission mode, */
/* custom filter on/off. */
inst_code so_do_MeasControlRequest(
ss *p,
ss_ctt ct,		/* Control to query  */
ss_ctt *rct		/* Return current state */
) {
	ss_add_soreq(p, ss_MeasControlRequest);
	ss_add_1(p, ct);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_MeasControlAnswer);
	ss_sub_1(p);			/* Should be the same as ct */
	*rct = ss_sub_1(p);
	ss_incorp_remerrset(p, ss_sub_2(p));
	chended(p);
	return ss_inst_err(p);
}

/* Queries specific device data */
inst_code so_do_DeviceDataRequest(
ss *p,
char dn[19],		/* Return the device name */
ss_dnot *dno,		/* Return device number */
char pn[9],			/* Return the part number */
unsigned int *sn,	/* Return serial number */
char sv[13]			/* Return software version */
) {
	char rsv[17];	/* Space for resered field */
	ss_add_soreq(p, ss_DeviceDataRequest);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_DeviceDataAnswer);
	ss_sub_string(p, dn, 18);
	*dno = ss_sub_1(p);
	ss_sub_string(p, pn, 8);
	*sn = (unsigned int)ss_sub_4(p);
	ss_sub_string(p, sv, 12);
	ss_sub_string(p, rsv, 16);
	chended(p);
	return ss_inst_err(p);
}

/* Query special device data */
inst_code so_do_TargetIdRequest(
ss *p,
char dn[19],	/* Return Device Name */
int *sn,		/* Return Serial Number (1-65535) */
int *sr,		/* Return Software Release */
int *yp,		/* Return Year of production (e.g. 1996) */
int *mp,		/* Return Month of production (1-12) */
int *dp,		/* Return Day of production (1-31) */
int *hp,		/* Return Hour of production (0-23) */
int *np,		/* Return Minuite of production (0-59) */
ss_ttt *tt,		/* Return Target Tech Type (SPM/Spectrolino etc.) */
int *fswl,		/* Return First spectral wavelength (nm) */
int *nosw,		/* Return Number of spectral wavelengths */
int *dpsw		/* Return Distance between spectral wavelengths (nm) */
) {
	ss_add_soreq(p, ss_TargetIdRequest);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_TargetIdAnswer);
	ss_sub_string(p, dn, 18);
	*sn = ss_sub_2(p);
	*sr = ss_sub_2(p);
	*yp = ss_sub_2(p);
	*mp = ss_sub_2(p);
	*dp = ss_sub_2(p);
	*hp = ss_sub_2(p);
	*np = ss_sub_2(p);
	*tt = ss_sub_1(p);
	*fswl = ss_sub_2(p);
	*nosw = ss_sub_2(p);
	*dpsw = ss_sub_2(p);
	chended(p);
	return ss_inst_err(p);
}

/* - - - - - - - - - - - - - */
/* Measurement configuration */

/* Query the standard or user definable densitometric tables */
inst_code so_do_DensTabRequest(
ss *p,
ss_dst ds,			/* Density standard (ANSI/DIN/User etc.) */
ss_dst *rds,		/* Return Density standard (ANSI/DIN/User etc.) */
double sp[4][36]	/* Return 4 * 36 spectral weighting values */
) {
	int n,i;
	ss_add_soreq(p, ss_DensTabRequest);
	ss_add_1(p, 0x00);
	ss_add_1(p, ds);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_DensTabAnswer);
	ss_sub_soans(p, 0x00);
	*rds = ss_sub_1(p);
	for (n = 0; n < 4; n++)
		for (i = 0; i < 36; i++)
			sp[n][i] = ss_sub_double(p);
	ss_incorp_remerrset(p, ss_sub_2(p));
	chended(p);
	return ss_inst_err(p);
}

/* Download user definable densitometric tables */
inst_code so_do_DensTabDownload(
ss *p,
double sp[4][36]	/* 4 * 36 spectral weighting values */
) {
	int i, n;
	ss_add_soreq(p, ss_DensTabDownload);
	ss_add_1(p, 0x08);
	for (n = 0; n < 4; n++)
		for (i = 0; i < 36; i++)
			ss_add_double(p, sp[n][i]);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_DownloadError);
	ss_incorp_remerrset(p, ss_sub_2(p));
	chended(p);
	return ss_inst_err(p);
}

/* Set slope values for densitometry */
inst_code so_do_SlopeDownload(
ss *p,
double dv[4]	/* Db Dc Dm Dy density values */
) {
	int i;
	ss_add_soreq(p, ss_SlopeDownload);
	for (i = 0; i < 4; i++)
		ss_add_double(p, dv[i]);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_DownloadError);
	ss_incorp_remerrset(p, ss_sub_2(p));
	chended(p);
	return ss_inst_err(p);
}

/* Query slope values of densitometry */
inst_code so_do_SlopeRequest(
ss *p,
double dv[4]	/* Return Db Dc Dm Dy density values */
) {
	int i;
	ss_add_soreq(p, ss_SlopeRequest);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_SlopeAnswer);
	for (i = 0; i < 4; i++)
		dv[i] = ss_sub_double(p);
	chended(p);
	return ss_inst_err(p);
}

/* Set the colorimetric parameters */
inst_code so_do_ParameterDownload(
ss *p,
ss_dst ds,	/* Density standard (ANSI/DIN etc.) */
ss_wbt wb,	/* White base (Paper/Absolute) */
ss_ilt it,	/* Illuminant type (A/C/D65 etc.) */
ss_ot  ot	/* Observer type (2deg/10deg) */
) {
	ss_add_soreq(p, ss_ParameterDownload);
	ss_add_1(p, ds);
	ss_add_1(p, wb);
	ss_add_1(p, it);
	ss_add_1(p, ot);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_DownloadError);
	ss_incorp_remerrset(p, ss_sub_2(p));
	chended(p);
	return ss_inst_err(p);
}

/* Query colorimetric parameters */
inst_code so_do_ParameterRequest(
ss *p,
ss_dst *ds,		/* Return Density Standard */
ss_wbt *wb,		/* Return White Base */
ss_ilt *it,		/* Return Illuminant type (A/C/D65/User etc.) */
ss_ot  *ot,		/* Return Observer type (2deg/10deg) */
ss_aft *af		/* Return Filter being used (None/Pol/D65/UV/custom */
) {
	ss_add_soreq(p, ss_ParameterRequest);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_ParameterAnswer);
	*ds = ss_sub_1(p);
	*wb = ss_sub_1(p);
	*it = ss_sub_1(p);
	*ot = ss_sub_1(p);
	*af = ss_sub_1(p);
	chended(p);
	return ss_inst_err(p);
}

/* Query the standard or user defined illuminant tables (Colorimetry) */
inst_code so_do_IllumTabRequest(
ss *p,
ss_ilt it,		/* Illuminant type (A/C/D65/User etc.) */
ss_ilt *rit,	/* Return Illuminant type (A/C/D65/User etc.) */
double sp[36]	/* Return 36 spectral values */
) {
	int i;
	ss_add_soreq(p, ss_IllumTabRequest);
	ss_add_1(p, 0x00);
	ss_add_1(p, it);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_IllumTabAnswer);
	ss_sub_soans(p, 0x00);
	*rit = ss_sub_1(p);
	for (i = 0; i < 36; i++)
		sp[i] = ss_sub_double(p);
	ss_incorp_remerrset(p, ss_sub_2(p));
	chended(p);
	return ss_inst_err(p);
}

/* Download user definable illuminant tables (Colorimetry) */
inst_code so_do_IllumTabDownload(
ss *p,
double sp[36]	/* 36 spectral values to set */
) {
	int i;
	ss_add_soreq(p, ss_IllumTabDownload);
	ss_add_1(p, 0x08);
	for (i = 0; i < 36; i++)
		ss_add_double(p, sp[i]);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_DownloadError);
	ss_incorp_remerrset(p, ss_sub_2(p));
	chended(p);
	return ss_inst_err(p);
}

/* Query for the color temperature of daylight illuminant Dxx */
inst_code so_do_GetValNr(
ss *p,
int *ct		/* Return color temperature in deg K/100 */
) {
	ss_add_soreq(p, ss_GetValNr);
	ss_add_1(p, 0x60);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_ValNrAnswer);
	ss_sub_soans(p, 0x60);
	*ct = ss_sub_1(p);
	ss_incorp_remerrset(p, ss_sub_2(p));
	chended(p);
	return ss_inst_err(p);
}

/* Download user definable illuminant tables (Colorimetry) */
inst_code so_do_SetValNr(
ss *p,
int ct		/* Color temperature to set for illuminant Dxx in deg K/100 */
) {
	ss_add_soreq(p, ss_IllumTabDownload);
	ss_add_1(p, 0x60);
	ss_add_2(p, ct);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_DownloadError);
	ss_incorp_remerrset(p, ss_sub_2(p));
	chended(p);
	return ss_inst_err(p);
}

/* Queries the spectra of the white reference for the desired filter */
inst_code so_do_WhiteReferenceRequest(
ss *p,
ss_aft af,		/* Filter being queried (None/Pol/D65/UV/custom */
ss_aft *raf,	/* Return filter being queried (None/Pol/D65/UV/custom */
double sp[36],	/* Return 36 spectral values */
ss_owrt *owr,	/* Return original white reference */
char dtn[19]	/* Return name of data table */
) {
	int i;
	ss_add_soreq(p, ss_WhiteReferenceRequest);
	ss_add_1(p, af);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_WhiteReferenceAnswer);
	*raf = ss_sub_1(p);
	for (i = 0; i < 36; i++)
		sp[i] = ss_sub_double(p);
	*owr = ss_sub_1(p);
	ss_sub_string(p, dtn, 18);
	chended(p);
	return ss_inst_err(p);
}

/* Load spectra of a user defined white reference for the desired filter. */
/* A name can be given to the white reference. */
inst_code so_do_WhiteReferenceDownld(
ss *p,
ss_aft af,		/* Filter being set (None/Pol/D65/UV/custom */
double sp[36],	/* 36 spectral values being set */
char dtn[19]	/* Name for data table */
) {
	int i;
	ss_add_soreq(p, ss_WhiteReferenceDownld);
	ss_add_1(p, af);
	for (i = 0; i < 36; i++)
		ss_add_double(p, sp[i]);
	ss_add_string(p, dtn, 18);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_DownloadError);
	ss_incorp_remerrset(p, ss_sub_2(p));
	chended(p);
	return ss_inst_err(p);
}

/* Query the reference value for the relative photometric (emission) reference value */
inst_code so_do_FloatRequest(
ss *p,
ss_comft comf,		/* Choose common float type (PhotometricYRef) */
ss_comft *rcomf,	/* Return common float type (PhotometricYRef) */
double *comfv		/* Return the reference value */
) {
	ss_add_soreq(p, ss_FloatRequest);
	ss_add_1(p, comf);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_FloatAnswer);
	*rcomf = ss_sub_1(p);
	*comfv = ss_sub_double(p);
	chended(p);
	return ss_inst_err(p);
}

/* Set the reference value for the relative photometric (emission) reference value */
inst_code so_do_FloatDownload(
ss *p,
ss_comft comf,		/* Choose common float type (PhotometricYRef) */
double comfv		/* The reference value */
) {
	ss_add_soreq(p, ss_FloatDownload);
	ss_add_1(p, comf);
	ss_add_double(p, comfv);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_DownloadError);
	ss_incorp_remerrset(p, ss_sub_2(p));
	chended(p);
	return ss_inst_err(p);
}

/* - - - - - - */
/* Calibration */

/* Reset the spectra of the respective white reference to the original data */
inst_code so_do_ExecWhiteRefToOrigDat(
ss *p
) {
	ss_add_soreq(p, ss_ExecWhiteRefToOrigDat);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_DownloadError);		/* Instrument behavour doesn't match doco. */
	ss_incorp_remerrset(p, ss_sub_2(p));
	chended(p);
	return ss_inst_err(p);
}


/* Perform a Reference Measurement */
inst_code so_do_ExecRefMeasurement(
ss *p,
ss_mmt mm	/* Measurement Mode (Meas/Cal etc.) */
) {
#ifdef EMSST
	if (p->tmode != 0) {
		ss_rvt rv;
		double sp[36];
		ss_nmt nm;
		p->tmode = 0;
		ss_do_MoveAndMeasure(p, 155.0, 230.0, sp, &rv);
		so_do_NewMeasureRequest(p, &nm);
		ss_do_MoveAbsolut(p, p->sbr, p->sbx, p->sby);
		p->tmode = 1;
		return (inst_notify | ss_et_WhiteMeasOK);
	}
#endif
	ss_add_soreq(p, ss_ExecRefMeasurement);
	ss_add_1(p, 0x09);
	ss_add_1(p, mm);
	ss_command(p, 2.0 * DF_TMO);
	ss_sub_soans(p, ss_ExecError);
	ss_incorp_err(p, ss_sub_1(p));
	chended(p);
	return ss_inst_err(p);
}

/* Perform a White Measurement - not recommended */
/* (ExecRefMeasuremen is preferred instead) */
inst_code so_do_ExecWhiteMeasurement(
ss *p
) {
	ss_add_soreq(p, ss_ExecWhiteMeasurement);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_ExecError);
	ss_incorp_err(p, ss_sub_1(p));
	chended(p);
	return ss_inst_err(p);
}

/* - - - - - - - - - - - - */
/* Performing measurements */


/* Perform a Measurement */
inst_code so_do_ExecMeasurement(
ss *p
) {
#ifdef EMSST
	if (p->tmode != 0) {
		inst_code rc;
		p->tmode = 0;
		ss_do_MoveAbsolut(p, ss_rt_SensorRef, 155.0, 230.0);
		ss_do_MoveDown(p);
		rc = so_do_ExecMeasurement(p);
		ss_do_MoveUp(p);
		ss_do_MoveAbsolut(p, p->sbr, p->sbx, p->sby);
		p->tmode = 1;
		return rc;
	}
#endif
	ss_add_soreq(p, ss_ExecMeasurement);
	ss_command(p, 2.0 * DF_TMO);
	ss_sub_soans(p, ss_ExecError);
	ss_incorp_err(p, ss_sub_1(p));
	chended(p);
	return ss_inst_err(p);
}

/* Define automatic output after each measurement */
/* [Note that dealing with the resulting measurement replies */
/*  isn't directly supported, currently.] */
inst_code so_do_SetMeasurementOutput(
ss *p,
ss_ost os,		/* Type of output to request */
ss_os o			/* bitmask of output */
) {
	ss_add_soreq(p, ss_SetMeasurementOutput);
	ss_add_1(p, os);
	ss_add_1(p, o.i);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_DownloadError);
	ss_incorp_remerrset(p, ss_sub_2(p));
	chended(p);
	
	return ss_inst_err(p);
}

/* - - - - - - - - */
/* Getting results */

/* so_do_Printout isn't currently defined. */
/* It needs to cope with the expected number of values that */
/* will be returned. This could probably be figured out from */
/* the string itself ? */

/* Query Density measurement results and associated parameters */
inst_code so_do_DensityParameterRequest(
ss *p,
ss_cst *rct,	/* Return Color Type (Lab/XYZ etc.) */
double dv[4],	/* Return Db Dc Dm Dy density values */
ss_sdft *sdf,	/* Return Standard Density Filter (Db/Dc/Dm/Dy) */
ss_rvt *rv,		/* Return Reference Valid Flag */
ss_aft *af,		/* Return filter being used (None/Pol/D65/UV/custom */
ss_wbt *wb,		/* Return white base (Paper/Absolute) */
ss_dst *ds,		/* Return Density standard (ANSI/DIN/User etc.) */
ss_ilt *rit,	/* Return Illuminant type (A/C/D65/User etc.) */
ss_ot  *ot		/* Return Observer type (2deg/10deg) */
) {
	int i;
	ss_add_soreq(p, ss_DensityParameterRequest);
	ss_add_1(p, 0x09);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_DensityParameterAnswer);
	ss_sub_soans(p, 0x09);
	for (i = 0; i < 4; i++)
		dv[i] = ss_sub_double(p);
	*sdf = ss_sub_1(p);
	*rv = ss_sub_1(p);
	*af = ss_sub_1(p);
	*wb = ss_sub_1(p);
	ss_sub_soans(p, 0x02);
	*ds = ss_sub_1(p);
	ss_incorp_remerrset(p, ss_sub_2(p));
	chended(p);
	return ss_inst_err(p);
}

/* Query Densitometric measurement values - not recommended */
/* (DensityParameterRequest is preferred instead) */
inst_code so_do_DensityRequest(
ss *p,
double dv[4],	/* Return Db Dc Dm Dy density values */
ss_sdft *sdf,	/* Return Standard Density Filter (Db/Dc/Dm/Dy) */
ss_rvt  *rv		/* Return Reference Valid */
) {
	int i;
	ss_add_soreq(p, ss_DensityRequest);
	ss_add_1(p, 0x09);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_DensityAnswer);
	ss_sub_soans(p, 0x09);
	for (i = 0; i < 4; i++)
		dv[i] = ss_sub_double(p);
	*sdf = ss_sub_1(p);
	*rv = ss_sub_1(p);
	ss_incorp_remerrset(p, ss_sub_2(p));
	chended(p);
	return ss_inst_err(p);
}

/* Query maximum density reading */
inst_code so_do_DmaxRequest(
ss *p,
double *Dmax,	/* Return Value of Maximum Density */
int *lambda,	/* Return wavelength where maximum density was found */
ss_dmot *dmo,	/* Return Dmax OK flag. */
ss_rvt *rv		/* Return Reference Valid Flag */
) {
	ss_add_soreq(p, ss_DmaxRequest);
	ss_add_1(p, 0x09);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_DmaxAnswer);
	ss_sub_soans(p, 0x09);
	*Dmax = ss_sub_double(p);
	*lambda = ss_sub_2(p);
	*dmo = ss_sub_1(p);
	*rv = ss_sub_1(p);
	ss_incorp_remerrset(p, ss_sub_2(p));
	chended(p);
	return ss_inst_err(p);
}

/* Query Color measurement results and associated parameters */
inst_code so_do_CParameterRequest(
ss *p,
ss_cst ct,		/* Choose Color Type (Lab/XYZ etc.) */
ss_cst *rct,	/* Return Color Type (Lab/XYZ etc.) */
double cv[3],	/* Return 3 color values */
ss_rvt *rv,		/* Return Reference Valid Flag */
ss_aft *af,		/* Return filter being used (None/Pol/D65/UV/custom) */
ss_wbt *wb,		/* Return white base (Paper/Absolute) */
ss_ilt *it,		/* Return Illuminant type (A/C/D65/User etc.) */
ss_ot  *ot		/* Return Observer type (2deg/10deg) */
) {
	int i;
	ss_add_soreq(p, ss_CParameterRequest);
	ss_add_1(p, 0x09);
	ss_add_1(p,ct);
	ss_command(p, DF_TMO);

	ss_sub_soans(p, ss_CParameterAnswer);
	ss_sub_soans(p, 0x09);
	*rct = ss_sub_1(p);
	for (i = 0; i < 3; i++)
		cv[i] = ss_sub_double(p);
	*rv = ss_sub_1(p);
	*af = ss_sub_1(p);
	*wb = ss_sub_1(p);
	ss_sub_soans(p, 0x02);
	*it = ss_sub_1(p);
	*ot = ss_sub_1(p);
	ss_incorp_remerrset(p, ss_sub_2(p));
	chended(p);
	return ss_inst_err(p);
}

/* Query Colorimetric measurement results - not recommended */
/* (CParameterRequest is prefered instead) */
inst_code so_do_CRequest(
ss *p,
ss_cst *ct,		/* Return Color Type (Lab/XYZ etc.) */
double *cv[3],	/* Return 3 color values */
ss_rvt *rv		/* Return Reference Valid Flag */
) {
	int i;
	ss_add_soreq(p, ss_CRequest);
	ss_add_1(p, 0x09);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_CAnswer);
	ss_sub_soans(p, 0x09);
	*ct = ss_sub_1(p);
	for (i = 0; i < 3; i++)
		*cv[i] = ss_sub_double(p);
	*rv = ss_sub_1(p);
	ss_incorp_remerrset(p, ss_sub_2(p));
	chended(p);
	return ss_inst_err(p);
}

/* Query Spectral measurement results and associated parameters */
inst_code so_do_SpecParameterRequest(
ss *p,
ss_st st,		/* Choose Spectrum Type (Reflectance/Density) */
ss_st *rst,		/* Return Spectrum Type (Reflectance/Density) */
double sp[36],	/* Return 36 spectral values */
ss_rvt *rv,		/* Return Reference Valid Flag */
ss_aft *af,		/* Return filter being used (None/Pol/D65/UV/custom */
ss_wbt *wb		/* Return white base (Paper/Absolute) */
) {
	int i;
	ss_add_soreq(p, ss_SpecParameterRequest);
	ss_add_1(p, 0x09);
	ss_add_1(p,st);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_SpecParameterAnswer);
	ss_sub_soans(p, 0x09);
	*rst = ss_sub_1(p);
	for (i = 0; i < 36; i++)
		sp[i] = ss_sub_double(p);
	*rv = ss_sub_1(p);
	*af = ss_sub_1(p);
	*wb = ss_sub_1(p);
	ss_sub_soans(p, 0x02);
	ss_incorp_remerrset(p, ss_sub_2(p));
	chended(p);
	return ss_inst_err(p);
}

/* Query Spectral measurement results - not recommended */
/* (SpecParameterRequest is preferred instead) */
inst_code so_do_SpectrumRequest(
ss *p,
ss_st *st,		/* Return Spectrum Type (Reflectance/Density) */
double sp[36],	/* Return 36 spectral values */
ss_rvt *rv		/* Return Reference Valid Flag */
) {
	int i;
	ss_add_soreq(p, ss_SpectrumRequest);
	ss_add_1(p, 0x09);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_SpectrumAnswer);
	ss_sub_soans(p, 0x09);
	*st = ss_sub_1(p);
	for (i = 0; i < 36; i++)
		sp[i] = ss_sub_double(p);
	*rv = ss_sub_1(p);
	ss_incorp_remerrset(p, ss_sub_2(p));
	chended(p);
	return ss_inst_err(p);
}

/* - - - - - -  */
/* Miscelanious */

/* Query whether a new measurement was performed since the last access */
inst_code so_do_NewMeasureRequest(
ss *p,
ss_nmt *nm		/* Return New Measurement (None/Meas/White etc.) */
) {
	ss_add_soreq(p, ss_NewMeasureRequest);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_NewMeasureAnswer);
	*nm = ss_sub_1(p);
	ss_sub_soans(p, 0x09);
	chended(p);
	return ss_inst_err(p);
}

/* Query whether a key was pressed since the last access */
inst_code so_do_NewKeyRequest(
ss *p,
ss_nkt *nk,		/* Return whether a new key was pressed */
ss_ks *k		/* Return the key that was pressed (none/meas) */
) {
	ss_add_soreq(p, ss_NewKeyRequest);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_NewKeyAnswer);
	*nk = ss_sub_1(p);
	*k = ss_sub_2(p);
	chended(p);
	return ss_inst_err(p);
}

/* Query for the general error status */
inst_code so_do_ActErrorRequest(
ss *p
) {
	ss_add_soreq(p, ss_ActErrorRequest);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_ActErrorAnswer);
	ss_incorp_err(p, ss_sub_1(p));
	chended(p);
	return ss_inst_err(p);
}

/* Set Target On/Off status (locks key function of device?) */
inst_code so_do_TargetOnOffStDownload(
ss *p,
ss_toost oo		/* Activated/Deactivated */
) {
	ss_add_soreq(p, ss_TargetOnOffStDownload);
	ss_add_1(p, 0x00);
	ss_add_1(p, oo);
	ss_add_1(p, 0x00);
	ss_command(p, DF_TMO);
	ss_sub_soans(p, ss_DownloadError);
	ss_incorp_remerrset(p, ss_sub_2(p));
	chended(p);
	return ss_inst_err(p);
}

/* =========================================== */
/* SpectroScan/T specific commands and queries */

/* - - - - - - - - - - - - - - - - - - - - */
/* Device Initialisation and configuration */

/* Initialise the device. Scans the Spectrolino */
/* (Doesn't work when device is offline ) */
inst_code ss_do_ScanInitializeDevice(ss *p) {
	ss_add_ssreq(p, ss_InitializeDevice);
	ss_command(p, IT_TMO);
	ss_sub_ssans(p, ss_ErrorAnswer);
	ss_incorp_scanerr(p, ss_sub_1(p));
	chended(p);
	return ss_inst_err(p);
}

/* Establish communications between the SpectroScan and Spectrolino */
/* at the highest possible baud rate. */
/* (Doesn't work when device is offline ) */
inst_code ss_do_ScanSpectrolino(ss *p) {
	ss_add_ssreq(p, ss_ScanSpectrolino);
	ss_command(p, IT_TMO);
	ss_sub_ssans(p, ss_ErrorAnswer);
	ss_incorp_scanerr(p, ss_sub_1(p));
	chended(p);
	return ss_inst_err(p);
}

/* Establish the zero position of the motors and set the position to 0,0 */ 
/* (Doesn't work when device is offline ) */
inst_code ss_do_InitMotorPosition(ss *p) {
	ss_add_ssreq(p, ss_InitMotorPosition);
	ss_command(p, MV_TMO);
	ss_sub_ssans(p, ss_ErrorAnswer);
	ss_incorp_scanerr(p, ss_sub_1(p));
	chended(p);
	return ss_inst_err(p);
}

/* Change the SpectroScan baud rate */
inst_code ss_do_ChangeBaudRate(
ss *p,
ss_bt br		/* Baud rate (110 - 57600) */
) {
	ss_add_ssreq(p, ss_ChangeBaudRate);
	ss_add_1(p, br);
	ss_command(p, SH_TMO);		/* Don't really expect an answer */
	ss_sub_ssans(p, ss_ErrorAnswer);
	ss_incorp_scanerr(p, ss_sub_1(p));
	chended(p);
	return ss_inst_err(p);		/* Will probably bomb because comms will break down */
}

/* Change the SpectroScan handshaking mode. */
inst_code ss_do_ChangeHandshake(
ss *p,
ss_hst hs		/* Handshake type (None/XonXoff/HW) */
) {
	ss_add_ssreq(p, ss_ChangeHandshake);
	ss_add_1(p, hs);
	ss_command(p, DF_TMO);
	ss_sub_ssans(p, ss_ErrorAnswer);
	ss_incorp_scanerr(p, ss_sub_1(p));
	chended(p);
	return ss_inst_err(p);
}

/* Query the type of XY table */
inst_code ss_do_OutputType(
ss *p,
char dt[19]		/* Return Device Type ("SpectroScan", "SpectroScan " or "SpectroScanT") */
) {
	ss_add_ssreq(p, ss_OutputType);
	ss_command(p, DF_TMO);
	ss_sub_ssans(p, ss_TypeAnswer);
	ss_sub_string(p, dt, 18);
	chended(p);
#ifdef EMSST
	if (strcmp(dt,"SpectroScan ") == 0
	 || strcmp(dt,"SpectroScan") == 0)
		sprintf(dt,"SpectroScanT");
#endif
	return ss_inst_err(p);
}

/* Query the serial number of the XY table */
inst_code ss_do_OutputSerialNumber(
ss *p,
unsigned int *sn	/* Return serial number */
) {
	ss_add_ssreq(p, ss_OutputSerialNumber);
	ss_command(p, DF_TMO);
	ss_sub_ssans(p, ss_SerialNumberAnswer);
	*sn = (unsigned int)ss_sub_4(p);
	chended(p);
	return ss_inst_err(p);
}

/* Query the part number of the XY table */
inst_code ss_do_OutputArticleNumber(
ss *p,
char pn[9]		/* Return Part Number */
) {
	ss_add_ssreq(p, ss_OutputArticleNumber);
	ss_command(p, DF_TMO);
	ss_sub_ssans(p, ss_ArticleNumberAnswer);
	ss_sub_string(p, pn, 8);
	chended(p);
	return ss_inst_err(p);
}

/* Query the production date of the XY table */
inst_code ss_do_OutputProductionDate(
ss *p,
int *yp,	/* Return Year of production (e.g. 1996) */
int *mp,	/* Return Month of production (1-12) */
int *dp		/* Return Day of production (1-31) */
) {
	ss_add_ssreq(p, ss_OutputProductionDate);
	ss_command(p, DF_TMO);
	ss_sub_ssans(p, ss_ProductionDateAnswer);
	*dp = ss_sub_2(p);
	*mp = ss_sub_2(p);
	*yp = ss_sub_2(p);
	chended(p);
	return ss_inst_err(p);
}

/* Query the Software Version of the XY table */
inst_code ss_do_OutputSoftwareVersion(
ss *p,
char sv[13]		/* Return Software Version */
) {
	ss_add_ssreq(p, ss_OutputSoftwareVersion);
	ss_command(p, DF_TMO);
	ss_sub_ssans(p, ss_SoftwareVersionAnswer);
	ss_sub_string(p, sv, 12);
	chended(p);
	return ss_inst_err(p);
}

/* - - - - - - - - - - - - - */
/* Measurement configuration */

/* Set the SpectroScanT to reflectance or transmission. */
/* The Spectrolino is also automatically set to the corresponding mode. */
/* (Doesn't work when device is offline ) */
inst_code ss_do_SetTableMode(
ss *p,
ss_tmt tm	/* Table mode (Reflectance/Transmission) */
) {
#ifdef EMSST
	if (tm == ss_tmt_Transmission) {
		if (p->tmode == 0) {
			ss_do_MoveAbsolut(p, p->sbr, p->sbx, p->sby);	/* ?? */
		}
		p->tmode = 1;
	} else {
		if (p->tmode != 0) {
			p->tmode = 0;
			ss_do_MoveHome(p);
		}
		p->tmode = 0;
	}
	return inst_ok;
#endif
	ss_add_ssreq(p, ss_SetTableMode);
	ss_add_1(p, tm);
	ss_command(p, IT_TMO);
	ss_sub_ssans(p, ss_ErrorAnswer);
	ss_incorp_scanerr(p, ss_sub_1(p));
	chended(p);
	return ss_inst_err(p);
}

/* - - - - - - - - - - - - - */
/* Table operation */

/* Set the SpectrScan to online. All moving keys are disabled. */
/* (Only valid when device is in reflectance mode.) */
inst_code ss_do_SetDeviceOnline(ss *p) {
#ifdef EMSST
	if (p->tmode != 0)
		*((char *)0) = 55;
//	return inst_unsupported;
#endif
	ss_add_ssreq(p, ss_SetDeviceOnline);
	ss_command(p, DF_TMO);
	ss_sub_ssans(p, ss_ErrorAnswer);
	ss_incorp_scanerr(p, ss_sub_1(p));
	chended(p);
	return ss_inst_err(p);
}

/* Set the SpectrScan to offline. All moving keys are enabled. */
/* (Only valid when device is in reflectance mode.) */
/* (All remote commands to move the device will be ignored.) */
inst_code ss_do_SetDeviceOffline(ss *p) {
#ifdef EMSST
	if (p->tmode != 0)
		*((char *)0) = 55;
#endif
	ss_add_ssreq(p, ss_SetDeviceOffline);
	ss_command(p, DF_TMO);
	ss_sub_ssans(p, ss_ErrorAnswer);
	ss_incorp_scanerr(p, ss_sub_1(p));
	chended(p);
	return ss_inst_err(p);
}

/* Enable electrostatic paper hold. */
/* (Not valid when device is offline) */
inst_code ss_do_HoldPaper(ss *p) {
	ss_add_ssreq(p, ss_HoldPaper);
	ss_command(p, DF_TMO);
	ss_sub_ssans(p, ss_ErrorAnswer);
	ss_incorp_scanerr(p, ss_sub_1(p));
	chended(p);
	return ss_inst_err(p);
}

/* Disable electrostatic paper hold. */
/* (Not valid when device is offline) */
inst_code ss_do_ReleasePaper(ss *p) {
	ss_add_ssreq(p, ss_ReleasePaper);
	ss_command(p, DF_TMO);
	ss_sub_ssans(p, ss_ErrorAnswer);
	ss_incorp_scanerr(p, ss_sub_1(p));
	chended(p);
	return ss_inst_err(p);
}

/* - - - - - - */
/* Positioning */

/* Move either the sight or sensor to an absolute position. */
/* (Doesn't work when device is offline or transmissioin mode.) */
inst_code ss_do_MoveAbsolut(
ss *p,
ss_rt  r,	/* Reference (Sensor/Sight) */
double x,	/* X coord in mm, 0-310.0, accurate to 0.1mm */
double y	/* Y coord in mm, 0-230.0, accurate to 0.1mm */
) {
#ifdef EMSST
	if (p->tmode != 0)
		*((char *)0) = 55;
#endif
	ss_add_ssreq(p, ss_MoveAbsolut);
	ss_add_1(p, r);
	ss_add_2(p, (int)(x * 10 + 0.5));
	ss_add_2(p, (int)(y * 10 + 0.5));
	ss_command(p, MV_TMO);
	ss_sub_ssans(p, ss_ErrorAnswer);
	ss_incorp_scanerr(p, ss_sub_1(p));
	chended(p);
	return ss_inst_err(p);
}

/* Move relative to current position. */
/* (Doesn't work when device is offline or transmissioin mode.) */
inst_code ss_do_MoveRelative(
ss *p,
double x,	/* X distance in mm, 0-310.0, accurate to 0.1mm */
double y	/* Y distance in mm, 0-230.0, accurate to 0.1mm */
) {
#ifdef EMSST
	if (p->tmode != 0)
		*((char *)0) = 55;
#endif
	ss_add_ssreq(p, ss_MoveRelative);
	ss_add_2(p, (int)(x * 10 + 0.5));
	ss_add_2(p, (int)(y * 10 + 0.5));
	ss_command(p, MV_TMO);
	ss_sub_ssans(p, ss_ErrorAnswer);
	ss_incorp_scanerr(p, ss_sub_1(p));
	chended(p);
	return ss_inst_err(p);
}

/* Move to the home position (== 0,0). */
/* (Doesn't work when device is offline or transmissioin mode.) */
inst_code ss_do_MoveHome(
ss *p
) {
#ifdef EMSST
	if (p->tmode != 0)
		*((char *)0) = 55;
#endif
	ss_add_ssreq(p, ss_MoveHome);
	ss_command(p, MV_TMO);
	ss_sub_ssans(p, ss_ErrorAnswer);
	ss_incorp_scanerr(p, ss_sub_1(p));
	chended(p);
	return ss_inst_err(p);
}

/* Move to the sensor up. */
/* (Doesn't work when device is offline or transmissioin mode.) */
inst_code ss_do_MoveUp(
ss *p
) {
#ifdef EMSST
	if (p->tmode != 0)
		*((char *)0) = 55;
#endif
	ss_add_ssreq(p, ss_MoveUp);
	ss_command(p, MV_TMO);
	ss_sub_ssans(p, ss_ErrorAnswer);
	ss_incorp_scanerr(p, ss_sub_1(p));
	chended(p);
	return ss_inst_err(p);
}

/* Move to the sensor down. */
/* (Doesn't work when device is offline or transmission mode.) */
inst_code ss_do_MoveDown(
ss *p
) {
#ifdef EMSST
	if (p->tmode != 0)
		*((char *)0) = 55;
#endif
	ss_add_ssreq(p, ss_MoveDown);
	ss_command(p, MV_TMO);
	ss_sub_ssans(p, ss_ErrorAnswer);
	ss_incorp_scanerr(p, ss_sub_1(p));
	chended(p);
	return ss_inst_err(p);
}

/* Query the current absolute position of the sensor or sight. */
/* (Doesn't work when device is offline or transmissioin mode.) */
inst_code ss_do_OutputActualPosition(
ss *p,
ss_rt  r,	/* Reference (Sensor/Sight) */
ss_rt  *rr,	/* Return reference (Sensor/Sight) */
double *x,	/* Return the X coord in mm, 0-310.0, accurate to 0.1mm */
double *y,	/* Return the Y coord in mm, 0-230.0, accurate to 0.1mm */
ss_zkt *zk	/* Return the Z coordinate (Up/Down) */
) {
#ifdef EMSST
	if (p->tmode != 0)
		*((char *)0) = 55;
#endif
	ss_add_ssreq(p, ss_OutputActualPosition);
	ss_add_1(p, r);
	ss_command(p, DF_TMO);
	ss_sub_ssans(p, ss_PositionAnswer);
	*rr = ss_sub_1(p);
	ss_sub_soans(p, 0x00);
	ss_sub_soans(p, 0x00);
	*x = ss_sub_2(p)/10.0;
	*y = ss_sub_2(p)/10.0;
	*zk = ss_sub_1(p);
	chended(p);
	return ss_inst_err(p);
}

/* Move to the white reference position */
/* (Doesn't work when device is offline or transmissioin mode.) */
inst_code ss_do_MoveToWhiteRefPos(
ss *p,
ss_wrpt wrp		/* White Reference Position (Tile1/Tile2) */
) {
#ifdef EMSST
	if (p->tmode != 0)
		*((char *)0) = 55;
#endif
	ss_add_ssreq(p, ss_MoveToWhiteRefPos);
	ss_add_1(p, wrp);
	ss_command(p, MV_TMO);
	ss_sub_ssans(p, ss_ErrorAnswer);
	ss_incorp_scanerr(p, ss_sub_1(p));
	chended(p);
	return ss_inst_err(p);
}

/* - - - - - - - - - - - - */
/* Performing measurements */

/* Move the sensor to an absolute position, move the */
/* sensor down, execute a measurement, move the head up, */
/* and return spectral measuring results. */
inst_code ss_do_MoveAndMeasure(
ss *p,
double x,		/* X coord in mm, 0-310.0, accurate to 0.1mm */
double y,		/* Y coord in mm, 0-230.0, accurate to 0.1mm */
double sp[36],	/* Return 36 spectral values */
ss_rvt *rv		/* Return Reference Valid Flag */
) {
#ifdef EMSST
	/* Not sure if this is valid on the SpectroScanT in trans mode ? */
	if (p->tmode != 0) {
		inst_code rc;
		p->tmode = 0;
		rc = ss_do_MoveAndMeasure(p, 155.0, 230.0, sp, rv);
		ss_do_MoveAbsolut(p, p->sbr, p->sbx, p->sby);
		p->tmode = 1;
		return rc;
	}
#endif
	ss_add_ssreq(p, ss_MoveAndMeasure);
	ss_add_2(p, (int)(x * 10 + 0.5));
	ss_add_2(p, (int)(y * 10 + 0.5));
	ss_command(p, MV_TMO);
	if (ss_peek_ans(p) == ss_SpectrumAnswer) {
		int i;
		ss_sub_soans(p, ss_SpectrumAnswer);
		ss_sub_soans(p, 0x09);
		ss_sub_soans(p, 0x00);
		for (i = 0; i < 36; i++)
			sp[i] = ss_sub_double(p);
		*rv = ss_sub_1(p);
		ss_incorp_remerrset(p, ss_sub_2(p));
	} else {
		ss_sub_ssans(p, ss_ErrorAnswer);
		ss_incorp_scanerr(p, ss_sub_1(p));
	}
	chended(p);
	return ss_inst_err(p);
}

/* - - - - - -  */
/* Miscelanious */

/* Set the SpectroScanT transmission light level during standby. */
/* (Only valid on SpectroScanT in transmission mode) */
inst_code ss_do_SetLightLevel(
ss *p,
ss_llt ll	/* Transmission light level (Off/Surround/Low) */
) {
#ifdef EMSST
	if (p->tmode != 0)
		return inst_ok;
	else
		*((char *)0) = 55;
#endif
	ss_add_ssreq(p, ss_SetLightLevel);
	ss_add_1(p, ll);
	ss_command(p, DF_TMO);
	ss_sub_ssans(p, ss_ErrorAnswer);
	ss_incorp_scanerr(p, ss_sub_1(p));
	chended(p);
	return ss_inst_err(p);
}

/* Set tranmission standby position. */
/* (Only valid on SpectroScanT in transmission mode) */
inst_code ss_do_SetTransmStandbyPos(
ss *p,
ss_rt  r,	/* Reference (Sensor/Sight) */
double x,	/* X coord in mm, 0-310.0, accurate to 0.1mm */
double y	/* Y coord in mm, 0-230.0, accurate to 0.1mm */
) {
#ifdef EMSST
	if (p->tmode != 0) {
		p->sbr = r;
		p->sbx = x;
		p->sby = y;
		return inst_ok;
	} else {
		*((char *)0) = 55;
	}
#endif
	ss_add_ssreq(p, ss_SetTransmStandbyPos);
	ss_add_1(p, r);
	ss_add_2(p, (int)(x * 10 + 0.5));
	ss_add_2(p, (int)(y * 10 + 0.5));
	ss_command(p, DF_TMO);
	ss_sub_ssans(p, ss_ErrorAnswer);
	ss_incorp_scanerr(p, ss_sub_1(p));
	chended(p);
	return ss_inst_err(p);
}

/* Set digitizing mode. Clears digitizing buffer, */
/* and puts the device offline. The user can move */
/* and enter positions. */
inst_code ss_do_SetDigitizingMode(ss *p) {
	ss_add_ssreq(p, ss_SetDigitizingMode);
	ss_command(p, DF_TMO);
	ss_sub_ssans(p, ss_ErrorAnswer);
	ss_incorp_scanerr(p, ss_sub_1(p));
	chended(p);
	return ss_inst_err(p);
}

/* Get last digitized position from memory. */
inst_code ss_do_OutputDigitizingValues(
ss *p,
ss_rt  r,	/* Reference (Sensor/Sight) */
ss_rt  *rr,	/* Return reference (Sensor/Sight) */
int    *nrp,/* Return the number of remaining positions in memory. */
double *x,	/* Return the X coord in mm, 0-310.0, accurate to 0.1mm */
double *y,	/* Return the Y coord in mm, 0-230.0, accurate to 0.1mm */
ss_zkt *zk	/* Return the Z coordinate (Up/Down) */
) {
	ss_add_ssreq(p, ss_OutputDigitizingValues);
	ss_add_1(p, r);
	ss_command(p, DF_TMO);
	ss_sub_ssans(p, ss_PositionAnswer);
	*rr = ss_sub_1(p);
	*nrp = ss_sub_2(p);		/* Should be unsigned ?? */
	*x = ss_sub_2(p)/10.0;
	*y = ss_sub_2(p)/10.0;
	*zk = ss_sub_1(p);
	chended(p);
	return ss_inst_err(p);
}

/* Turn on key aknowledge mode. Causes a KeyAnswer message */
/* to be generated whenever a key is pressed. */
/* (KetAnswer isn't well supported here ?) */
inst_code ss_do_SetKeyAcknowlge(ss *p) {
	ss_add_ssreq(p, ss_SetKeyAcknowlge);
	ss_command(p, DF_TMO);
	ss_sub_ssans(p, ss_ErrorAnswer);
	ss_incorp_scanerr(p, ss_sub_1(p));
	chended(p);
	return ss_inst_err(p);
}

/* Turn off key aknowledge mode. */
inst_code ss_do_ResetKeyAcknowlge(ss *p) {
	ss_add_ssreq(p, ss_ResetKeyAcknowlge);
	ss_command(p, DF_TMO);
	ss_sub_ssans(p, ss_ErrorAnswer);
	ss_incorp_scanerr(p, ss_sub_1(p));
	chended(p);
	return ss_inst_err(p);
}

/* Query the keys that are currently pressed */
inst_code ss_do_OutputActualKey(
ss *p,
ss_sks *sk,	/* Return Scan Key Set (Key bitmask) */
ss_ptt *pt	/* Return press time (Short/Long) */
) {
	ss_add_ssreq(p, ss_OutputActualKey);
	ss_command(p, DF_TMO);
	ss_sub_ssans(p, ss_KeyAnswer);
	*sk = ss_sub_1(p);
	*pt = ss_sub_1(p);
	chended(p);
	return ss_inst_err(p);
}

/* Query the keys that were last pressed */
inst_code ss_do_OutputLastKey(
ss *p,
ss_sks *sk,	/* Return Scan Key bitmask (Keys) */
ss_ptt *pt	/* Return press time (Short/Long) */
) {
	ss_add_ssreq(p, ss_OutputLastKey);
	ss_command(p, DF_TMO);
	ss_sub_ssans(p, ss_KeyAnswer);
	*sk = ss_sub_1(p);
	*pt = ss_sub_1(p);
	chended(p);
	return ss_inst_err(p);
}

/* Query the status register */
inst_code ss_do_OutputStatus(
ss *p,
ss_sts *st	/* Return status bitmask (Enter key/Online/Digitize/KeyAck/Paper) */
) {
	ss_add_ssreq(p, ss_OutputStatus);
	ss_command(p, DF_TMO);
	ss_sub_ssans(p, ss_StatusAnswer);
	*st = ss_sub_1(p);
	chended(p);
	return ss_inst_err(p);
}

/* Clear the status register */
inst_code ss_do_ClearStatus(
ss *p,
ss_sts st	/* Status to reset (Enter key/Online/Digitize/KeyAck/Paper) */
) {
	ss_add_ssreq(p, ss_ClearStatus);
	ss_add_1(p, st);
	ss_command(p, DF_TMO);
	ss_sub_ssans(p, ss_ErrorAnswer);
	ss_incorp_scanerr(p, ss_sub_1(p));
	chended(p);
	return ss_inst_err(p);
}

/* Set the special status register */
/* (Set to all 0 on reset) */
inst_code ss_do_SetSpecialStatus(
ss *p,
ss_sss sss	/* Status bits to set (HeadDwnOnMv/TableInTransMode/AllLightsOn) */
) {
	ss_add_ssreq(p, ss_SetSpecialStatus);
	ss_add_1(p, sss);
	ss_command(p, DF_TMO);
	ss_sub_ssans(p, ss_ErrorAnswer);
	ss_incorp_scanerr(p, ss_sub_1(p));
	chended(p);
	return ss_inst_err(p);
}

/* Clear the special status register */
inst_code ss_do_ClearSpecialStatus(
ss *p,
ss_sss sss	/* Status bits to clear (HeadDwnOnMv/TableInTransMode/AllLightsOn) */
) {
	ss_add_ssreq(p, ss_ClearSpecialStatus);
	ss_add_1(p, sss);
	ss_command(p, DF_TMO);
	ss_sub_ssans(p, ss_ErrorAnswer);
	ss_incorp_scanerr(p, ss_sub_1(p));
	chended(p);
	return ss_inst_err(p);
}

/* Query the special status register */
inst_code ss_do_OutputSpecialStatus(
ss *p,
ss_sss *sss	/* Return Special Status bits */
) {
	ss_add_ssreq(p, ss_OutputSpecialStatus);
	ss_command(p, DF_TMO);
	ss_sub_ssans(p, ss_StatusAnswer);
	*sss = ss_sub_1(p);
	chended(p);
	return ss_inst_err(p);
}


#define SS_IMP_H
#endif /* SS_IMP_H */


