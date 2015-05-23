
/*
 * Copyright 1996 - 2010 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

/*
 * Heapsort macro - sort smallest to largest.
 * Heapsort is guaranteed nlogn, doesn't need any
 * extra storage, but often isn't as fast as quicksort.
 */

/* Need to #define HEAP_COMPARE(A,B) so it returns true if A < B */
/* Note that A will be ARRAY[a], and B will be ARRAY[b] where a and b are indexes. */
/* TYPE should be the type of each entry of the ARRAY */
#define HEAPSORT(TYPE,ARRAY,NUMBER) \
		{	\
		TYPE *hs_ncb = ARRAY;	\
		int hs_l,hs_j,hs_ir,hs_i;	\
		TYPE hs_rra;	\
		\
		if (NUMBER >= 2)	\
			{	\
			hs_l = NUMBER >> 1;	\
			hs_ir = NUMBER-1;	\
			for (;;)	\
				{	\
				if (hs_l > 0)	\
					hs_rra = hs_ncb[--hs_l];	\
				else	\
					{	\
					hs_rra = hs_ncb[hs_ir];	\
					hs_ncb[hs_ir] = hs_ncb[0];	\
					if (--hs_ir == 0)	\
						{	\
						hs_ncb[0] = hs_rra;	\
						break;	\
						}	\
					}	\
				hs_i = hs_l;	\
				hs_j = hs_l+hs_l+1;	\
				while (hs_j <= hs_ir)	\
					{	\
					if (hs_j < hs_ir && HEAP_COMPARE(hs_ncb[hs_j],hs_ncb[hs_j+1]))	\
						hs_j++;	\
					if (HEAP_COMPARE(hs_rra,hs_ncb[hs_j]))	\
						{	\
						hs_ncb[hs_i] = hs_ncb[hs_j];	\
						hs_i = hs_j;	\
						hs_j = hs_j+hs_j+1;	\
						}	\
					else	\
						hs_j = hs_ir + 1;	\
					}	\
				hs_ncb[hs_i] = hs_rra;	\
				}	\
			}	\
		}

