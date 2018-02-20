/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2007 Association Homecinema Francophone.  All rights reserved.
/////////////////////////////////////////////////////////////////////////////
//
//  This file is subject to the terms of the GNU General Public License as
//  published by the Free Software Foundation.  A copy of this license is
//  included with this software distribution in the file COPYING.htm. If you
//  do not have a copy, you may obtain a copy by writing to the Free
//  Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
//  This software is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details
/////////////////////////////////////////////////////////////////////////////
//  Author(s):
//	FranÁois-Xavier CHABOUD
//  Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////
//  Modifications:
//  JÈrÙme Duquennoy -> made OS agnostic
//    ASSERT -> assert
//    CRITICAL_SECTION -> pthreads mutex
//    CString -> std::string
//      CString.LoadString -> string passÈe en argument
//    BOOL, FALSE, TRUE -> bool, false, true
//    const added to the param of the copy constructor
//    GetColorReference linked to global app var -> replaced by arguments in impacted functions

#include "Color.h"
#include "Endianness.h"
#include "CriticalSection.h"
#include "LockWhileInScope.h"
#include <math.h>
#include <assert.h>
#include <stdexcept>
#include <sstream>

// critical section to be used in this file to
// ensure config matrices don't get changed mid-calculation
static CriticalSection m_matrixSection;

/*
 *      Name:   XYZtoCorColorTemp.c
 *
 *      Author: Bruce Justin Lindbloom
 *
 *      Copyright (c) 2003 Bruce Justin Lindbloom. All rights reserved.
 *
 *      Input:  xyz = pointer to the input array of X, Y and Z color components (in that order).
 *              temp = pointer to where the computed correlated color temperature should be placed.
 *
 *      Output: *temp = correlated color temperature, if successful.
 *                    = unchanged if unsuccessful.
 *
 *      Return: 0 if successful, else -1.
 *
 *      Description:
 *              This is an implementation of Robertson's method of computing the correlated color
 *              temperature of an XYZ color. It can compute correlated color temperatures in the
 *              range [1666.7K, infinity].
 *
 *      Reference:
 *              "Color Science: Concepts and Methods, Quantitative Data and Formulae", Second Edition,
 *              Gunter Wyszecki and W. S. Stiles, John Wiley & Sons, 1982, pp. 227, 228.
 */

#include <float.h>
#include <math.h>
                        
/* LERP(a,b,c) = linear interpolation macro, is 'a' when c == 0.0 and 'b' when c == 1.0 */
#define LERP(a,b,c)     (((b) - (a)) * (c) + (a))

typedef struct UVT {
        double  u;
        double  v;
        double  t;
} UVT;

typedef struct EOTF {
        double  x;
        double  y;
} EOTF;

double rt[31] = {       /* reciprocal temperature (K) */
         DBL_MIN,  10.0e-6,  20.0e-6,  30.0e-6,  40.0e-6,  50.0e-6,
         60.0e-6,  70.0e-6,  80.0e-6,  90.0e-6, 100.0e-6, 125.0e-6,
        150.0e-6, 175.0e-6, 200.0e-6, 225.0e-6, 250.0e-6, 275.0e-6,
        300.0e-6, 325.0e-6, 350.0e-6, 375.0e-6, 400.0e-6, 425.0e-6,
        450.0e-6, 475.0e-6, 500.0e-6, 525.0e-6, 550.0e-6, 575.0e-6,
        600.0e-6
};
UVT uvt[31] = {
        {0.18006, 0.26352, -0.24341},
        {0.18066, 0.26589, -0.25479},
        {0.18133, 0.26846, -0.26876},
        {0.18208, 0.27119, -0.28539},
        {0.18293, 0.27407, -0.30470},
        {0.18388, 0.27709, -0.32675},
        {0.18494, 0.28021, -0.35156},
        {0.18611, 0.28342, -0.37915},
        {0.18740, 0.28668, -0.40955},
        {0.18880, 0.28997, -0.44278},
        {0.19032, 0.29326, -0.47888},
        {0.19462, 0.30141, -0.58204},
        {0.19962, 0.30921, -0.70471},
        {0.20525, 0.31647, -0.84901},
        {0.21142, 0.32312, -1.0182},
        {0.21807, 0.32909, -1.2168},
        {0.22511, 0.33439, -1.4512},
        {0.23247, 0.33904, -1.7298},
        {0.24010, 0.34308, -2.0637},
        {0.24702, 0.34655, -2.4681},
        {0.25591, 0.34951, -2.9641},
        {0.26400, 0.35200, -3.5814},
        {0.27218, 0.35407, -4.3633},
        {0.28039, 0.35577, -5.3762},
        {0.28863, 0.35714, -6.7262},
        {0.29685, 0.35823, -8.5955},
        {0.30505, 0.35907, -11.324},
        {0.31320, 0.35968, -15.628},
        {0.32129, 0.36011, -23.325},
        {0.32931, 0.36038, -40.770},
        {0.33724, 0.36051, -116.45}
};

EOTF DV_500_EOTF[220] = {
{	0	,	0.005	},
{	0.00456621	,	0.005	},
{	0.00913242	,	0.005	},
{	0.01369863	,	0.005	},
{	0.01826484	,	0.006	},
{	0.02283105	,	0.008	},
{	0.02739726	,	0.011	},
{	0.03196347	,	0.014	},
{	0.03652968	,	0.018	},
{	0.04109589	,	0.023	},
{	0.0456621	,	0.029	},
{	0.050228311	,	0.036	},
{	0.054794521	,	0.043	},
{	0.059360731	,	0.052	},
{	0.063926941	,	0.061	},
{	0.068493151	,	0.072	},
{	0.073059361	,	0.084	},
{	0.077625571	,	0.099	},
{	0.082191781	,	0.115	},
{	0.086757991	,	0.133	},
{	0.091324201	,	0.152	},
{	0.095890411	,	0.172	},
{	0.100456621	,	0.194	},
{	0.105022831	,	0.218	},
{	0.109589041	,	0.243	},
{	0.114155251	,	0.27	},
{	0.118721461	,	0.299	},
{	0.123287671	,	0.334	},
{	0.127853881	,	0.373	},
{	0.132420091	,	0.414	},
{	0.136986301	,	0.458	},
{	0.141552511	,	0.504	},
{	0.146118721	,	0.553	},
{	0.150684932	,	0.604	},
{	0.155251142	,	0.658	},
{	0.159817352	,	0.715	},
{	0.164383562	,	0.774	},
{	0.168949772	,	0.842	},
{	0.173515982	,	0.92	},
{	0.178082192	,	1.001	},
{	0.182648402	,	1.087	},
{	0.187214612	,	1.176	},
{	0.191780822	,	1.27	},
{	0.196347032	,	1.368	},
{	0.200913242	,	1.469	},
{	0.205479452	,	1.575	},
{	0.210045662	,	1.685	},
{	0.214611872	,	1.808	},
{	0.219178082	,	1.949	},
{	0.223744292	,	2.096	},
{	0.228310502	,	2.25	},
{	0.232876712	,	2.409	},
{	0.237442922	,	2.574	},
{	0.242009132	,	2.745	},
{	0.246575342	,	2.923	},
{	0.251141553	,	3.106	},
{	0.255707763	,	3.295	},
{	0.260273973	,	3.505	},
{	0.264840183	,	3.745	},
{	0.269406393	,	3.994	},
{	0.273972603	,	4.251	},
{	0.278538813	,	4.518	},
{	0.283105023	,	4.793	},
{	0.287671233	,	5.077	},
{	0.292237443	,	5.369	},
{	0.296803653	,	5.67	},
{	0.301369863	,	5.98	},
{	0.305936073	,	6.319	},
{	0.310502283	,	6.707	},
{	0.315068493	,	7.107	},
{	0.319634703	,	7.52	},
{	0.324200913	,	7.944	},
{	0.328767123	,	8.38	},
{	0.333333333	,	8.831	},
{	0.337899543	,	9.291	},
{	0.342465753	,	9.764	},
{	0.347031963	,	10.249	},
{	0.351598174	,	10.77	},
{	0.356164384	,	11.37	},
{	0.360730594	,	11.99	},
{	0.365296804	,	12.625	},
{	0.369863014	,	13.275	},
{	0.374429224	,	13.943	},
{	0.378995434	,	14.629	},
{	0.383561644	,	15.331	},
{	0.388127854	,	16.048	},
{	0.392694064	,	16.782	},
{	0.397260274	,	17.533	},
{	0.401826484	,	18.43	},
{	0.406392694	,	19.352	},
{	0.410958904	,	20.297	},
{	0.415525114	,	21.268	},
{	0.420091324	,	22.256	},
{	0.424657534	,	23.268	},
{	0.429223744	,	24.298	},
{	0.433789954	,	25.354	},
{	0.438356164	,	26.428	},
{	0.442922374	,	27.524	},
{	0.447488584	,	28.723	},
{	0.452054795	,	30.062	},
{	0.456621005	,	31.426	},
{	0.461187215	,	32.82	},
{	0.465753425	,	34.242	},
{	0.470319635	,	35.693	},
{	0.474885845	,	37.17	},
{	0.479452055	,	38.674	},
{	0.484018265	,	40.203	},
{	0.488584475	,	41.759	},
{	0.493150685	,	43.336	},
{	0.497716895	,	45.077	},
{	0.502283105	,	46.997	},
{	0.506849315	,	48.949	},
{	0.511415525	,	50.934	},
{	0.515981735	,	52.953	},
{	0.520547945	,	55.011	},
{	0.525114155	,	57.093	},
{	0.529680365	,	59.215	},
{	0.534246575	,	61.361	},
{	0.538812785	,	63.538	},
{	0.543378995	,	65.744	},
{	0.547945205	,	67.989	},
{	0.552511416	,	70.652	},
{	0.557077626	,	73.353	},
{	0.561643836	,	76.094	},
{	0.566210046	,	78.87	},
{	0.570776256	,	81.689	},
{	0.575342466	,	84.548	},
{	0.579908676	,	87.437	},
{	0.584474886	,	90.369	},
{	0.589041096	,	93.336	},
{	0.593607306	,	96.326	},
{	0.598173516	,	99.352	},
{	0.602739726	,	102.406	},
{	0.607305936	,	105.905	},
{	0.611872146	,	109.579	},
{	0.616438356	,	113.285	},
{	0.621004566	,	117.056	},
{	0.625570776	,	120.841	},
{	0.630136986	,	124.685	},
{	0.634703196	,	128.565	},
{	0.639269406	,	132.463	},
{	0.643835616	,	136.416	},
{	0.648401826	,	140.395	},
{	0.652968037	,	144.418	},
{	0.657534247	,	148.462	},
{	0.662100457	,	152.54	},
{	0.666666667	,	156.965	},
{	0.671232877	,	161.834	},
{	0.675799087	,	166.738	},
{	0.680365297	,	171.674	},
{	0.684931507	,	176.667	},
{	0.689497717	,	181.693	},
{	0.694063927	,	186.77	},
{	0.698630137	,	191.848	},
{	0.703196347	,	196.995	},
{	0.707762557	,	202.147	},
{	0.712328767	,	207.324	},
{	0.716894977	,	212.565	},
{	0.721461187	,	217.806	},
{	0.726027397	,	223.073	},
{	0.730593607	,	228.363	},
{	0.735159817	,	234.261	},
{	0.739726027	,	240.559	},
{	0.744292237	,	246.876	},
{	0.748858447	,	253.219	},
{	0.753424658	,	259.602	},
{	0.757990868	,	266.024	},
{	0.762557078	,	272.451	},
{	0.767123288	,	278.925	},
{	0.771689498	,	285.393	},
{	0.776255708	,	291.93	},
{	0.780821918	,	298.45	},
{	0.785388128	,	304.974	},
{	0.789954338	,	311.546	},
{	0.794520548	,	318.117	},
{	0.799086758	,	324.719	},
{	0.803652968	,	331.285	},
{	0.808219178	,	337.921	},
{	0.812785388	,	345.288	},
{	0.817351598	,	353.083	},
{	0.821917808	,	360.911	},
{	0.826484018	,	368.748	},
{	0.831050228	,	376.64	},
{	0.835616438	,	384.483	},
{	0.840182648	,	392.37	},
{	0.844748858	,	400.24	},
{	0.849315068	,	408.153	},
{	0.853881279	,	416.038	},
{	0.858447489	,	423.924	},
{	0.863013699	,	431.816	},
{	0.867579909	,	439.725	},
{	0.872146119	,	447.585	},
{	0.876712329	,	455.515	},
{	0.881278539	,	463.335	},
{	0.885844749	,	471.225	},
{	0.890410959	,	479.11	},
{	0.894977169	,	486.918	},
{	0.899543379	,	494.779	},
{	0.904109589	,	500.072	},
{	0.908675799	,	500.072	},
{	0.913242009	,	500.072	},
{	0.917808219	,	500.072	},
{	0.922374429	,	500.072	},
{	0.926940639	,	500.072	},
{	0.931506849	,	500.072	},
{	0.936073059	,	500.072	},
{	0.940639269	,	500.072	},
{	0.945205479	,	500.072	},
{	0.949771689	,	500.072	},
{	0.9543379	,	500.072	},
{	0.95890411	,	500.072	},
{	0.96347032	,	500.072	},
{	0.96803653	,	500.072	},
{	0.97260274	,	500.072	},
{	0.97716895	,	500.072	},
{	0.98173516	,	500.072	},
{	0.98630137	,	500.072	},
{	0.99086758	,	500.072	},
{	0.99543379	,	500.072	},
{	1.0	,	500.072	}
};

EOTF DV_400_EOTF[220] = {
{	0	,	0.005	},
{	0.00456621	,	0.005	},
{	0.00913242	,	0.005	},
{	0.01369863	,	0.005	},
{	0.01826484	,	0.006	},
{	0.02283105	,	0.008	},
{	0.02739726	,	0.01	},
{	0.03196347	,	0.013	},
{	0.03652968	,	0.017	},
{	0.04109589	,	0.022	},
{	0.0456621	,	0.027	},
{	0.050228311	,	0.033	},
{	0.054794521	,	0.041	},
{	0.059360731	,	0.049	},
{	0.063926941	,	0.057	},
{	0.068493151	,	0.067	},
{	0.073059361	,	0.078	},
{	0.077625571	,	0.092	},
{	0.082191781	,	0.107	},
{	0.086757991	,	0.124	},
{	0.091324201	,	0.142	},
{	0.095890411	,	0.161	},
{	0.100456621	,	0.182	},
{	0.105022831	,	0.204	},
{	0.109589041	,	0.228	},
{	0.114155251	,	0.253	},
{	0.118721461	,	0.28	},
{	0.123287671	,	0.313	},
{	0.127853881	,	0.35	},
{	0.132420091	,	0.388	},
{	0.136986301	,	0.429	},
{	0.141552511	,	0.472	},
{	0.146118721	,	0.518	},
{	0.150684932	,	0.566	},
{	0.155251142	,	0.617	},
{	0.159817352	,	0.67	},
{	0.164383562	,	0.725	},
{	0.168949772	,	0.793	},
{	0.173515982	,	0.865	},
{	0.178082192	,	0.942	},
{	0.182648402	,	1.022	},
{	0.187214612	,	1.105	},
{	0.191780822	,	1.193	},
{	0.196347032	,	1.284	},
{	0.200913242	,	1.379	},
{	0.205479452	,	1.477	},
{	0.210045662	,	1.58	},
{	0.214611872	,	1.706	},
{	0.219178082	,	1.838	},
{	0.223744292	,	1.975	},
{	0.228310502	,	2.117	},
{	0.232876712	,	2.265	},
{	0.237442922	,	2.418	},
{	0.242009132	,	2.577	},
{	0.246575342	,	2.742	},
{	0.251141553	,	2.912	},
{	0.255707763	,	3.095	},
{	0.260273973	,	3.31	},
{	0.264840183	,	3.532	},
{	0.269406393	,	3.762	},
{	0.273972603	,	4	},
{	0.278538813	,	4.246	},
{	0.283105023	,	4.499	},
{	0.287671233	,	4.761	},
{	0.292237443	,	5.031	},
{	0.296803653	,	5.308	},
{	0.301369863	,	5.617	},
{	0.305936073	,	5.962	},
{	0.310502283	,	6.319	},
{	0.315068493	,	6.686	},
{	0.319634703	,	7.064	},
{	0.324200913	,	7.453	},
{	0.328767123	,	7.853	},
{	0.333333333	,	8.264	},
{	0.337899543	,	8.686	},
{	0.342465753	,	9.119	},
{	0.347031963	,	9.606	},
{	0.351598174	,	10.14	},
{	0.356164384	,	10.688	},
{	0.360730594	,	11.251	},
{	0.365296804	,	11.827	},
{	0.369863014	,	12.421	},
{	0.374429224	,	13.026	},
{	0.378995434	,	13.646	},
{	0.383561644	,	14.281	},
{	0.388127854	,	14.931	},
{	0.392694064	,	15.654	},
{	0.397260274	,	16.448	},
{	0.401826484	,	17.262	},
{	0.406392694	,	18.092	},
{	0.410958904	,	18.943	},
{	0.415525114	,	19.811	},
{	0.420091324	,	20.698	},
{	0.424657534	,	21.604	},
{	0.429223744	,	22.525	},
{	0.433789954	,	23.466	},
{	0.438356164	,	24.459	},
{	0.442922374	,	25.599	},
{	0.447488584	,	26.763	},
{	0.452054795	,	27.95	},
{	0.456621005	,	29.161	},
{	0.461187215	,	30.398	},
{	0.465753425	,	31.654	},
{	0.470319635	,	32.932	},
{	0.474885845	,	34.231	},
{	0.479452055	,	35.55	},
{	0.484018265	,	36.895	},
{	0.488584475	,	38.366	},
{	0.493150685	,	39.984	},
{	0.497716895	,	41.63	},
{	0.502283105	,	43.305	},
{	0.506849315	,	44.999	},
{	0.511415525	,	46.725	},
{	0.515981735	,	48.477	},
{	0.520547945	,	50.256	},
{	0.525114155	,	52.06	},
{	0.529680365	,	53.884	},
{	0.534246575	,	55.731	},
{	0.538812785	,	57.653	},
{	0.543378995	,	59.866	},
{	0.547945205	,	62.111	},
{	0.552511416	,	64.384	},
{	0.557077626	,	66.691	},
{	0.561643836	,	69.02	},
{	0.566210046	,	71.382	},
{	0.570776256	,	73.773	},
{	0.575342466	,	76.19	},
{	0.579908676	,	78.631	},
{	0.584474886	,	81.095	},
{	0.589041096	,	83.59	},
{	0.593607306	,	86.093	},
{	0.598173516	,	89.051	},
{	0.602739726	,	92.035	},
{	0.607305936	,	95.057	},
{	0.611872146	,	98.114	},
{	0.616438356	,	101.201	},
{	0.621004566	,	104.315	},
{	0.625570776	,	107.454	},
{	0.630136986	,	110.605	},
{	0.634703196	,	113.804	},
{	0.639269406	,	117.004	},
{	0.643835616	,	120.248	},
{	0.648401826	,	123.501	},
{	0.652968037	,	126.768	},
{	0.657534247	,	130.414	},
{	0.662100457	,	134.281	},
{	0.666666667	,	138.175	},
{	0.671232877	,	142.117	},
{	0.675799087	,	146.056	},
{	0.680365297	,	150.031	},
{	0.684931507	,	154.044	},
{	0.689497717	,	158.047	},
{	0.694063927	,	162.098	},
{	0.698630137	,	166.149	},
{	0.703196347	,	170.221	},
{	0.707762557	,	174.309	},
{	0.712328767	,	178.414	},
{	0.716894977	,	182.537	},
{	0.721461187	,	186.668	},
{	0.726027397	,	191.226	},
{	0.730593607	,	196.079	},
{	0.735159817	,	200.932	},
{	0.739726027	,	205.839	},
{	0.744292237	,	210.74	},
{	0.748858447	,	215.666	},
{	0.753424658	,	220.604	},
{	0.757990868	,	225.551	},
{	0.762557078	,	230.505	},
{	0.767123288	,	235.459	},
{	0.771689498	,	240.411	},
{	0.776255708	,	245.398	},
{	0.780821918	,	250.382	},
{	0.785388128	,	255.349	},
{	0.789954338	,	260.337	},
{	0.794520548	,	265.302	},
{	0.799086758	,	270.281	},
{	0.803652968	,	275.269	},
{	0.808219178	,	281.071	},
{	0.812785388	,	286.918	},
{	0.817351598	,	292.744	},
{	0.821917808	,	298.56	},
{	0.826484018	,	304.374	},
{	0.831050228	,	310.215	},
{	0.835616438	,	316.002	},
{	0.840182648	,	321.82	},
{	0.844748858	,	327.608	},
{	0.849315068	,	333.422	},
{	0.853881279	,	339.204	},
{	0.858447489	,	344.986	},
{	0.863013699	,	350.761	},
{	0.867579909	,	356.477	},
{	0.872146119	,	362.232	},
{	0.876712329	,	367.968	},
{	0.881278539	,	373.642	},
{	0.885844749	,	379.336	},
{	0.890410959	,	385.006	},
{	0.894977169	,	390.656	},
{	0.899543379	,	396.284	},
{	0.904109589	,	400.015	},
{	0.908675799	,	400.015	},
{	0.913242009	,	400.015	},
{	0.917808219	,	400.015	},
{	0.922374429	,	400.015	},
{	0.926940639	,	400.015	},
{	0.931506849	,	400.015	},
{	0.936073059	,	400.015	},
{	0.940639269	,	400.015	},
{	0.945205479	,	400.015	},
{	0.949771689	,	400.015	},
{	0.9543379	,	400.015	},
{	0.95890411	,	400.015	},
{	0.96347032	,	400.015	},
{	0.96803653	,	400.015	},
{	0.97260274	,	400.015	},
{	0.97716895	,	400.015	},
{	0.98173516	,	400.015	},
{	0.98630137	,	400.015	},
{	0.99086758	,	400.015	},
{	0.99543379	,	400.015	},
{	1	,	400.015	}
};

std::vector<double> BT2390x,BT2390y;

double m_HDRRefLevel = 1.0 / 94.37844 * 10000.; // 50.23% 8-bit level 126 (504) input luminance
double Scale = 10000.;

int XYZtoCorColorTemp(double *xyz, double *temp)
{
        double us, vs, p, di, dm;
        int i;


        if ((xyz[0] < 1.0e-20) && (xyz[1] < 1.0e-20) && (xyz[2] < 1.0e-20))
                return(-1);     /* protect against possible divide-by-zero failure */
        us = (4.0 * xyz[0]) / (xyz[0] + 15.0 * xyz[1] + 3.0 * xyz[2]);
        vs = (6.0 * xyz[1]) / (xyz[0] + 15.0 * xyz[1] + 3.0 * xyz[2]);
        dm = 0.0;
        for (i = 0; i < 31; i++) {
                di = (vs - uvt[i].v) - uvt[i].t * (us - uvt[i].u);
                if ((i > 0) && (((di < 0.0) && (dm >= 0.0)) || ((di >= 0.0) && (dm < 0.0))))
                        break;  /* found lines bounding (us, vs) : i-1 and i */
                dm = di;
        }
        if (i == 31)
                return(-1);     /* bad XYZ input, color temp would be less than minimum of 1666.7 degrees, or too far towards blue */
        di = di / sqrt(1.0 + uvt[i    ].t * uvt[i    ].t);
        dm = dm / sqrt(1.0 + uvt[i - 1].t * uvt[i - 1].t);
        p = dm / (dm - di);     /* p = interpolation parameter, 0.0 : i-1, 1.0 : i */
        p = 1.0 / (LERP(rt[i - 1], rt[i], p));
        *temp = p;
        return(0);      /* success */
}

///////////////////////////////////////////////////////////////////
// Color references
///////////////////////////////////////////////////////////////////

ColorXYZ illuminantA(ColorxyY(0.447593,0.407539));
ColorXYZ illuminantB(ColorxyY(0.348483,0.351747));
ColorXYZ illuminantC(ColorxyY(0.310115,0.316311));
ColorXYZ illuminantE(ColorxyY(0.333334,0.333333));
ColorXYZ illuminantD50(ColorxyY(0.345669,0.358496));
ColorXYZ illuminantD55(ColorxyY(0.332406,0.347551));
ColorXYZ illuminantD65(ColorxyY(0.312712,0.329008));
ColorXYZ illuminantD75(ColorxyY(0.299023,0.314963));
ColorXYZ illuminantD93(ColorxyY(0.284863,0.293221));
//ColorXYZ illuminantDCI(ColorxyY(0.333,0.334));//check this
ColorXYZ illuminantDCI(ColorxyY(0.314,0.351));

//initialize manual colors
ColorxyY primariesPAL[3] =	{	ColorxyY(0.6400, 0.3300),
								ColorxyY(0.2900, 0.6000),
								ColorxyY(0.1500, 0.0600) };

ColorxyY primariesRec601[3] ={	ColorxyY(0.6300, 0.3400),
								ColorxyY(0.3100, 0.5950),
								ColorxyY(0.1550, 0.0700) };

ColorxyY primariesRec709[3] ={	ColorxyY(0.6400, 0.3300),
								ColorxyY(0.3000, 0.6000),
								ColorxyY(0.1500, 0.0600) };

ColorxyY primariesP3[3] = {		ColorxyY(0.6800, 0.3200),
								ColorxyY(0.265, 0.6900),
								ColorxyY(0.1500, 0.0600) };

ColorxyY primaries2020[3] ={	ColorxyY(0.708, 0.292),
								ColorxyY(0.170, 0.797),
								ColorxyY(0.131, 0.046) };

ColorxyY primariesRec709a[3] ={	ColorxyY(0.5575, 0.3298), //75% sat/lum Rec709 w/2.2 gamma
								ColorxyY(0.3032, 0.5313),
								ColorxyY(0.1911, 0.1279) };

//optimized for plasma
ColorxyY primariesCC6[3] ={	ColorxyY(0.625, 0.330), 
								ColorxyY(0.303, 0.533),
								ColorxyY(0.245, 0.217)};

/* The 75% saturation 75% amplitude and color checker xy locations are calculated 
assuming gamma=2.2, starting with the follow triplets from the GCD disk, and then used as pseudo-primaries/secondaries

75% (16-235)
	R	G	B	Y	C	M
R'	165	77	58	180	95	156
G'	60	176	58	180	176	80
B'	60	77	126	88	176	156

CC6 (16-235)
	R	G	B	Y	C	M
R'	182	97	93	80	152	213
G'	145	121	108	95	176	154
B'	128	150	73	156	71	55

*/


Matrix ComputeRGBtoXYZMatrix(Matrix primariesChromacities,Matrix whiteChromacity)
{
	// Compute RGB to XYZ matrix
	Matrix coefJ(0.0,3,1);
	coefJ=primariesChromacities.GetInverse();

	coefJ=coefJ*whiteChromacity;

	Matrix scaling(0.0,3,3);
	scaling(0,0)=coefJ(0,0)/whiteChromacity(1,0);
	scaling(1,1)=coefJ(1,0)/whiteChromacity(1,0);
	scaling(2,2)=coefJ(2,0)/whiteChromacity(1,0);

	Matrix m=primariesChromacities*scaling;
	return m;
}

CColorReference::CColorReference(ColorStandard aColorStandard, WhiteTarget aWhiteTarget, double aGamma, string	strModified, ColorXYZ c_whiteColor, ColorxyY c_redColor, ColorxyY c_greenColor, ColorxyY c_blueColor)
{
	m_standard = aColorStandard;
    ColorxyY* primaries = primariesRec601;

	switch(aColorStandard)
	{
		case CUSTOM:
		{
			standardName="CUSTOM";
			whiteColor=illuminantD65;
			whiteName="D65";
			m_white=D65;
			primaries[0] = c_redColor.isValid()?c_redColor:primariesRec709[0];
			primaries[1] = c_greenColor.isValid()?c_greenColor:primariesRec709[1];
			primaries[2] = c_blueColor.isValid()?c_blueColor:primariesRec709[2];
			break;
		}
		case PALSECAM:
		{
			standardName="PAL/SECAM";
			whiteColor=illuminantD65;
			whiteName="D65";
			m_white=D65;
            primaries = primariesPAL;
			break;
		}
		case SDTV:
		{
			standardName="SDTV Rec601";
			whiteColor=illuminantD65;
			m_white=D65;
			whiteName="D65";
            primaries = primariesRec601;
			break;
		}
		case UHDTV:
		{
			standardName="UHDTV DCI-P3/D65";
			whiteColor=illuminantD65;
			whiteName="D65";
			m_white=D65;
            primaries = primariesP3;
			break;
		}
		case UHDTV2:
		{
			standardName="UHDTV Rec2020";
			whiteColor=illuminantD65;
			whiteName="D65";
			m_white=D65;
            primaries = primaries2020;
			break;
		}
		case UHDTV3:
		{
			standardName="UHDTV P3 in Rec2020";
			whiteColor=illuminantD65;
			whiteName="D65";
			m_white=D65;
            primaries = primariesP3;
			break;
		}
		case UHDTV4:
		{
			standardName="UHDTV Rec709 in Rec2020";
			whiteColor=illuminantD65;
			whiteName="D65";
			m_white=D65;
            primaries = primariesRec709;
			break;
		}
		case HDTV:
		{
			standardName="HDTV Rec709";
			whiteColor=illuminantD65;
			whiteName="D65";
			m_white=D65;
            primaries = primariesRec709;
			break;
		}
		case HDTVa:
		{
			standardName="75% HDTV Rec709";
			whiteColor=illuminantD65;
			whiteName="D65";
			m_white=D65;
            primaries = primariesRec709a;
			break;
		}
		case HDTVb:
		{
			standardName="Color Checker HDTV OPT-Plasma";
			whiteColor=illuminantD65;
			whiteName="D65";
			m_white=D65;
            primaries = primariesCC6;
			break;
		}
		case sRGB:
		{
			standardName="sRGB";
			whiteColor=illuminantD65;
			whiteName="D65";
			m_white=D65;
            primaries = primariesRec709;
			break;
		}
		default:
		{
            standardName="Unknown";
			whiteColor=illuminantD65;
			whiteName="D65";
			m_white=D65;
		}
	}

    redPrimary = ColorXYZ(primaries[0]);
    greenPrimary = ColorXYZ(primaries[1]);
    bluePrimary = ColorXYZ(primaries[2]);

	if(aWhiteTarget != Default)
		m_white = aWhiteTarget;

	switch(aWhiteTarget)
	{
		case D65:
			standardName+=strModified;
			whiteColor=illuminantD65;
			whiteName="D65";
			break;
		case DCUST:
			standardName+=strModified;
			whiteColor=c_whiteColor;
			whiteName="CUSTOM";
			break;
		case D55:
			standardName+=strModified;
			whiteColor=illuminantD55;
			whiteName="D55";
			break;
		case D50:
			standardName+=strModified;
			whiteColor=illuminantD50;
			whiteName="D50";
			break;
		case D75:
			standardName+=strModified;
			whiteColor=illuminantD75;
			whiteName="D75";
			break;
		case A:
			standardName+=strModified;
			whiteColor=illuminantA;
			whiteName="A";
			break;
		case B:
			standardName+=strModified;
			whiteColor=illuminantB;
			whiteName="B";
			break;
		case C:
			standardName+=strModified;
			whiteColor=illuminantC;
			whiteName="C";
			break;
		case E:
			if(aColorStandard == SDTV)
				break;
			standardName+=strModified;
			whiteColor=illuminantE;
			whiteName="E";
			break;
		case D93:
			standardName+=strModified;
			whiteColor=illuminantD93;
			whiteName="D93";
			break;
		case DCI:
			standardName+=strModified;
			whiteColor=illuminantDCI;
			whiteName="DCI";
			break;
		case Default:
		default:
			break;
	}

    // Compute transformation matrices
	Matrix primariesMatrix(redPrimary);
	primariesMatrix.CMAC(greenPrimary);
	primariesMatrix.CMAC(bluePrimary);	
	
	RGBtoXYZMatrix=ComputeRGBtoXYZMatrix(primariesMatrix, whiteColor);

	XYZtoRGBMatrix=RGBtoXYZMatrix.GetInverse();

	// Adjust reference primary colors Y values relatively to white Y
	ColorxyY aColor(redPrimary);
	aColor[2] = GetRedReferenceLuma (false);
	redPrimary = ColorXYZ(aColor);

	aColor = ColorxyY(greenPrimary);
	aColor[2] = GetGreenReferenceLuma (false);
    greenPrimary = ColorXYZ(aColor);

	aColor = ColorxyY(bluePrimary);
	aColor[2] = GetBlueReferenceLuma (false);
	bluePrimary = ColorXYZ(aColor);

    UpdateSecondary ( yellowSecondary, redPrimary, greenPrimary, bluePrimary );
    UpdateSecondary ( cyanSecondary, greenPrimary, bluePrimary, redPrimary );
    UpdateSecondary ( magentaSecondary, bluePrimary, redPrimary, greenPrimary );
}

CColorReference::~CColorReference()
{
}

CColorReference GetStandardColorReference(ColorStandard aColorStandard)
{
	CColorReference aStandardRef(aColorStandard);
	return aStandardRef;
}

void CColorReference::UpdateSecondary ( ColorXYZ & secondary, const ColorXYZ& primary1, const ColorXYZ& primary2, const ColorXYZ& primaryOpposite )
{
	// Compute intersection between line (primary1-primary2) and line (primaryOpposite-white)
	ColorxyY	prim1(primary1);
	ColorxyY	prim2(primary2);
	ColorxyY	primOppo(primaryOpposite);
	ColorxyY	white(GetWhite());

	double x1 = prim1[0];
	double y1 = prim1[1];
	double x2 = white[0];
	double y2 = white[1];
	double dx1 = prim2[0] - x1;
	double dy1 = prim2[1] - y1;
	double dx2 = primOppo[0] - x2;
	double dy2 = primOppo[1] - y2;
	
	double k = ( ( ( x2 - x1 ) / dx1 ) + ( dx2 / ( dx1 * dy2 ) ) * ( y1 - y2 ) ) / ( 1.0 - ( ( dx2 * dy1 ) / ( dx1 * dy2 ) ) );

	ColorxyY aColor;
	if (CColorReference::m_standard != HDTVb )
	{
    	aColor = ColorxyY ( x1 + k * dx1, y1 + k * dy1, prim1[2] + prim2[2] );
	}
	else //optimized
	{
		if (x1 <= 0.630 && x1 >= 0.610) aColor = ColorxyY(0.418,0.502,0.5644);
		if (x1 <= 0.305 && x1 >= 0.301) aColor = ColorxyY(0.226,0.329,0.4798);
		if (x1 <= 0.247 && x1 >= 0.243) aColor = ColorxyY(0.321,0.157,0.1775);
	}
	secondary = ColorXYZ(aColor);
}

///////////////////////////////////////////////////////////////////
// Sensor references
///////////////////////////////////////////////////////////////////

double defaultSensorToXYZ[3][3] = {	{   7.79025E-05,  5.06389E-05,   6.02556E-05  }, 
									{   3.08665E-05,  0.000131285,   2.94813E-05  },
									{  -9.41924E-07, -4.42599E-05,   0.000271669  } };

//Matrix defaultSensorToXYZMatrix=IdentityMatrix(3);
Matrix defaultSensorToXYZMatrix(&defaultSensorToXYZ[0][0],3,3);
Matrix defaultXYZToSensorMatrix=defaultSensorToXYZMatrix.GetInverse();

CColor noDataColor(FX_NODATA,FX_NODATA,FX_NODATA);

/////////////////////////////////////////////////////////////////////
// ColorRGBDisplay Class
//
//////////////////////////////////////////////////////////////////////

ColorRGBDisplay::ColorRGBDisplay()
{
}

ColorRGBDisplay::ColorRGBDisplay(double aGreyPercent)
{
	// by default use virtual DisplayRGBColor function
    (*this)[0] = aGreyPercent;
    (*this)[1] = aGreyPercent;
    (*this)[2] = aGreyPercent;
}

ColorRGBDisplay::ColorRGBDisplay(double aRedPercent,double aGreenPercent,double aBluePercent)
{
    (*this)[0] = aRedPercent;
    (*this)[1] = aGreenPercent;
    (*this)[2] = aBluePercent;
}

#ifdef LIBHCFR_HAS_WIN32_API

ColorRGBDisplay::ColorRGBDisplay(COLORREF aColor)
{
    (*this)[0] = double(GetRValue(aColor)) / 2.55;
    (*this)[1] = double(GetGValue(aColor)) / 2.55;
    (*this)[2] = double(GetBValue(aColor)) / 2.55;
}

COLORREF ColorRGBDisplay::GetColorRef(bool is16_235) const
{

    BYTE r(ConvertPercentToBYTE((*this)[0], is16_235));
    BYTE g(ConvertPercentToBYTE((*this)[1], is16_235));
    BYTE b(ConvertPercentToBYTE((*this)[2], is16_235));
    return RGB(r, g, b);
}

BYTE ColorRGBDisplay::ConvertPercentToBYTE(double percent, bool is16_235)
{
    double coef;
    double offset;

    if (is16_235)
    {
        coef = (235.0 - 16.0) / 100.0;
        offset = 16.0;
    }
    else
    {
        coef = 255.0 / 100.0;
        offset = 0.0;
    }

    int result((int)floor(offset + percent * coef + 0.5));

    if(result < 0)
    {
        return 0;
    }
    else if(result > 255)
    {
        return 255;
    }
    else
    {
        return (BYTE)result;
    }
}
#endif // #ifdef LIBHCFR_HAS_WIN32_API

/////////////////////////////////////////////////////////////////////
// implementation of the ColorTriplet class.
//////////////////////////////////////////////////////////////////////
ColorTriplet::ColorTriplet() :
    Matrix(FX_NODATA, 3, 1)
{
}

ColorTriplet::ColorTriplet(const Matrix& matrix) :
    Matrix(matrix)
{
    if(GetRows() != 3 || GetColumns() != 1)
    {
        throw std::logic_error("matrix not right shape to create color");
    }
}

ColorTriplet::ColorTriplet(double a, double b, double c) :
    Matrix(FX_NODATA, 3, 1)
{
    (*this)[0] = a;
    (*this)[1] = b;
    (*this)[2] = c;
}

const double& ColorTriplet::operator[](const int nRow) const
{
    return (*this)(nRow, 0);
}

double& ColorTriplet::operator[](const int nRow)
{
    return (*this)(nRow, 0);
}

bool ColorTriplet::isValid() const
{
    // allow small negative values but reject anything based off FX_NODATA
    return ((*this)[0] > -1.0) && ((*this)[1] > -1.0) && ((*this)[2] > -1.0);
}

/////////////////////////////////////////////////////////////////////
// implementation of the ColorXYZ class.
//////////////////////////////////////////////////////////////////////
ColorXYZ::ColorXYZ()
{
}

ColorXYZ::ColorXYZ(const Matrix& matrix) :
    ColorTriplet(matrix)
{
}

ColorXYZ::ColorXYZ(const ColorRGB& RGB, CColorReference colorReference)
{
    if(RGB.isValid())
    {
        CLockWhileInScope dummy(m_matrixSection);
        *this = ColorXYZ(colorReference.RGBtoXYZMatrix*RGB);
    }
}

ColorXYZ::ColorXYZ(const ColorxyY& xyY)
{
    if(xyY.isValid())
    {
        if(xyY[1] != 0)
        {
            (*this)[0] = (xyY[0]*xyY[2])/xyY[1];
            (*this)[1] = xyY[2];
            (*this)[2] = ((1.0 - xyY[0] - xyY[1])*xyY[2])/xyY[1];
        }
    }
}

ColorXYZ::ColorXYZ(double X, double Y, double Z) :
    ColorTriplet(X, Y, Z)
{
}

int ColorXYZ::GetColorTemp(const CColorReference& colorReference) const
{
    double aXYZ[3];
    double aColorTemp;

    aXYZ[0]=(*this)[0];
    aXYZ[1]=(*this)[1];
    aXYZ[2]=(*this)[2];

    if(XYZtoCorColorTemp(aXYZ,&aColorTemp) == 0)
    {
        return (int)aColorTemp;
    }
    else
    {
        ColorRGB rgb(*this, colorReference);
        if(rgb[0] >  rgb[2]) // if red > blue then TC < 1500 else TC > 12000
        {
            return 1499;
        }
        else
        {
            return 12001;
        }
    }

}

double ColorXYZ::GetDeltaLCH(double YWhite, const ColorXYZ& refColor, double YWhiteRef, const CColorReference & colorReference, int dE_form, bool isGS, int gw_Weight, double &dChrom, double &dHue ) const
{
	CColorReference cRef=CColorReference(HDTV, D65, 2.2); //special modes assume rec.709
	double dLight;
	if (YWhite <= 0) YWhite = 120.;
	if (YWhiteRef <= 0) YWhiteRef = 1.0;
    //gray world weighted white reference
    switch (gw_Weight)
    {
    case 1:
    {
        YWhiteRef = 0.15 * YWhiteRef;
        YWhite = 0.15 * YWhite;
        break;
    }
    case 2:
    {
        YWhiteRef = 0.05 * YWhiteRef;
        YWhite = 0.05 * YWhite;
    }
    }
	if (!(colorReference.m_standard == HDTVb || colorReference.m_standard == CC6 || colorReference.m_standard == HDTVa))
		cRef=colorReference;
	switch (dE_form)
	{
		case 0:
		{
			//CIE76uv
			ColorLuv LuvRef(refColor, YWhiteRef, cRef);
			ColorLuv Luv(*this, YWhite, cRef);
			dLight = sqrt(pow ((Luv[0] - LuvRef[0]),2));
			dChrom = sqrt(pow((Luv[1] - LuvRef[1]),2));
			dHue = sqrt(pow((Luv[2] - LuvRef[2]),2));
			break;
		}
		case 1:
		{
			//CIE76ab
			ColorLab LabRef(refColor, YWhiteRef, cRef);
			ColorLab Lab(*this, YWhite, cRef);
			dLight = sqrt(pow ((Lab[0] - LabRef[0]),2));
			dChrom = sqrt(pow((Lab[1] - LabRef[1]),2));
			dHue = sqrt(pow((Lab[2] - LabRef[2]),2));
			break;
		}
		//CIE94
		case 2:
		{
			ColorLab LabRef(refColor, YWhiteRef, cRef);
			ColorLab Lab(*this, YWhite, cRef);
			double dL2 = pow ((LabRef[0] - Lab[0]),2.0);
			double C1 = sqrt ( pow (LabRef[1],2.0) + pow (LabRef[2],2.0));
			double C2 = sqrt ( pow (Lab[1],2.0) + pow (Lab[2],2.0));
			double dC2 = pow ((C1-C2),2.0);
			double da2 = pow (LabRef[1] - Lab[1],2.0);
			double db2 = pow (LabRef[2] - Lab[2],2.0);
			double dH2 =  (da2 + db2 - dC2);
			//kl=kc=kh=1
//			dE = sqrt ( dL2 + dC2/pow((1+0.045*C1),2.0) + dH2/pow((1+0.015*C1),2.0) );
			dLight = sqrt(dL2);
			dChrom = sqrt(dC2/pow((1+0.045*C1),2.0));
			dHue = sqrt(dH2/pow((1+0.015*C1),2.0));
			break;
		}
		case 3:
		{
			//CIE2000
			ColorLab LabRef(refColor, YWhiteRef, cRef);
			ColorLab Lab(*this, YWhite, cRef);
			double L1 = LabRef[0];
			double L2 = Lab[0];
			double Lp = (L1 + L2) / 2.0;
			double a1 = LabRef[1];
			double a2 = Lab[1];
			double b1 = LabRef[2];
			double b2 = Lab[2];
			double C1 = sqrt( pow(a1, 2.0) + pow(b1, 2.0) );
			double C2 = sqrt( pow(a2, 2.0) + pow(b2, 2.0) );
			double C = (C1 + C2) / 2.0;
			double G = (1 - sqrt ( pow(C, 7.0) / (pow(C, 7.0) + pow(25, 7.0)) ) ) / 2;
			double a1p = a1 * (1 + G);
			double a2p = a2 * (1 + G);
			double C1p = sqrt ( pow(a1p, 2.0) + pow(b1, 2.0) );
			double C2p = sqrt ( pow(a2p, 2.0) + pow(b2, 2.0) );
			double Cp = (C1p + C2p) / 2.0;
			double h1p = (atan2(b1, a1p) >= 0?atan2(b1, a1p):atan2(b1, a1p) + PI * 2.0);
			double h2p = (atan2(b2, a2p) >= 0?atan2(b2, a2p):atan2(b2, a2p) + PI * 2.0);
			double Hp = ( abs(h1p - h2p) > PI ?(h1p + h2p + PI * 2) / 2.0:(h1p + h2p) / 2.0);
			double T = 1 - 0.17 * cos(Hp - 30. / 180. * PI) + 0.24 * cos(2 * Hp) + 0.32 * cos(3 * Hp + 6.0 / 180. * PI) - 0.20 * cos(4 * Hp - 63.0 / 180. * PI);
			double dhp = ( abs(h2p - h1p) <= PI?(h2p - h1p):((h2p <= h1p)?(h2p - h1p + 2 * PI):(h2p - h1p - 2 * PI)));
			double dLp = L2 - L1;
			double dCp = C2p - C1p;
			double dHp = 2 * sqrt( C1p * C2p ) * sin( dhp / 2);
			double SL = 1 + (0.015 * pow( (Lp - 50), 2.0 ) / sqrt( 20 + pow( Lp - 50, 2.0) ) );
			double SC = 1 + 0.045 * Cp;
			double SH = 1 + 0.015 * Cp * T;
			double dtheta = 30 * exp( -1.0 * pow(( (Hp * 180. / PI - 275.0) / 25.0), 2.0) );
			double RC = 2 * sqrt ( pow(Cp, 7.0) / (pow(Cp, 7.0) + pow(25.0, 7.0)) );
			double RT = -1.0 * RC * sin(2 * dtheta / 180. * PI);
//			dE = sqrt ( pow( dLp / SL, 2.0) + pow( dCp / SC, 2.0) + pow( dHp / SH, 2.0) + RT * (dCp / SC) * (dHp / SH));
			dLight = sqrt(pow( dLp / SL, 2.0));
			dChrom = sqrt(pow( dCp / SC, 2.0));
			dHue = sqrt( pow( dHp / SH, 2.0));
			break;
		}
		case 4:
			//CMC(1:1)
		{
	        ColorLab LabRef(refColor, YWhiteRef, cRef);
		    ColorLab Lab(*this, YWhite, cRef);
			double L1 = LabRef[0];
			double L2 = Lab[0];
			double a1 = LabRef[1];
			double a2 = Lab[1];
			double b1 = LabRef[2];
			double b2 = Lab[2];
			double C1 = sqrt (pow(a1, 2.0) + pow(b1,2.0));
			double C2 = sqrt (pow(a2, 2.0) + pow(b2,2.0));
			double dC = C1 - C2;
			double delL = L1 - L2;
			double da = a1 - a2;
			double db = b1 - b2;
			double dH = sqrt( abs(pow(da, 2.0) + pow(db,2.0) - pow(dC,2.0)) );
			double SL = (L1 < 16?0.511:0.040975*L1/(1+0.01765*L1));
			double SC = 0.0638 * C1/(1 + 0.0131*C1) + 0.638;
			double F = sqrt ( pow(C1,4.0)/(pow(C1,4.0) + 1900) );
			double H1 = atan2(b1,a1) / PI * 180.; 
			H1 = (H1 < 0?H1 + 360:( (H1 >= 360) ? H1-360:H1 ) );
			double T = (H1 <= 345 && H1 >= 164 ? 0.56 + abs(0.2 * cos ((H1 + 168)/180.*PI)) : 0.36 + abs(0.4 * cos((H1+35)/180.*PI)) ); 
			double SH = SC * (F * T + 1 - F);
//			dE = sqrt( pow(delL/SL,2.0)  + pow(dC/SC,2.0) + pow(dH/SH,2.0) );
			dLight = sqrt(pow(delL/SL,2.0));
			dChrom = sqrt(pow(dC/SC,2.0));
			dHue = sqrt( pow(dH/SH,2.0));
			break;
		}
		case 5:
		{
            if (isGS)
            {
    		//CIE76uv
                ColorLuv LuvRef(refColor, YWhiteRef, cRef);
                ColorLuv Luv(*this, YWhite, cRef);
//                dE = sqrt ( pow ((Luv[0] - LuvRef[0]),2) + pow((Luv[1] - LuvRef[1]),2) + pow((Luv[2] - LuvRef[2]),2) );
				dLight = sqrt(pow ((Luv[0] - LuvRef[0]),2));
				dChrom = sqrt(pow((Luv[1] - LuvRef[1]),2));
				dHue = sqrt(pow((Luv[2] - LuvRef[2]),2));
            }
            else
            {
        		//CIE2000
                ColorLab LabRef(refColor, YWhiteRef, cRef);
                ColorLab Lab(*this, YWhite, cRef);
		        double L1 = LabRef[0];
		        double L2 = Lab[0];
		        double Lp = (L1 + L2) / 2.0;
		        double a1 = LabRef[1];
		        double a2 = Lab[1];
		        double b1 = LabRef[2];
		        double b2 = Lab[2];
		        double C1 = sqrt( pow(a1, 2.0) + pow(b1, 2.0) );
		        double C2 = sqrt( pow(a2, 2.0) + pow(b2, 2.0) );
		        double C = (C1 + C2) / 2.0;
		        double G = (1 - sqrt ( pow(C, 7.0) / (pow(C, 7.0) + pow(25, 7.0)) ) ) / 2;
		        double a1p = a1 * (1 + G);
		        double a2p = a2 * (1 + G);
		        double C1p = sqrt ( pow(a1p, 2.0) + pow(b1, 2.0) );
		        double C2p = sqrt ( pow(a2p, 2.0) + pow(b2, 2.0) );
		        double Cp = (C1p + C2p) / 2.0;
		        double h1p = (atan2(b1, a1p) >= 0?atan2(b1, a1p):atan2(b1, a1p) + PI * 2.0);
		        double h2p = (atan2(b2, a2p) >= 0?atan2(b2, a2p):atan2(b2, a2p) + PI * 2.0);
		        double Hp = ( abs(h1p - h2p) > PI ?(h1p + h2p + PI * 2) / 2.0:(h1p + h2p) / 2.0);
		        double T = 1 - 0.17 * cos(Hp - 30. / 180. * PI) + 0.24 * cos(2 * Hp) + 0.32 * cos(3 * Hp + 6.0 / 180. * PI) - 0.20 * cos(4 * Hp - 63.0 / 180. * PI);
		        double dhp = ( abs(h2p - h1p) <= PI?(h2p - h1p):((h2p <= h1p)?(h2p - h1p + 2 * PI):(h2p - h1p - 2 * PI)));
		        double dLp = L2 - L1;
		        double dCp = C2p - C1p;
		        double dHp = 2 * sqrt( C1p * C2p ) * sin( dhp / 2);
		        double SL = 1 + (0.015 * pow( (Lp - 50), 2.0 ) / sqrt( 20 + pow( Lp - 50, 2.0) ) );
		        double SC = 1 + 0.045 * Cp;
		        double SH = 1 + 0.015 * Cp * T;
		        double dtheta = 30 * exp( -1.0 * pow(( (Hp * 180. / PI - 275.0) / 25.0), 2.0) );
		        double RC = 2 * sqrt ( pow(Cp, 7.0) / (pow(Cp, 7.0) + pow(25.0, 7.0)) );
		        double RT = -1.0 * RC * sin(2 * dtheta / 180. * PI);
//		        dE = sqrt ( pow( dLp / SL, 2.0) + pow( dCp / SC, 2.0) + pow( dHp / SH, 2.0) + RT * (dCp / SC) * (dHp / SH));
				dLight = sqrt(pow( dLp / SL, 2.0));
				dChrom = sqrt(pow( dCp / SC, 2.0));
				dHue = sqrt( pow( dHp / SH, 2.0));
            }
		break;
		}
	}
	return dLight;
}

double getL_EOTF ( double valx, CColor White, CColor Black, double g_rel, double split, int mode, double m_diffuseL, double m_MinML, double m_MaxML, double m_MinTL, double m_MaxTL, bool ToneMap, bool cBT2390, double bbc_gamma, double b_fact, double E2_fact, double E2_fact1)
{
	if (valx == 0 && mode > 4) return (ToneMap?m_MinTL / 100.:0.0);
	if (valx < 0)
		valx = 0;
	if (valx > 1)
	{
		if (mode == 5)
			return 100.;
		else
			return 1.;
	}
	if (ToneMap && mode == 5)
		mode = 10;

	if (ToneMap && mode == -5)
		mode = -10;

//Returns relative output luminance given input luma (stimulus)
//exception is ST2084 which returns an absolute value
//BT1886
	double maxL = White.isValid()?White.GetY():100.0;
	double minL = Black.isValid()?Black.GetY():0.012;
    double yh =  maxL * pow(0.5, g_rel);
    double exp0 = g_rel==0.0?2.4:g_rel;
	double outL, Lbt, offset;
//SMPTE2084
	double m1=0.1593017578125;
    double m2=78.84375;
    double c1=0.8359375;
    double c2=18.8515625;
	double c3=18.6875;
//sRGB
	double outL_sRGB;
    if( valx <= 0.04045 ) {
        outL_sRGB = valx / 12.92;
    }
    else
		outL_sRGB = pow( ( valx + 0.055 ) / 1.055, 2.4 );
//L*
	double outL_lab,t,outL_labF;
	t = 1.0 / 116. * (valx * 100. + 16);
	if ( 29 * t > 6)
		outL_lab = pow(t,3.0);
	else
		outL_lab = 3 * pow((6. / 29.),2) * (t - 4. / 29.);

	t = valx;
	if ( t > pow(6./29.,3))
		outL_labF = pow(t,1.0/3.0);
	else
		outL_labF = t / (3.0 * pow(6./29.,2)) + 4.0 / 29.0;

//BBC hybrid log gamma BT.2100 version

	double outL_bbc, bbc_a = 0.17883277, bbc_b = 1.0 - 4 * bbc_a, bbc_c = 0.5 - bbc_a * log(4 * bbc_a);
	double bbc_alpha = (m_MaxTL - m_MinTL), bbc_beta = m_MinTL;

	offset = split / 100.0 * minL;
    double a = pow ( ( pow (maxL,1.0/exp0 ) - pow ( offset,1.0/exp0 ) ),exp0 );
    double b = ( pow ( offset,1.0/exp0 ) ) / ( pow (maxL,1.0/exp0 ) - pow ( offset,1.0/exp0 ) );

	if (g_rel != 0.)
	{
        exp0 = (log(yh)-log(a))/log(0.5+b);

		//refine
		for (int i = 0;i < 9;i++)
		{
			a = pow ( ( pow (maxL,1.0/exp0 ) - pow ( offset,1.0/exp0 ) ),exp0 );
			b = ( pow ( offset,1.0/exp0 ) ) / ( pow (maxL,1.0/exp0 ) - pow ( offset,1.0/exp0 ) );
			exp0 = (log(yh)-log(a))/log(0.5+b);
		}
	}

	Lbt = ( a * pow ( (valx + b)<0?0:(valx+b), exp0 ) );

	double value, E3 = 0.0;
	Scale = m_diffuseL / 94.37844 * 10000.;

	switch (mode)
	{
		case 4: //BT.1886
			outL = (Lbt + minL * (1 - split / 100.))/(maxL + minL * (1 - split / 100.));
		break;
		case -4: //BT.1886 OETF
			outL = pow((valx*(maxL+minL*(1-split/100.)-minL*(1-split/100.)))/a,1.0/exp0)-b;
//			outL = (Lbt + minL * (1 - split / 100.))/(maxL + minL * (1 - split / 100.));
		break;
		case 5: //BT.2084
			outL = pow(max(pow(valx,1.0 / m2) - c1,0) / (c2 - c3 * pow(valx, 1.0 / m2)), 1.0 / m1);
			outL = outL * 10000. / 100.00 * m_diffuseL / 94.37844; 
			outL = min(outL, m_MaxTL / 100.0);
//			outL = min(outL, 100.0);
		break;
		case -5: //BT.2084 inverse
			outL = pow( (c1 + c2 * pow(valx,m1)) / (1 + c3 * pow(valx,m1)), m2); 
		break;
		case 6: //L*
			outL = outL_lab;
		break;
		case -6: //L* OETF
			outL = (outL_labF * 116.0 - 16.0) / 100.;
		break;
		case 7: //bbc non-linear to linear OOTF[EOTF = OETF^-1]
			if (valx <= 0.5)
				outL_bbc = pow(valx, 2) / 3.0;
			else
				outL_bbc = (exp( (valx - bbc_c)/bbc_a ) + bbc_b) / 12.0;

			outL = (bbc_alpha * pow(outL_bbc, bbc_gamma - 1) * outL_bbc + bbc_beta) / White.GetY();
		break;
		case -7: //bbc linear to non-linear OETF
			if (valx <=  1.0 / 12.0)
				outL_bbc = pow(3 * valx, 0.5);
			else
				outL_bbc = bbc_a * log(12.0 * valx - bbc_b) + bbc_c;

			outL = outL_bbc;
		break;
		case 10: //BT.2084/2390
			double E1,E2,E4,b,d,KS,T,p;
			if (!cBT2390)
			{
				double tmWhite = getL_EOTF(0.5022283, White, Black, g_rel, split, 5, m_diffuseL, m_MinML, m_MaxML, m_MinTL, m_MaxTL, ToneMap, TRUE, bbc_gamma, b_fact, E2_fact) * 100.0;
				m_MaxML = m_MaxML * m_diffuseL / 94.37844 ; 

				if (m_MinML > m_MinTL)
					m_MinML = m_MinTL;

				E1 = valx - pow( (c1 + c2 * pow(m_MinML/Scale,m1)) / (1 + c3 * pow(m_MinML/Scale,m1)), m2);
				d = pow( (c1 + c2 * pow(m_MaxML/Scale,m1)) / (1 + c3 * pow(m_MaxML/Scale,m1)), m2) - pow( (c1 + c2 * pow(m_MinML/Scale,m1)) / (1 + c3 * pow(m_MinML/Scale,m1)), m2);
				E1 = E1 / d;
				minL = (pow( (c1 + c2 * pow(m_MinTL/Scale,m1)) / (1 + c3 * pow(m_MinTL/Scale,m1)), m2)-pow( (c1 + c2 * pow(m_MinML/Scale,m1)) / (1 + c3 * pow(m_MinML/Scale,m1)), m2)) / d;
				maxL = (pow( (c1 + c2 * pow(m_MaxTL/Scale,m1)) / (1 + c3 * pow(m_MaxTL/Scale,m1)), m2)-pow( (c1 + c2 * pow(m_MinML/Scale,m1)) / (1 + c3 * pow(m_MinML/Scale,m1)), m2)) / d;
				KS = 1.5 * maxL - 0.5;
				b = minL;
				E2 = E1;

				double slope = 1.0;
				double lower = KS;
				double upper = pow( (c1 + c2 * pow(m_MaxML/Scale,m1)) / (1 + c3 * pow(m_MaxML/Scale,m1)), m2);
				double mmaxL = lower + E2_fact1 / 100 * (upper - lower);

				if (E1 >= mmaxL && (upper > lower))
				{
						slope = pow((E1 - mmaxL),2.0) * pow((1.0-E1),0.75)*E2_fact*(2.5*pow(mmaxL,2.0)+3*pow(mmaxL,3.0)) + 1.0;
						E1 = E1 / slope;
				}

				if (E1 >= KS && E1 <= 1.0)
				{
					T = (E1 - KS) / (1.0 - KS);
					E2 = ((2 * pow(T,3.0) - 3 * pow(T,2.0) + 1.0)*KS + (pow(T,3.0) - 2 * pow(T, 2.0) + T) * (1.0 - KS)) + ( -2.0 * pow(T,3.0) + 3.0 * pow(T,2.0)) * maxL;
				}

				if (E2 >= 0.0 && E2 <= 1.0)
				{
					p = min(1.0 / b, 4 * b_fact);
					E3 = E2 + b * pow((1.0 - E2),p);
					E3 = (E3 * d + pow( (c1 + c2 * pow(m_MinML/Scale,m1)) / (1 + c3 * pow(m_MinML/Scale,m1)), m2)); 
					E4 = pow(max(pow(E3,1.0 / m2) - c1,0) / (c2 - c3 * pow(E3, 1.0 / m2)), 1.0 / m1);
					outL = E4 * 10000. / 100.00 * m_diffuseL / 94.37844;
					outL = min(outL, m_MaxTL / 100.0 );
				}
				else
				{
					outL = pow(max(pow(valx,1.0 / m2) - c1,0) / (c2 - c3 * pow(valx, 1.0 / m2)), 1.0 / m1);
					outL = outL * 10000. / 100.00 * m_diffuseL / 94.37844; 
					outL = min(outL, m_MaxTL / 100.0);
//					outL = min(outL, 100.0);
				}
			}
			else
			{
				BT2390x.clear();
				BT2390y.clear();
				int ii = 1024;

				if (m_MinML > m_MinTL)
					m_MinML = m_MinTL;
				m_MaxML = m_MaxML * m_diffuseL / 94.37844; 

				if (valx == 0.5022283)
					ii = 1;
				for (int i=0; i < ii;i++)
				{
					if (valx != 0.5022283)
						valx = i / 1024.;

					E1 = valx - pow( (c1 + c2 * pow(m_MinML/Scale,m1)) / (1 + c3 * pow(m_MinML/Scale,m1)), m2);
					d = pow( (c1 + c2 * pow(m_MaxML/Scale,m1)) / (1 + c3 * pow(m_MaxML/Scale,m1)), m2) - pow( (c1 + c2 * pow(m_MinML/Scale,m1)) / (1 + c3 * pow(m_MinML/Scale,m1)), m2);
					E1 = E1 / d;
					minL = (pow( (c1 + c2 * pow(m_MinTL/Scale,m1)) / (1 + c3 * pow(m_MinTL/Scale,m1)), m2)-pow( (c1 + c2 * pow(m_MinML/Scale,m1)) / (1 + c3 * pow(m_MinML/Scale,m1)), m2)) / d;
					maxL = (pow( (c1 + c2 * pow(m_MaxTL/Scale,m1)) / (1 + c3 * pow(m_MaxTL/Scale,m1)), m2)-pow( (c1 + c2 * pow(m_MinML/Scale,m1)) / (1 + c3 * pow(m_MinML/Scale,m1)), m2)) / d;
					KS = 1.5 * maxL - 0.5;
					b = minL;
					E2 = E1;

					double slope = 1.0;
					double lower = KS;
					double upper = pow( (c1 + c2 * pow(m_MaxML/Scale,m1)) / (1 + c3 * pow(m_MaxML/Scale,m1)), m2);
					double mmaxL = lower + E2_fact1 / 100 * (upper - lower);

					if (E1 >= mmaxL && (upper > lower))
					{
							slope = pow((E1 - mmaxL),2.0) * pow((1.0-E1),0.75)*E2_fact*(2.5*pow(mmaxL,2.0)+3*pow(mmaxL,3.0)) + 1.0;
							E1 = E1 / slope;
					}

					if (E1 >= KS && E1 <= 1.2)
					{
						T = (E1 - KS) / (1.0 - KS);
						E2 = ((2 * pow(T,3.0) - 3 * pow(T,2.0) + 1.0)*KS + (pow(T,3.0) - 2 * pow(T, 2.0) + T) * (1.0 - KS))  + ( -2.0 * pow(T,3.0) + 3.0 * pow(T,2.0)) * maxL;
					}
			
					if (E2 >= 0.0 && E2 <= 1.0)
					{
						p = min(1.0 / b, 4 * b_fact); //Hoech mod
						E3 = E2 + b * pow((1.0 - E2),p);
						E3 = (E3 * d + pow( (c1 + c2 * pow(m_MinML/Scale,m1)) / (1 + c3 * pow(m_MinML/Scale,m1)), m2)); 
						E4 = pow(max(pow(E3,1.0 / m2) - c1,0) / (c2 - c3 * pow(E3, 1.0 / m2)), 1.0 / m1);
						outL = E4 * 10000. / 100.00 * m_diffuseL / 94.37844;
						outL = min(outL, m_MaxTL / 100.0);
					}
					else
					{
						outL = pow(max(pow(valx,1.0 / m2) - c1,0) / (c2 - c3 * pow(valx, 1.0 / m2)), 1.0 / m1);
						outL = outL * 10000. / 100.00 * m_diffuseL / 94.37844; 
						outL = min(outL, m_MaxTL / 100.0);
//						outL = min(outL, 100.0);
					}
					BT2390x.push_back(valx);
					BT2390y.push_back(outL / 100.0 / m_diffuseL * 94.37844);
				}
			}
		break;
		case -10: //BT.2084/2390 inverse curve look-up
			{
//				double tmWhite = getL_EOTF(0.5022283, White, Black, g_rel, split, 5, m_diffuseL, m_MinML, m_MaxML, m_MinTL, m_MaxTL, ToneMap, TRUE, bbc_gamma, b_fact, E2_fact, E2_fact1) * 100.0;
//				m_MaxML = m_MaxML * tmWhite / 94.37844;
//				m_MaxML = m_MaxML * m_diffuseL / 94.37844 ; 
				getL_EOTF(valx, White, Black, g_rel, split, 5, m_diffuseL, m_MinML, m_MaxML, m_MinTL, m_MaxTL, ToneMap, TRUE, bbc_gamma, b_fact, E2_fact, E2_fact1);
				value = abs(valx - BT2390y[0]);
				outL = BT2390x[0];
				for (int i = 0; i < (int)BT2390y.size(); i++)
				{
					if (value > abs(valx - BT2390y[i]))
					{
						 value = abs(valx - BT2390y[i]);
						 outL = BT2390x[i];
					}
				}
			}
		break;
		case 99: //sRGB
			outL = outL_sRGB;
		break;
		case 8: //DV_500 Dolby vision 500 cd/m^2 peak
			value = abs(valx - DV_500_EOTF[0].x);
			outL = DV_500_EOTF[0].y;

			for (int i = 0; i < 220; i++)
			{
				if (value > abs(valx - DV_500_EOTF[i].x))
				{
					 value = abs(valx - DV_500_EOTF[i].x);
					 outL = DV_500_EOTF[i].y / DV_500_EOTF[219].y;
				}
			}
		break;
		case -8: //DV_500 Dolby vision
			value = abs(valx - DV_500_EOTF[0].y);
			outL = DV_500_EOTF[0].x;

			for (int i = 0; i < 220; i++)
			{
				if (value > abs(valx - DV_500_EOTF[i].y))
				{
					 value = abs(valx - DV_500_EOTF[i].y);
					 outL = DV_500_EOTF[i].x;
				}
			}
		break;
		case 9: //DV_400 Dolby vision use for 400 cd/m^2 peak
			value = abs(valx - DV_400_EOTF[0].x);
			outL = DV_400_EOTF[0].y;

			for (int i = 0; i < 220; i++)
			{
				if (value > abs(valx - DV_400_EOTF[i].x))
				{
					 value = abs(valx - DV_400_EOTF[i].x);
					 outL = DV_400_EOTF[i].y / DV_400_EOTF[219].y;
				}
			}
		break;
		case -9: //DV_400 Dolby vision
			value = abs(valx - DV_400_EOTF[0].y);
			outL = DV_400_EOTF[0].x;

			for (int i = 0; i < 220; i++)
			{
				if (value > abs(valx - DV_400_EOTF[i].y))
				{
					 value = abs(valx - DV_400_EOTF[i].y);
					 outL = DV_400_EOTF[i].x;
				}
			}
		break;
	}
	return min(max(0,outL),100);
}

double ColorXYZ::GetDeltaE(double YWhite, const ColorXYZ& refColor, double YWhiteRef, const CColorReference & colorReference, int dE_form, bool isGS, int gw_Weight ) const
{
	CColorReference cRef=CColorReference(HDTV, D65, 2.2); //special modes assume rec.709
	double dE;
	if (YWhite <= 0) YWhite = 120.;
	if (YWhiteRef <= 0) YWhiteRef = 1.0;
    //gray world weighted white reference
    switch (gw_Weight)
    {
		case 1:
		{
			YWhiteRef = 0.15 * YWhiteRef;
			YWhite = 0.15 * YWhite;
			break;
		}
		case 2:
		{
			YWhiteRef = 0.05 * YWhiteRef;
			YWhite = 0.05 * YWhite;
		}
    }
	if (!(colorReference.m_standard == HDTVb || colorReference.m_standard == CC6 || colorReference.m_standard == HDTVa))
		cRef=colorReference;
	switch (dE_form)
	{
		case 0:
		{
			//CIE76uv
			ColorLuv LuvRef(refColor, YWhiteRef, cRef);
			ColorLuv Luv(*this, YWhite, cRef);
			dE = sqrt ( pow ((Luv[0] - LuvRef[0]),2) + pow((Luv[1] - LuvRef[1]),2) + pow((Luv[2] - LuvRef[2]),2) );
			break;
		}
		case 1:
		{
			//CIE76ab
			ColorLab LabRef(refColor, YWhiteRef, cRef);
			ColorLab Lab(*this, YWhite, cRef);
			dE = sqrt ( pow ((Lab[0] - LabRef[0]),2) + pow((Lab[1] - LabRef[1]),2) + pow((Lab[2] - LabRef[2]),2) );
			break;
		}
		//CIE94
		case 2:
		{
			ColorLab LabRef(refColor, YWhiteRef, cRef);
			ColorLab Lab(*this, YWhite, cRef);
			double dL2 = pow ((LabRef[0] - Lab[0]),2.0);
			double C1 = sqrt ( pow (LabRef[1],2.0) + pow (LabRef[2],2.0));
			double C2 = sqrt ( pow (Lab[1],2.0) + pow (Lab[2],2.0));
			double dC2 = pow ((C1-C2),2.0);
			double da2 = pow (LabRef[1] - Lab[1],2.0);
			double db2 = pow (LabRef[2] - Lab[2],2.0);
			double dH2 =  (da2 + db2 - dC2);
			//kl=kc=kh=1
			dE = sqrt ( dL2 + dC2/pow((1+0.045*C1),2.0) + dH2/pow((1+0.015*C1),2.0) );
			break;
		}
		case 3:
		{
			//CIE2000
			ColorLab LabRef(refColor, YWhiteRef, cRef);
			ColorLab Lab(*this, YWhite, cRef);
			double L1 = LabRef[0];
			double L2 = Lab[0];
			double Lp = (L1 + L2) / 2.0;
			double a1 = LabRef[1];
			double a2 = Lab[1];
			double b1 = LabRef[2];
			double b2 = Lab[2];
			double C1 = sqrt( pow(a1, 2.0) + pow(b1, 2.0) );
			double C2 = sqrt( pow(a2, 2.0) + pow(b2, 2.0) );
			double C = (C1 + C2) / 2.0;
			double G = (1 - sqrt ( pow(C, 7.0) / (pow(C, 7.0) + pow(25, 7.0)) ) ) / 2;
			double a1p = a1 * (1 + G);
			double a2p = a2 * (1 + G);
			double C1p = sqrt ( pow(a1p, 2.0) + pow(b1, 2.0) );
			double C2p = sqrt ( pow(a2p, 2.0) + pow(b2, 2.0) );
			double Cp = (C1p + C2p) / 2.0;
			double h1p = (atan2(b1, a1p) >= 0?atan2(b1, a1p):atan2(b1, a1p) + PI * 2.0);
			double h2p = (atan2(b2, a2p) >= 0?atan2(b2, a2p):atan2(b2, a2p) + PI * 2.0);
			double Hp = ( abs(h1p - h2p) > PI ?(h1p + h2p + PI * 2) / 2.0:(h1p + h2p) / 2.0);
			double T = 1 - 0.17 * cos(Hp - 30. / 180. * PI) + 0.24 * cos(2 * Hp) + 0.32 * cos(3 * Hp + 6.0 / 180. * PI) - 0.20 * cos(4 * Hp - 63.0 / 180. * PI);
			double dhp = ( abs(h2p - h1p) <= PI?(h2p - h1p):((h2p <= h1p)?(h2p - h1p + 2 * PI):(h2p - h1p - 2 * PI)));
			double dLp = L2 - L1;
			double dCp = C2p - C1p;
			double dHp = 2 * sqrt( C1p * C2p ) * sin( dhp / 2);
			double SL = 1 + (0.015 * pow( (Lp - 50), 2.0 ) / sqrt( 20 + pow( Lp - 50, 2.0) ) );
			double SC = 1 + 0.045 * Cp;
			double SH = 1 + 0.015 * Cp * T;
			double dtheta = 30 * exp( -1.0 * pow(( (Hp * 180. / PI - 275.0) / 25.0), 2.0) );
			double RC = 2 * sqrt ( pow(Cp, 7.0) / (pow(Cp, 7.0) + pow(25.0, 7.0)) );
			double RT = -1.0 * RC * sin(2 * dtheta / 180. * PI);
			dE = sqrt ( pow( dLp / SL, 2.0) + pow( dCp / SC, 2.0) + pow( dHp / SH, 2.0) + RT * (dCp / SC) * (dHp / SH));
			break;
		}
		case 4:
			//CMC(1:1)
		{
	        ColorLab LabRef(refColor, YWhiteRef, cRef);
		    ColorLab Lab(*this, YWhite, cRef);
			double L1 = LabRef[0];
			double L2 = Lab[0];
			double a1 = LabRef[1];
			double a2 = Lab[1];
			double b1 = LabRef[2];
			double b2 = Lab[2];
			double C1 = sqrt (pow(a1, 2.0) + pow(b1,2.0));
			double C2 = sqrt (pow(a2, 2.0) + pow(b2,2.0));
			double dC = C1 - C2;
			double delL = L1 - L2;
			double da = a1 - a2;
			double db = b1 - b2;
			double dH = sqrt( abs(pow(da, 2.0) + pow(db,2.0) - pow(dC,2.0)) );
			double SL = (L1 < 16?0.511:0.040975*L1/(1+0.01765*L1));
			double SC = 0.0638 * C1/(1 + 0.0131*C1) + 0.638;
			double F = sqrt ( pow(C1,4.0)/(pow(C1,4.0) + 1900) );
			double H1 = atan2(b1,a1) / PI * 180.; 
			H1 = (H1 < 0?H1 + 360:( (H1 >= 360) ? H1-360:H1 ) );
			double T = (H1 <= 345 && H1 >= 164 ? 0.56 + abs(0.2 * cos ((H1 + 168)/180.*PI)) : 0.36 + abs(0.4 * cos((H1+35)/180.*PI)) ); 
			double SH = SC * (F * T + 1 - F);
			dE = sqrt( pow(delL/SL,2.0)  + pow(dC/SC,2.0) + pow(dH/SH,2.0) );
			break;
		}
		case 5:
		{
            if (isGS)
            {
    		//CIE76uv
                ColorLuv LuvRef(refColor, YWhiteRef, cRef);
                ColorLuv Luv(*this, YWhite, cRef);
                dE = sqrt ( pow ((Luv[0] - LuvRef[0]),2) + pow((Luv[1] - LuvRef[1]),2) + pow((Luv[2] - LuvRef[2]),2) );
            }
            else
            {
        		//CIE2000
                ColorLab LabRef(refColor, YWhiteRef, cRef);
                ColorLab Lab(*this, YWhite, cRef);
		        double L1 = LabRef[0];
		        double L2 = Lab[0];
		        double Lp = (L1 + L2) / 2.0;
		        double a1 = LabRef[1];
		        double a2 = Lab[1];
		        double b1 = LabRef[2];
		        double b2 = Lab[2];
		        double C1 = sqrt( pow(a1, 2.0) + pow(b1, 2.0) );
		        double C2 = sqrt( pow(a2, 2.0) + pow(b2, 2.0) );
		        double C = (C1 + C2) / 2.0;
		        double G = (1 - sqrt ( pow(C, 7.0) / (pow(C, 7.0) + pow(25, 7.0)) ) ) / 2;
		        double a1p = a1 * (1 + G);
		        double a2p = a2 * (1 + G);
		        double C1p = sqrt ( pow(a1p, 2.0) + pow(b1, 2.0) );
		        double C2p = sqrt ( pow(a2p, 2.0) + pow(b2, 2.0) );
		        double Cp = (C1p + C2p) / 2.0;
		        double h1p = (atan2(b1, a1p) >= 0?atan2(b1, a1p):atan2(b1, a1p) + PI * 2.0);
		        double h2p = (atan2(b2, a2p) >= 0?atan2(b2, a2p):atan2(b2, a2p) + PI * 2.0);
		        double Hp = ( abs(h1p - h2p) > PI ?(h1p + h2p + PI * 2) / 2.0:(h1p + h2p) / 2.0);
		        double T = 1 - 0.17 * cos(Hp - 30. / 180. * PI) + 0.24 * cos(2 * Hp) + 0.32 * cos(3 * Hp + 6.0 / 180. * PI) - 0.20 * cos(4 * Hp - 63.0 / 180. * PI);
		        double dhp = ( abs(h2p - h1p) <= PI?(h2p - h1p):((h2p <= h1p)?(h2p - h1p + 2 * PI):(h2p - h1p - 2 * PI)));
		        double dLp = L2 - L1;
		        double dCp = C2p - C1p;
		        double dHp = 2 * sqrt( C1p * C2p ) * sin( dhp / 2);
		        double SL = 1 + (0.015 * pow( (Lp - 50), 2.0 ) / sqrt( 20 + pow( Lp - 50, 2.0) ) );
		        double SC = 1 + 0.045 * Cp;
		        double SH = 1 + 0.015 * Cp * T;
		        double dtheta = 30 * exp( -1.0 * pow(( (Hp * 180. / PI - 275.0) / 25.0), 2.0) );
		        double RC = 2 * sqrt ( pow(Cp, 7.0) / (pow(Cp, 7.0) + pow(25.0, 7.0)) );
		        double RT = -1.0 * RC * sin(2 * dtheta / 180. * PI);
		        dE = sqrt ( pow( dLp / SL, 2.0) + pow( dCp / SC, 2.0) + pow( dHp / SH, 2.0) + RT * (dCp / SC) * (dHp / SH));
					std::cerr << "Unexpected Exception in measurement thread" << std::endl;
				}
		break;
		}
		case 6:
		{
			//Used for HDR
			ColorICtCp ICCRef(refColor, YWhite, cRef);
			ColorICtCp ICC(*this, 1.0, cRef);
			dE = sqrt ( pow ((ICC[0] - ICCRef[0]),2) + pow((ICC[1] - ICCRef[1]),2) + pow((ICC[2] - ICCRef[2]),2) );
			break;
		}
		
	}
	return dE;
}

double ColorXYZ::GetOldDeltaE(const ColorXYZ& refColor) const
{
    ColorxyY xyY(*this);
    ColorxyY xyYRef(refColor);
    double x=xyY[0];
    double y=xyY[1];
    double u = 4.0*x / (-2.0*x + 12.0*y + 3.0); 
    double v = 9.0*y / (-2.0*x + 12.0*y + 3.0); 
    double xRef=xyYRef[0];
    double yRef=xyYRef[1];
    double uRef = 4.0*xRef / (-2.0*xRef + 12.0*yRef + 3.0); 
    double vRef = 9.0*yRef / (-2.0*xRef + 12.0*yRef + 3.0); 

    double dE = 1300.0 * sqrt ( pow((u - uRef),2) + pow((v - vRef),2) );

    return dE;
}

double ColorXYZ::GetDeltaxy(const ColorXYZ& refColor, const CColorReference& colorReference) const
{
	ColorxyY xyY(*this);
    ColorxyY xyYRef(refColor);
    if(!xyYRef.isValid())
    {
        xyYRef = ColorxyY(colorReference.GetWhite());
    }
    double x=xyY[0];
    double y=xyY[1];
    double xRef=xyYRef[0];
    double yRef=xyYRef[1];
    double dxy = sqrt ( pow((x - xRef),2) + pow((y - yRef),2) );
    return dxy;
}

////////////////////////////////////////////////////////////////////
// implementation of the ColorxyY class.
//////////////////////////////////////////////////////////////////////
ColorxyY::ColorxyY()
{
}

ColorxyY::ColorxyY(const Matrix& matrix) :
    ColorTriplet(matrix)
{
}

ColorxyY::ColorxyY(const ColorXYZ& XYZ)
{
    if(XYZ.isValid())
    {
        double sum = XYZ[0] + XYZ[1] + XYZ[2];
        if(sum > 0.0)
        {
            (*this)[0] = XYZ[0] / sum;
            (*this)[1] = XYZ[1] / sum;
            (*this)[2] = XYZ[1];
        }
    }
}

ColorxyY::ColorxyY(double x, double y, double YY) :
    ColorTriplet(x, y, YY)
{
}

////////////////////////////////////////////////////////////////////
// implementation of the Colorxyz class.
//////////////////////////////////////////////////////////////////////
Colorxyz::Colorxyz()
{
}

Colorxyz::Colorxyz(const Matrix& matrix) :
    ColorTriplet(matrix)
{
}

Colorxyz::Colorxyz(const ColorXYZ& XYZ)
{
    if(XYZ.isValid())
    {
        double sum = XYZ[0] + XYZ[1] + XYZ[2];
        if(sum > 0.0)
        {
            (*this)[0] = XYZ[0] / sum;
            (*this)[1] = XYZ[1] / sum;
            (*this)[2] = XYZ[2] / sum;
        }
    }
}

Colorxyz::Colorxyz(double x, double y, double z) :
    ColorTriplet(x, y, z)
{
}

////////////////////////////////////////////////////////////////////
// implementation of the ColorRGB class.
//////////////////////////////////////////////////////////////////////
ColorRGB::ColorRGB()
{
}

ColorRGB::ColorRGB(const Matrix& matrix) :
    ColorTriplet(matrix)
{
}

ColorRGB::ColorRGB(const ColorXYZ& XYZ, CColorReference colorReference)
{
    if(XYZ.isValid())
    {
        CLockWhileInScope dummy(m_matrixSection);
        *this = ColorRGB(colorReference.XYZtoRGBMatrix*XYZ);
    }
}

ColorRGB::ColorRGB(double r, double g, double b) :
    ColorTriplet(r, g, b)
{
}

////////////////////////////////////////////////////////////////////
// implementation of the ColorLab class.
//////////////////////////////////////////////////////////////////////
ColorLab::ColorLab()
{
}

ColorLab::ColorLab(const Matrix& matrix) :
    ColorTriplet(matrix)
{
}

ColorLab::ColorLab(const ColorXYZ& XYZ, double YWhiteRef, CColorReference colorReference)
{
    if(XYZ.isValid())
    {
        double scaling = YWhiteRef/colorReference.GetWhite()[1];
        double var_X = XYZ[0]/(colorReference.GetWhite()[0]*scaling);
        double var_Y = XYZ[1]/(colorReference.GetWhite()[1]*scaling);
        double var_Z = XYZ[2]/(colorReference.GetWhite()[2]*scaling);
        const double epsilon(216.0 / 24389.0);
        const double kappa(24389.0 / 27.0);

        if ( var_X > epsilon )
        {
            var_X = pow(var_X,1.0/3.0);
        }
        else
        {
            var_X = (kappa*var_X + 16.0) / 116.0;
        }

        if (var_Y > epsilon)
        {
            var_Y = pow(var_Y,1.0/3.0);
        }
        else
        {
            var_Y = (kappa*var_Y + 16.0) / 116.0;
        }

        if (var_Z > epsilon)
        {
            var_Z = pow(var_Z,1.0/3.0);
        }
        else
        {
            var_Z = (kappa*var_Z + 16.0) / 116.0;
        }

        (*this)[0] = 116.0*var_Y-16.0;	 // CIE-L*
        (*this)[1] = 500.0*(var_X-var_Y); // CIE-a*
        (*this)[2] = 200.0*(var_Y-var_Z); //CIE-b*
    }
}

ColorLab::ColorLab(double l, double a, double b) :
    ColorTriplet(l, a, b)
{
}

//////////////////////////////////////////////////////////////////////
// implementation of the ColorICtCp class.
//////////////////////////////////////////////////////////////////////
ColorICtCp::ColorICtCp()
{
}

ColorICtCp::ColorICtCp(const Matrix& matrix) :
    ColorTriplet(matrix)
{
}

ColorICtCp::ColorICtCp(const ColorXYZ& XYZ, double YWhiteRef, CColorReference colorReference)
{
    if(XYZ.isValid())
    {
//		double scaling = YWhiteRef / XYZ[1];
		double scaling = YWhiteRef;
        double var_X = XYZ[0] * scaling ;
        double var_Y = XYZ[1] * scaling ;
        double var_Z = XYZ[2] * scaling ;

        double L = 0.3593 * var_X + 0.6976 * var_Y - 0.0359 * var_Z;
        double M = -0.1921 * var_X + 1.1005 * var_Y + 0.0754 * var_Z;
        double S = 0.0071 * var_X + 0.0748 * var_Y + 0.8433 * var_Z;
		L = min(max(L,0),10000.);
		M = min(max(M,0),10000.);
		S = min(max(S,0),10000.);

		double Lp = getL_EOTF(L / 10000., noDataColor, noDataColor, 0.0, 0.0, -5);
		double Mp = getL_EOTF(M / 10000., noDataColor, noDataColor, 0.0, 0.0, -5);
		double Sp = getL_EOTF(S / 10000., noDataColor, noDataColor, 0.0, 0.0, -5);
		Lp = min(max(Lp,0),1.0);
		Mp = min(max(Mp,0),1.0);
		Sp = min(max(Sp,0),1.0);

        (*this)[0] = (0.5 * Lp + 0.5 * Mp);	 // I
        (*this)[1] = (6610. * Lp - 13613. * Mp + 7003. * Sp) / 4096.; // Ct
        (*this)[2] = (17933. * Lp - 17390. * Mp - 543. * Sp) / 4096.; // Cp
    }
}

ColorICtCp::ColorICtCp(double I, double Ct, double Cp) :
    ColorTriplet(I, Ct, Cp)
{
}

////////////////////////////////////////////////////////////////////
// implementation of the ColorLuv class.
//////////////////////////////////////////////////////////////////////
ColorLuv::ColorLuv()
{
}

ColorLuv::ColorLuv(const Matrix& matrix) :
    ColorTriplet(matrix)
{
}

ColorLuv::ColorLuv(const ColorXYZ& XYZ, double YWhiteRef, CColorReference colorReference)
{
    if(XYZ.isValid())
    {
        ColorxyY white(colorReference.GetWhite());
        double scaling = YWhiteRef/white[2];
        const double epsilon(216.0 / 24389.0);
        const double kappa(24389.0 / 27.0);

        double var_Y = XYZ[1]/(white[2]*scaling);

        if(var_Y <= 0.0)
        {
            (*this)[0] = 0.0;
            (*this)[1] = 0.0;
            (*this)[2] = 0.0;
            return;
        }

        if (var_Y > epsilon)
        {
            var_Y = pow(var_Y,1.0/3.0);
        }
        else
        {
            var_Y = (kappa * var_Y + 16.0)/116.0;
        }
        ColorxyY xyY(XYZ);
        double u = 4.0*xyY[0] / (-2.0*xyY[0] + 12.0*xyY[0] + 3.0); 
        double v = 9.0*xyY[1] / (-2.0*xyY[0] + 12.0*xyY[1] + 3.0);
        double u_white = 4.0*white[0] / (-2.0*white[0] + 12.0*white[0] + 3.0); 
        double v_white = 9.0*white[1] / (-2.0*white[0] + 12.0*white[1] + 3.0); 
        (*this)[0] = 116.0*var_Y-16.0;	 // CIE-L*
        (*this)[1] = 13.0 * (*this)[0] * (u - u_white);
        (*this)[2] = 13.0 * (*this)[0] * (v - v_white);
    }
}

ColorLuv::ColorLuv(double l, double a, double b) :
    ColorTriplet(l, a, b)
{
}

////////////////////////////////////////////////////////////////////
// implementation of the ColorLCH class.
//////////////////////////////////////////////////////////////////////
ColorLCH::ColorLCH()
{
}

ColorLCH::ColorLCH(const Matrix& matrix) :
    ColorTriplet(matrix)
{
}

ColorLCH::ColorLCH(const ColorXYZ& XYZ, double YWhiteRef, CColorReference colorReference)
{
    if(XYZ.isValid())
    {
        ColorLab Lab(XYZ, YWhiteRef, colorReference);

        double L = Lab[0];
        double C = sqrt ( (Lab[1]*Lab[1]) + (Lab[2]*Lab[2]) );
        double H = atan2 ( Lab[2], Lab[1] ) / acos(-1.0) * 180.0;

        if ( H < 0.0 )
        {
            H += 360.0;
        }

        (*this)[0] = L;
        (*this)[1] = C;
        (*this)[2] = H;
    }
}

ColorLCH::ColorLCH(double l, double c, double h) :
    ColorTriplet(l, c, h)
{
}

/////////////////////////////////////////////////////////////////////
// ColorMeasure.cpp: implementation of the CColor class.
//
//////////////////////////////////////////////////////////////////////

CColor::CColor():m_XYZValues(1.0,3,1)
{
	m_pSpectrum = NULL;
	m_pLuxValue = NULL;
}

CColor::CColor(const CColor &aColor):m_XYZValues(aColor.m_XYZValues)
{
	m_pSpectrum = NULL;
	m_pLuxValue = NULL;

	if ( aColor.m_pSpectrum )
		m_pSpectrum = new CSpectrum ( *aColor.m_pSpectrum );

	if ( aColor.m_pLuxValue )
		m_pLuxValue = new double ( *aColor.m_pLuxValue );
}

CColor::CColor(const ColorXYZ& aMatrix):m_XYZValues(aMatrix)
{
	assert(aMatrix.GetColumns() == 1);
	assert(aMatrix.GetRows() == 3);

	m_pSpectrum = NULL;
	m_pLuxValue = NULL;
}

CColor::CColor(double ax,double ay):m_XYZValues(0.0,3,1)
{
    ColorxyY tempColor(ax,ay,1);
    SetxyYValue(tempColor);

	m_pSpectrum = NULL;
	m_pLuxValue = NULL;
}

CColor::CColor(double aX,double aY, double aZ):m_XYZValues(0.0,3,1)
{
	SetX(aX);
	SetY(aY);
	SetZ(aZ);

	m_pSpectrum = NULL;
	m_pLuxValue = NULL;
}

CColor::CColor(ifstream &theFile):m_XYZValues(0.0,3,1)
{
    uint32_t  version = 0;

    m_pSpectrum = NULL;
    m_pLuxValue = NULL;

    theFile.read((char*)&version, 4);
    version = littleEndianUint32ToHost(version);

    // les données XYZ (on utilise le constructeur de Matrix)
    m_XYZValues = ColorXYZ(Matrix(theFile));

    Matrix XYZtoSensorMatrix = Matrix(theFile);
    Matrix SensorToXYZMatrix = Matrix(theFile);
    if ( version == 2 || version == 4 || version == 5 || version == 6 )
        m_pSpectrum = new CSpectrum (theFile, (version == 2 || version == 4));
    if ( version == 3 || version == 4 || version == 6 )
    {
        double readValue;
        theFile.read((char*)&readValue, 8);
        readValue = littleEndianDoubleToHost(readValue);
        m_pLuxValue = new double(readValue);
    }
}
CColor::~CColor()
{
	if ( m_pSpectrum )
	{
		delete m_pSpectrum;
		m_pSpectrum = NULL;
	}

	if ( m_pLuxValue )
	{
		delete m_pLuxValue;
		m_pLuxValue = NULL;
	}
}

CColor& CColor::operator =(const CColor& aColor)
{
	if(&aColor == this)
		return *this;

	m_XYZValues = aColor.m_XYZValues;

	if ( m_pSpectrum )
	{
		delete m_pSpectrum;
		m_pSpectrum = NULL;
	}

	if ( aColor.m_pSpectrum )
		m_pSpectrum = new CSpectrum ( *aColor.m_pSpectrum );

	if ( m_pLuxValue )
	{
		delete m_pLuxValue;
		m_pLuxValue = NULL;
	}

	if ( aColor.m_pLuxValue )
		m_pLuxValue = new double ( *aColor.m_pLuxValue );

	return *this;
}

const double & CColor::operator[](const int nRow) const
{ 
	assert(nRow < m_XYZValues.GetRows());
	return m_XYZValues[nRow];
}

double & CColor::operator[](const int nRow)
{ 
	assert(nRow < m_XYZValues.GetRows());
	return m_XYZValues[nRow];
}

double CColor::GetLuminance() const
{
	return GetY();
}

double CColor::GetDeltaE(const CColor & refColor) const
{
    return m_XYZValues.GetOldDeltaE(refColor.m_XYZValues);
}

double CColor::GetDeltaE(double YWhite, const CColor & refColor, double YWhiteRef, const CColorReference & colorReference, int dE_form, bool isGS, int gw_Weight ) const
{
		return m_XYZValues.GetDeltaE(YWhite, refColor.m_XYZValues, YWhiteRef, colorReference, dE_form, isGS, gw_Weight );
}

double CColor::GetDeltaLCH(double YWhite, const CColor & refColor, double YWhiteRef, const CColorReference & colorReference, int dE_form, bool isGS, int gw_Weight, double &dC, double &dH ) const
{
		return m_XYZValues.GetDeltaLCH(YWhite, refColor.m_XYZValues, YWhiteRef, colorReference, dE_form, isGS, gw_Weight, dC, dH );
}

double CColor::GetDeltaxy(const CColor & refColor, const CColorReference& colorReference) const
{
    return m_XYZValues.GetDeltaxy(refColor.m_XYZValues, colorReference);
}

ColorXYZ CColor::GetXYZValue() const 
{
	return m_XYZValues;
}

ColorRGB CColor::GetRGBValue(CColorReference colorReference) const 
{
	if(isValid())
	{
		CColorReference aColorRef=CColorReference(HDTV, D65);
        CLockWhileInScope dummy(m_matrixSection);
		return ColorRGB((colorReference.m_standard==HDTVb||colorReference.m_standard==HDTVa)?aColorRef.XYZtoRGBMatrix*(m_XYZValues):colorReference.XYZtoRGBMatrix*(m_XYZValues));
	}
	else
    {
        return ColorRGB();
    }
}

ColorxyY CColor::GetxyYValue() const
{
    return ColorxyY(m_XYZValues);
}

Colorxyz CColor::GetxyzValue() const
{
    return Colorxyz(m_XYZValues);
}

ColorLab CColor::GetLabValue(double YWhiteRef, CColorReference colorReference) const 
{
    return ColorLab(m_XYZValues, YWhiteRef, colorReference);
}

ColorLCH CColor::GetLCHValue(double YWhiteRef, CColorReference colorReference) const 
{
    return ColorLCH(m_XYZValues, YWhiteRef, colorReference);
}

void CColor::SetXYZValue(const ColorXYZ& aColor) 
{
    m_XYZValues = aColor;
}

void CColor::SetRGBValue(const ColorRGB& aColor, CColorReference colorReference) 
{
    if(!aColor.isValid())
    {
		m_XYZValues = ColorXYZ();
        return;
    }
    CLockWhileInScope dummy(m_matrixSection);
	CColorReference aColorRef=CColorReference(HDTV);
	m_XYZValues = ColorXYZ( (colorReference.m_standard==HDTVb||colorReference.m_standard==CC6||colorReference.m_standard==HDTVa)?aColorRef.RGBtoXYZMatrix*aColor:colorReference.RGBtoXYZMatrix*aColor);
}

void CColor::SetxyYValue(double x, double y, double Y) 
{
    ColorxyY xyY(x, y, Y);
    m_XYZValues = ColorXYZ(xyY);
}

void CColor::SetxyYValue(const ColorxyY& xyY)
{
    m_XYZValues = ColorXYZ(xyY);
}

bool CColor::isValid() const
{
    return m_XYZValues.isValid();
}

void CColor::SetSpectrum ( CSpectrum & aSpectrum )
{
	if ( m_pSpectrum )
	{
		delete m_pSpectrum;
		m_pSpectrum = NULL;
	}

	m_pSpectrum = new CSpectrum ( aSpectrum );
}

void CColor::ResetSpectrum ()
{
	if ( m_pSpectrum )
	{
		delete m_pSpectrum;
		m_pSpectrum = NULL;
	}
}

bool CColor::HasSpectrum () const
{
	return ( m_pSpectrum != NULL );
}

CSpectrum CColor::GetSpectrum () const
{
	if ( m_pSpectrum )
		return CSpectrum ( *m_pSpectrum );
	else
		return CSpectrum ( 0, 0, 0, 0 );
}

void CColor::SetLuxValue ( double LuxValue )
{
	if ( m_pLuxValue == NULL )
		m_pLuxValue = new double;

	* m_pLuxValue = LuxValue;
}

void CColor::ResetLuxValue ()
{
	if ( m_pLuxValue )
	{
		delete m_pLuxValue;
		m_pLuxValue = NULL;
	}
}

bool CColor::HasLuxValue () const
{
	return ( m_pLuxValue != NULL );
}

double CColor::GetLuxValue () const
{
	return ( m_pLuxValue ? (*m_pLuxValue) : 0.0 );

}

double CColor::GetLuxOrLumaValue (const int luminanceCurveMode) const
{
	// Return Luxmeter value when option authorize it and luxmeter value exists.
	return ( luminanceCurveMode == 1 && m_pLuxValue ? (*m_pLuxValue) : GetY() );
}

double CColor::GetPreferedLuxValue (bool preferLuxmeter) const
{
	// Return Luxmeter value when option authorize it and luxmeter value exists.
	return ( preferLuxmeter && m_pLuxValue ? (*m_pLuxValue) : GetY() );
}

void CColor::applyAdjustmentMatrix(const Matrix& adjustment)
{
    if(m_XYZValues.isValid())
    {
        ColorXYZ newValue(adjustment * m_XYZValues);
        m_XYZValues = newValue;
    }
}

void CColor::Output(ostream& ostr) const
{
    m_XYZValues.Output(ostr);
}

ostream& operator <<(ostream& ostr, const CColor& obj)
{
    obj.Output(ostr);
    return ostr;
}

#ifdef LIBHCFR_HAS_MFC
void CColor::Serialize(CArchive& archive)
{
	if (archive.IsStoring())
	{
		int version=1;

		if ( m_pLuxValue )
		{
			if ( m_pSpectrum )
				version = 6;
			else
				version = 3;
		}
		else if ( m_pSpectrum )
		{
			version = 5;
		}

		archive << version;
		m_XYZValues.Serialize(archive) ;

        Matrix ignore(0.0, 3, 3);
		ignore.Serialize(archive);
		ignore.Serialize(archive);

		if ( m_pSpectrum )
		{
			archive << m_pSpectrum -> GetRows ();
			archive << m_pSpectrum -> m_WaveLengthMin;
			archive << m_pSpectrum -> m_WaveLengthMax;
			archive << m_pSpectrum -> m_BandWidth;
			m_pSpectrum -> Serialize(archive);
		}

		if ( m_pLuxValue )
			archive << (* m_pLuxValue);
	}
	else
	{
		int version;
		archive >> version;
		
		if ( version > 6 )
			AfxThrowArchiveException ( CArchiveException::badSchema );
		
		m_XYZValues.Serialize(archive) ;
		Matrix ignore(0.0, 3, 3);
		ignore.Serialize(archive);
		ignore.Serialize(archive);

		if ( m_pSpectrum )
		{
			delete m_pSpectrum;
			m_pSpectrum = NULL;
		}

		if ( m_pLuxValue )
		{
			delete m_pLuxValue;
			m_pLuxValue = NULL;
		}

		if ( version == 2 || version == 4 || version == 5 || version == 6 )
		{
			int NbBands, WaveLengthMin, WaveLengthMax;
			double dBandWidth;
			archive >> NbBands;
			archive >> WaveLengthMin;
			archive >> WaveLengthMax;
			if ( version == 2 || version == 4 )
			{
				int nBandWidth;
				archive >> nBandWidth;
				dBandWidth = nBandWidth;
			}
			else
			{
				archive >> dBandWidth;
			}
			m_pSpectrum = new CSpectrum ( NbBands, WaveLengthMin, WaveLengthMax, dBandWidth );
			m_pSpectrum -> Serialize(archive);
		}

		if ( version == 3 || version == 4 || version == 6 )
		{
			m_pLuxValue = new double;
			archive >> (* m_pLuxValue);
		}
	}
}
#endif

/////////////////////////////////////////////////////////////////////
// Implementation of the CSpectrum class.

CSpectrum::CSpectrum(int NbBands, int WaveLengthMin, int WaveLengthMax, double BandWidth) : Matrix(0.0,NbBands,1)
{
	m_WaveLengthMin	= WaveLengthMin;
	m_WaveLengthMax = WaveLengthMax;
	m_BandWidth = BandWidth;    
}

CSpectrum::CSpectrum(int NbBands, int WaveLengthMin, int WaveLengthMax, double BandWidth, double * pValues) : Matrix(0.0,NbBands,1)
{
	m_WaveLengthMin	= WaveLengthMin;
	m_WaveLengthMax = WaveLengthMax;
	m_BandWidth = BandWidth;    

	for ( int i = 0; i < NbBands ; i ++ )
		Matrix::operator()(i,0) = pValues [ i ];
}

CSpectrum::CSpectrum (ifstream &theFile, bool oldFileFormat):Matrix()
{
  uint32_t NbBands, WaveLengthMin, WaveLengthMax;
  theFile.read((char*)&NbBands, 4);
  NbBands = littleEndianUint32ToHost(NbBands);
  theFile.read((char*)&WaveLengthMin, 4);
  m_WaveLengthMin = littleEndianUint32ToHost(WaveLengthMin);
  theFile.read((char*)&WaveLengthMax, 4);
  m_WaveLengthMax = littleEndianUint32ToHost(WaveLengthMax);
  if(oldFileFormat)
  {
      uint32_t nBandwidth;
      theFile.read((char*)&nBandwidth, 4);
      m_BandWidth = littleEndianUint32ToHost(nBandwidth);
  }
  else
  {
      double value;
      theFile.read((char*)&value, 8);
      m_BandWidth = littleEndianDoubleToHost(value);
  }
  
  // on saute la version (toujours a 1)
  theFile.seekg(4, ios::cur);
  
  Matrix::readFromFile(theFile);
  
}

const double & CSpectrum::operator[](const int nRow) const
{ 
	assert(nRow < GetRows());
	return Matrix::operator ()(nRow,0); 
}

CColor CSpectrum::GetXYZValue() const
{
	// Not implemented yet
	return noDataColor;
}

#ifdef LIBHCFR_HAS_MFC
void CSpectrum::Serialize(CArchive& archive)
{
	if (archive.IsStoring())
	{
		int version=1;
		archive << version;
		Matrix::Serialize(archive);
	}
	else
	{
		int version;
		archive >> version;
		if ( version > 1 )
			AfxThrowArchiveException ( CArchiveException::badSchema );
		Matrix::Serialize(archive);
	}
}
#endif

bool GenerateCC24Colors (const CColorReference& colorReference, ColorRGBDisplay* GenColors, int aCCMode, int mode)
{
	//six cases, one for GCD sequence, one for Mascior's disk (Chromapure based), and four different generator only cases
	//GCD
	//MCD
    //CCGS 96 CalMAN ColorChecker SG patterns
    //USER user defined
	char * path;
	char appPath[255];
	path = getenv("APPDATA");
	strcpy(appPath, path);
	strcat(appPath, "\\color");
	bool bOk = true, constant_XYZ = FALSE, m_bRecalc = FALSE;
	int n_elements=24;
	switch (aCCMode)
	{
	case GCD:
		{
//GCD
			constant_XYZ = TRUE;
			m_bRecalc = TRUE;
			GenColors [ 0 ] = ColorRGBDisplay( 0, 0, 0 );
            GenColors [ 1 ] = ColorRGBDisplay( 62.1, 62.1, 62.1 );
            GenColors [ 2 ] = ColorRGBDisplay( 73.06, 73.06, 73.06 );
            GenColors [ 3 ] = ColorRGBDisplay( 82.19, 82.19, 82.19 );
            GenColors [ 4 ] = ColorRGBDisplay( 89.95, 89.95, 89.95 );
            GenColors [ 5 ] = ColorRGBDisplay( 100, 100, 100 );
            GenColors [ 6 ] = ColorRGBDisplay( 45.2, 31.96, 26.03 );
            GenColors [ 7 ] = ColorRGBDisplay( 75.8, 58.9, 51.14 );
            GenColors [ 8 ] = ColorRGBDisplay( 36.99, 47.95, 61.19 );
            GenColors [ 9 ] = ColorRGBDisplay( 35.16,42.01,26.03);
            GenColors [ 10 ] = ColorRGBDisplay( 51.14,50.23,68.95);
            GenColors [ 11 ] = ColorRGBDisplay( 38.81,73.97,66.21 );
            GenColors [ 12 ] = ColorRGBDisplay( 84.93,47.03,15.98);
            GenColors [ 13 ] = ColorRGBDisplay( 29.22,36.07,63.93);
            GenColors [ 14 ] = ColorRGBDisplay( 75.80, 32.88,37.90 );
            GenColors [ 15 ] = ColorRGBDisplay( 36.07, 24.20, 42.01);
            GenColors [ 16 ] = ColorRGBDisplay( 62.10, 73.06, 25.11 );
            GenColors [ 17 ] = ColorRGBDisplay( 89.95, 63.01, 17.81 );
            GenColors [ 18 ] = ColorRGBDisplay( 20.09, 24.20, 58.90);
            GenColors [ 19 ] = ColorRGBDisplay( 27.85, 57.99, 27.85);
            GenColors [ 20 ] = ColorRGBDisplay( 68.95, 19.18, 22.83);
            GenColors [ 21 ] = ColorRGBDisplay( 93.15, 78.08, 12.79 );
            GenColors [ 22 ] = ColorRGBDisplay( 73.06, 32.88, 57.08);
            GenColors [ 23 ] = ColorRGBDisplay( 0, 52.05, 63.93);
        break;
		}
	case MCD:
		{
			constant_XYZ = TRUE;
			m_bRecalc = TRUE;
	 	    GenColors [ 23 ] = ColorRGBDisplay( 21, 20.5, 21 );
            GenColors [ 22 ] = ColorRGBDisplay( 32.88, 32.88, 32.88 );
            GenColors [ 21 ] = ColorRGBDisplay( 47.49, 47.49, 47.03 );
            GenColors [ 20 ] = ColorRGBDisplay( 62.56, 62.56, 62.56 );
            GenColors [ 19 ] = ColorRGBDisplay( 78.54, 78.54, 78.08 );
            GenColors [ 18 ] = ColorRGBDisplay( 94.98, 94.52, 92.69 );
            GenColors [ 0 ] = ColorRGBDisplay( 44.74, 31.51, 26.03 );
            GenColors [ 1 ] = ColorRGBDisplay( 75.80, 58.45, 50.68 );
            GenColors [ 2 ] = ColorRGBDisplay( 36.99, 47.95, 60.73 );
            GenColors [ 3 ] = ColorRGBDisplay( 34.70,42.47,26.48);
            GenColors [ 4 ] = ColorRGBDisplay( 50.68,50.22,68.49);
            GenColors [ 5 ] = ColorRGBDisplay( 39.27,73.97,66.21 );
            GenColors [ 6 ] = ColorRGBDisplay( 84.47,47.49,16.44);
            GenColors [ 7 ] = ColorRGBDisplay( 28.77,35.62,64.38);
            GenColors [ 8 ] = ColorRGBDisplay( 75.80, 33.33,38.36 );
            GenColors [ 9 ] = ColorRGBDisplay( 36.07, 24.20, 42.01);
            GenColors [ 10 ] = ColorRGBDisplay( 62.10, 73.06, 24.66 );
            GenColors [ 11 ] = ColorRGBDisplay( 89.95, 63.47, 18.26 );
            GenColors [ 12 ] = ColorRGBDisplay( 19.63, 24.20, 59.36);
            GenColors [ 13 ] = ColorRGBDisplay( 28.31, 57.99, 27.85);
            GenColors [ 14 ] = ColorRGBDisplay( 68.95, 19.18, 22.83);
            GenColors [ 15 ] = ColorRGBDisplay( 93.15, 78.08, 12.79 );
            GenColors [ 16 ] = ColorRGBDisplay( 72.61, 32.42, 57.53);
            GenColors [ 17 ] = ColorRGBDisplay( 11.87, 51.60, 59.82);
		break;
		}
	case CMC:
		{
			constant_XYZ = TRUE;
			m_bRecalc = TRUE;
            GenColors [ 0 ] = ColorRGBDisplay( 100,	100,	100);
            GenColors [ 1 ] = ColorRGBDisplay( 89.9543379,	89.9543379,	89.9543379);
            GenColors [ 2 ] = ColorRGBDisplay( 82.19178082,	82.19178082,	82.19178082);
            GenColors [ 3 ] = ColorRGBDisplay( 73.05936073,	73.05936073,	73.05936073);
            GenColors [ 4 ] = ColorRGBDisplay( 62.10045662,	62.10045662,	62.10045662);
            GenColors [ 5 ] = ColorRGBDisplay( 0,	0,	0);
            GenColors [ 6 ] = ColorRGBDisplay( 45.20547945,	31.96347032,	26.02739726);
            GenColors [ 7 ] = ColorRGBDisplay( 75.79908676,	58.90410959,	51.14155251);
            GenColors [ 8 ] = ColorRGBDisplay( 36.98630137,	47.94520548,	61.18721461);
            GenColors [ 9 ] = ColorRGBDisplay( 35.15981735,	42.00913242,	26.02739726);
            GenColors [ 10 ] = ColorRGBDisplay( 51.14155251,	50.2283105,	68.94977169);
            GenColors [ 11 ] = ColorRGBDisplay( 38.81278539,	73.97260274,	66.21004566);
            GenColors [ 12 ] = ColorRGBDisplay( 84.93150685,	47.03196347,	15.98173516);
            GenColors [ 13 ] = ColorRGBDisplay( 29.22374429,	36.07305936,	63.92694064);
            GenColors [ 14 ] = ColorRGBDisplay( 75.79908676,	32.87671233,	37.89954338);
            GenColors [ 15 ] = ColorRGBDisplay( 36.07305936,	24.20091324,	42.00913242);
            GenColors [ 16 ] = ColorRGBDisplay( 62.10045662,	73.05936073,	25.11415525);
            GenColors [ 17 ] = ColorRGBDisplay( 89.9543379,	63.01369863,	17.80821918);
            GenColors [ 18 ] = ColorRGBDisplay( 20.0913242,	24.20091324,	58.90410959);
            GenColors [ 19 ] = ColorRGBDisplay( 27.85388128,	57.99086758,	27.85388128);
            GenColors [ 20 ] = ColorRGBDisplay( 68.94977169,	19.17808219,	22.83105023);
            GenColors [ 21 ] = ColorRGBDisplay( 93.15068493,	78.08219178,	12.78538813);
            GenColors [ 22 ] = ColorRGBDisplay( 73.05936073,	32.87671233,	57.07762557);
            GenColors [ 23 ] = ColorRGBDisplay( 0,	52.05479452,	63.92694064);		
        break;
		}
		//CalMan classic
	case CMS:
		{
			constant_XYZ = TRUE;
			m_bRecalc = TRUE;
            GenColors [ 0 ] = ColorRGBDisplay( 100, 100, 100 );
            GenColors [ 1 ] = ColorRGBDisplay( 0, 0, 0 );
            GenColors [ 2 ] = ColorRGBDisplay( 43.83561644,	25.11415525,	15.06849315 );
            GenColors [ 3 ] = ColorRGBDisplay( 79.9086758,	53.88127854,	40.1826484);
            GenColors [ 4 ] = ColorRGBDisplay( 100,	78.08219178,	59.8173516);
            GenColors [ 5 ] = ColorRGBDisplay( 100,	78.08219178,	67.12328767);
            GenColors [ 6 ] = ColorRGBDisplay( 96.80365297,	67.12328767,	48.85844749);
            GenColors [ 7 ] = ColorRGBDisplay( 78.08219178,	54.79452055,	36.07305936);
            GenColors [ 8 ] = ColorRGBDisplay( 56.16438356,	36.07305936,	20.0913242);
			GenColors [ 9 ] = ColorRGBDisplay( 80.82191781,	58.90410959,	45.20547945);
            GenColors [ 10 ] = ColorRGBDisplay( 63.01369863,	33.78995434,	12.78538813);
            GenColors [ 11 ] = ColorRGBDisplay( 84.01826484,	52.05479452,	36.07305936);
            GenColors [ 12 ] = ColorRGBDisplay( 82.19178082,	53.88127854,	41.09589041);
            GenColors [ 13 ] = ColorRGBDisplay( 98.17351598,	59.8173516,	45.20547945);
            GenColors [ 14 ] = ColorRGBDisplay( 78.08219178,	56.16438356,	42.00913242);
            GenColors [ 15 ] = ColorRGBDisplay( 78.99543379,	54.79452055,	42.00913242);
            GenColors [ 16 ] = ColorRGBDisplay( 79.9086758,	56.16438356,	41.09589041);
            GenColors [ 17 ] = ColorRGBDisplay( 47.94520548,	29.22374429,	15.06849315);
            GenColors [ 18 ] = ColorRGBDisplay( 84.93150685,	54.79452055,	36.98630137);
			n_elements= 19;
        break;
		}
		//CalMAN skin
	case CPS:
		{
			constant_XYZ = TRUE;
			m_bRecalc = TRUE;
            GenColors [ 0 ] = ColorRGBDisplay( 100, 100, 100 );
            GenColors [ 1 ] = ColorRGBDisplay( 84.47488584,	49.31506849,	34.24657534);
            GenColors [ 2 ] = ColorRGBDisplay( 78.99543379,	55.25114155,	48.40182648);
            GenColors [ 3 ] = ColorRGBDisplay( 93.60730594,	67.57990868,	57.99086758);
            GenColors [ 4 ] = ColorRGBDisplay( 94.52054795,	61.18721461,	52.96803653);
            GenColors [ 5 ] = ColorRGBDisplay( 75.34246575,	56.16438356,	42.92237443);
            GenColors [ 6 ] = ColorRGBDisplay( 74.88584475,	57.07762557,	49.31506849);
            GenColors [ 7 ] = ColorRGBDisplay( 55.25114155,	36.52968037,	24.65753425);
            GenColors [ 8 ] = ColorRGBDisplay( 76.25570776,	56.62100457,	49.7716895);
			GenColors [ 9 ] = ColorRGBDisplay( 75.79908676,	58.90410959,	51.14155251);
            GenColors [ 10 ] = ColorRGBDisplay( 77.16894977,	56.62100457,	48.85844749);
            GenColors [ 11 ] = ColorRGBDisplay( 62.10045662,	34.24657534,	17.80821918);
            GenColors [ 12 ] = ColorRGBDisplay( 47.03196347,	29.6803653,	18.72146119);
            GenColors [ 13 ] = ColorRGBDisplay( 81.73515982,	52.96803653,	42.46575342);
            GenColors [ 14 ] = ColorRGBDisplay( 82.64840183,	56.16438356,	43.83561644);
            GenColors [ 15 ] = ColorRGBDisplay( 93.15068493,	68.49315068,	48.40182648);
            GenColors [ 16 ] = ColorRGBDisplay( 81.73515982,	60.2739726,	42.46575342);
            GenColors [ 17 ] = ColorRGBDisplay( 45.20547945,	31.96347032,	26.48401826);
            GenColors [ 18 ] = ColorRGBDisplay( 76.25570776,	58.90410959,	51.14155251);
			n_elements= 19;
        break;
		}
		//Chromapure skin
	case SKIN:
		{
			constant_XYZ = TRUE;
			m_bRecalc = TRUE;
            GenColors [ 0 ] = ColorRGBDisplay(100,87.67123288,76.71232877);
            GenColors [ 1 ] = ColorRGBDisplay(94.06392694,83.56164384,74.42922374);
            GenColors [ 2 ] = ColorRGBDisplay(93.15068493,80.82191781,70.3196347);
            GenColors [ 3 ] = ColorRGBDisplay(88.12785388,72.14611872,59.8173516);
            GenColors [ 4 ] = ColorRGBDisplay(89.9543379,76.25570776,59.8173516);
            GenColors [ 5 ] = ColorRGBDisplay(100,86.30136986,69.8630137);
            GenColors [ 6 ] = ColorRGBDisplay(89.9543379,72.14611872,56.16438356);
            GenColors [ 7 ] = ColorRGBDisplay(89.9543379,62.55707763,45.20547945);
            GenColors [ 8 ] = ColorRGBDisplay(90.4109589,62.10045662,42.92237443);
            GenColors [ 9 ] = ColorRGBDisplay(85.84474886,56.62100457,39.7260274);
            GenColors [ 10 ] = ColorRGBDisplay(80.82191781,58.90410959,48.40182648);
            GenColors [ 11 ] = ColorRGBDisplay(77.62557078,47.03196347,33.78995434);
            GenColors [ 12 ] = ColorRGBDisplay(73.05936073,42.46575342,28.76712329);
            GenColors [ 13 ] = ColorRGBDisplay(64.84018265,44.74885845,34.24657534);
            GenColors [ 14 ] = ColorRGBDisplay(94.06392694,78.53881279,78.99543379);
            GenColors [ 15 ] = ColorRGBDisplay(86.75799087,65.75342466,62.55707763);
            GenColors [ 16 ] = ColorRGBDisplay(72.60273973,48.40182648,42.92237443);
            GenColors [ 17 ] = ColorRGBDisplay(65.75342466,45.66210046,42.46575342);
            GenColors [ 18 ] = ColorRGBDisplay(68.03652968,39.26940639,31.96347032);
            GenColors [ 19 ] = ColorRGBDisplay(36.07305936,21.91780822,21.00456621);
            GenColors [ 20 ] = ColorRGBDisplay(79.45205479,51.59817352,26.02739726);
            GenColors [ 21 ] = ColorRGBDisplay(73.97260274,44.74885845,23.74429224);
            GenColors [ 22 ] = ColorRGBDisplay(43.83561644,25.57077626,22.37442922);
            GenColors [ 23 ] = ColorRGBDisplay(63.92694064,52.51141553,41.55251142);
        break;
		}
	case AXIS:
		{
			m_bRecalc = FALSE;
			GenColors [ 0 ] = ColorRGBDisplay(0,0,0);
			for (int i=0;i<10;i++) {GenColors [ i + 1 ] = ColorRGBDisplay( (i+1) * 10,	(i+1) * 10,	(i+1) * 10);}
			for (int i=0;i<10;i++) {GenColors [ i + 11 ] = ColorRGBDisplay( (i+1) * 10,	0,	0);}
			for (int i=0;i<10;i++) {GenColors [ i + 21 ] = ColorRGBDisplay( 0, (i+1) * 10,	0);}
			for (int i=0;i<10;i++) {GenColors [ i + 31] = ColorRGBDisplay( 0, 0, (i+1) * 10);}
			for (int i=0;i<10;i++) {GenColors [ i + 61 ] = ColorRGBDisplay( (i+1) * 10,	(i+1) * 10,	0);}
			for (int i=0;i<10;i++) {GenColors [ i + 41 ] = ColorRGBDisplay( 0, (i+1) * 10,	(i+1) * 10);}
			for (int i=0;i<10;i++) {GenColors [ i + 51] = ColorRGBDisplay( (i+1) * 10, 0, (i+1) * 10);}
			n_elements= 70;
        break;
		}
		//ColorCheckerSG 96 colors
    case CCSG:
        {
			constant_XYZ = TRUE;
			m_bRecalc = TRUE;
            GenColors [ 0 ] = ColorRGBDisplay(	100	,	100	,	100	);
            GenColors [ 1 ] = ColorRGBDisplay(	87.2146119	,	87.2146119	,	87.2146119	);
            GenColors [ 2 ] = ColorRGBDisplay(	77.1689498	,	77.1689498	,	77.1689498	);
            GenColors [ 3 ] = ColorRGBDisplay(	72.1461187	,	72.1461187	,	72.1461187	);
            GenColors [ 4 ] = ColorRGBDisplay(	67.1232877	,	67.1232877	,	67.1232877	);
            GenColors [ 5 ] = ColorRGBDisplay(	61.1872146	,	61.1872146	,	61.1872146	);
            GenColors [ 6 ] = ColorRGBDisplay(	56.1643836	,	56.1643836	,	56.1643836	);
            GenColors [ 7 ] = ColorRGBDisplay(	46.1187215	,	46.1187215	,	46.1187215	);
            GenColors [ 8 ] = ColorRGBDisplay(	42.0091324	,	42.0091324	,	42.0091324	);
            GenColors [ 9 ] = ColorRGBDisplay(	36.9863014	,	36.9863014	,	36.9863014	);
            GenColors [ 10 ] = ColorRGBDisplay(	32.8767123	,	32.8767123	,	32.8767123	);
            GenColors [ 11 ] = ColorRGBDisplay(	29.2237443	,	29.2237443	,	29.2237443	);
            GenColors [ 12 ] = ColorRGBDisplay(	21.0045662	,	21.0045662	,	21.0045662	);
            GenColors [ 13 ] = ColorRGBDisplay(	16.8949772	,	16.8949772	,	16.8949772	);
            GenColors [ 14 ] = ColorRGBDisplay(	0	,	0	,	0	);
            GenColors [ 15 ] = ColorRGBDisplay(	57.0776256	,	10.9589041	,	32.8767123	);
            GenColors [ 16 ] = ColorRGBDisplay(	29.2237443	,	17.8082192	,	27.8538813	);
            GenColors [ 17 ] = ColorRGBDisplay(	84.9315068	,	82.1917808	,	75.7990868	);
            GenColors [ 18 ] = ColorRGBDisplay(	43.8356164	,	25.1141553	,	15.0684932	);
            GenColors [ 19 ] = ColorRGBDisplay(	79.9086758	,	53.8812785	,	40.1826484	);
            GenColors [ 20 ] = ColorRGBDisplay(	35.1598174	,	43.8356164	,	51.1415525	);
            GenColors [ 21 ] = ColorRGBDisplay(	32.8767123	,	37.8995434	,	11.8721461	);
            GenColors [ 22 ] = ColorRGBDisplay(	50.2283105	,	46.1187215	,	57.0776256	);
            GenColors [ 23 ] = ColorRGBDisplay(	42.0091324	,	70.7762557	,	56.1643836	);
            GenColors [ 24 ] = ColorRGBDisplay(	100	,	78.0821918	,	59.8173516	);
            GenColors [ 25 ] = ColorRGBDisplay(	38.8127854	,	10.9589041	,	15.9817352	);
            GenColors [ 26 ] = ColorRGBDisplay(	74.8858447	,	11.8721461	,	29.2237443	);
            GenColors [ 27 ] = ColorRGBDisplay(	73.9726027	,	50.2283105	,	61.1872146	);
            GenColors [ 28 ] = ColorRGBDisplay(	42.9223744	,	35.1598174	,	53.8812785	);
            GenColors [ 29 ] = ColorRGBDisplay(	99.086758	,	78.0821918	,	70.7762557	);
            GenColors [ 30 ] = ColorRGBDisplay(	90.8675799	,	43.8356164	,	0	);
            GenColors [ 31 ] = ColorRGBDisplay(	24.2009132	,	31.0502283	,	56.1643836	);
            GenColors [ 32 ] = ColorRGBDisplay(	78.0821918	,	25.1141553	,	26.9406393	);
            GenColors [ 33 ] = ColorRGBDisplay(	31.9634703	,	15.0684932	,	31.9634703	);
            GenColors [ 34 ] = ColorRGBDisplay(	66.2100457	,	70.7762557	,	0	);
            GenColors [ 35 ] = ColorRGBDisplay(	91.7808219	,	58.9041096	,	0	);
            GenColors [ 36 ] = ColorRGBDisplay(	84.0182648	,	90.8675799	,	70.7762557	);
            GenColors [ 37 ] = ColorRGBDisplay(	79.9086758	,	0	,	8.2191781	);
            GenColors [ 38 ] = ColorRGBDisplay(	33.7899543	,	12.7853881	,	21.0045662	);
            GenColors [ 39 ] = ColorRGBDisplay(	46.1187215	,	11.8721461	,	43.8356164	);
            GenColors [ 40 ] = ColorRGBDisplay(	0	,	21.0045662	,	35.1598174	);
            GenColors [ 41 ] = ColorRGBDisplay(	73.9726027	,	85.8447489	,	72.1461187	);
            GenColors [ 42 ] = ColorRGBDisplay(	5.0228311	,	16.8949772	,	46.1187215	);
            GenColors [ 43 ] = ColorRGBDisplay(	26.0273973	,	54.7945205	,	15.9817352	);
            GenColors [ 44 ] = ColorRGBDisplay(	69.8630137	,	0	,	10.0456621	);
            GenColors [ 45 ] = ColorRGBDisplay(	96.803653	,	74.8858447	,	0	);
            GenColors [ 46 ] = ColorRGBDisplay(	75.7990868	,	26.0273973	,	47.9452055	);
            GenColors [ 47 ] = ColorRGBDisplay(	0	,	50.2283105	,	54.7945205	);
            GenColors [ 48 ] = ColorRGBDisplay(	90.8675799	,	79.9086758	,	74.8858447	);
            GenColors [ 49 ] = ColorRGBDisplay(	84.0182648	,	46.1187215	,	47.0319635	);
            GenColors [ 50 ] = ColorRGBDisplay(	74.8858447	,	0	,	15.0684932	);
            GenColors [ 51 ] = ColorRGBDisplay(	0	,	48.8584475	,	68.0365297	);
            GenColors [ 52 ] = ColorRGBDisplay(	32.8767123	,	59.8173516	,	68.0365297	);
            GenColors [ 53 ] = ColorRGBDisplay(	100	,	78.0821918	,	67.1232877	);
            GenColors [ 54 ] = ColorRGBDisplay(	74.8858447	,	84.0182648	,	75.7990868	);
            GenColors [ 55 ] = ColorRGBDisplay(	87.2146119	,	46.1187215	,	40.1826484	);
            GenColors [ 56 ] = ColorRGBDisplay(	94.0639269	,	20.0913242	,	15.0684932	);
            GenColors [ 57 ] = ColorRGBDisplay(	19.1780822	,	63.0136986	,	64.8401826	);
            GenColors [ 58 ] = ColorRGBDisplay(	0	,	22.8310502	,	26.9406393	);
            GenColors [ 59 ] = ColorRGBDisplay(	85.8447489	,	83.1050228	,	52.0547945	);
            GenColors [ 60 ] = ColorRGBDisplay(	100	,	40.1826484	,	0	);
            GenColors [ 61 ] = ColorRGBDisplay(	100	,	63.0136986	,	0	);
            GenColors [ 62 ] = ColorRGBDisplay(	0	,	22.8310502	,	20.0913242	);
            GenColors [ 63 ] = ColorRGBDisplay(	46.1187215	,	57.9908676	,	68.9497717	);
            GenColors [ 64 ] = ColorRGBDisplay(	85.8447489	,	47.9452055	,	27.8538813	);
            GenColors [ 65 ] = ColorRGBDisplay(	96.803653	,	67.1232877	,	48.8584475	);
            GenColors [ 66 ] = ColorRGBDisplay(	78.0821918	,	54.7945205	,	36.0730594	);
            GenColors [ 67 ] = ColorRGBDisplay(	56.1643836	,	36.0730594	,	20.0913242	);
            GenColors [ 68 ] = ColorRGBDisplay(	80.8219178	,	58.9041096	,	45.2054795	);
            GenColors [ 69 ] = ColorRGBDisplay(	63.0136986	,	33.7899543	,	12.7853881	);
            GenColors [ 70 ] = ColorRGBDisplay(	84.0182648	,	52.0547945	,	36.0730594	);
            GenColors [ 71 ] = ColorRGBDisplay(	78.0821918	,	69.8630137	,	0	);
            GenColors [ 72 ] = ColorRGBDisplay(	100	,	74.8858447	,	0	);
            GenColors [ 73 ] = ColorRGBDisplay(	0	,	63.0136986	,	56.1643836	);
            GenColors [ 74 ] = ColorRGBDisplay(	0	,	54.7945205	,	46.1187215	);
            GenColors [ 75 ] = ColorRGBDisplay(	82.1917808	,	53.8812785	,	41.0958904	);
            GenColors [ 76 ] = ColorRGBDisplay(	98.173516	,	59.8173516	,	45.2054795	);
            GenColors [ 77 ] = ColorRGBDisplay(	78.0821918	,	56.1643836	,	42.0091324	);
            GenColors [ 78 ] = ColorRGBDisplay(	78.9954338	,	54.7945205	,	42.0091324	);
            GenColors [ 79 ] = ColorRGBDisplay(	79.9086758	,	56.1643836	,	41.0958904	);
            GenColors [ 80 ] = ColorRGBDisplay(	47.9452055	,	29.2237443	,	15.0684932	);
            GenColors [ 81 ] = ColorRGBDisplay(	84.9315068	,	54.7945205	,	36.9863014	);
            GenColors [ 82 ] = ColorRGBDisplay(	72.1461187	,	56.1643836	,	9.1324201	);
            GenColors [ 83 ] = ColorRGBDisplay(	73.0593607	,	69.8630137	,	0	);
            GenColors [ 84 ] = ColorRGBDisplay(	25.1141553	,	21.0045662	,	14.1552511	);
            GenColors [ 85 ] = ColorRGBDisplay(	35.1598174	,	63.0136986	,	35.1598174	);
            GenColors [ 86 ] = ColorRGBDisplay(	0	,	53.8812785	,	31.0502283	);
            GenColors [ 87 ] = ColorRGBDisplay(	11.8721461	,	25.1141553	,	15.9817352	);
            GenColors [ 88 ] = ColorRGBDisplay(	22.8310502	,	63.0136986	,	42.9223744	);
            GenColors [ 89 ] = ColorRGBDisplay(	47.9452055	,	61.1872146	,	21.9178082	);
            GenColors [ 90 ] = ColorRGBDisplay(	20.0913242	,	53.8812785	,	10.0456621	);
            GenColors [ 91 ] = ColorRGBDisplay(	29.2237443	,	66.2100457	,	15.9817352	);
            GenColors [ 92 ] = ColorRGBDisplay(	78.9954338	,	52.0547945	,	16.8949772	);
            GenColors [ 93 ] = ColorRGBDisplay(	62.1004566	,	58.9041096	,	10.0456621	);
            GenColors [ 94 ] = ColorRGBDisplay(	64.8401826	,	73.0593607	,	0	);
            GenColors [ 95 ] = ColorRGBDisplay(	30.1369863	,	16.8949772	,	10.0456621	);
			n_elements= 96;
            break;
        }
    case USER:
        {//read in user defined colors
			m_bRecalc = FALSE;
            char m_ApplicationPath [MAX_PATH];
			LPSTR lpStr;
            GetModuleFileName ( NULL, m_ApplicationPath, sizeof ( m_ApplicationPath ) );
			lpStr = strrchr ( m_ApplicationPath, (int) '\\' );
			lpStr [ 1 ] = '\0';
            CString strPath = m_ApplicationPath;
            ifstream colorFile(strPath+"usercolors.csv");
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 1000 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 - 16) / 219.) * 100	, (	(n2 - 16) / 219.) * 100	,	( (n3 - 16) /219. ) * 100.	);
                cnt++;
            }
			n_elements= cnt;
			}
            break;
        }

	case CM10SAT:
        {//read in user defined colors
			m_bRecalc = TRUE;
			strcat(appPath, "\\CM 10-Point Saturation (100AMP).csv");
			ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 100 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 - 16) / 219.) * 100	, (	(n2 - 16) / 219.) * 100	,	( (n3 - 16) /219. ) * 100.	);
                cnt++;
            }
			n_elements= cnt;
			}
            break;
        }

	case CM10SAT75:
        {//read in user defined colors
			m_bRecalc = TRUE;
			strcat(appPath, "\\CM 10-Point Saturation (75AMP).csv");
            ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 100 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 - 16) / 219.) * 100	, (	(n2 - 16) / 219.) * 100	,	( (n3 - 16) /219. ) * 100.	);
                cnt++;
            }
			n_elements= cnt;
			}
            break;
        }
	
	case CM4LUM:
        {//read in user defined colors
			m_bRecalc = FALSE;
			strcat(appPath, "\\CM 4-Point Luminance.csv");
            ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 100 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 - 16) / 219.) * 100	, (	(n2 - 16) / 219.) * 100	,	( (n3 - 16) /219. ) * 100.	);
                cnt++;
            }
			n_elements= cnt;
			}
            break;
        }

	case CM5LUM:
        {//read in user defined colors
			m_bRecalc = FALSE;
			strcat(appPath, "\\CM 5-Point Luminance.csv");
            ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 100 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 - 16) / 219.) * 100	, (	(n2 - 16) / 219.) * 100	,	( (n3 - 16) /219. ) * 100.	);
                cnt++;
            }
			n_elements= cnt;
			}
            break;
        }
	
	case CM10LUM:
        {//read in user defined colors
			m_bRecalc = FALSE;
			strcat(appPath, "\\CM 10-Point Luminance.csv");
            ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 100 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 - 16) / 219.) * 100	, (	(n2 - 16) / 219.) * 100	,	( (n3 - 16) /219. ) * 100.	);
                cnt++;
            }
			n_elements= cnt;
			}
            break;
        }
	
	case CM4SAT:
        {//read in user defined colors
			m_bRecalc = TRUE;
			strcat(appPath, "\\CM 4-Point Saturation (100AMP).csv");
            ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 100 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 - 16) / 219.) * 100	, (	(n2 - 16) / 219.) * 100	,	( (n3 - 16) /219. ) * 100.	);
                cnt++;
            }
			n_elements= cnt;
			}
            break;
        }
	
	case CM4SAT75:
        {//read in user defined colors
			m_bRecalc = TRUE;
			strcat(appPath, "\\CM 4-Point Saturation (75AMP).csv");
            ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 100 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 - 16) / 219.) * 100	, (	(n2 - 16) / 219.) * 100	,	( (n3 - 16) /219. ) * 100.	);
                cnt++;
            }
			n_elements= cnt;
			}
            break;
        }
	
	case CM5SAT:
        {//read in user defined colors
			m_bRecalc = TRUE;
			strcat(appPath, "\\CM 5-Point Saturation (100AMP).csv");
            ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 100 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 - 16) / 219.) * 100	, (	(n2 - 16) / 219.) * 100	,	( (n3 - 16) /219. ) * 100.	);
                cnt++;
            }
			n_elements= cnt;
			}
            break;
        }
	
	case CM5SAT75:
        {//read in user defined colors
			m_bRecalc = TRUE;
			strcat(appPath, "\\CM 5-Point Saturation (75AMP).csv");
            ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 100 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 - 16) / 219.) * 100	, (	(n2 - 16) / 219.) * 100	,	( (n3 - 16) /219. ) * 100.	);
                cnt++;
            }
			n_elements= cnt;
			}
            break;
        }
	
	case CM6NB:
        {//read in user defined colors
			m_bRecalc = FALSE;
			strcat(appPath, "\\CM 6-Point Near Black.csv");
            ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 100 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 - 16) / 219.) * 100	, (	(n2 - 16) / 219.) * 100	,	( (n3 - 16) /219. ) * 100.	);
                cnt++;
            }
			n_elements= cnt;
			}
            break;
        }

	case MASCIOR50:
        {//read in user defined colors
			strcat(appPath, "\\Mascior50_50_BT2020_HDR.csv");
            ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 100 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 - 16) / 219.) * 100	, (	(n2 - 16) / 219.) * 100	,	( (n3 - 16) /219. ) * 100.	);
                cnt++;
            }
			n_elements= cnt;
			}
            break;
        }

		case LG54017:
        {//read in user defined colors
			strcat(appPath, "\\LG_540_Base_Tone_Curve_2017.csv");
            ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 100 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 - 16) / 219.) * 100	, (	(n2 - 16) / 219.) * 100	,	( (n3 - 16) /219. ) * 100.	);
                cnt++;
            }
			n_elements= cnt;
			}
            break;
        }

		case LG100017:
        {//read in user defined colors
			strcat(appPath, "\\LG_1000_Base_Tone_Curve_2017.csv");
            ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 100 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 - 16) / 219.) * 100	, (	(n2 - 16) / 219.) * 100	,	( (n3 - 16) /219. ) * 100.	);
                cnt++;
            }
			n_elements= cnt;
			}
            break;
        }

		case LG400017:
        {//read in user defined colors
			strcat(appPath, "\\LG_4000_Base_Tone_Curve_2017.csv");
            ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 100 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 - 16) / 219.) * 100	, (	(n2 - 16) / 219.) * 100	,	( (n3 - 16) /219. ) * 100.	);
                cnt++;
            }
			n_elements= cnt;
			}
            break;
        }

		case LG54016:
        {//read in user defined colors
			strcat(appPath, "\\LG_540_Base_Tone_Curve_2016.csv");
            ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 100 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 - 16) / 219.) * 100	, (	(n2 - 16) / 219.) * 100	,	( (n3 - 16) /219. ) * 100.	);
                cnt++;
            }
			n_elements= cnt;
			}
            break;
        }

		case CMDNR:
        {//read in user defined colors
			m_bRecalc = FALSE;
			strcat(appPath, "\\CM Dynamic Range (Clipping).csv");
            ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 100 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 - 16) / 219.) * 100	, (	(n2 - 16) / 219.) * 100	,	( (n3 - 16) /219. ) * 100.	);
                cnt++;
            }
			n_elements = cnt;
			}
            break;
        }

	case RANDOM250:
        {//read in user defined colors
			m_bRecalc = FALSE;
			strcat(appPath, "\\Random_250.csv");
            ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 250 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 - 16) / 219.) * 100	, (	(n2 - 16) / 219.) * 100	,	( (n3 - 16) /219. ) * 100.	);
                cnt++;
            }
			n_elements = cnt;
			}
            break;
        }
	
	case RANDOM500:
        {//read in user defined colors
			m_bRecalc = FALSE;
			strcat(appPath, "\\Random_500.csv");
            ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 500 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 - 16) / 219.) * 100	, (	(n2 - 16) / 219.) * 100	,	( (n3 - 16) /219. ) * 100.	);
                cnt++;
            }
			n_elements = cnt;
			}
            break;
        }
	}//switch

	//HDR mode target recalculation
	//Will recalc color checker and saturation based levels, luminance, random and user left as-is
	if ( (mode == 5 || mode == 7) && m_bRecalc )
	{
		CColor tempColor;
		for (int i=0; i<n_elements; i++)
		{

			//linearize
			double r = (GenColors[i][0]<=0.0||GenColors[i][0]>100.0)?min(max(GenColors[i][0],0),100):pow(GenColors[i][0] / 100.,2.22);
			double g = (GenColors[i][1]<=0.0||GenColors[i][1]>100.0)?min(max(GenColors[i][1],0),100):pow(GenColors[i][1] / 100.,2.22);
			double b = (GenColors[i][2]<=0.0||GenColors[i][2]>100.0)?min(max(GenColors[i][2],0),100):pow(GenColors[i][2] / 100.,2.22);

			//Constant XYZ - levels calculated to generate the same 709 XYZ values
			if (constant_XYZ)		
			{
				tempColor.SetRGBValue(ColorRGB(r,g,b),CColorReference(HDTV));
				if (mode == 5)
				{
					tempColor.SetX(tempColor.GetX() / m_HDRRefLevel); //50.23% 8-bit reference for HDR-10
					tempColor.SetY(tempColor.GetY() / m_HDRRefLevel);
					tempColor.SetZ(tempColor.GetZ() / m_HDRRefLevel);
				}
			}
			else
				tempColor.SetRGBValue(ColorRGB(r,g,b),(colorReference.m_standard==UHDTV3||colorReference.m_standard==UHDTV4)?CColorReference(UHDTV2):colorReference);

			ColorRGB aRGBColor = tempColor.GetRGBValue((colorReference.m_standard==UHDTV3||colorReference.m_standard==UHDTV4)?CColorReference(UHDTV2):colorReference);	
			
			r = (aRGBColor[0]<=0.0||aRGBColor[0]>1.0)?min(max(aRGBColor[0],0),1):getL_EOTF(aRGBColor[0], noDataColor, noDataColor,0.0,0.0,-1*mode);
			g = (aRGBColor[1]<=0.0||aRGBColor[1]>1.0)?min(max(aRGBColor[1],0),1):getL_EOTF(aRGBColor[1], noDataColor, noDataColor,0.0,0.0,-1*mode);
			b = (aRGBColor[2]<=0.0||aRGBColor[2]>1.0)?min(max(aRGBColor[2],0),1):getL_EOTF(aRGBColor[2], noDataColor, noDataColor,0.0,0.0,-1*mode);

			//re-quantize to 8-bit video %
			GenColors[i][0] = floor( (r * 219.) + 0.5 ) / 2.19;
			GenColors[i][1] = floor( (g * 219.) + 0.5 ) / 2.19;
			GenColors[i][2] = floor( (b * 219.) + 0.5 ) / 2.19;

		}
	}

	return bOk;
}

void GenerateSaturationColors (const CColorReference& colorReference, ColorRGBDisplay* GenColors, int nSteps, bool bRed, bool bGreen, bool bBlue, int mode)
{
	//use fully saturated space if user has special color space modes set
	//UHDTV pseudo-spaces XYZ is set in original space and mapped to BT.2020
	int m_cRef=colorReference.m_standard;
	CColorReference cRef=((m_cRef==HDTVa  || m_cRef==HDTVb || m_cRef==UHDTV4 )?CColorReference(HDTV):m_cRef==UHDTV3?CColorReference(UHDTV):colorReference);

	// Retrieve color luminance coefficients matching actual reference
    const double KR = cRef.GetRedReferenceLuma (true);  
    const double KG = cRef.GetGreenReferenceLuma (true);
    const double KB = cRef.GetBlueReferenceLuma (true); 
    double K = ( bRed ? KR : 0.0 ) + ( bGreen ? KG : 0.0 ) + ( bBlue ? KB : 0.0 );

    // Compute vector between neutral gray and saturated color in CIExy space
    ColorRGB Clr1;
    double	xstart, ystart, xend, yend;

    // Retrieve gray xy coordinates
    ColorxyY whitexyY(cRef.GetWhite());
    xstart = whitexyY[0];
    ystart = whitexyY[1];

    // Define target color in RGB mode
    Clr1[0] = ( bRed ? 1.0 : 0.0 );
    Clr1[1] = ( bGreen ? 1.0 : 0.0 );
    Clr1[2] = ( bBlue ? 1.0 : 0.0 );

    // Compute xy coordinates of 100% saturated color
    ColorXYZ Clr2(Clr1, cRef);

    ColorxyY Clr3(Clr2);
    xend=Clr3[0];
    yend=Clr3[1];

    for ( int i = 0; i < nSteps ; i++ )
    {
        double	clr, comp;

        if ( i == 0 )
        {
            clr = K;
            comp = K;
        }
        else if ( i == nSteps - 1 )
        {
            clr = 1.0;
            comp = 0.0;
        }
        else
        {
            double x, y;

            x = xstart + ( (xend - xstart) * (double) i / (double) (nSteps - 1) );
            y = ystart + ( (yend - ystart) * (double) i / (double)(nSteps - 1) );

            ColorxyY UnsatClr_xyY(x,y,K);
            ColorXYZ UnsatClr(UnsatClr_xyY);

            ColorRGB UnsatClr_rgb(UnsatClr, cRef);

            // Both components are theoretically equal, get medium value
            clr = ( ( ( bRed ? UnsatClr_rgb[0] : 0.0 ) + ( bGreen ? UnsatClr_rgb[1] : 0.0 ) + ( bBlue ? UnsatClr_rgb[2] : 0.0 ) ) / (double) ( bRed + bGreen + bBlue ) );
            comp = ( ( K - ( K * (double) clr ) ) / ( 1.0 - K ) );

            if ( clr < 0.0 )
            {
                clr = 0.0;
            }
            else if ( clr > 1.0 )
            {
                clr = 1.0;
            }
            if ( comp < 0.0 )
            {
                comp = 0.0;
            }
            else if ( comp > 1.0 )
            {
                comp = 1.0;
            }
        }

		// adjust "color gamma"
		// here we use encoding gamma of 1 / 2.22 for SDR and targets get adjusted for user gamma: Targets assume all generated RGB triplets @2.22 gamma
		CColor aColor;

		ColorRGB rgbColor(( bRed ? clr : comp ), ( bGreen ? clr : comp ), ( bBlue ? clr : comp ));

		if (m_cRef == UHDTV3 || m_cRef == UHDTV4)
		{
			if (m_cRef == UHDTV3)
				aColor.SetRGBValue(rgbColor, CColorReference(UHDTV));
			else
				aColor.SetRGBValue(rgbColor, CColorReference(HDTV));

			if (mode == 5)
			{
				aColor.SetX(aColor.GetX() / m_HDRRefLevel );
				aColor.SetY(aColor.GetY() / m_HDRRefLevel );
				aColor.SetZ(aColor.GetZ() / m_HDRRefLevel );
			}
			rgbColor = aColor.GetRGBValue(CColorReference(UHDTV2));
		}
		else
		{
			aColor.SetRGBValue(rgbColor, colorReference);
			if (mode == 5)
			{
				aColor.SetX(aColor.GetX() / m_HDRRefLevel );
				aColor.SetY(aColor.GetY() / m_HDRRefLevel );
				aColor.SetZ(aColor.GetZ() / m_HDRRefLevel );
			}
			rgbColor = aColor.GetRGBValue(colorReference);
		}

		if (mode == 5 || mode == 7)
		{
				rgbColor[0] = 100.0 * ( (rgbColor[0]<=0.0||rgbColor[0]>1.0)?min(max(rgbColor[0],0),1):getL_EOTF(rgbColor[0], noDataColor, noDataColor, 2.4, 0.9, -1*mode));
				rgbColor[1] = 100.0 * ( (rgbColor[1]<=0.0||rgbColor[1]>1.0)?min(max(rgbColor[1],0),1):getL_EOTF(rgbColor[1], noDataColor, noDataColor, 2.4, 0.9, -1*mode));
				rgbColor[2] = 100.0 * ( (rgbColor[2]<=0.0||rgbColor[2]>1.0)?min(max(rgbColor[2],0),1):getL_EOTF(rgbColor[2], noDataColor, noDataColor, 2.4, 0.9, -1*mode));
		}
		else
		{
			rgbColor[0] = 100.0 * ( (rgbColor[0]<=0.0||rgbColor[0]>=1.0)?min(max(rgbColor[0],0),1):pow(rgbColor[0], 1.0 / 2.22) );
			rgbColor[1] = 100.0 * ( (rgbColor[1]<=0.0||rgbColor[1]>=1.0)?min(max(rgbColor[1],0),1):pow(rgbColor[1], 1.0 / 2.22) );
			rgbColor[2] = 100.0 * ( (rgbColor[2]<=0.0||rgbColor[2]>=1.0)?min(max(rgbColor[2],0),1):pow(rgbColor[2], 1.0 / 2.22) );
		}

		//quantize to 8-bit video %
		rgbColor[0] = floor( (rgbColor[0] / 100. * 219.) + 0.5 ) / 2.19;
		rgbColor[1] = floor( (rgbColor[1] / 100. * 219.) + 0.5 ) / 2.19;
		rgbColor[2] = floor( (rgbColor[2] / 100. * 219.) + 0.5 ) / 2.19;
		
		GenColors [ i ] = ColorRGBDisplay( rgbColor[0], rgbColor[1], rgbColor[2] );
        
    }
}

Matrix ComputeConversionMatrix3Colour(const ColorXYZ measures[3], const ColorXYZ references[3])
{
    Matrix measuresXYZ(measures[0]);
    measuresXYZ.CMAC(measures[1]);
    measuresXYZ.CMAC(measures[2]);

    Matrix referencesXYZ(references[0]);
    referencesXYZ.CMAC(references[1]);
    referencesXYZ.CMAC(references[2]);

    if(measuresXYZ.Determinant() == 0) // check that reference matrix is inversible
    {
        throw std::logic_error("Can't invert measures matrix");
    }

    // Use only primary colors to compute transformation matrix
    Matrix result = referencesXYZ*measuresXYZ.GetInverse();
    return result;
}

Matrix ComputeConversionMatrix(const ColorXYZ measures[3], const ColorXYZ references[3], const ColorXYZ & WhiteTest, const ColorXYZ & WhiteRef, bool	bUseOnlyPrimaries)
{
    if (!WhiteTest.isValid() || !WhiteRef.isValid() || bUseOnlyPrimaries)
    {
        return ComputeConversionMatrix3Colour(measures, references);
    }

    // implement algorithm from :
    // http://www.avsforum.com/avs-vb/attachment.php?attachmentid=246852&d=1337250619

    // get the inputs as xyz
    Matrix measuresxyz = Colorxyz(measures[0]);
    measuresxyz.CMAC(Colorxyz(measures[1]));
    measuresxyz.CMAC(Colorxyz(measures[2]));
    Matrix referencesxyz = Colorxyz(references[0]);
    referencesxyz.CMAC(Colorxyz(references[1]));
    referencesxyz.CMAC(Colorxyz(references[2]));
    Colorxyz whiteMeasurexyz(WhiteTest);
    Colorxyz whiteReferencexyz(WhiteRef);

    Matrix RefWhiteGain(referencesxyz.GetInverse() * whiteReferencexyz);
    Matrix MeasWhiteGain(measuresxyz.GetInverse() * whiteMeasurexyz);

    // Transform component gain matrix into a diagonal matrix
    Matrix kr(0.0,3,3);
    kr(0,0) = RefWhiteGain(0,0);
    kr(1,1) = RefWhiteGain(1,0);
    kr(2,2) = RefWhiteGain(2,0);

    // Transform component gain matrix into a diagonal matrix
    Matrix km(0.0,3,3);
    km(0,0) = MeasWhiteGain(0,0);
    km(1,1) = MeasWhiteGain(1,0);
    km(2,2) = MeasWhiteGain(2,0);

    // Compute component distribution for reference white
    Matrix N(referencesxyz * kr);

    // Compute component distribution for measured white
    Matrix M(measuresxyz * km);

    // Compute transformation matrix
    Matrix transform(N * M.GetInverse());

    // find out error adjustment in white value with this matrix
    ColorXYZ testResult(transform * WhiteTest);
    double errorAdjustment(WhiteRef[1] / testResult[1]);

    // and scale the matrix with this value
    transform *= errorAdjustment;

    return transform;
}

double ArrayIndexToGrayLevel ( int nCol, int nSize, bool m_bUseRoundDown, bool m_b10bit)
{
    // Gray percent: return a value between 0 and 100 corresponding to whole number level based on
	// normal rounding (GCD disk), round down (AVSHD disk)
	// 10-bit target levels used for HDR/Ryan Mascior's disk
	
		if (m_bUseRoundDown)
		{
			return (  floor((double)nCol / (double)(nSize-1) * 219.0) / 219.0 * 100.0 );
		}
		else if (m_b10bit)
		{
			return (  floor((double)nCol / (double)(nSize-1) * (219.0 * 4) + 0.5) / ( 219.0 * 4) * 100.0 ); //test 10-bit rounding
		}
		else
		{
			return ( floor((double)nCol / (double)(nSize-1) * 219.0 + 0.5) / 219.0 * 100.0 );
		}
}

double GrayLevelToGrayProp ( double Level, bool m_bUseRoundDown, bool m_b10bit)
{
    // Gray Level: return a value between 0 and 1 based on percentage level input
    //    normal rounding (GCD disk), round down (AVSHD disk)
	if (m_bUseRoundDown)
	{
    	return Level = (floor(Level / 100.0 * 219.0 + 16.0) - 16.0) / 219.0;
	}
	else if (m_b10bit)
	{
    	return Level = (floor(Level / 100.0 * (219.0 * 4) + (64.5)) - (16.0 * 4.0)) / (219.0 * 4.0);
	}
	else
	{
		return Level = (floor(Level / 100.0 * 219.0 + 16.5) - 16.0) / 219.0;
	}
}

