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
//	François-Xavier CHABOUD
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// Generator.cpp: implementation of the CGenerator class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ColorHCFR.h"
#include "GDIGenerator.h"
#include "Color.h"
#include "madTPG.h"
#include "Generator.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IMPLEMENT_SERIAL(CGenerator, CObject, 1) ;
dispwin *dw;
typedef int (__stdcall *RB8PG_send)(SOCKET sock,const char *message);
typedef int (__stdcall *RB8PG_close)(SOCKET sock);
typedef SOCKET (__stdcall *RB8PG_connect)(const char *server_addr);
typedef char * (__stdcall *RB8PG_discovery)();

CGenerator::CGenerator()
{
	m_isModified=FALSE;
	m_doScreenBlanking=GetConfig()->GetProfileInt("Generator","Blanking",0);
	m_rectSizePercent=GetConfig()->GetProfileInt("GDIGenerator","SizePercent",10);
	m_ccastIp = 0;
	sock = NULL;
	rPi_xWidth = 1980;
	rPi_yHeight = 1080;
	rPi_memSize  = 0;
	AddPropertyPage(&m_GeneratorPropertiePage);

	CString str;
	str.LoadString(IDS_GENERATOR_PROPERTIES_TITLE);
	m_propertySheet.SetTitle(str);

	SetName("Not defined");  // Needs to be set for real generators
	m_blankingWindow.m_bDisableCursorHiding = TRUE;
	ccwin = dw;
}

CGenerator::~CGenerator()
{

}
void CGenerator::Copy(CGenerator * p)
{
	m_doScreenBlanking = p->m_doScreenBlanking;
	m_name = p->m_name;
	m_b16_235 = p->m_b16_235;
	m_busePic = p->m_busePic;
	m_bdispTrip = p->m_bdispTrip;
	m_bLinear = p->m_bLinear;
	m_brPi_user = p->m_brPi_user;
}

void CGenerator::Serialize(CArchive& archive)
{
	CObject::Serialize(archive) ;
	if (archive.IsStoring())
	{
		int version=6;
		archive << version;
		archive << m_doScreenBlanking;
		archive << m_b16_235;
		archive << m_busePic;
		archive << m_bdispTrip;
		archive << m_bLinear;
		archive << m_brPi_user;
	}
	else
	{
		int version;
		archive >> version;
		if ( version > 6 )
			AfxThrowArchiveException ( CArchiveException::badSchema );
		archive >> m_doScreenBlanking;
		if ( version > 1 )
			archive >> m_b16_235;
		if ( version > 2 )
			archive >> m_busePic;
		if ( version > 3 )
			archive >> m_bdispTrip;
		if ( version > 4 )
			archive >> m_bLinear;
		if ( version > 5 )
			archive >>	m_brPi_user;
	}
}

void CGenerator::SetPropertiesSheetValues()
{
	m_GeneratorPropertiePage.m_doScreenBlanking=m_doScreenBlanking;
}

void CGenerator::GetPropertiesSheetValues()
{
	if( m_doScreenBlanking != m_GeneratorPropertiePage.m_doScreenBlanking )
	{
		m_doScreenBlanking=m_GeneratorPropertiePage.m_doScreenBlanking;
		GetConfig()->WriteProfileInt("Generator","Blanking",m_doScreenBlanking);
		SetModifiedFlag(TRUE);
	}
}

void CGenerator::AddPropertyPage(CPropertyPageWithHelp *apPage)
{
	m_propertySheet.AddPage(apPage);
}


BOOL CGenerator::Configure()
{
	SetPropertiesSheetValues();
	m_propertySheet.SetActivePage(1);
	int result=m_propertySheet.DoModal();
	if(result == IDOK)
		GetPropertiesSheetValues();

	return result==IDOK;
}

BOOL CGenerator::Init(UINT nbMeasure, bool isSpecial)
{
	nMeasureNumber = nbMeasure; 
	CGDIGenerator Cgen;
	CString str;
	str.LoadString(IDS_MANUALDVDGENERATOR_NAME);
	BOOL madVR_Found;
	char *	m_piIP = "";
	if (m_name != str)
	{
		if (Cgen.m_nDisplayMode == DISPLAY_rPI)
		{
			int x2 = Cgen.m_offsetx;
			int y2 = Cgen.m_offsety;
			if (!sock) //initialization
			{
				hInstLibrary = LoadLibrary("RB8PGenerator.dll");
				if (hInstLibrary)
				{
					_RB8PG_discovery = (RB8PG_discovery)GetProcAddress(hInstLibrary, "RB8PG_discovery@0");
					_RB8PG_connect = (RB8PG_connect)GetProcAddress(hInstLibrary, "RB8PG_connect@4");
					_RB8PG_get = (RB8PG_get)GetProcAddress(hInstLibrary, "RB8PG_get@8");
					_RB8PG_send = (RB8PG_send)GetProcAddress(hInstLibrary, "RB8PG_send@8");
					_RB8PG_close = (RB8PG_close)GetProcAddress(hInstLibrary, "RB8PG_close@4");

					if (_RB8PG_discovery)
						m_piIP = _RB8PG_discovery();
								
					if(strlen(m_piIP) > 5)
					{
						CString cs = m_piIP;
						if (_RB8PG_connect)
							sock = _RB8PG_connect(m_piIP);
						else
						{
							GetColorApp()->InMeasureMessageBox( "Error connecting with rPI: "+cs, "Error", MB_ICONINFORMATION);
							return false;
						}

						if (sock)
						{
							if (_RB8PG_send)
							{
								pi_Res = _RB8PG_get(sock,"CMD:GET_RESOLUTION");
							
								CString cs1(pi_Res);
								CString cs2,cs3,xW,yH;
								AfxExtractSubString(cs2, cs1, 0, ':');
								if (cs2 != "OK")
								{
									GetColorApp()->InMeasureMessageBox( "Failed to get rPi resolution", "GET_RESOLUTION", MB_ICONINFORMATION);
									return false;
								}
								AfxExtractSubString(cs3, cs1, 1, ':');
								AfxExtractSubString(xW, cs3, 0, 'x');
							
								int xsize = xW.GetLength();
								yH = cs3.Mid(xsize+1);

								rPi_xWidth = atoi(xW);
								rPi_yHeight = atoi(yH);

								pi_Res = _RB8PG_get(sock,"CMD:GET_GPU_MEMORY");
								CString cs4(pi_Res),cs5,cs6;
								AfxExtractSubString(cs5, cs4, 0, ':');
								if (cs5 != "OK")
								{
									GetColorApp()->InMeasureMessageBox( "Failed to get rPi GPU memory size", "GET_GPU_MEMORY", MB_ICONINFORMATION);
									return false;
								}
								cs6=cs4.Mid(3);
								cs6.Remove('M');
								rPi_memSize = atoi(cs6);
								GetConfig()->WriteProfileInt("GDIGenerator", "rPiSock", sock);
								GetConfig()->WriteProfileInt("GDIGenerator", "rPiGPU", rPi_memSize);
								GetConfig()->WriteProfileInt("GDIGenerator", "rPiWidth", rPi_xWidth);
								GetConfig()->WriteProfileInt("GDIGenerator", "rPiHeight", rPi_yHeight);
								CString msg;
								msg.Format("RGB=TEXT;12,0;100;16,128,128;0,0,0;100,300;Initializing PGenerator at: "+cs+" Res [%dx%d], GPU Mem [%dM]",rPi_xWidth, rPi_yHeight,rPi_memSize);
								_RB8PG_send(sock,msg);
								Sleep(3000);
								if (rPi_memSize >= 192)
								{
									_RB8PG_send(sock,"RGB=IMAGE;1920,1080;100;255,255,255;0,0,0;-1,-1;/var/lib/PGenerator/images-HCFR/gbramp.png");
									Sleep(1000);
								}
								if (m_bdispTrip)
								{
									CGDIGenerator Cgen;
									double bgstim = Cgen.m_bgStimPercent / 100.;
									int rb,gb,bb;

									if (m_b16_235)
									{
										rb = floor(bgstim * 219.0 + 16.5);
										gb = floor(bgstim * 219.0 + 16.5);
										bb = floor(bgstim * 219.0 + 16.5);
										rb=min(max(rb,0),235);
										gb=min(max(gb,0),235);
										bb=min(max(bb,0),235);
									}
									else
									{
										rb = floor(bgstim * 255.0 + 0.5);
										gb = floor(bgstim * 255.0 + 0.5);
										bb = floor(bgstim * 255.0 + 0.5);
										rb=min(max(rb,0),255);
										gb=min(max(gb,0),255);
										bb=min(max(bb,0),255);
									}

									if (x2 > 0)
										x2 = min(x2, rPi_xWidth / 2. - pow(Cgen.m_rectSizePercent/100.0,0.5) * rPi_xWidth / 2. );
									else
										x2 = max(x2, -1*(rPi_xWidth / 2. - pow(Cgen.m_rectSizePercent/100.0,0.5) * rPi_xWidth / 2.) );
									if (y2 > 0)
										y2 = min(y2, rPi_yHeight / 2. - pow(Cgen.m_rectSizePercent/100.0,0.5) * rPi_yHeight / 2.);
									else
										y2 = max(y2, -1*(rPi_yHeight / 2. - pow(Cgen.m_rectSizePercent/100.0,0.5) * rPi_yHeight / 2.) );

									CString templ;
									int x1 = (int)(pow((double)(Cgen.m_rectSizePercent)/100.0,0.5) * rPi_xWidth);
									int y1 = (int)(pow((double)(Cgen.m_rectSizePercent)/100.0,0.5) * rPi_yHeight);
									double t_fact = rPi_xWidth / 1920.; 
									templ.Format("SETCONF:HCFR:TEMPLATERAMDISK:DRAW=TEXT\nDIM=18,0\nRESOLUTION=100\nRGB=20,128,128\nBG=20,40,60\n" \
										"POSITION=%d,20\nTEXT=RGB Triplet $RGB\nEND=1\n" \
										"DRAW=RECTANGLE\nDIM=%d,%d\nRESOLUTION=100\n" \
										"RGB=DYNAMIC\nBG=DYNAMIC\nPOSITION=-1,-1,%d,%d\nEND=1",rPi_xWidth / 2 - int(175 * t_fact),x1,y1,x2,y2);
//										"RGB=DYNAMIC\nBG=-1,-1,-1\nPOSITION=-1,-1\nEND=1",rb,gb,bb,rPi_xWidth / 2 - int(175 * t_fact),x1,y1);
							
									CGenerator::_RB8PG_send(sock,templ);
								}
							}
							else
							{
								GetColorApp()->InMeasureMessageBox( "Error communicating with rPI", "Error", MB_ICONINFORMATION);
								return false;
							}
						}
					}
					else
					{
						GetColorApp()->InMeasureMessageBox( "    ** Raspberry Pi generator not found **", "Error", MB_ICONERROR);
						OutputDebugString("    ** RB8PG_discovery failed **");
						return false;
					}			
				}
				else
				{
					GetColorApp()->InMeasureMessageBox( "    ** RB8PGenerator.dll not found **", "Error", MB_ICONERROR);
					OutputDebugString("    ** Load_dll failed **");
					return false;
				}
			}
			else //in case template needs updating
			{
				if (m_bdispTrip || x2 !=0 || y2 != 0)
				{
					CGDIGenerator Cgen;
					double bgstim = Cgen.m_bgStimPercent / 100.;
					int rb,gb,bb;
					if (m_b16_235)
					{
						rb = floor(bgstim * 219.0 + 16.5);
						gb = floor(bgstim * 219.0 + 16.5);
						bb = floor(bgstim * 219.0 + 16.5);
						rb=min(max(rb,0),235);
						gb=min(max(gb,0),235);
						bb=min(max(bb,0),235);
					}
					else
					{
						rb = floor(bgstim * 255.0 + 0.5);
						gb = floor(bgstim * 255.0 + 0.5);
						bb = floor(bgstim * 255.0 + 0.5);
						rb=min(max(rb,0),255);
						gb=min(max(gb,0),255);
						bb=min(max(bb,0),255);
					}
					CString templ;
					int x1 = (UINT)(pow((double)(Cgen.m_rectSizePercent)/100.0,0.5) * rPi_xWidth);
					int y1 = (UINT)(pow((double)(Cgen.m_rectSizePercent)/100.0,0.5) * rPi_yHeight);
					if (x2 > 0)
						x2 = min(x2, rPi_xWidth / 2. - pow(Cgen.m_rectSizePercent/100.0,0.5) * rPi_xWidth / 2.);
					else
						x2 = max(x2, -1*(rPi_xWidth / 2. - pow(Cgen.m_rectSizePercent/100.0,0.5) * rPi_xWidth) / 2. );
					if (y2 > 0)
						y2 = min(y2, rPi_yHeight / 2. - pow(Cgen.m_rectSizePercent/100.0,0.5) * rPi_yHeight / 2.);
					else
						y2 = max(y2, -1*(rPi_yHeight / 2. - pow(Cgen.m_rectSizePercent/100.0,0.5) * rPi_yHeight / 2.));

					double t_fact = rPi_xWidth / 1920.; 
					templ.Format("SETCONF:HCFR:TEMPLATERAMDISK:DRAW=TEXT\nDIM=18,0\nRESOLUTION=100\nRGB=20,128,128\nBG=20,40,60\n" \
						"POSITION=%d,20\nTEXT=RGB Triplet $RGB\nEND=1\n" \
						"DRAW=RECTANGLE\nDIM=%d,%d\nRESOLUTION=100\n" \
						"RGB=DYNAMIC\nBG=DYNAMIC\nPOSITION=-1,-1,%d,%d\nEND=1",rPi_xWidth / 2 - int(175 * t_fact),x1,y1,x2,y2);
//						"RGB=DYNAMIC\nBG=-1,-1,-1\nPOSITION=-1,-1\nEND=1",rb,gb,bb,rPi_xWidth / 2 - int(175 * t_fact),x1,y1);
				
					CGenerator::_RB8PG_send(sock,templ);
				}
			}
		}
		else if (Cgen.m_nDisplayMode == DISPLAY_ccast)
		{
			OutputDebugString("CGenerator::Init");
			m_ccastIp = GetConfig()->GetProfileInt("GDIGenerator", "CCastIp", 0);
			CGoogleCastWrapper GCast;
			GCast.RefreshList();
			if (GCast.getCount() == 0)
			{
				GetColorApp()->InMeasureMessageBox( "    ** No ChromeCasts found **", "Error", MB_ICONERROR);
				OutputDebugString("    ** No ChromeCasts found **");
				return false;
			} else 
			{
				const ccast_id *id = m_ccastIp ? GCast.getCcastByIp(m_ccastIp) : GCast[0];
				if (id == NULL && (id = GCast[0]) == NULL)
				{
					GetColorApp()->InMeasureMessageBox( "    ** Error discovering ChromeCasts **", "Error", MB_ICONERROR);
					OutputDebugString("    ** Error discovering ChromeCasts **");
					return false;
				}
				else 
				{
					OutputDebugString("Casting to: ");OutputDebugString(id->name);
					double rx = sqrt( double( (double)Cgen.m_rectSizePercent / 100.));
					dw = new_ccwin((ccast_id *)id, 1000.0 * rx  , 565.0 * rx, 0.0, 0.0, 0, 0.1234);
					if (dw == NULL) 
					{
						GetColorApp()->InMeasureMessageBox( id->name, "new_ccwin failed!", MB_ICONERROR);
						OutputDebugString("new_ccwin failed! ");OutputDebugString(id->name);
						return -1;
					} 
					ccwin = dw;				
				}
			}
		} else if (Cgen.m_nDisplayMode == DISPLAY_madVR)
		{
			if (madVR_IsAvailable())
			{
				int nSettling=GetConfig()->m_isSettling?26:0;
				madVR_Found = madVR_Connect(CM_ConnectToLocalInstance, CM_ConnectToLanInstance, CM_StartLocalInstance  );
				if (m_madVR_vLUT)
					madVR_SetDeviceGammaRamp(NULL);
				if (m_madVR_3d)
					madVR_Disable3dlut();
				if (m_madVR_OSD)
					madVR_ShowProgressBar(nMeasureNumber + nSettling);
				madVR_SetHdrButton(m_madVR_HDR);
				if (m_madVR_HDR)
					madVR_SetHdrMetadata(GetConfig()->m_manualRedx, GetConfig()->m_manualRedy, GetConfig()->m_manualGreenx, GetConfig()->m_manualGreeny, GetConfig()->m_manualBluex, GetConfig()->m_manualBluey, GetConfig()->m_manualWhitex, GetConfig()->m_manualWhitey, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_ContentMaxL, GetConfig()->m_FrameAvgMaxL);
			}
			else
			{
				GetColorApp()->InMeasureMessageBox( "madVR dll not found, is madVR installed?", "Error", MB_ICONERROR);
				return false;
			}
		} else
		{
			CGenerator *	m_pGenerator;
			CGenerator *	 t1;			
			m_pGenerator=new CGDIGenerator();
			t1 = m_pGenerator;
		}
	}

	if(m_doScreenBlanking)
	{
		m_blankingWindow.DisplayRGBColor(ColorRGBDisplay(0.0), TRUE);	// show black screen
		Sleep(50);
		m_blankingWindow.DisplayRGBColor(ColorRGBDisplay(0.0), TRUE);	// show black screen again to be sure task bar is hidden
	}
	
	GetColorApp() -> BeginMeasureCursor ();

	return TRUE;
}

BOOL CGenerator::DisplayRGBColorrPI(const ColorRGBDisplay& aRGBColor, MeasureType nPatternType, UINT nPatternInfo )
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayRGBColormadVR(const ColorRGBDisplay& aRGBColor, MeasureType nPatternType, UINT nPatternInfo )
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayRGBCCast(const ColorRGBDisplay& aRGBColor, MeasureType nPatternType, UINT nPatternInfo )
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayRGBColor(const ColorRGBDisplay& aRGBColor, MeasureType nPatternType, UINT nPatternInfo,  BOOL bChangePattern,BOOL bSilentMode)
{
	return TRUE;	  // need to be overriden
}


BOOL CGenerator::DisplayAnsiBWRects(BOOL bInvert)
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayAnimatedBlack()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayAnimatedWhite()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayGradient()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayRG()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayRB()
{
	return TRUE;	  // need to be overriden
}
BOOL CGenerator::DisplayGB()
{
	return TRUE;	  // need to be overriden
}
BOOL CGenerator::DisplayRGd()
{
	return TRUE;	  // need to be overriden
}
BOOL CGenerator::DisplayRBd()
{
	return TRUE;	  // need to be overriden
}
BOOL CGenerator::DisplayGBd()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayGradient2()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayLramp()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayGranger()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::Display80()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayTV()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayTV2()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplaySpectrum()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplaySramp()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayVSMPTE()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayEramp()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayTC0()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayTC1()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayTC2()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayTC3()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayTC4()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayTC5()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayBN()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayDR0()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayDR1()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayDR2()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayAlign()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayAlign2()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayUser1()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayUser2()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayUser3()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayUser4()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayUser5()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayUser6()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplaySharp()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayClipH()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayClipHO()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayNBO()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayClipL()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayClipLO()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayTestimg()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayISO12233()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayNB()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayBBCHD()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayCROSSl()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayCROSSd()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayPM5644()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayZONE()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::CanDisplayAnsiBWRects()
{
	return FALSE;	  // need to be overriden if display AnsiBWRects is implemented
}

BOOL CGenerator::DisplayGray(double aLevel, MeasureType nPatternType , BOOL bChangePattern)
{
	return DisplayRGBColor(ColorRGBDisplay(aLevel), nPatternType ,bChangePattern); 
}


BOOL CGenerator::Release(INT nbNext)
{
	CGDIGenerator Cgen;

	if (Cgen.m_nDisplayMode == DISPLAY_madVR)
	{
	  if (madVR_IsAvailable())
	    madVR_Disconnect();
	} else if (Cgen.m_nDisplayMode == DISPLAY_ccast && dw)
		dw->del(dw);

	if (sock && Cgen.m_nDisplayMode == DISPLAY_rPI)
	{
			if (_RB8PG_send)
			{
				CString msg;
				if (nbNext == -1)
				{
					msg.Format("RGB=TEXT;14,0;100;16,128,128;0,0,0;100,300;End of sequence");
					_RB8PG_send(sock,msg);
					Sleep(2000);
				}
				_RB8PG_send(sock,"TESTTEMPLATE:PatternDynamic:0,0,0");
			}
			else
				GetColorApp()->InMeasureMessageBox( "Error communicating with rPI", "Error", MB_ICONINFORMATION);
	}

	if (sock && Cgen.m_nDisplayMode != DISPLAY_rPI && hInstLibrary) //disconnect only after generator change
	{

			if (_RB8PG_send)
			{
				CString msg;
				msg.Format("RGB=TEXT;14,0;100;16,128,128;0,0,0;100,300;Disconnecting from PGenerator");
				_RB8PG_send(sock,msg);
				Sleep(3000);
				_RB8PG_send(sock,"TESTTEMPLATE:PatternDynamic:0,0,0");
			}
			else
				GetColorApp()->InMeasureMessageBox( "Error communicating with rPI", "Error", MB_ICONINFORMATION);

			if (_RB8PG_close)
				_RB8PG_close(sock);
			else
				GetColorApp()->InMeasureMessageBox( "Error communicating with rPI", "Error", MB_ICONINFORMATION);

			sock = NULL;
			GetConfig()->WriteProfileInt("GDIGenerator", "rPiSock", 0);
			GetConfig()->WriteProfileInt("GDIGenerator", "rPiGPU", 0);
			FreeLibrary(hInstLibrary);
	}

	if(m_doScreenBlanking)
		m_blankingWindow.Hide();

	return TRUE;
}

BOOL CGenerator::ChangePatternSeries()
{
	return TRUE;
}
