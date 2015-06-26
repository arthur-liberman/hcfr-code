/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2011 Association Homecinema Francophone.  All rights reserved.
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
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// CIEChartImage.cpp : CIE chart drawing
//

#include "stdafx.h"
#include "color.h"
#include "ColorHCFR.h"
#include <math.h>


/* The following tables gives the  spectral  chromaticity  co-ordinates
   x(\lambda) and y(\lambda) for wavelengths in 2 nanometre increments
   from 390 nm through  730  nm.   These  co-ordinates  represent  the
   position in the CIE x-y space of pure spectral colours of the given
   wavelength, and  thus  define  the  outline  of  the  CIE  "tongue"
   diagram. 
*/

// The following table gives coordinates for the left side of the "tongue",
// in top to bottom order (wavelengths in reverse order)

	static double spectral_chromaticity_left[][2] = {
{0.093218329	,0.840634868},
{0.078823221	,0.839593793},
{0.064847649	,0.834844596},
{0.050622056	,0.826919712},
{0.036449887	,0.814976286},
{0.023816396	,0.796175656},
{0.014239761	,0.767706546},
{0.008018852	,0.730183784},
{0.004712871	,0.686564943},
{0.003636384	,0.639843022},
{0.004182885	,0.591944317},
{0.005959173	,0.544243383},
{0.009334011	,0.496734903},
{0.014769904	,0.449089257},
{0.022216179	,0.401746848},
{0.031165264	,0.355795702},
{0.041078948	,0.312238568},
{0.051378322	,0.272120993},
{0.061549512	,0.236151406},
{0.071302106	,0.204289027},
{0.080550481	,0.176011212},
{0.089227357	,0.1509709},
{0.097151483	,0.129303861},
{0.104183936	,0.11102652},
{0.110341629	,0.095819734},
{0.113128382	,0.089215517},
{0.115738496	,0.083202675},
{0.118182545	,0.077730205},
{0.120464544	,0.072751119},
{0.122587967	,0.068221918},
{0.124557872	,0.064102932},
{0.126380084	,0.060357853},
{0.12806521		,0.056950567},
{0.129638308	,0.053837252},
{0.131124358	,0.050977268},
{0.132544347	,0.048335457},
{0.133915997	,0.04588143},
{0.135248912	,0.04359107},
{0.136532209	,0.041451621},
{0.137752788	,0.039453189},
{0.138900812	,0.037586433},
{0.139968517	,0.035842464},
{0.140952976	,0.03421536},
{0.141862327	,0.03270865},
{0.14270621		,0.031326817},
{0.143493342	,0.030073028},
{0.144231808	,0.028949585},
{0.144929517	,0.027953136},
{0.145596337	,0.027061169},
{0.146871401	,0.025495361},
{0.148109762	,0.02409121},
{0.149294066	,0.022816033},
{0.150364736	,0.021733604},
{0.151312575	,0.020862901},
{0.152252746	,0.020081999},
{0.153291995	,0.019260827},
{0.154387884	,0.018414805},
{0.15540432		,0.017674755},
{0.156278749	,0.017122691},
{0.15709108		,0.016709427},
{0.157928892	,0.016366787},
{0.158785171	,0.0160827},
{0.159581463	,0.015892612},
{0.160275124	,0.015823429},
{0.160927679	,0.015872262},
{0.161617352	,0.016031261},
{0.162331252	,0.016268958},
{0.162957957	,0.016525218},
{0.163409927	,0.016757637},
{0.163758875	,0.017018069},
{0.164122434	,0.017383199},
{0.16454698		,0.017839402},
{0.164994968	,0.018272109},
{0.165419824	,0.018574175},
{0.165792139	,0.018723341},
{0.166089132	,0.018722584},
{0.1662905		,0.018578286},
{0.166379917	,0.018299802}
};

// The following table gives coordinates for the right side of the "tongue",
// in top to bottom order (wavelengths in ascending order) 
// The ending chromaticity is 380 nm to add the closing segment

	static double spectral_chromaticity_right[][2] = {
		{0.093218329	,0.840634868},
{0.108493722	,0.837860868},
{0.124307396	,0.832205673},
{0.140198493	,0.824632547},
{0.155817794	,0.815798889},
{0.17090499		,0.806174854},
{0.185364306	,0.796087208},
{0.19958013		,0.785422478},
{0.213991019	,0.773942797},
{0.228623268	,0.761716575},
{0.243156313	,0.749115666},
{0.257358021	,0.736442234},
{0.27124628		,0.723763549},
{0.284889639	,0.711081861},
{0.298335897	,0.69840766},
{0.311614271	,0.685757659},
{0.324812475	,0.673074222},
{0.338247883	,0.660055239},
{0.352216419	,0.646418801},
{0.366525565	,0.632373878},
{0.380605859	,0.618505495},
{0.394158043	,0.605124316},
{0.407630299	,0.591789991},
{0.421521831	,0.57800948},
{0.435794845	,0.563825653},
{0.442933937	,0.556724311},
{0.450007566	,0.549684489},
{0.456983652	,0.542738676},
{0.463877351	,0.535872074},
{0.470712399	,0.529061271},
{0.477509187	,0.522286168},
{0.484285069	,0.515529674},
{0.491052797	,0.508779294},
{0.497815994	,0.502031686},
{0.504575199	,0.495286559},
{0.511329862	,0.488544668},
{0.51807818		,0.481807986},
{0.524805256	,0.475091535},
{0.531450328	,0.468456123},
{0.537949995	,0.461965192},
{0.544250552	,0.455672498},
{0.550306492	,0.449623606},
{0.55609666		,0.443839738},
{0.561666785	,0.438275255},
{0.567071761	,0.432875346},
{0.572359445	,0.427592226},
{0.577570845	,0.422384946},
{0.582731307	,0.417228213},
{0.587825003	,0.412137892},
{0.597722921	,0.402245801},
{0.607123765	,0.392849743},
{0.616036566	,0.383940891},
{0.624568961	,0.375411778},
{0.632758187	,0.367225292},
{0.640464948	,0.35952081},
{0.647561918	,0.35242573},
{0.654046555	,0.345942663},
{0.659998796	,0.339991737},
{0.665484535	,0.334507109},
{0.670547164	,0.329445418},
{0.675227828	,0.324772172},
{0.679581652	,0.320418348},
{0.683693376	,0.316306624},
{0.687582753	,0.312417247},
{0.691109932	,0.308890068},
{0.694148589	,0.305851411},
{0.696746634	,0.303253366},
{0.6990722		,0.3009278},
{0.701244482	,0.298755518},
{0.703285547	,0.296714453},
{0.705198924	,0.294801076},
{0.707032743	,0.292967257},
{0.708866844	,0.291133156},
{0.710708309	,0.289291691},
{0.712355107	,0.287644893},
{0.713613015	,0.286386985},
{0.714517247	,0.285482753},
{0.715279392	,0.284720608},
{0.716054852	,0.283945148},
{0.716849228	,0.283150772},
{0.71763598		,0.28236402},
{0.718395912	,0.281604088},
{0.719116083	,0.280883917},
{0.719781999	,0.280218001},
{0.720366876	,0.279633124},
{0.720844947	,0.279155053},
{0.721224073	,0.278775927},
{0.721540928	,0.278459072},
{0.721824346	,0.278175654},
{0.722077302	,0.277922698},
{0.722296914	,0.277703086},
{0.722484777	,0.277515223},
{0.722646848	,0.277353152},
{0.722787641	,0.277212359},
{0.722906468	,0.277093532},
{0.723001878	,0.276998122},
{0.723077863	,0.276922137},
{0.72314318		,0.27685682},
{0.723204058	,0.276795942},
{0.723253465	,0.276746535},
{0.723281713	,0.276718287},
{0.723290142	,0.276709858},
{0.723290807	,0.276709193},
{0.723293119	,0.276706881},
{0.723293926	,0.276706074},
{0.723286857	,0.276713143},
{0.723266781	,0.276733219},
{0.723230051	,0.276769949},
{0.723175196	,0.276824804},
{0.72311015		,0.27688985},
{0.72304532		,0.27695468},
{0.722982798	,0.277017202},
{0.722917252	,0.277082748},
{0.722843771	,0.277156229},
{0.722763819	,0.277236181},
{0.722679947	,0.277320053},
{0.72259418		,0.27740582},
{0.722507438	,0.277492562}, 
{0.166379917	,0.018299802} //close polygon
};


// The following table gives coordinates for the left side of the CIE u'v' diagram,
// in top to bottom order (wavelengths in reverse order)

static double spectral_chromaticity_uv_left[][2] = {
	{0.028902261	,0.58643572},
{0.024408236	,0.584970496},
{0.020125834	,0.58297214},
{0.015792505	,0.580439704},
{0.011474122	,0.577232469},
{0.007617301	,0.572949681},
{0.004674906	,0.567084657},
{0.002730713	,0.55947218},
{0.001678768	,0.550261816},
{0.00136311		,0.539656232},
{0.001657414	,0.527738165},
{0.002504117	,0.514569733},
{0.004175287	,0.499948415},
{0.007067336	,0.483496404},
{0.011427297	,0.464953098},
{0.017296696	,0.444299224},
{0.024654624	,0.421646141},
{0.033347956	,0.397405491},
{0.043111576	,0.372170839},
{0.05372306		,0.346326673},
{0.065077709	,0.319953577},
{0.077033094	,0.293261515},
{0.089184143	,0.267074374},
{0.101052561	,0.242301336},
{0.112331195	,0.219481777},
{0.117709352	,0.208863383},
{0.122898726	,0.198787629},
{0.127889438	,0.189257748},
{0.132667122	,0.180271164},
{0.137219432	,0.171820199},
{0.141538234	,0.163893981},
{0.145618717	,0.156478566},
{0.149465868	,0.149551727},
{0.153111422	,0.143067055},
{0.156590775	,0.136975179},
{0.159935897	,0.131229988},
{0.163175623	,0.12578889},
{0.166327393	,0.120617423},
{0.169376148	,0.115702083},
{0.172302298	,0.111033825},
{0.175090452	,0.106603461},
{0.177728065	,0.102401432},
{0.18020769		,0.098424384},
{0.182531238	,0.094692428},
{0.18470251		,0.091228117},
{0.18672543		,0.088050413},
{0.188604174	,0.085175583},
{0.190347433	,0.082604409},
{0.191982004	,0.080285874},
{0.195035292	,0.076176259},
{0.197949814	,0.072445689},
{0.200717736	,0.069018555},
{0.203190525	,0.066080258},
{0.205327614	,0.063698552},
{0.207395009	,0.06154923},
{0.209662626	,0.059273284},
{0.212056567	,0.056909943},
{0.214255595	,0.05482833},
{0.216084829	,0.053269532},
{0.217703488	,0.052102425},
{0.219304285	,0.051136556},
{0.220886072	,0.050338453},
{0.222293257	,0.049810585},
{0.223432054	,0.04963208},
{0.224397989	,0.049797731},
{0.225318148	,0.050287307},
{0.226201117	,0.051007597},
{0.226930388	,0.051778181},
{0.227410543	,0.052471979},
{0.227703865	,0.05324249},
{0.227919849	,0.054315828},
{0.228143065	,0.05565192},
{0.228424012	,0.056917032},
{0.228792445	,0.057802441},
{0.229224539	,0.058245589},
{0.22968304		,0.058255379},
{0.230131319	,0.057849079},
{0.230535872	,0.057051427}
};

// The following table gives coordinates for the right side of the CIE u'v' diagram,
// in top to bottom order (wavelengths in ascending order) 
// The ending chromaticity is 380 nm to add the closing segment

static double spectral_chromaticity_uv_right[][2] = {
{0.028902261	,0.58643572},
{0.033805663	,0.587407209},
{0.039035587	,0.587999476},
{0.044453854	,0.588313836},
{0.049949801	,0.58841311},
{0.055433343	,0.588339611},
{0.060863395	,0.588129857},
{0.06638338		,0.587797731},
{0.072176418	,0.587342141},
{0.078273174	,0.586770728},
{0.084553497	,0.586107696},
{0.090918422	,0.585376637},
{0.097372095	,0.584588068},
{0.103943943	,0.583746987},
{0.110656457	,0.582857997},
{0.117525282	,0.581925178},
{0.124601208	,0.580945011},
{0.132074333	,0.579890692},
{0.140149483	,0.578733202},
{0.14876078		,0.577484877},
{0.157586835	,0.576196403},
{0.166431218	,0.574898958},
{0.175585041	,0.573549876},
{0.185425528	,0.572093392},
{0.195987973	,0.570525002},
{0.201452101	,0.56971224},
{0.20699047		,0.568887689},
{0.212577805	,0.568055204},
{0.218225644	,0.567213108},
{0.223953856	,0.566358465},
{0.229781189	,0.565488507},
{0.235725326	,0.564600623},
{0.24180127		,0.563692603},
{0.248016741	,0.562763339},
{0.254377042	,0.561812081},
{0.260886699	,0.560838188},
{0.267549178	,0.559841169},
{0.274354615	,0.558822528},
{0.281243743	,0.557791161},
{0.288148302	,0.556757311},
{0.295003983	,0.555730633},
{0.301749843	,0.554720275},
{0.308348011	,0.553731928},
{0.314836987	,0.552759832},
{0.321270706	,0.551795909},
{0.327699784	,0.550832581},
{0.334170948	,0.549862848},
{0.340714985	,0.548882102},
{0.34731143		,0.54789342},
{0.360535373	,0.545911204},
{0.373617534	,0.543950033},
{0.386519557	,0.542015702},
{0.399353316	,0.540091461},
{0.412140633	,0.538174063},
{0.424618596	,0.536302962},
{0.436510659	,0.534519654},
{0.447730316	,0.532837131},
{0.458340196	,0.531246012},
{0.468394104	,0.529738238},
{0.477916758	,0.528310107},
{0.486930526	,0.526960421},
{0.495515409	,0.525672689},
{0.50379993		,0.52443001},
{0.511799805	,0.523230029},
{0.51919617		,0.522120575},
{0.525679012	,0.521148148},
{0.531305213	,0.520304218},
{0.53640794		,0.519538809},
{0.541232246	,0.518815163},
{0.545817019	,0.518127447},
{0.550161377	,0.517475793},
{0.554367914	,0.516844813},
{0.55861766		,0.516207351},
{0.562927947	,0.515560808},
{0.566820025	,0.514976996},
{0.569817129	,0.514527431},
{0.57198461		,0.514202309},
{0.573820051	,0.513926992},
{0.57569564		,0.513645654},
{0.577625484	,0.513356177},
{0.579545351	,0.513068197},
{0.581407892	,0.512788816},
{0.583180399	,0.51252294},
{0.584825832	,0.512276125},
{0.586276173	,0.512058574},
{0.587465258	,0.511880211},
{0.588410548	,0.511738418},
{0.589202148	,0.511619678},
{0.589911427	,0.511513286},
{0.590545444	,0.511418183},
{0.591096632	,0.511335505},
{0.591568687	,0.511264697},
{0.591976341	,0.511203549},
{0.592330782	,0.511150383},
{0.592630147	,0.511105478},
{0.592870663	,0.5110694},
{0.593062307	,0.511040654},
{0.593227112	,0.511015933},
{0.593380773	,0.510992884},
{0.593505519	,0.510974172},
{0.593576857	,0.510963471},
{0.593598145	,0.510960278},
{0.593599826	,0.510960026},
{0.593605665	,0.51095915},
{0.593607704	,0.510958844},
{0.593589849	,0.510961523},
{0.593539145	,0.510969128},
{0.593446397	,0.51098304},
{0.593307916	,0.511003813},
{0.593143765	,0.511028435},
{0.59298022		,0.511052967},
{0.592822555	,0.511076617},
{0.592657326	,0.511101401},
{0.592472165	,0.511129175},
{0.592270789	,0.511159382},
{0.59205964		,0.511191054},
{0.591843822	,0.511223427},
{0.591625663	,0.511256151},
{0.230535872	,0.057051427},
{0.230131319	,0.057849079},
{0.22968304		,0.058255379}
//{0.229224539	,0.058245589},
//{0.228792445	,0.057802441},
//{0.228424012	,0.056917032}
};

// The following table gives coordinates for the left side of the CIE a*b* diagram,
// in top to bottom order (wavelengths in reverse order)

static double spectral_chromaticity_ab_left[][2] = {
{-21.08267575,	129.212372},
{-51.99946835,	124.9723232},
{-81.17164525,	121.1297287},
{-109.2313165,	117.5502015},
{-136.8723075,	114.14244},
{-165.0140351,	110.8401993},
{-195.1815732,	107.5922864},
{-187.8398305,	80.09591085},
{-179.132837,	59.71999371},
{-168.6552844,	41.15295981},
{-155.8005056,	22.49880761},
{-139.6072581,	2.39927527},
{-118.4318269,	-20.71759918},
{-95.38391349,	-35.86662301},
{-69.6816743,	-51.57481355},
{-40.43744929,	-68.12648678},
{-6.215001599,	-85.84779009},
{35.39884257,	-104.8038209},
{85.67372899,	-119.6751496}
};

// The following table gives coordinates for the right side of the CIE u'v' diagram,
// in top to bottom order (wavelengths in ascending order) 
// The ending chromaticity is 380 nm to add the closing segment

static double spectral_chromaticity_ab_right[][2] = {
{-21.07969122,	129.1940803},
{6.46010528,	126.1200054},
{32.87394378,	122.929737},
{58.02989578,	119.6612324},
{81.57084081,	116.4636326},
{102.7194064,	112.0827554},
{119.7730924,	103.1785166},
{121.6458519,	46.99679808},
{123.5937942,	21.7413108},
{125.6512568,	1.163817277},
{127.8677151,	-18.12904548},
{130.3178732,	-37.62041234},
{133.1209479,	-58.40598939},
{127.8759988,	-71.33733147},
{122.0979932,	-83.75535396},
{115.6017904,	-95.61235885},
{108.0571507,	-106.6193279},
{98.76118455,	-115.8195724},
{85.67372899,	-119.6751496}
};

static double fBlackBody_xy[][2] = {
//	{ 0.6499, 0.3474 }, //  1000 K 
//	{ 0.6361, 0.3594 }, //  1100 K 
//	{ 0.6226, 0.3703 }, //  1200 K 
//	{ 0.6095, 0.3801 }, //  1300 K 
//	{ 0.5966, 0.3887 }, //  1400 K 
//	{ 0.5841, 0.3962 }, //  1500 K 
//	{ 0.5720, 0.4025 }, //  1600 K 
//	{ 0.5601, 0.4076 }, //  1700 K 
//	{ 0.5486, 0.4118 }, //  1800 K 
//	{ 0.5375, 0.4150 }, //  1900 K 
//	{ 0.5267, 0.4173 }, //  2000 K 
//	{ 0.5162, 0.4188 }, //  2100 K 
//	{ 0.5062, 0.4196 }, //  2200 K 
//	{ 0.4965, 0.4198 }, //  2300 K 
//	{ 0.4872, 0.4194 }, //  2400 K 
//	{ 0.4782, 0.4186 }, //  2500 K 
	{ 0.4696, 0.4173 }, //  2600 K 
	{ 0.4614, 0.4158 }, //  2700 K 
	{ 0.4535, 0.4139 }, //  2800 K 
	{ 0.4460, 0.4118 }, //  2900 K 
	{ 0.4388, 0.4095 }, //  3000 K 
	{ 0.4254, 0.4044 }, //  3200 K 
	{ 0.4132, 0.3990 }, //  3400 K 
	{ 0.4021, 0.3934 }, //  3600 K 
	{ 0.3919, 0.3877 }, //  3800 K 
	{ 0.3827, 0.3820 }, //  4000 K 
	{ 0.3743, 0.3765 }, //  4200 K 
	{ 0.3666, 0.3711 }, //  4400 K 
	{ 0.3596, 0.3659 }, //  4600 K 
	{ 0.3532, 0.3609 }, //  4800 K 
	{ 0.3473, 0.3561 }, //  5000 K 
	{ 0.3419, 0.3516 }, //  5200 K 
	{ 0.3369, 0.3472 }, //  5400 K 
	{ 0.3323, 0.3431 }, //  5600 K 
	{ 0.3281, 0.3392 }, //  5800 K 
	{ 0.3242, 0.3355 }, //  6000 K 
	{ 0.3205, 0.3319 }, //  6200 K 
	{ 0.3171, 0.3286 }, //  6400 K 
	{ 0.3140, 0.3254 }, //  6600 K 
	{ 0.3083, 0.3195 }, //  7000 K 
	{ 0.3022, 0.3129 }, //  7500 K 
	{ 0.2970, 0.3071 }, //  8000 K 
	{ 0.2887, 0.2975 }, //  9000 K 
	{ 0.2824, 0.2898 }, // 10000 K 
	{ 0.2734, 0.2785 }, // 12000 K 
	{ 0.2675, 0.2707 }, // 14000 K 
	{ 0.2634, 0.2650 }, // 16000 K 
	{ 0.2580, 0.2574 }, // 20000 K 
	{ 0.2516, 0.2481 }, // 30000 K 
	{ 0.2487, 0.2438 }  // 40000 K 
};


// Draw Black Body curve on CIE 1931(xy) or 1976(u'v') charts, depending on flag bCIEuv

void DrawBlackBodyCurve ( CDC* pDC, int cxMax, int cyMax, BOOL doFullChart, BOOL bCIEuv, BOOL bCIEab )
{
	int		i;
	int		nbPoints = sizeof ( fBlackBody_xy ) / sizeof ( fBlackBody_xy[0] );
	POINT	pt [ sizeof ( fBlackBody_xy ) / sizeof ( fBlackBody_xy[0] ) ];
	double	xCie, yCie;
	double	uCie, vCie;

	for ( i = 0; i < nbPoints ; i ++ )
	{
		xCie = fBlackBody_xy [ i ] [ 0 ];
		yCie = fBlackBody_xy [ i ] [ 1 ];

		if ( bCIEuv )
		{
			uCie = ( 4.0 * xCie ) / ( (-2.0 * xCie ) + ( 12.0 * yCie ) + 3.0 );
			vCie = ( 9.0 * yCie ) / ( (-2.0 * xCie ) + ( 12.0 * yCie ) + 3.0 );

			xCie = uCie;
			yCie = vCie;
		} else if (bCIEab)
		{
			ColorxyY tmp;
			tmp=ColorxyY(xCie,yCie,1);

			ColorXYZ tmpXYZ=ColorXYZ(tmp);
			ColorLab cLab=ColorLab(tmpXYZ,1,GetColorReference());
			xCie = cLab[1];
			yCie = cLab[2];
		}

		if (bCIEab)
		{
			pt [ i ].x = (int) ( ( (xCie + 220) / (400) ) * (double) cxMax );
			pt [ i ].y = (int) ( ( 1.0 - ( (yCie + 200) / (400) ) ) * (double) cyMax );
		}
		else
		{
			pt [ i ].x = (int) ( ( (xCie+.075) / (bCIEuv ? 0.8 : 0.9) ) * (double) cxMax );
			pt [ i ].y = (int) ( ( 1.0 - ( (yCie+0.05) / (bCIEuv ? 0.8 : 1.0) ) ) * (double) cyMax );
		}
	}

	CPen bbcPen ( PS_SOLID, 2, ( doFullChart ? RGB(0,0,0) : RGB(180,180,180) ) );
	CPen * pOldPen = pDC -> SelectObject ( & bbcPen );
	pDC -> Polyline ( pt, nbPoints );
	pDC -> SelectObject ( pOldPen );
}



#define MODE_SEGMENT_POINT 				0
#define MODE_SEGMENT_POINT_FLAG			1
#define MODE_SEGMENT_START_LINE			2
#define MODE_SEGMENT_START_LINE_FLAG	3
#define MODE_SEGMENT_END_LINE			4

/////////////////////////////////////////////////////////////////////////////
// This structure contains data describing each segment
// (bresenham algorythm + data used to compute points in texture)

struct Elem_Bres
{
	int  AbsDeltaX, IncX, S, S1, S2, X, First_X, Last_X, First_Y, Last_Y;
	WORD Config;
};

/////////////////////////////////////////////////////////////////////////////
// This function is a specialized polygon tracing function
// It assumes polygon has a simple shape, with one left side and one right side.
// This algorythm is efficient with this simple shape, even when number of vertex is
// very high. Pixel color is computed to match CIE diagram.

void DrawCIEChart(CDC* pDC, int cxMax, int cyMax, BOOL doFullChart, BOOL doShowBlack, BOOL bCIEuv, BOOL bCIEab ) 
{
	int			i, j, k, l;
	int			MinY, MaxY;
	int			Y_Cour;
	int			NbLeftPoints, NbRightPoints;
	BOOL		bInPolygon;
	BOOL		bOnLine;
	int			Limit [ 8 ];
	int			Mode [ 8 ];
	POINT		ptLimit [ 8 ];
	int			NbLimits;
	int			From [ 4 ];
	int			To [ 4 ];
	POINT		ptFrom [ 4 ];
	POINT		ptTo [ 4 ];
	int			NbSegments;
	COLORREF	Color;

	int			DeltaX;
	int			AbsDeltaY;
	int			delta;
	int			nMaxRgbVal = ( bCIEab ? 240 : (doFullChart ? 255 : 200) );
	double		d;
	double		squared_white_ray = 0.02;	// defines a diffusion zone around white point, to make it esthetically visible
	double		xCie, yCie, zCie, yCieLine;
	double		uCie, vCie;
//	double		aCie, bCie;
	double		val_r, val_g, val_b;
	double		val_min, val_max;
	int			nCurrentPoint [ 2 ];	// Index 0 is left side of the tongue, Index 1 is right side
	int			nbptShape [ 2 ];		// Index 0 is left side of the tongue, Index 1 is right side
	POINT		ptShape [ 2 ] [ 256 ];	// Index 0 is left side of the tongue, Index 1 is right side
	Elem_Bres	TB [ 2 ];				// Index 0 is left side of the tongue, Index 1 is right side
	double		(*pLeft) [2];
	double		(*pRight) [2];

	// Beautifying: something between 1 and 1/2.2 is fine. 
	// Value 1 make primary colors larger than secondary, 1/2.2 makes large secondaries and reduces primaries 
	const double gamma = 1.0 / 2.2;

	// Variables for CIExy to rgb conversion
    CColor	WhiteReference = GetColorReference().GetWhite();
	CColor	RedReference = GetColorReference().GetRed();
	CColor	GreenReference = GetColorReference().GetGreen();
	CColor	BlueReference = GetColorReference().GetBlue();

	const double xr = 0.6400;
	const double yr = 0.3300;
	const double zr = 1 - (xr + yr);

    const double xg = 0.3000;
	const double yg = 0.6000;
	const double zg = 1 - (xg + yg);

    const double xb = 0.1500;
	const double yb = 0.0600;
	const double zb = 1 - (xb + yb);

    const double xw = WhiteReference.GetxyYValue()[0];
	const double yw = WhiteReference.GetxyYValue()[1];
	const double zw = 1 - (xw + yw);

    /* xyz -> rgb matrix, before scaling to white. */
    double rx = yg*zb - yb*zg;
	double ry = xb*zg - xg*zb;
	double rz = xg*yb - xb*yg;
    double gx = yb*zr - yr*zb;
	double gy = xr*zb - xb*zr;
	double gz = xb*yr - xr*yb;
    double bx = yr*zg - yg*zr;
	double by = xg*zr - xr*zg;
	double bz = xr*yg - xg*yr;

    /* White scaling factors.
       Dividing by yw scales the white luminance to unity, as conventional. */
    const double rw = (rx*xw + ry*yw + rz*zw) / yw;
    const double gw = (gx*xw + gy*yw + gz*zw) / yw;
    const double bw = (bx*xw + by*yw + bz*zw) / yw;

    /* xyz -> rgb matrix, correctly scaled to white. */
    rx = rx / rw;
	ry = ry / rw;
	rz = rz / rw;
    gx = gx / gw;
	gy = gy / gw;
	gz = gz / gw;
    bx = bx / bw;
	by = by / bw;
	bz = bz / bw;
	ColorLab tmp;

	double lxr,lyr,lzr,fx,fy,fz;
	double ep = 216. / 24389.;
	double kap = 24389. / 27.;
	ColorXYZ White = WhiteReference.GetXYZValue();

	if ( bCIEuv )
	{
		NbLeftPoints = sizeof ( spectral_chromaticity_uv_left ) / sizeof ( spectral_chromaticity_uv_left [ 0 ] );
		pLeft = spectral_chromaticity_uv_left;

		NbRightPoints = sizeof ( spectral_chromaticity_uv_right ) / sizeof ( spectral_chromaticity_uv_right [ 0 ] );
		pRight = spectral_chromaticity_uv_right;
	}
	else if (bCIEab)
	{
		NbLeftPoints = sizeof ( spectral_chromaticity_ab_left ) / sizeof ( spectral_chromaticity_ab_left [ 0 ] );
		pLeft = spectral_chromaticity_ab_left;

		NbRightPoints = sizeof ( spectral_chromaticity_ab_right ) / sizeof ( spectral_chromaticity_ab_right [ 0 ] );
		pRight = spectral_chromaticity_ab_right;
	}
	else
	{
		NbLeftPoints = sizeof ( spectral_chromaticity_left ) / sizeof ( spectral_chromaticity_left [ 0 ] );
		pLeft = spectral_chromaticity_left;

		NbRightPoints = sizeof ( spectral_chromaticity_right ) / sizeof ( spectral_chromaticity_right [ 0 ] );
		pRight = spectral_chromaticity_right;
	}


	// compute parameter points 
	nbptShape [ 0 ] = 0;
	for ( i = 0; i < NbLeftPoints ; i ++ )
	{
		// Get CIExy or CIE u'v' coordinates and convert it in pixel coordinates in (0,0) (cxMax,cyMax)
		// Note: max CIExy coordinates are (0.8, 0.9), max CIEuv are (0.7, 0.7)
		xCie = pLeft [ i ] [ 0 ];
		yCie = pLeft [ i ] [ 1 ];
		ptShape [ 0 ] [ nbptShape [ 0 ] ].x = (int) ( ( (xCie+.075) / (bCIEuv ? 0.8 : 0.9) ) * (double) cxMax );
		ptShape [ 0 ] [ nbptShape [ 0 ] ].y = (int) ( ( 1.0 - ( (yCie+0.05) / (bCIEuv ? 0.8 : 1.0) ) ) * (double) cyMax );

		if (bCIEab)
		{
			ptShape [ 0 ] [ nbptShape [ 0 ] ].x = (int) ( ( (xCie + 220.) / (400.) ) * (double) cxMax );
			ptShape [ 0 ] [ nbptShape [ 0 ] ].y = (int) ( ( 1.0 - ( (yCie + 200) / (400.) ) ) * (double) cyMax );
		}


		if ( nbptShape [ 0 ] == 0 || ptShape [ 0 ] [ nbptShape [ 0 ] ].y > ptShape [ 0 ] [ nbptShape [ 0 ] - 1 ].y )
		{
			// Accept this point
			if ((i*2 % 20) == 0 && !bCIEab)
			{
				// Initializes a CFont object with the specified characteristics. 
				CFont font;
				font.CreateFont(24,0,0,0,FW_THIN,FALSE,FALSE,FALSE,0,OUT_TT_ONLY_PRECIS,CLIP_DEFAULT_PRECIS,PROOF_QUALITY,VARIABLE_PITCH | FF_MODERN,_T("Garamond"));
				CFont* pOldFont = pDC->SelectObject(&font);

				CString szText;
				if (520 - i*2 != 520)
				{
					szText.Format("%d nm",520 - i*2);
					pDC->SetTextColor(RGB(220,220,255));
					pDC->SetTextAlign(TA_CENTER);
					pDC->SetBkMode(TRANSPARENT);
					CPen bbcPen ( PS_SOLID, 1, RGB(220,220,255)  );
					CPen * pOldPen = pDC -> SelectObject ( & bbcPen );
					pDC -> MoveTo (ptShape[0][nbptShape[0]].x,ptShape[0][nbptShape[0]].y );
					pDC -> LineTo ( ptShape[0][nbptShape[0]].x-30,ptShape[0][nbptShape[0]].y+10 );
					pDC->TextOutA( ptShape[0][nbptShape[0]].x-50,ptShape[0][nbptShape[0]].y+10,szText);
					pDC->SelectObject(pOldFont);
					pDC -> SelectObject (pOldPen);
				}
			}
			
			if (bCIEab)
			{
				// Initializes a CFont object with the specified characteristics. 
				CFont font,font1;
				font.CreateFont(24,0,0,0,FW_THIN,FALSE,FALSE,FALSE,0,OUT_TT_ONLY_PRECIS,CLIP_DEFAULT_PRECIS,PROOF_QUALITY,VARIABLE_PITCH | FF_MODERN,_T("Garamond"));
				font1.CreateFont(24,0,-180,-180,FW_THIN,FALSE,FALSE,FALSE,0,OUT_TT_ONLY_PRECIS,CLIP_DEFAULT_PRECIS,PROOF_QUALITY,VARIABLE_PITCH | FF_MODERN,_T("Garamond"));
				CFont* pOldFont = pDC->SelectObject(&font);
				BOOL doPrint = FALSE;

				CString szText;
				switch ( i ) 
				{
				case 0:
					szText.Format("%d%% Pointer's Gamut Coverage with Real Primaries",100);
					doPrint = TRUE;
					break;
				case 6:
					szText.Format("%d nm",527);
					doPrint = TRUE;
					break;
				case 18:
					szText.Format("%d nm",467);
					doPrint = TRUE;
					break;
				}

				if (doPrint)
				{
					pDC->SetTextColor(RGB(220,220,255));
					pDC->SetTextAlign(TA_CENTER);
					pDC->SetBkMode(TRANSPARENT);
					if (i == 0)
					{
						pDC->SelectObject(pOldFont);
						CFont* pOldFont = pDC->SelectObject(&font1);
						pDC->TextOutA( 600, 710, szText);
					}
					else if (i == 6)
						pDC->TextOutA( ptShape[0][nbptShape[0]].x+30,ptShape[0][nbptShape[0]].y - 30,szText);
					else
						pDC->TextOutA( ptShape[0][nbptShape[0]].x,ptShape[0][nbptShape[0]].y + 20,szText);

					pDC->SelectObject(pOldFont);
				}
			}
			nbptShape [ 0 ] ++;
		}
	}

	nbptShape [ 1 ] = 0;
	for ( i = 0; i < NbRightPoints ; i ++ )
	{
		// Get CIExy or CIE u'v' coordinates and convert it in pixel coordinates in (0,0) (cxMax,cyMax)
		// Note: max CIExy coordinates are (0.8, 0.9)
		xCie = pRight [ i ] [ 0 ];
		yCie = pRight [ i ] [ 1 ];

		ptShape [ 1 ] [ nbptShape [ 1 ] ].x = (int) ( ( (xCie+.075) / (bCIEuv ? 0.8 : 0.9) ) * (double) cxMax );
		ptShape [ 1 ] [ nbptShape [ 1 ] ].y = (int) ( ( 1.0 - ( (yCie+.05) / (bCIEuv ? 0.8 : 1.0) ) ) * (double) cyMax );

		if (bCIEab)
		{
			ptShape [ 1 ] [ nbptShape [ 1 ] ].x = (int) ( ( (xCie + 220.) / (400.) ) * (double) cxMax );
			ptShape [ 1 ] [ nbptShape [ 1 ] ].y = (int) ( ( 1.0 - ( (yCie + 200.) / (400.) ) ) * (double) cyMax );
		}
		
		if ( nbptShape [ 1 ] == 0 || ptShape [ 1 ] [ nbptShape [ 1 ] ].y > ptShape [ 1 ] [ nbptShape [ 1 ] - 1 ].y )
		{
			// 2nm increments label every 10nm mod(i*2 /10) = 0
			if ((i*2 % 20) == 0 && !bCIEab)
			{
				// Initializes a CFont object with the specified characteristics. 
				CFont font;
				font.CreateFont(24,0,(bCIEuv?300:0),(bCIEuv?300:0),FW_THIN,FALSE,FALSE,FALSE,0,OUT_TT_ONLY_PRECIS,CLIP_DEFAULT_PRECIS,PROOF_QUALITY,VARIABLE_PITCH | FF_MODERN,_T("Garamond"));
				CFont* pOldFont = pDC->SelectObject(&font);
				CString szText;
				szText.Format("%d nm",i*2 + 520);
				pDC->SetTextColor(RGB(220,220,255));
				pDC->SetTextAlign(TA_LEFT | TA_TOP);
				pDC->SetBkMode(TRANSPARENT);
				CPen bbcPen ( PS_SOLID, 1, RGB(220,220,255) );
				CPen * pOldPen = pDC -> SelectObject ( & bbcPen );
				pDC->TextOutA( ptShape[1][nbptShape[1]].x+30,ptShape[1][nbptShape[1]].y-50,szText);
				pDC -> MoveTo (ptShape[1][nbptShape[1]].x+30,ptShape[1][nbptShape[1]].y-40 );
				pDC -> LineTo ( ptShape[1][nbptShape[1]].x,ptShape[1][nbptShape[1]].y );
				pDC->SelectObject(pOldFont);
				pDC -> SelectObject (pOldPen);
			}
			if (bCIEab)
			{
				// Initializes a CFont object with the specified characteristics. 
				CFont font;
				font.CreateFont(24,0,0,0,FW_THIN,FALSE,FALSE,FALSE,0,OUT_TT_ONLY_PRECIS,CLIP_DEFAULT_PRECIS,PROOF_QUALITY,VARIABLE_PITCH | FF_MODERN,_T("Garamond"));
				CFont* pOldFont = pDC->SelectObject(&font);
				BOOL doPrint = FALSE;

				CString szText;
				switch ( i ) 
				{
				case 6:
					szText.Format("%d nm",630);
					doPrint = TRUE;
					break;
				}

				if (doPrint)
				{
					pDC->SetTextColor(RGB(220,220,255));
					pDC->SetTextAlign(TA_CENTER);
					pDC->SetBkMode(TRANSPARENT);
					pDC->TextOutA( ptShape[1][nbptShape[1]].x,ptShape[1][nbptShape[1]].y - 35,szText);
					pDC->SelectObject(pOldFont);
				}
			}
			// Accept this point
			nbptShape [ 1 ] ++;
		}
	}

	// Retrieve minimum and maximum Y coordinate
	// We do not have to search them like in a classical polygon drawing scheme because they
	// are perfectly ordered in constant arrays
	MinY = ptShape [ 0 ] [ 0 ].y;
	MaxY = ptShape [ 0 ] [ nbptShape [ 0 ] - 1  ].y;

	// Initialize first left and right segments
	nCurrentPoint [ 0 ] = 0;
	nCurrentPoint [ 1 ] = 0;

	for ( i = 0; i < 2; i ++ )	
	{
		// Compute bresenham values
		TB [ i ].X = ptShape [ i ] [ nCurrentPoint [ i ] ].x;
		TB [ i ].First_X = ptShape [ i ] [ nCurrentPoint [ i ] ].x;
		TB [ i ].Last_X = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].x;
		TB [ i ].First_Y = ptShape [ i ] [ nCurrentPoint [ i ] ].y;
		TB [ i ].Last_Y = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].y;
		DeltaX = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].x - ptShape [ i ] [ nCurrentPoint [ i ] ].x;
		TB [ i ].IncX = ( DeltaX > 0 ) - ( DeltaX < 0 );
		AbsDeltaY = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].y - ptShape [ i ] [ nCurrentPoint [ i ] ].y;
		TB [ i ].AbsDeltaX = abs ( DeltaX );
		
		if ( TB [ i ].AbsDeltaX > AbsDeltaY )
			delta = TB [ i ].AbsDeltaX;
		else
			delta = AbsDeltaY;

		if ( ! TB [ i ].AbsDeltaX )
			TB [ i ].Config = 1;
		else if ( ! AbsDeltaY )
			TB [ i ].Config = 2;
		else if ( TB [ i ].AbsDeltaX > AbsDeltaY )
		{
			TB [ i ].Config = 3;
			TB [ i ].S1 = AbsDeltaY * 2;
			TB [ i ].S = TB [ i ].S1 - TB [ i ].AbsDeltaX;
			TB [ i ].S2 = TB [ i ].S - TB [ i ].AbsDeltaX;
		}
		else
		{
			TB [ i ].Config = 4;
			TB [ i ].S1 = TB [ i ].AbsDeltaX * 2;
			TB [ i ].S = TB [ i ].S1 - AbsDeltaY;
			TB [ i ].S2 = TB [ i ].S - AbsDeltaY;
		}
	}

	// Run along Y
	Y_Cour = MinY;
	while ( Y_Cour <= MaxY )
	{
		NbLimits = 0;
		// i is zéro for left side, 1 for right side
		for ( i = 0 ; i < 2 ; i ++ )
		{
			if ( Y_Cour <= TB [ i ].Last_Y )
			{
				// Run Bresenham algorythm and compute limits
				switch ( TB [ i ].Config )
				{
					case 1:
						 // Vertical segment
						 Limit [ NbLimits ] = TB [ i ].X;
						 if ( TB [ i ].Last_Y == Y_Cour )
							Mode [ NbLimits ] = MODE_SEGMENT_POINT;
						 else
							Mode [ NbLimits ] = MODE_SEGMENT_POINT_FLAG;

						 NbLimits ++;
						 break;

					case 2:
						 // Horizontal segment
						 if ( TB [ i ].IncX > 0 )
						 {
							Limit [ NbLimits ] = TB [ i ].X;
							Limit [ NbLimits + 1 ] = TB [ i ].X + TB [ i ].AbsDeltaX;
						 }
						 else
						 {
							Limit [ NbLimits ] = TB [ i ].X - TB [ i ].AbsDeltaX;
							Limit [ NbLimits + 1 ] = TB [ i ].X;
						 }

						 Mode [ NbLimits ++ ] = MODE_SEGMENT_START_LINE;
						 Mode [ NbLimits ++ ] = MODE_SEGMENT_END_LINE;
						 break;

					case 3:
						 // Quite horizontal (more than one point on a line)
						 // Runs Bresenham algorythm and count number of pixels on this line
						 j = 0;
						 while ( TB [ i ].AbsDeltaX > 0 )
						 {
							j ++;
							TB [ i ].X += TB [ i ].IncX;
							TB [ i ].AbsDeltaX  --;
					
							if ( TB [ i ].S < 0 )
								TB [ i ].S += TB [ i ].S1;
							else
							{
								TB [ i ].S += TB [ i ].S2;
								break;
							}
						 }
					
						 // Stores limit values
						 if ( TB [ i ].IncX > 0 )
						 {
							Limit [ NbLimits ] = TB [ i ].X - j;
							Limit [ NbLimits + 1 ] = TB [ i ].X;
						 }
						 else
						 {
							Limit [ NbLimits ] = TB [ i ].X;
							Limit [ NbLimits + 1 ] = TB [ i ].X + j;
						 }

						 if ( TB [ i ].Last_Y == Y_Cour )
							Mode [ NbLimits ] = MODE_SEGMENT_START_LINE;
						 else
							Mode [ NbLimits ] = MODE_SEGMENT_START_LINE_FLAG;
						 NbLimits ++;
				 
						 Mode [ NbLimits ] = MODE_SEGMENT_END_LINE;
						 NbLimits ++;
						 break;

					case 4:
						 // Quite vertical (only one point on a line)
						 Limit [ NbLimits ] = TB [ i ].X;
						 if ( TB [ i ].Last_Y == Y_Cour )
							Mode [ NbLimits ] = MODE_SEGMENT_POINT;
						 else
							Mode [ NbLimits ] = MODE_SEGMENT_POINT_FLAG;
						 NbLimits ++;

						 // Runs Bresenham algorythm
						 if ( TB [ i ].S < 0 )
							TB [ i ].S += TB [ i ].S1;
						 else
						 {
							TB [ i ].S += TB [ i ].S2;
							TB [ i ].X += TB [ i ].IncX;
						 }
						 break;
				}
			}
			
			if ( Y_Cour == TB [ i ].Last_Y && Y_Cour < MaxY )
			{
				// We need to change to the next segment
				nCurrentPoint [ i ] ++;

				// Compute bresenham values
				TB [ i ].X = ptShape [ i ] [ nCurrentPoint [ i ] ].x;
				TB [ i ].First_X = ptShape [ i ] [ nCurrentPoint [ i ] ].x;
				TB [ i ].Last_X = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].x;
				TB [ i ].First_Y = ptShape [ i ] [ nCurrentPoint [ i ] ].y;
				TB [ i ].Last_Y = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].y;
				DeltaX = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].x - ptShape [ i ] [ nCurrentPoint [ i ] ].x;
				TB [ i ].IncX = ( DeltaX > 0 ) - ( DeltaX < 0 );
				AbsDeltaY = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].y - ptShape [ i ] [ nCurrentPoint [ i ] ].y;
				TB [ i ].AbsDeltaX = abs ( DeltaX );
				
				if ( TB [ i ].AbsDeltaX > AbsDeltaY )
					delta = TB [ i ].AbsDeltaX;
				else
					delta = AbsDeltaY;

				if ( ! TB [ i ].AbsDeltaX )
					TB [ i ].Config = 1;
				else if ( ! AbsDeltaY )
					TB [ i ].Config = 2;
				else if ( TB [ i ].AbsDeltaX > AbsDeltaY )
				{
					TB [ i ].Config = 3;
					TB [ i ].S1 = AbsDeltaY * 2;
					TB [ i ].S = TB [ i ].S1 - TB [ i ].AbsDeltaX;
					TB [ i ].S2 = TB [ i ].S - TB [ i ].AbsDeltaX;
				}
				else
				{
					TB [ i ].Config = 4;
					TB [ i ].S1 = TB [ i ].AbsDeltaX * 2;
					TB [ i ].S = TB [ i ].S1 - AbsDeltaY;
					TB [ i ].S2 = TB [ i ].S - AbsDeltaY;
				}

				// Run Bresenham algorythm and compute limits for the newly replaced segment
				// (this is simply duplicated code)
				switch ( TB [ i ].Config )
				{
					case 1:
						 // Vertical segment
						 Limit [ NbLimits ] = TB [ i ].X;
						 if ( TB [ i ].Last_Y == Y_Cour )
							Mode [ NbLimits ] = MODE_SEGMENT_POINT;
						 else
							Mode [ NbLimits ] = MODE_SEGMENT_POINT_FLAG;

						 NbLimits ++;
						 break;

					case 2:
						 // Horizontal segment
						 if ( TB [ i ].IncX > 0 )
						 {
							Limit [ NbLimits ] = TB [ i ].X;
							Limit [ NbLimits + 1 ] = TB [ i ].X + TB [ i ].AbsDeltaX;
						 }
						 else
						 {
							Limit [ NbLimits ] = TB [ i ].X - TB [ i ].AbsDeltaX;
							Limit [ NbLimits + 1 ] = TB [ i ].X;
						 }

						 Mode [ NbLimits ++ ] = MODE_SEGMENT_START_LINE;
						 Mode [ NbLimits ++ ] = MODE_SEGMENT_END_LINE;
						 break;

					case 3:
						 // Quite horizontal (more than one point on a line)
						 // Runs Bresenham algorythm and count number of pixels on this line
						 j = 0;
						 while ( TB [ i ].AbsDeltaX > 0 )
						 {
							j ++;
							TB [ i ].X += TB [ i ].IncX;
							TB [ i ].AbsDeltaX  --;
					
							if ( TB [ i ].S < 0 )
								TB [ i ].S += TB [ i ].S1;
							else
							{
								TB [ i ].S += TB [ i ].S2;
								break;
							}
						 }
					
						 // Stores limit values
						 if ( TB [ i ].IncX > 0 )
						 {
							Limit [ NbLimits ] = TB [ i ].X - j;
							Limit [ NbLimits + 1 ] = TB [ i ].X;
						 }
						 else
						 {
							Limit [ NbLimits ] = TB [ i ].X;
							Limit [ NbLimits + 1 ] = TB [ i ].X + j;
						 }

						 if ( TB [ i ].Last_Y == Y_Cour )
							Mode [ NbLimits ] = MODE_SEGMENT_START_LINE;
						 else
							Mode [ NbLimits ] = MODE_SEGMENT_START_LINE_FLAG;
						 NbLimits ++;
				 
						 Mode [ NbLimits ] = MODE_SEGMENT_END_LINE;
						 NbLimits ++;
						 break;

					case 4:
						 // Quite vertical (only one point on a line)
						 Limit [ NbLimits ] = TB [ i ].X;
						 if ( TB [ i ].Last_Y == Y_Cour )
							Mode [ NbLimits ] = MODE_SEGMENT_POINT;
						 else
							Mode [ NbLimits ] = MODE_SEGMENT_POINT_FLAG;
						 NbLimits ++;

						 // Runs Bresenham algorythm
						 if ( TB [ i ].S < 0 )
							TB [ i ].S += TB [ i ].S1;
						 else
						 {
							TB [ i ].S += TB [ i ].S2;
							TB [ i ].X += TB [ i ].IncX;
						 }
						 break;
				}
			}
		}

		// Compute segments limits
		NbSegments = 0;
		bInPolygon = FALSE;
		bOnLine = FALSE;

		for ( i = 0 ; i < NbLimits ; i ++ )
		{
			switch ( Mode [ i ] )
			{
				case MODE_SEGMENT_POINT_FLAG:
					 bInPolygon = ! bInPolygon;

				case MODE_SEGMENT_POINT:
					 if ( bOnLine || bInPolygon )
					 {
						if ( NbSegments && To [ NbSegments - 1 ] == Limit [ i ] )
						{
							To [ NbSegments - 1 ] = Limit [ i + 1 ];
							ptTo [ NbSegments - 1 ] = ptLimit [ i + 1 ];
						}
						else
						{
							From [ NbSegments ] = Limit [ i ];
							To [ NbSegments ] = Limit [ i + 1 ];
							ptFrom [ NbSegments ] = ptLimit [ i ];
							ptTo [ NbSegments ] = ptLimit [ i + 1 ];
							NbSegments ++;
						}
					 }
					 else
					 {
						if ( ! NbSegments || To [ NbSegments - 1 ] != Limit [ i ] )
						{
							From [ NbSegments ] = Limit [ i ];
							To [ NbSegments ] = Limit [ i ];
							ptFrom [ NbSegments ] = ptLimit [ i ];
							ptTo [ NbSegments ] = ptLimit [ i ];
							NbSegments ++;
						}
					 }
					 break;

				case MODE_SEGMENT_START_LINE_FLAG:
					 bInPolygon = ! bInPolygon;
			
				case MODE_SEGMENT_START_LINE:
					 bOnLine ++;
					 if ( NbSegments && To [ NbSegments - 1 ] == Limit [ i ] )
					 {
						To [ NbSegments - 1 ] = Limit [ i + 1 ];
						ptTo [ NbSegments - 1 ] = ptLimit [ i + 1 ];
					 }
					 else
					 {
						From [ NbSegments ] = Limit [ i ];
						To [ NbSegments ] = Limit [ i + 1 ];
						ptFrom [ NbSegments ] = ptLimit [ i ];
						ptTo [ NbSegments ] = ptLimit [ i + 1 ];
						NbSegments ++;
					 }
					 break;

				case MODE_SEGMENT_END_LINE:
					 bOnLine --;
					 if ( bOnLine || bInPolygon )
					 {
						if ( NbSegments && To [ NbSegments - 1 ] == Limit [ i ] )
						{
							To [ NbSegments - 1 ] = Limit [ i + 1 ];
							ptTo [ NbSegments - 1 ] = ptLimit [ i + 1 ];
						}
						else
						{
							From [ NbSegments ] = Limit [ i ];
							To [ NbSegments ] = Limit [ i + 1 ];
							ptFrom [ NbSegments ] = ptLimit [ i ];
							ptTo [ NbSegments ] = ptLimit [ i + 1 ];
							NbSegments ++;
						}
					 }
					 break;
			}
		}
		
		for ( i = 0 ; i < NbSegments ; i ++ )
		{
			k = max ( 0, From [ i ] );
			l = min ( cxMax - 1, To [ i ] ) - k;
			if ( l >= 0 )
			{
				// Compute color of each pixel
				// Transform pixel coordinates (k, Y_Cour) in CIExy coordinates to determine its color
				if (bCIEab)
					yCieLine = (double) (( cyMax - Y_Cour ) * (400.)) / (double) cyMax - 200;
				else
					yCieLine = (double) (( cyMax - Y_Cour ) * (bCIEuv ? 0.8 : 1.0)) / (double) cyMax - 0.05;

				for ( j = 0 ; j <= l ; j ++ )
				{
					if ( bCIEuv )
					{
						// Use CIE u'v' coordinates: convert them into CIE xy coordinates to define pixel color
						uCie = (double) (k * 0.8) / (double) cxMax - 0.075;
						vCie = yCieLine;

						xCie = ( 9.0 * uCie ) / ( ( 6.0 * uCie ) - ( 16.0 * vCie ) + 12.0 );
						yCie = ( 4.0 * vCie ) / ( ( 6.0 * uCie ) - ( 16.0 * vCie ) + 12.0 );
					}
					else if (bCIEab)
					{
						xCie = (double) (k * 400.) / (double) cxMax - 220.;
						tmp[0]=xCie;
						tmp[1]=yCieLine;
						fx = tmp[0] / 500. + 1;
						fy = 1.0;
						fz = fy - tmp[1] / 200.;

						if (pow(fx,3.0) > ep)
							lxr = pow(fx,3.0);
						else
							lxr = (116. * fx - 16.) / kap;

						lyr = pow(fy,3);

						if (pow(fz,3.0) > ep)
							lzr = pow(fz,3.0);
						else
							lzr = (116 * fz -16) / kap;

						double X = (lxr * White[0]);
						double Y = (lyr * White[1]);
						double Z = (lzr * White[2]);

						xCie = X / (X + Y + Z);
						yCie = Y / (X + Y + Z);
					}
					else
					{
						xCie = (double) (k * 0.9) / (double) cxMax - 0.075;
						yCie = yCieLine;
					}

					// Test if we are inside white circle to enlight
					d = (xCie-xw)*(xCie-xw) + (yCie-yw)*(yCie-yw);
					if ( d < squared_white_ray && !bCIEab)
					{
						// Enlarge smoothly white zone
						d = pow ( d / squared_white_ray, 0.5 );
						xCie = xw + (xCie - xw ) * d;
						yCie = yw + (yCie - yw ) * d;
					}

					zCie = 1.0 - ( xCie + yCie );

					/* rgb of the desired point */
					val_r = rx*xCie + ry*yCie + rz*zCie;
					val_g = gx*xCie + gy*yCie + gz*zCie;
					val_b = bx*xCie + by*yCie + bz*zCie;
					
					val_min = min ( val_r, val_g );
					if ( val_b < val_min )
						val_min = val_b;
					
					if (val_min < 0) 
					{
						val_r -= val_min;
						val_g -= val_min;
						val_b -= val_min;
					}
						
					// Scale to max(rgb) = 1. 
					val_max = max ( val_r, val_g );
					if ( val_b > val_max )
						val_max = val_b;
					
					if ( val_max > 0 ) 
					{
						val_r /= val_max;
						val_g /= val_max;
						val_b /= val_max;
					}
					
					Color = RGB ( (int)(pow(val_r,gamma) * nMaxRgbVal), (int)(pow(val_g,gamma) * nMaxRgbVal), (int)(pow(val_b,gamma) * nMaxRgbVal) );

					pDC -> SetPixel ( k ++, Y_Cour, Color );
				}
			}
		}

		Y_Cour ++;
	}

	if ( doShowBlack )
	{
		// Draw black body curve on CIE diagram
		DrawBlackBodyCurve ( pDC, cxMax, cyMax, doFullChart, bCIEuv, bCIEab );
	}

	CPen bbcPen ( PS_SOLID, 2, ( doFullChart ? RGB(0,0,0) : RGB(255,167,0) ) );
	CPen * pOldPen = pDC -> SelectObject ( & bbcPen );
	pDC -> MoveTo(ptShape[0][0].x,ptShape[0][0].y);
	for (i = 1; i < nbptShape[0]; i++)
			pDC -> LineTo ( ptShape[0][i].x,ptShape[0][i].y );

	pDC -> MoveTo(ptShape[1][0].x,ptShape[1][0].y);
	for (i = 1; i < nbptShape[1]; i++)
			pDC -> LineTo ( ptShape[1][i].x,ptShape[1][i].y );

	pDC -> LineTo ( ptShape[0][nbptShape[0]-1].x,ptShape[0][nbptShape[0]-1].y );
	pDC -> SelectObject ( pOldPen );
}

/////////////////////////////////////////////////////////////////////////////
// This function is a specialized "outside-polygon" tracing function
// It draws a white surrounding around CIE tongue.
// The algorythm is very similar to DrawCIEChart, to draw the outside of the same figure

void DrawCIEChartWhiteSurrounding(CDC* pDC, int cxMax, int cyMax, BOOL bCIEuv, BOOL bCIEab ) 
{
	int			i, j;
	int			MinY, MaxY;
	int			Y_Cour;
	int			NbLeftPoints, NbRightPoints;
	BOOL		bInPolygon;
	BOOL		bOnLine;
	int			Limit [ 8 ];
	int			Mode [ 8 ];
	POINT		ptLimit [ 8 ];
	int			NbLimits;
	int			From [ 4 ];
	int			To [ 4 ];
	POINT		ptFrom [ 4 ];
	POINT		ptTo [ 4 ];
	int			NbSegments;

	int			DeltaX;
	int			AbsDeltaY;
	int			delta;
	int			nCurrentPoint [ 2 ];	// Index 0 is left side of the tongue, Index 1 is right side
	int			nbptShape [ 2 ];		// Index 0 is left side of the tongue, Index 1 is right side
	POINT		ptShape [ 2 ] [ 128 ];	// Index 0 is left side of the tongue, Index 1 is right side
	Elem_Bres	TB [ 2 ];				// Index 0 is left side of the tongue, Index 1 is right side
	double		xCie, yCie;
	double		(*pLeft) [2];
	double		(*pRight) [2];

	CPen		WhitePen ( PS_SOLID,1,RGB(255,255,255 ) );
	
	if ( bCIEuv )
	{
		NbLeftPoints = sizeof ( spectral_chromaticity_uv_left ) / sizeof ( spectral_chromaticity_uv_left [ 0 ] );
		pLeft = spectral_chromaticity_uv_left;

		NbRightPoints = sizeof ( spectral_chromaticity_uv_right ) / sizeof ( spectral_chromaticity_uv_right [ 0 ] );
		pRight = spectral_chromaticity_uv_right;
	}
	else if (bCIEab)
	{
		NbLeftPoints = sizeof ( spectral_chromaticity_ab_left ) / sizeof ( spectral_chromaticity_ab_left [ 0 ] );
		pLeft = spectral_chromaticity_ab_left;

		NbRightPoints = sizeof ( spectral_chromaticity_ab_right ) / sizeof ( spectral_chromaticity_ab_right [ 0 ] );
		pRight = spectral_chromaticity_ab_right;
	}
	else
	{
		NbLeftPoints = sizeof ( spectral_chromaticity_left ) / sizeof ( spectral_chromaticity_left [ 0 ] );
		pLeft = spectral_chromaticity_left;

		NbRightPoints = sizeof ( spectral_chromaticity_right ) / sizeof ( spectral_chromaticity_right [ 0 ] );
		pRight = spectral_chromaticity_right;
	}

	// compute parameter points 
	nbptShape [ 0 ] = 0;
	for ( i = 0; i < NbLeftPoints ; i ++ )
	{
		// Get CIExy or CIE u'v' coordinates and convert it in pixel coordinates in (0,0) (cxMax,cyMax)
		// Note: max CIExy coordinates are (0.8, 0.9), max CIEuv are (0.7, 0.7)
		xCie = pLeft [ i ] [ 0 ];
		yCie = pLeft [ i ] [ 1 ];

		ptShape [ 0 ] [ nbptShape [ 0 ] ].x = (int) ( ( (xCie+.075) / (bCIEuv ? 0.8 : 0.9) ) * (double) cxMax );
		ptShape [ 0 ] [ nbptShape [ 0 ] ].y = (int) ( ( 1.0 - ( (yCie+.05) / (bCIEuv ? 0.8 : 1.0) ) ) * (double) cyMax );

		if (bCIEab)
		{
			ptShape [ 0 ] [ nbptShape [ 0 ] ].x = (int) ( ( (xCie + 220.) / (400.) ) * (double) cxMax );
			ptShape [ 0 ] [ nbptShape [ 0 ] ].y = (int) ( ( 1.0 - ( (yCie + 200.) / (400.) ) ) * (double) cyMax );
		}

		if ( nbptShape [ 0 ] == 0 || ptShape [ 0 ] [ nbptShape [ 0 ] ].y > ptShape [ 0 ] [ nbptShape [ 0 ] - 1 ].y )
		{
			// Accept this point
			nbptShape [ 0 ] ++;
		}
	}

	nbptShape [ 1 ] = 0;
	for ( i = 0; i < NbRightPoints ; i ++ )
	{
		// Get CIExy or CIE u'v' coordinates and convert it in pixel coordinates in (0,0) (cxMax,cyMax)
		// Note: max CIExy coordinates are (0.8, 0.9)
		xCie = pRight [ i ] [ 0 ];
		yCie = pRight [ i ] [ 1 ];

		ptShape [ 1 ] [ nbptShape [ 1 ] ].x = (int) ( ( (xCie+.075) / (bCIEuv ? 0.8 : 0.9) ) * (double) cxMax );
		ptShape [ 1 ] [ nbptShape [ 1 ] ].y = (int) ( ( 1.0 - ( (yCie+.05) / (bCIEuv ? 0.8 : 1.0) ) ) * (double) cyMax );
		
		if (bCIEab)
		{
			ptShape [ 1 ] [ nbptShape [ 1 ] ].x = (int) ( ( (xCie + 220.) / (400.) ) * (double) cxMax );
			ptShape [ 1 ] [ nbptShape [ 1 ] ].y = (int) ( ( 1.0 - ( (yCie + 200.) / (400.) ) ) * (double) cyMax );
		}

		if ( nbptShape [ 1 ] == 0 || ptShape [ 1 ] [ nbptShape [ 1 ] ].y > ptShape [ 1 ] [ nbptShape [ 1 ] - 1 ].y )
		{
			// Accept this point
			nbptShape [ 1 ] ++;
		}
	}

	// Retrieve minimum and maximum Y coordinate
	// We do not have to search them like in a classical polygon drawing scheme because they
	// are perfectly ordered in constant arrays
	MinY = ptShape [ 0 ] [ 0 ].y;
	MaxY = ptShape [ 0 ] [ nbptShape [ 0 ] - 1  ].y;

	// Initialize first left and right segments
	nCurrentPoint [ 0 ] = 0;
	nCurrentPoint [ 1 ] = 0;

	for ( i = 0; i < 2; i ++ )	
	{
		// Compute bresenham values
		TB [ i ].X = ptShape [ i ] [ nCurrentPoint [ i ] ].x;
		TB [ i ].First_X = ptShape [ i ] [ nCurrentPoint [ i ] ].x;
		TB [ i ].Last_X = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].x;
		TB [ i ].First_Y = ptShape [ i ] [ nCurrentPoint [ i ] ].y;
		TB [ i ].Last_Y = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].y;
		DeltaX = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].x - ptShape [ i ] [ nCurrentPoint [ i ] ].x;
		TB [ i ].IncX = ( DeltaX > 0 ) - ( DeltaX < 0 );
		AbsDeltaY = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].y - ptShape [ i ] [ nCurrentPoint [ i ] ].y;
		TB [ i ].AbsDeltaX = abs ( DeltaX );
		
		if ( TB [ i ].AbsDeltaX > AbsDeltaY )
			delta = TB [ i ].AbsDeltaX;
		else
			delta = AbsDeltaY;

		if ( ! TB [ i ].AbsDeltaX )
			TB [ i ].Config = 1;
		else if ( ! AbsDeltaY )
			TB [ i ].Config = 2;
		else if ( TB [ i ].AbsDeltaX > AbsDeltaY )
		{
			TB [ i ].Config = 3;
			TB [ i ].S1 = AbsDeltaY * 2;
			TB [ i ].S = TB [ i ].S1 - TB [ i ].AbsDeltaX;
			TB [ i ].S2 = TB [ i ].S - TB [ i ].AbsDeltaX;
		}
		else
		{
			TB [ i ].Config = 4;
			TB [ i ].S1 = TB [ i ].AbsDeltaX * 2;
			TB [ i ].S = TB [ i ].S1 - AbsDeltaY;
			TB [ i ].S2 = TB [ i ].S - AbsDeltaY;
		}
	}

	pDC -> FillSolidRect ( 0, 0, cxMax, MinY, RGB(255,255,255) );
	pDC -> FillSolidRect ( 0, MaxY, cxMax, cyMax, RGB(255,255,255) );

	CPen * pOldPen = pDC -> SelectObject ( & WhitePen );

	// Run along Y
	Y_Cour = MinY;
	while ( Y_Cour <= MaxY )
	{
		NbLimits = 0;
		// i is zéro for left side, 1 for right side
		for ( i = 0 ; i < 2 ; i ++ )
		{
			if ( Y_Cour <= TB [ i ].Last_Y )
			{
				// Run Bresenham algorythm and compute limits
				switch ( TB [ i ].Config )
				{
					case 1:
						 // Vertical segment
						 Limit [ NbLimits ] = TB [ i ].X;
						 if ( TB [ i ].Last_Y == Y_Cour )
							Mode [ NbLimits ] = MODE_SEGMENT_POINT;
						 else
							Mode [ NbLimits ] = MODE_SEGMENT_POINT_FLAG;

						 NbLimits ++;
						 break;

					case 2:
						 // Horizontal segment
						 if ( TB [ i ].IncX > 0 )
						 {
							Limit [ NbLimits ] = TB [ i ].X;
							Limit [ NbLimits + 1 ] = TB [ i ].X + TB [ i ].AbsDeltaX;
						 }
						 else
						 {
							Limit [ NbLimits ] = TB [ i ].X - TB [ i ].AbsDeltaX;
							Limit [ NbLimits + 1 ] = TB [ i ].X;
						 }

						 Mode [ NbLimits ++ ] = MODE_SEGMENT_START_LINE;
						 Mode [ NbLimits ++ ] = MODE_SEGMENT_END_LINE;
						 break;

					case 3:
						 // Quite horizontal (more than one point on a line)
						 // Runs Bresenham algorythm and count number of pixels on this line
						 j = 0;
						 while ( TB [ i ].AbsDeltaX > 0 )
						 {
							j ++;
							TB [ i ].X += TB [ i ].IncX;
							TB [ i ].AbsDeltaX  --;
					
							if ( TB [ i ].S < 0 )
								TB [ i ].S += TB [ i ].S1;
							else
							{
								TB [ i ].S += TB [ i ].S2;
								break;
							}
						 }
					
						 // Stores limit values
						 if ( TB [ i ].IncX > 0 )
						 {
							Limit [ NbLimits ] = TB [ i ].X - j;
							Limit [ NbLimits + 1 ] = TB [ i ].X;
						 }
						 else
						 {
							Limit [ NbLimits ] = TB [ i ].X;
							Limit [ NbLimits + 1 ] = TB [ i ].X + j;
						 }

						 if ( TB [ i ].Last_Y == Y_Cour )
							Mode [ NbLimits ] = MODE_SEGMENT_START_LINE;
						 else
							Mode [ NbLimits ] = MODE_SEGMENT_START_LINE_FLAG;
						 NbLimits ++;
				 
						 Mode [ NbLimits ] = MODE_SEGMENT_END_LINE;
						 NbLimits ++;
						 break;

					case 4:
						 // Quite vertical (only one point on a line)
						 Limit [ NbLimits ] = TB [ i ].X;
						 if ( TB [ i ].Last_Y == Y_Cour )
							Mode [ NbLimits ] = MODE_SEGMENT_POINT;
						 else
							Mode [ NbLimits ] = MODE_SEGMENT_POINT_FLAG;
						 NbLimits ++;

						 // Runs Bresenham algorythm
						 if ( TB [ i ].S < 0 )
							TB [ i ].S += TB [ i ].S1;
						 else
						 {
							TB [ i ].S += TB [ i ].S2;
							TB [ i ].X += TB [ i ].IncX;
						 }
						 break;
				}
			}
			
			if ( Y_Cour == TB [ i ].Last_Y )
			{
				// We need to change to the next segment
				nCurrentPoint [ i ] ++;

				// Compute bresenham values
				TB [ i ].X = ptShape [ i ] [ nCurrentPoint [ i ] ].x;
				TB [ i ].First_X = ptShape [ i ] [ nCurrentPoint [ i ] ].x;
				TB [ i ].Last_X = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].x;
				TB [ i ].First_Y = ptShape [ i ] [ nCurrentPoint [ i ] ].y;
				TB [ i ].Last_Y = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].y;
				DeltaX = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].x - ptShape [ i ] [ nCurrentPoint [ i ] ].x;
				TB [ i ].IncX = ( DeltaX > 0 ) - ( DeltaX < 0 );
				AbsDeltaY = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].y - ptShape [ i ] [ nCurrentPoint [ i ] ].y;
				TB [ i ].AbsDeltaX = abs ( DeltaX );
				
				if ( TB [ i ].AbsDeltaX > AbsDeltaY )
					delta = TB [ i ].AbsDeltaX;
				else
					delta = AbsDeltaY;

				if ( ! TB [ i ].AbsDeltaX )
					TB [ i ].Config = 1;
				else if ( ! AbsDeltaY )
					TB [ i ].Config = 2;
				else if ( TB [ i ].AbsDeltaX > AbsDeltaY )
				{
					TB [ i ].Config = 3;
					TB [ i ].S1 = AbsDeltaY * 2;
					TB [ i ].S = TB [ i ].S1 - TB [ i ].AbsDeltaX;
					TB [ i ].S2 = TB [ i ].S - TB [ i ].AbsDeltaX;
				}
				else
				{
					TB [ i ].Config = 4;
					TB [ i ].S1 = TB [ i ].AbsDeltaX * 2;
					TB [ i ].S = TB [ i ].S1 - AbsDeltaY;
					TB [ i ].S2 = TB [ i ].S - AbsDeltaY;
				}

				// Run Bresenham algorythm and compute limits for the newly replaced segment
				// (this is simply duplicated code)
				switch ( TB [ i ].Config )
				{
					case 1:
						 // Vertical segment
						 Limit [ NbLimits ] = TB [ i ].X;
						 if ( TB [ i ].Last_Y == Y_Cour )
							Mode [ NbLimits ] = MODE_SEGMENT_POINT;
						 else
							Mode [ NbLimits ] = MODE_SEGMENT_POINT_FLAG;

						 NbLimits ++;
						 break;

					case 2:
						 // Horizontal segment
						 if ( TB [ i ].IncX > 0 )
						 {
							Limit [ NbLimits ] = TB [ i ].X;
							Limit [ NbLimits + 1 ] = TB [ i ].X + TB [ i ].AbsDeltaX;
						 }
						 else
						 {
							Limit [ NbLimits ] = TB [ i ].X - TB [ i ].AbsDeltaX;
							Limit [ NbLimits + 1 ] = TB [ i ].X;
						 }

						 Mode [ NbLimits ++ ] = MODE_SEGMENT_START_LINE;
						 Mode [ NbLimits ++ ] = MODE_SEGMENT_END_LINE;
						 break;

					case 3:
						 // Quite horizontal (more than one point on a line)
						 // Runs Bresenham algorythm and count number of pixels on this line
						 j = 0;
						 while ( TB [ i ].AbsDeltaX > 0 )
						 {
							j ++;
							TB [ i ].X += TB [ i ].IncX;
							TB [ i ].AbsDeltaX  --;
					
							if ( TB [ i ].S < 0 )
								TB [ i ].S += TB [ i ].S1;
							else
							{
								TB [ i ].S += TB [ i ].S2;
								break;
							}
						 }
					
						 // Stores limit values
						 if ( TB [ i ].IncX > 0 )
						 {
							Limit [ NbLimits ] = TB [ i ].X - j;
							Limit [ NbLimits + 1 ] = TB [ i ].X;
						 }
						 else
						 {
							Limit [ NbLimits ] = TB [ i ].X;
							Limit [ NbLimits + 1 ] = TB [ i ].X + j;
						 }

						 if ( TB [ i ].Last_Y == Y_Cour )
							Mode [ NbLimits ] = MODE_SEGMENT_START_LINE;
						 else
							Mode [ NbLimits ] = MODE_SEGMENT_START_LINE_FLAG;
						 NbLimits ++;
				 
						 Mode [ NbLimits ] = MODE_SEGMENT_END_LINE;
						 NbLimits ++;
						 break;

					case 4:
						 // Quite vertical (only one point on a line)
						 Limit [ NbLimits ] = TB [ i ].X;
						 if ( TB [ i ].Last_Y == Y_Cour )
							Mode [ NbLimits ] = MODE_SEGMENT_POINT;
						 else
							Mode [ NbLimits ] = MODE_SEGMENT_POINT_FLAG;
						 NbLimits ++;

						 // Runs Bresenham algorythm
						 if ( TB [ i ].S < 0 )
							TB [ i ].S += TB [ i ].S1;
						 else
						 {
							TB [ i ].S += TB [ i ].S2;
							TB [ i ].X += TB [ i ].IncX;
						 }
						 break;
				}
			}
		}

		// Compute segments limits
		NbSegments = 0;
		bInPolygon = FALSE;
		bOnLine = FALSE;

		for ( i = 0 ; i < NbLimits ; i ++ )
		{
			switch ( Mode [ i ] )
			{
				case MODE_SEGMENT_POINT_FLAG:
					 bInPolygon = ! bInPolygon;

				case MODE_SEGMENT_POINT:
					 if ( bOnLine || bInPolygon )
					 {
						if ( NbSegments && To [ NbSegments - 1 ] == Limit [ i ] )
						{
							To [ NbSegments - 1 ] = Limit [ i + 1 ];
							ptTo [ NbSegments - 1 ] = ptLimit [ i + 1 ];
						}
						else
						{
							From [ NbSegments ] = Limit [ i ];
							To [ NbSegments ] = Limit [ i + 1 ];
							ptFrom [ NbSegments ] = ptLimit [ i ];
							ptTo [ NbSegments ] = ptLimit [ i + 1 ];
							NbSegments ++;
						}
					 }
					 else
					 {
						if ( ! NbSegments || To [ NbSegments - 1 ] != Limit [ i ] )
						{
							From [ NbSegments ] = Limit [ i ];
							To [ NbSegments ] = Limit [ i ];
							ptFrom [ NbSegments ] = ptLimit [ i ];
							ptTo [ NbSegments ] = ptLimit [ i ];
							NbSegments ++;
						}
					 }
					 break;

				case MODE_SEGMENT_START_LINE_FLAG:
					 bInPolygon = ! bInPolygon;
			
				case MODE_SEGMENT_START_LINE:
					 bOnLine ++;
					 if ( NbSegments && To [ NbSegments - 1 ] == Limit [ i ] )
					 {
						To [ NbSegments - 1 ] = Limit [ i + 1 ];
						ptTo [ NbSegments - 1 ] = ptLimit [ i + 1 ];
					 }
					 else
					 {
						From [ NbSegments ] = Limit [ i ];
						To [ NbSegments ] = Limit [ i + 1 ];
						ptFrom [ NbSegments ] = ptLimit [ i ];
						ptTo [ NbSegments ] = ptLimit [ i + 1 ];
						NbSegments ++;
					 }
					 break;

				case MODE_SEGMENT_END_LINE:
					 bOnLine --;
					 if ( bOnLine || bInPolygon )
					 {
						if ( NbSegments && To [ NbSegments - 1 ] == Limit [ i ] )
						{
							To [ NbSegments - 1 ] = Limit [ i + 1 ];
							ptTo [ NbSegments - 1 ] = ptLimit [ i + 1 ];
						}
						else
						{
							From [ NbSegments ] = Limit [ i ];
							To [ NbSegments ] = Limit [ i + 1 ];
							ptFrom [ NbSegments ] = ptLimit [ i ];
							ptTo [ NbSegments ] = ptLimit [ i + 1 ];
							NbSegments ++;
						}
					 }
					 break;
			}
		}
		
		// Get left white segment xmax
		i = max ( 0, From [ 0 ] );
		if ( i > 0 )
		{
			pDC -> MoveTo ( 0, Y_Cour );
			pDC -> LineTo ( i, Y_Cour );
		}

		// Get right white segment xmin
		i = To [ NbSegments - 1 ] + 1;

		if ( i < cxMax )
		{
			pDC -> MoveTo ( i, Y_Cour );
			pDC -> LineTo ( cxMax, Y_Cour );
		}

		Y_Cour ++;
	}

	pDC -> SelectObject ( pOldPen );

	nbptShape [ 0 ] = 0;
	for ( i = 0; i < NbLeftPoints ; i ++ )
	{
		// Get CIExy or CIE u'v' coordinates and convert it in pixel coordinates in (0,0) (cxMax,cyMax)
		// Note: max CIExy coordinates are (0.8, 0.9), max CIEuv are (0.7, 0.7)
		xCie = pLeft [ i ] [ 0 ];
		yCie = pLeft [ i ] [ 1 ];

		ptShape [ 0 ] [ nbptShape [ 0 ] ].x = (int) ( ( (xCie+.075) / (bCIEuv ? 0.8 : 0.9) ) * (double) cxMax );
		ptShape [ 0 ] [ nbptShape [ 0 ] ].y = (int) ( ( 1.0 - ( (yCie+.05) / (bCIEuv ? 0.8 : 1.0) ) ) * (double) cyMax );

		if (bCIEab)
		{
			ptShape [ 0 ] [ nbptShape [ 0 ] ].x = (int) ( ( (xCie + 220.) / (400.) ) * (double) cxMax );
			ptShape [ 0 ] [ nbptShape [ 0 ] ].y = (int) ( ( 1.0 - ( (yCie + 200.) / (400.) ) ) * (double) cyMax );
		}

		if ( nbptShape [ 0 ] == 0 || ptShape [ 0 ] [ nbptShape [ 0 ] ].y > ptShape [ 0 ] [ nbptShape [ 0 ] - 1 ].y )
		{
			// Accept this point
			if ((i*2 % 20) == 0 && !bCIEab)
			{

				// Initializes a CFont object with the specified characteristics. 
				CFont font,font1;
				font.CreateFont(24,0,0,0,FW_THIN,FALSE,FALSE,FALSE,0,OUT_TT_ONLY_PRECIS,CLIP_DEFAULT_PRECIS,PROOF_QUALITY,VARIABLE_PITCH | FF_MODERN,_T("Garamond"));
				font1.CreateFont(24,0,-180,-180,FW_THIN,FALSE,FALSE,FALSE,0,OUT_TT_ONLY_PRECIS,CLIP_DEFAULT_PRECIS,PROOF_QUALITY,VARIABLE_PITCH | FF_MODERN,_T("Garamond"));
				CFont* pOldFont = pDC->SelectObject(&font);

				CString szText;
				if (520 - i*2 != 520)
				{
					szText.Format("%d nm",520 - i*2);
					pDC->SetTextColor(RGB(10,10,200));
					pDC->SetTextAlign(TA_CENTER);
					pDC->SetBkMode(TRANSPARENT);
					CPen bbcPen ( PS_SOLID, 1, RGB(10,10,200)  );
					CPen * pOldPen = pDC -> SelectObject ( & bbcPen );
					pDC -> MoveTo (ptShape[0][nbptShape[0]].x,ptShape[0][nbptShape[0]].y );
					pDC -> LineTo ( ptShape[0][nbptShape[0]].x-30,ptShape[0][nbptShape[0]].y+10 );
					pDC->TextOutA( ptShape[0][nbptShape[0]].x-50,ptShape[0][nbptShape[0]].y+10,szText);
					pDC->SelectObject(pOldFont);
					pDC -> SelectObject (pOldPen);
				}
			}
			if (bCIEab)
			{
				// Initializes a CFont object with the specified characteristics. 
				CFont font,font1;
				font.CreateFont(24,0,0,0,FW_THIN,FALSE,FALSE,FALSE,0,OUT_TT_ONLY_PRECIS,CLIP_DEFAULT_PRECIS,PROOF_QUALITY,VARIABLE_PITCH | FF_MODERN,_T("Garamond"));
				font1.CreateFont(24,0,-220,-220,FW_THIN,FALSE,FALSE,FALSE,0,OUT_TT_ONLY_PRECIS,CLIP_DEFAULT_PRECIS,PROOF_QUALITY,VARIABLE_PITCH | FF_MODERN,_T("Garamond"));
				CFont* pOldFont = pDC->SelectObject(&font);
				BOOL doPrint = FALSE;

				CString szText;
				switch ( i ) 
				{
				case 0:
					szText.Format("%d%% Pointer's Gamut Coverage with Real Primaries",100);
					doPrint = TRUE;
					break;
				case 6:
					szText.Format("%d nm",527);
					doPrint = TRUE;
					break;
				case 18:
					szText.Format("%d nm",467);
					doPrint = TRUE;
					break;
				}

				if (doPrint)
				{
					pDC->SetTextColor(RGB(10,10,200));
					pDC->SetTextAlign(TA_CENTER);
					pDC->SetBkMode(TRANSPARENT);
					if (i == 0)
					{
						pDC->SelectObject(pOldFont);
						CFont* pOldFont = pDC->SelectObject(&font1);
						pDC->TextOutA( 600, 710, szText);
					}
					else if (i == 6)
						pDC->TextOutA( ptShape[0][nbptShape[0]].x+30,ptShape[0][nbptShape[0]].y-25,szText);
					else
						pDC->TextOutA( ptShape[0][nbptShape[0]].x,ptShape[0][nbptShape[0]].y+25,szText);

					pDC->SelectObject(pOldFont);
				}
			}
			nbptShape [ 0 ] ++;
		}
	}

	nbptShape [ 1 ] = 0;
	for ( i = 0; i < NbRightPoints ; i ++ )
	{
		// Get CIExy or CIE u'v' coordinates and convert it in pixel coordinates in (0,0) (cxMax,cyMax)
		// Note: max CIExy coordinates are (0.8, 0.9)
		xCie = pRight [ i ] [ 0 ];
		yCie = pRight [ i ] [ 1 ];

		ptShape [ 1 ] [ nbptShape [ 1 ] ].x = (int) ( ( (xCie+.075) / (bCIEuv ? 0.8 : 0.9) ) * (double) cxMax);
		ptShape [ 1 ] [ nbptShape [ 1 ] ].y = (int) ( ( 1.0 - ( (yCie+.05) / (bCIEuv ? 0.8 : 1.0) ) ) * (double) cyMax );
		
		if (bCIEab)
		{
			ptShape [ 1 ] [ nbptShape [ 1 ] ].x = (int) ( ( (xCie + 220.) / (400.) ) * (double) cxMax );
			ptShape [ 1 ] [ nbptShape [ 1 ] ].y = (int) ( ( 1.0 - ( (yCie + 200.) / (400.) ) ) * (double) cyMax );
		}

		if ( nbptShape [ 1 ] == 0 || ptShape [ 1 ] [ nbptShape [ 1 ] ].y > ptShape [ 1 ] [ nbptShape [ 1 ] - 1 ].y )
		{
			// 2nm increments label every 10nm mod(i*2 /10) = 0
			if ((i*2 % 20) == 0 && !bCIEab)
			{
				// Initializes a CFont object with the specified characteristics. 
				CFont font;
				font.CreateFont(24,0,(bCIEuv?300:0),(bCIEuv?300:0),FW_THIN,FALSE,FALSE,FALSE,0,OUT_TT_ONLY_PRECIS,CLIP_DEFAULT_PRECIS,PROOF_QUALITY,VARIABLE_PITCH | FF_MODERN,_T("Garamond"));
				CFont* pOldFont = pDC->SelectObject(&font);
				CString szText;
				szText.Format("%d nm",i*2 + 520);
				pDC->SetTextColor(RGB(10,10,200));
				pDC->SetTextAlign(TA_LEFT | TA_TOP);
				pDC->SetBkMode(TRANSPARENT);
				CPen bbcPen ( PS_SOLID, 1, RGB(10,10,200) );
				CPen * pOldPen = pDC -> SelectObject ( & bbcPen );
				pDC->TextOutA( ptShape[1][nbptShape[1]].x+30,ptShape[1][nbptShape[1]].y-50,szText);
				pDC -> MoveTo (ptShape[1][nbptShape[1]].x+30,ptShape[1][nbptShape[1]].y-40 );
				pDC -> LineTo ( ptShape[1][nbptShape[1]].x,ptShape[1][nbptShape[1]].y );
				pDC->SelectObject(pOldFont);
				pDC -> SelectObject (pOldPen);
			}
			if (bCIEab)
			{
				// Initializes a CFont object with the specified characteristics. 
				CFont font;
				font.CreateFont(24,0,0,0,FW_THIN,FALSE,FALSE,FALSE,0,OUT_TT_ONLY_PRECIS,CLIP_DEFAULT_PRECIS,PROOF_QUALITY,VARIABLE_PITCH | FF_MODERN,_T("Garamond"));
				CFont* pOldFont = pDC->SelectObject(&font);
				BOOL doPrint = FALSE;

				CString szText;
				switch ( i ) 
				{
				case 1:
					szText.Format("%d nm",630);
					doPrint = TRUE;
					break;
				}

				if (doPrint)
				{
					pDC->SetTextColor(RGB(10,10,200));
					pDC->SetTextAlign(TA_CENTER);
					pDC->SetBkMode(TRANSPARENT);
					pDC->TextOutA( ptShape[1][nbptShape[1]].x,ptShape[1][nbptShape[1]].y-25,szText);
					pDC->SelectObject(pOldFont);
				}
			}
			// Accept this point
			nbptShape [ 1 ] ++;
		}
	}

}

// Draw a polygon matching DeltaE value around white reference point
// Delta E is computed in CIE u'v' space, and need conversion for CIE xy

void DrawDeltaECurve(CDC* pDC, int cxMax, int cyMax, double DeltaE, BOOL bCIEuv, BOOL bCIEab ) 
{
	int			i;
	const int	nbPoints = 24;
	POINT		pt [ nbPoints + 1 ];
	double		xCie, yCie;
	double		uCie, vCie;
	double		uvRay = DeltaE / 1300.0;
	double		ang;
	CColor		WhiteReference = GetColorReference().GetWhite();
	double		xr=WhiteReference.GetxyYValue()[0];
	double		yr=WhiteReference.GetxyYValue()[1];
	double		ur = 4.0*xr / (-2.0*xr + 12.0*yr + 3.0); 
	double		vr = 9.0*yr / (-2.0*xr + 12.0*yr + 3.0); 

	const double twopi = acos (-1.0) * 2.0;

	if (bCIEab)
	{
		ur = 0;
		vr = 0;
	}

	for ( i = 0; i < nbPoints ; i ++ )
	{
		ang = twopi * ( (double) i ) / ( (double) nbPoints );

		uCie = ur + ( uvRay * cos ( ang ) );
		vCie = vr + ( uvRay * sin ( ang ) );

		if ( bCIEuv || bCIEab)
		{
			xCie = uCie;
			yCie = vCie;
		}
		else
		{
			xCie = ( 9.0 * uCie ) / ( ( 6.0 * uCie ) - ( 16.0 * vCie ) + 12.0 );
			yCie = ( 4.0 * vCie ) / ( ( 6.0 * uCie ) - ( 16.0 * vCie ) + 12.0 );
		}

		pt [ i ].x = (int) ( ( (xCie+.075) / (bCIEuv ? 0.8 : 0.9) ) * (double) cxMax );
		pt [ i ].y = (int) ( ( 1.0 - ( (yCie+.05) / (bCIEuv ? 0.8 : 1.0) ) ) * (double) cyMax );
		if (bCIEab)
		{
			pt [ i ].x = (int) ( ( (xCie + 220) / (400) ) * (double) cxMax );
			pt [ i ].y = (int) ( ( 1.0 - ( (yCie + 200) / (400) ) ) * (double) cyMax );
		}
	}
	
	// Close figure
	pt [ nbPoints ] = pt [ 0 ];

	CPen dEPen ( PS_SOLID, 2, RGB(0,0,255) );
	CPen * pOldPen = pDC -> SelectObject ( & dEPen );
	pDC -> Polyline ( pt, nbPoints + 1 );
	pDC -> SelectObject ( pOldPen );
}
