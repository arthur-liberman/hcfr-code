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

// Export.cpp: implementation of the CExport class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ColorHCFR.h"
#include "Export.h"

#include "CSpreadSheet.h"
#include "ExportReplaceDialog.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <atlimage.h>
#include "libharu\include\hpdf.h"

#include "DocTempl.h"
#include "RGBHistoView.h"
#include "GraphControl.h"
#include "CxImage\ximage.h"
#include "GammaHistoView.h"
#include "Views\luminancehistoview.h"
#include "colortemphistoview.h"
#include "ciechartview.h"
#include "SatLumHistoView.h"
#include "SatLumShiftView.h"
#include "MultiFrm.h"
#include "Views\MainView.h"
#include "libHCFR\Color.h"
jmp_buf env;

void error_handler  (HPDF_STATUS   error_no,
                HPDF_STATUS   detail_no,
                void         *user_data)
{
	char msg[50];
    sprintf (msg,"ERROR: error_no=%04X, File already open?\n", (HPDF_UINT)error_no,
                (HPDF_UINT)detail_no);
			if(GetColorApp()->InMeasureMessageBox(msg,"PDF ERROR, Yes to continue",MB_YESNO)!=IDYES)
    longjmp(env, 1);
}

void
draw_image (HPDF_Doc     pdf,
            const char  *filename,
            float        x,
            float        y,
            const char  *text)
{
#ifdef __WIN32__
    const char* FILE_SEPARATOR = "\\";
#else
    const char* FILE_SEPARATOR = "/";
#endif
    char filename1[255];

    HPDF_Page page = HPDF_GetCurrentPage (pdf);
    HPDF_Image image;
	char * path;
	path = getenv("APPDATA");
    strcpy(filename1, path);
    strcat(filename1, FILE_SEPARATOR);
    strcat(filename1, filename);

    image = HPDF_LoadPngImageFromFile (pdf, filename1);

    /* Draw image to the canvas. */
    HPDF_Page_DrawImage (page, image, x, y, HPDF_Image_GetWidth (image),
                    HPDF_Image_GetHeight (image));

    /* Print the text. */
    HPDF_Page_BeginText (page);
    HPDF_Page_SetTextLeading (page, 16);
    HPDF_Page_MoveTextPos (page, x, y);
    HPDF_Page_ShowTextNextLine (page, text);
    HPDF_Page_EndText (page);
}
void
draw_image2 (HPDF_Doc     pdf,
            const char  *filename,
            float        x,
            float        y,
            const char  *text)
{
#ifdef __WIN32__
    const char* FILE_SEPARATOR = "\\";
#else
    const char* FILE_SEPARATOR = "/";
#endif
    char filename1[255];

	int wX = 300 , wY = 200;
	HPDF_Page page = HPDF_GetCurrentPage (pdf);
    HPDF_Image image;
	char * path;
	path = getenv("APPDATA");

	strcpy(filename1, path);
    strcat(filename1, FILE_SEPARATOR);
    strcat(filename1, filename);

    image = HPDF_LoadPngImageFromFile (pdf, filename1);

    /* Draw image to the canvas. */
    HPDF_Page_DrawImage (page, image, x, y, wX, wY);

    /* Print the text. */
    HPDF_Page_BeginText (page);
    HPDF_Page_SetTextLeading (page, 12);
    HPDF_Page_MoveTextPos (page, x, y);
    HPDF_Page_ShowTextNextLine (page, text);
    HPDF_Page_EndText (page);
}
void
draw_line  (HPDF_Page    page,
            float        x,
            float        y,
            const char  *label)
{
    HPDF_Page_BeginText (page);
    HPDF_Page_MoveTextPos (page, x, y - 10);
    HPDF_Page_ShowText (page, label);
    HPDF_Page_EndText (page);

    HPDF_Page_MoveTo (page, x, y - 15);
    HPDF_Page_LineTo (page, x + 600, y - 15);
    HPDF_Page_Stroke (page);
}
// check the Excel-ODBC driver is installed
bool IsExcelDriverInsalled()
{
    // TODO: there is an annoying issue with ODBC calls when using 
    // the staic libraries, this is only used to turn off a menu so
    // probably ok to diable for now
    return true;
#ifdef NEVER
	char szBuf[2001];
	WORD cbBufMax = 2000;
	WORD cbBufOut;
	char *pszBuf = szBuf;

	// Get the names of the installed drivers ("odbcinst.h" has to be included )
	if(!SQLGetInstalledDrivers(szBuf,cbBufMax,& cbBufOut))
	{
		return false;
	}
	
	// Search for the driver...
	do
	{
		if( strstr( pszBuf, "Excel" ) != 0 )
		{
			// Found !
			return true;
		}
		pszBuf = strchr( pszBuf, '\0' ) + 1;
	}
	while( pszBuf[1] != '\0' );
	return false;
#endif
}
void
draw_rect (HPDF_Page     page,
           double        x,
           double        y,
		   double		 w,
		   double		 h,
		   double		off,
           const char   *label)
{
    HPDF_Page_BeginText (page);
    HPDF_Page_MoveTextPos (page, x, y - off);
    HPDF_Page_ShowText (page, label);
    HPDF_Page_EndText (page);

	HPDF_Page_Rectangle(page, x, y, w, h);
}
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

char *legendXYZ[3]={"X","Y","Z"};
char *legendRGB[3]={"R","G","B"};
char *legendRow1cc[3]={"X","Z","G"};
char *legendRow2cc[3]={"Y","R","B"};
char *legendSensor[3]={"Rc","Gc","Bc"};
char *primariesName[6]={"Red","Green","Blue","Yellow","Cyan","Magenta"};

CExport::CExport(CDataSetDoc *pDoc, ExportType type)
{
	m_pDoc=pDoc;
	m_type=type;
	m_doReplace=false;
	m_doBackup=false;
	m_numToReplace=1;
	m_fileName="colorHCFR.xls";
	m_separator=";";
}

CExport::~CExport()
{

}

bool CExport::Save()
{
	char *ext[4]={"xls","csv","icc","pdf"};
	char *filter[4]={"Excel files (*.xls)|*.xls||","CSV files (*.cvs)|*.csv||","ICC files (*.icc)|*.icc||","PDF files (*.pdf)|*.pdf||"};
	CString	Msg, Title;
	int flag;

	flag = OFN_HIDEREADONLY;
	if (m_type == PDF)
		flag += flag | OFN_OVERWRITEPROMPT;

	CFileDialog fileSaveDialog( FALSE, ext[(int)m_type], NULL, flag, filter[(int)m_type]);


	if(fileSaveDialog.DoModal()!=IDOK)
		return false;

	CString SheetOrSeparator="GeneralSheet";

	m_fileName=fileSaveDialog.GetPathName();
	
	if(m_type == CSV)	// Format filename to append sheet name easily
	{
		m_fileName.Replace(".csv","");
		int n=m_fileName.Find(SheetOrSeparator,0);
		if(n != -1)
			m_fileName=m_fileName.Left(n-1);
	}

	if(m_type != PDF)
	{
		CFile testFile;
		BOOL testFileRes=testFile.Open(fileSaveDialog.GetPathName(),CFile::shareDenyNone|CFile::modeRead);
		if (testFileRes)
		{
			testFile.Close();
	
			CString aFileName;
			if(m_type == CSV)
			{
				aFileName=m_fileName+"."+SheetOrSeparator+".csv";
				SheetOrSeparator=m_separator;
			}
			else
				aFileName=m_fileName;

			CSpreadSheet generalSS(aFileName,SheetOrSeparator,false);

			CExportReplaceDialog replaceDialog;
			bool measureFound=false;
			for(int i=2;i<=generalSS.GetTotalRows();i++)
			{
				CRowArray rows;
				if(generalSS.ReadRow(rows,i))
				{
					replaceDialog.AddMeasure(rows.GetAt(0)+": "+rows.GetAt(1)+" du "+rows.GetAt(2));
					measureFound=true;
				}
			}

			if(measureFound)
			{
				int res=replaceDialog.DoModal();

				if(res==IDCANCEL)
					return false;

				m_numToReplace=replaceDialog.GetSelectedMeasure()+1;
				m_doReplace=replaceDialog.IsReplaceRadioChecked();
			}
			else
			{
				Msg.LoadString ( IDS_NOMEASURES );
				Title.LoadString ( IDS_EXPORT );
				if(GetColorApp()->InMeasureMessageBox(Msg,Title,MB_YESNO)!=IDYES)
					return false;
			}
		}
	}
	
	if (m_type == PDF)
		return SavePDF();
	else
		return SaveSheets();
}

bool CExport::SavePDF()
{
    HPDF_Doc  pdf;
    HPDF_Font font,font2,fontg;
    HPDF_Page page,page2;
    HPDF_Destination dst;
	CString afileName = m_fileName;
	int dX = 1200, dY = 900;
	CDataSetDoc *pDataRef = GetDataRef();
	CView *pView;
	POSITION		pos;
	double CR = NULL;

	bool bk=GetConfig()->m_bWhiteBkgndOnFile;
	GetConfig()->m_bWhiteBkgndOnFile=FALSE;
	pos = m_pDoc -> GetFirstViewPosition ();
	pView = m_pDoc -> GetNextView ( pos );
	int current_mode = ((CMainView*)pView)->m_displayMode;
	((CMainView*)pView)->m_displayMode = 11;
	((CMainView*)pView)->UpdateAllGrids();
	((CMainView*)pView)->m_displayMode = 10;
	((CMainView*)pView)->UpdateAllGrids();
	((CMainView*)pView)->m_displayMode = 9;
	((CMainView*)pView)->UpdateAllGrids();
	((CMainView*)pView)->m_displayMode = 8;
	((CMainView*)pView)->UpdateAllGrids();
	((CMainView*)pView)->m_displayMode = 7;
	((CMainView*)pView)->UpdateAllGrids();
	((CMainView*)pView)->m_displayMode = 6;
	((CMainView*)pView)->UpdateAllGrids();
	((CMainView*)pView)->m_displayMode = 5;
	((CMainView*)pView)->UpdateAllGrids();
	((CMainView*)pView)->m_displayMode = 1;
	((CMainView*)pView)->UpdateAllGrids();
	((CMainView*)pView)->m_displayMode = 0;
	((CMainView*)pView)->UpdateAllGrids();

	double dEavg_cc = ((CMainView*)pView)->dEavg_cc;
	double dEavg_gs = ((CMainView*)pView)->dEavg_gs;
	double dEmax_cc = ((CMainView*)pView)->dEmax_cc;
	double dEmax_gs = ((CMainView*)pView)->dEmax_gs;
	double dEavg_sr = ((CMainView*)pView)->dEavg_sr;
	double dEmax_sr = ((CMainView*)pView)->dEmax_sr;
	double dEavg_sg = ((CMainView*)pView)->dEavg_sg;
	double dEmax_sg = ((CMainView*)pView)->dEmax_sg;
	double dEavg_sb = ((CMainView*)pView)->dEavg_sb;
	double dEmax_sb = ((CMainView*)pView)->dEmax_sb;
	double dEavg_sy = ((CMainView*)pView)->dEavg_sy;
	double dEmax_sy = ((CMainView*)pView)->dEmax_sy;
	double dEavg_sc = ((CMainView*)pView)->dEavg_sc;
	double dEmax_sc = ((CMainView*)pView)->dEmax_sc;
	double dEavg_sm = ((CMainView*)pView)->dEavg_sm;
	double dEmax_sm = ((CMainView*)pView)->dEmax_sm;

	double White = m_pDoc->GetMeasure()->GetOnOffWhite().GetY();
	double Black = m_pDoc->GetMeasure()->GetOnOffBlack().GetY();
	if (Black > 0)
		CR = White /Black;

    pdf = HPDF_New (error_handler, NULL);

    if (!pdf) {
		CString	Msg, Title;
		Msg.LoadString ( IDS_ERREXPORT );
		Title.LoadString ( IDS_EXPORT );
		GetColorApp()->InMeasureMessageBox(Msg+afileName+"error: cannot create PdfDoc object\n"+m_errorStr,Title,MB_OK);
        return false;
    }

    /* error-handler */
    if (setjmp(env)) {
        HPDF_Free (pdf);
        return false;
    }
    
	/* create default-font */
    font = HPDF_GetFont (pdf, "Helvetica-Oblique", NULL);
    font2 = HPDF_GetFont (pdf, "Helvetica", NULL);
    fontg = HPDF_GetFont (pdf, "Symbol", NULL);
	HPDF_SetCompressionMode(pdf, HPDF_COMP_ALL);

    /* add a new page object. */
    page = HPDF_AddPage (pdf);
	HPDF_Page_SetSize (page, HPDF_PAGE_SIZE_LETTER, HPDF_PAGE_PORTRAIT);
    dst = HPDF_Page_CreateDestination (page);
    HPDF_Destination_SetXYZ (dst, 0, HPDF_Page_GetHeight (page), 1);
    HPDF_SetOpenAction(pdf, dst);

    HPDF_Page_BeginText (page);
    HPDF_Page_SetFontAndSize (page, font, 14);
    HPDF_Page_MoveTextPos (page, HPDF_Page_GetWidth (page) - 300, HPDF_Page_GetHeight (page) - 20);
    HPDF_Page_ShowText (page, "System data for file: "+m_pDoc->GetTitle());
    HPDF_Page_SetFontAndSize (page, font, 12);
    HPDF_Page_SetTextLeading (page, 14);
    HPDF_Page_ShowTextNextLine (page, "Sensor: "+m_pDoc->GetSensor()->GetName());
	if (m_pDoc->GetGenerator()->GetName() == "View images")
	{
		CString dName;
		int d = GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_DEFAULT_MODE);
		switch (d)
		{
		case 0:
			dName = " [GDI]";
			break;
		case 3:
			dName = " [GDI - no background]";
			break;
		case 2:
			dName = " [madTPG]";
			break;
		case 4:
			dName = " [Chromecast]";
			break;
		case 5:
			dName = " [GDI - detached window]";
			break;
		}
	    HPDF_Page_ShowTextNextLine (page, "Generator: "+m_pDoc->GetGenerator()->GetName()+dName);
	}
	else
	    HPDF_Page_ShowTextNextLine (page, "Generator: "+m_pDoc->GetGenerator()->GetName());

	CString info=m_pDoc->GetMeasure()->GetInfoString();
	int b1 = info.Find('\n', 0);
	CString line1 = info.Left(b1);
    HPDF_Page_ShowTextNextLine (page, line1);
	info = info.Mid(b1+1);
	b1 = info.Find('\n', 0);
	line1 = info.Left(b1);
	HPDF_Page_ShowTextNextLine (page, line1);
	info = info.Mid(b1+1);
	b1 = info.Find('\n', 0);
	line1 = info.Left(b1);
	HPDF_Page_ShowTextNextLine (page, line1);
    HPDF_Page_SetFontAndSize (page, font2, 10);
    HPDF_Page_SetTextLeading (page, 10);
	HPDF_Page_EndText (page);

	HPDF_Page_SetLineWidth (page, 2);
	HPDF_Page_SetRGBStroke (page, (HPDF_REAL)0.5, (HPDF_REAL)0.5, (HPDF_REAL)0.7);
	HPDF_Page_SetLineCap (page, HPDF_ROUND_END);
	draw_line (page, 6, HPDF_Page_GetHeight (page) - 95, "");

    HPDF_Page_SetLineWidth (page, 5);
	HPDF_Page_SetRGBStroke (page, (HPDF_REAL)0.8314, (HPDF_REAL)0.6863, (HPDF_REAL)0.2157);
	draw_rect (page, 3, 3, HPDF_Page_GetWidth (page) - 6, HPDF_Page_GetHeight (page) - 5, 10, "");
    HPDF_Page_Stroke (page);

    HPDF_Page_SetLineWidth (page, 2);
	HPDF_Page_SetRGBStroke (page, (HPDF_REAL)0.4, (HPDF_REAL)0.0, (HPDF_REAL)0.0);
	draw_rect (page, 6 + 300, 20, 298, 200, 10, "Summary");
    HPDF_Page_Stroke (page);

	CString dEform;
	char str[100];
	if (White > 0 && Black < 0.000001)
		sprintf(str,"White: %.2f cd/m^2 Black: %.4f cd/m^2 CR: Infinite", White, Black);
	else
		sprintf(str,"White: %.2f cd/m^2 Black: %.4f cd/m^2 CR: %.0f:1", White, Black, CR);

	HPDF_Page_BeginText (page);
	HPDF_Page_SetFontAndSize (page, font2, 9);
	HPDF_Page_TextRect(page,311,215,601,160,str,HPDF_TALIGN_CENTER,NULL);
	HPDF_Page_SetTextLeading (page, 14);
	switch (GetConfig()->m_dE_form)
	{
		case 0:
		dEform = " [CIE76(uv)]";
		break;
		case 1:
		dEform = " [CIE76(ab)]";
		break;
		case 2:
		dEform = " [CIE94]";
		break;
		case 3:
		dEform = " [CIE2000]";
		break;
		case 4:
		dEform = " [CMC(1:1)]";
		break;
		case 5:
		dEform = " [CIE76(uv)]";
		break;
	}

	CString dEform2 = GetConfig()->m_dE_form==5?" [CIE2000]":dEform;
	dEform += GetConfig()->m_dE_gray==0?" [Relative Y]":(GetConfig ()->m_dE_gray == 1?" [Absolute Y w/gamma]":" [Absolute Y w/o gamma]");

	double nCount = m_pDoc -> GetMeasure () -> GetGrayScaleSize ();
	double Gamma = GetConfig()->m_GammaAvg;
	double Offset;
	bool isHDR = (GetConfig()->m_GammaOffsetType == 5 || GetConfig()->m_GammaOffsetType== 7);
	if (nCount > 0 && !isHDR )
	{
		m_pDoc->ComputeGammaAndOffset(&Gamma, &Offset, 1, 1, nCount, false);
		sprintf(str,"Average measured gamma: %.2f, Color dE formula: "+dEform2,Gamma);
	}
	else
	{
		if (isHDR && nCount)
			sprintf(str,"Average measured gamma: HDR mode");
		else
			sprintf(str,"Average measured gamma: No data");
	}
	HPDF_Page_ShowTextNextLine (page, str);

	if (dEavg_gs > 0)
		sprintf(str,"Grayscale dE: %.2f/%.2f", dEavg_gs, dEmax_gs);
	else
		sprintf(str,"Grayscale dE: No data");

	HPDF_Page_ShowTextNextLine (page, str + dEform);
	double YWhite = m_pDoc->GetMeasure()->GetPrimeWhite().GetY();
	double RefWhite = 1.0;
	CColor rColor = m_pDoc->GetMeasure()->GetRedPrimary();
	CColor gColor = m_pDoc->GetMeasure()->GetGreenPrimary();
	CColor bColor = m_pDoc->GetMeasure()->GetBluePrimary();
	CColor yColor = m_pDoc->GetMeasure()->GetYellowSecondary();
	CColor cColor = m_pDoc->GetMeasure()->GetCyanSecondary();
	CColor mColor = m_pDoc->GetMeasure()->GetMagentaSecondary();
	double dEr=0,dEg=0,dEb=0,dEy=0,dEc=0,dEm=0;
	CColor NoDataColor;
	CColorReference  bRef = ((GetColorReference().m_standard == UHDTV3 || GetColorReference().m_standard == UHDTV4)?CColorReference(UHDTV2):(GetColorReference().m_standard == HDTVa || GetColorReference().m_standard == HDTVb)?CColorReference(HDTV):GetColorReference());
	double tmWhite = getL_EOTF(0.5022283, NoDataColor, NoDataColor, GetConfig()->m_GammaRel, GetConfig()->m_Split, 5, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS) * 100.0;

	if (GetConfig()->m_GammaOffsetType == 5)
	{
		if (GetColorReference().m_standard == UHDTV2 || GetColorReference().m_standard == HDTV || GetColorReference().m_standard == UHDTV)
			RefWhite = YWhite / (tmWhite) ;
		else
		{
			RefWhite = YWhite / (tmWhite) ;					
			YWhite = YWhite * 94.37844 / (tmWhite) ;
		}
	}

	if (rColor.isValid())
	 dEr = rColor.GetDeltaE(YWhite, m_pDoc->GetMeasure()->GetRefPrimary(0), RefWhite, bRef, GetConfig()->m_dE_form, false, GetConfig()->gw_Weight );
	if (gColor.isValid())
	 dEg = gColor.GetDeltaE(YWhite, m_pDoc->GetMeasure()->GetRefPrimary(1), RefWhite, bRef, GetConfig()->m_dE_form, false, GetConfig()->gw_Weight );
	if (bColor.isValid())
	 dEb = bColor.GetDeltaE(YWhite, m_pDoc->GetMeasure()->GetRefPrimary(2), RefWhite, bRef, GetConfig()->m_dE_form, false, GetConfig()->gw_Weight );
	if (yColor.isValid())
	 dEy = yColor.GetDeltaE(YWhite, m_pDoc->GetMeasure()->GetRefSecondary(0), RefWhite, bRef, GetConfig()->m_dE_form, false, GetConfig()->gw_Weight );
	if (cColor.isValid())
	 dEc = cColor.GetDeltaE(YWhite, m_pDoc->GetMeasure()->GetRefSecondary(1), RefWhite, bRef, GetConfig()->m_dE_form, false, GetConfig()->gw_Weight );
	if (mColor.isValid())
	 dEm = mColor.GetDeltaE(YWhite, m_pDoc->GetMeasure()->GetRefSecondary(2), RefWhite, bRef, GetConfig()->m_dE_form, false, GetConfig()->gw_Weight );
	sprintf(str,"Primary dE:     Red %.2f, Green %.2f, Blue %.2f",dEr,dEg,dEb);
	HPDF_Page_ShowTextNextLine (page, str);
	sprintf(str,"Secondary dE: Yellow %.2f, Cyan %.2f, Magenta %.2f",dEy,dEc,dEm);
	HPDF_Page_ShowTextNextLine (page, str);
	

	dEform = "Color checker sequence";
	switch (GetConfig()->m_CCMode)
	{
	case 0:
		dEform += " [GCD Classic]";
		break;
	case 1:
		dEform += " [MCD Classic]";
		break;
	case 2:
		dEform += " [Pantone Skin]";
		break;
	case 3:
		dEform += " [CalMAN Classic]";
		break;
	case 4:
		dEform += " [CalMAN Skin]";
		break;
	case 5:
		dEform += " [Chromapure Skin]";
		break;
	case 6:
		dEform += " [CalMAN SG]";
		break;
	case 7:
		dEform += " [CalMAN 10 pt Lum.]";
		break;
	case 8:
		dEform += " [CalMAN 4 pt Lum.]";
		break;
	case 9:
		dEform += " [CalMAN 5 pt Lum.]";
		break;
	case 10:
		dEform += " [CalMAN 10 pt Lum.]";
		break;
	case 11:
		dEform += " [CalMAN 4 pt Sat.(100AMP)]";
		break;
	case 12:
		dEform += " [CalMAN 4 pt Sat.(75AMP)]";
		break;
	case 13:
		dEform += " [CalMAN 5 pt Sat.(100AMP)]";
		break;
	case 14:
		dEform += " [CalMAN 5 pt Sat.(75AMP)]";
		break;
	case 15:
		dEform += " [CalMAN 10 pt Sat.(100AMP)]";
		break;
	case 16:
		dEform += " [CalMAN 10 pt Sat.(75AMP)]";
		break;
	case 17:
		dEform += " [CalMAN near black]";
		break;
	case 18:
		dEform += " [CalMan dynamic range]";
		break;
	case 19:
		dEform += " [Random 250]";
		break;
	case 20:
		dEform += " [Random 500]";
		break;
	case 21:
		dEform += " [User]";
		break;
	}

	HPDF_Page_SetRGBFill(page, (HPDF_REAL).1, 0, (HPDF_REAL).8);
	HPDF_Page_ShowTextNextLine (page, dEform);
	HPDF_Page_SetGrayFill(page, 0);

	if (dEavg_cc > 0)
		sprintf(str,"Colorchecker dE:          %.2f/%.2f", dEavg_cc, dEmax_cc);
	else
		sprintf(str,"Colorchecker dE:          No data");
	HPDF_Page_ShowTextNextLine (page, str);

	if (dEavg_sr > 0)
		sprintf(str,"Red Saturation dE:        %.2f/%.2f", dEavg_sr, dEmax_sr);
	else
		sprintf(str,"Red Saturation dE:        No data");
	HPDF_Page_ShowTextNextLine (page, str);

	if (dEavg_sg > 0)
		sprintf(str,"Green Saturation dE:     %.2f/%.2f", dEavg_sg, dEmax_sg);
	else
		sprintf(str,"Green Saturation dE:     No data");
	HPDF_Page_ShowTextNextLine (page, str);

	if (dEavg_sb > 0)
		sprintf(str,"Blue Saturation dE:       %.2f/%.2f", dEavg_sb, dEmax_sb);
	else
		sprintf(str,"Blue Saturation dE:       No data");
	HPDF_Page_ShowTextNextLine (page, str);

	if (dEavg_sy > 0)
		sprintf(str,"Yellow Saturation dE:    %.2f/%.2f", dEavg_sy, dEmax_sy);
	else
		sprintf(str,"Yellow Saturation dE:    No data");
	HPDF_Page_ShowTextNextLine (page, str);

	if (dEavg_sc > 0)
		sprintf(str,"Cyan Saturation dE:      %.2f/%.2f", dEavg_sc, dEmax_sc);
	else
		sprintf(str,"Cyan Saturation dE:      No data");
	HPDF_Page_ShowTextNextLine (page, str);

	if (dEavg_sm > 0)
		sprintf(str,"Magenta Saturation dE: %.2f/%.2f", dEavg_sm, dEmax_sm);
	else
		sprintf(str,"Magenta Saturation dE: No data");
	HPDF_Page_ShowTextNextLine (page, str);
	
	sprintf(str,"dE values are avg/max");
    HPDF_Page_SetFontAndSize (page, font, 6);
	HPDF_Page_ShowTextNextLine (page, str);
	HPDF_Page_SetFontAndSize (page, font2, 9);

	HPDF_Page_EndText (page);

	CString datetime = CTime::GetCurrentTime().Format("%#c");
	draw_image (pdf, "color\\logo.png", 6, HPDF_Page_GetHeight (page) - 89,
                "Calibration report for "+ datetime + (pDataRef&&pDataRef!=m_pDoc?" [page 1]":""));

	
	//RGB graph
	CRGBGrapher pRGB;
	CRect Rect(0,0,dX,dY);
	pRGB.UpdateGraph(m_pDoc);	
	CDC ScreenDC;
	ScreenDC.CreateDC ( "DISPLAY", NULL, NULL, NULL );
	CDC MemDC, MemDC2;
	MemDC.CreateCompatibleDC(&ScreenDC);
	MemDC2.CreateCompatibleDC(&ScreenDC);
	CBitmap Bmp;
	Bmp.CreateCompatibleBitmap(&ScreenDC, Rect.Width(),Rect.Height());
	ScreenDC.DeleteDC();
	MemDC.SelectObject(&Bmp);
	MemDC2.SelectObject(&Bmp);
	pRGB.m_graphCtrl.DrawGraphs(&MemDC, Rect);
	pRGB.m_graphCtrl.SaveGraphs(&pRGB.m_graphCtrl2, NULL, NULL, FALSE, 1);	
	draw_image2(pdf, "color\\temp.png", 6, HPDF_Page_GetHeight (page) - 320, "Grayscale/Grayscale dE");

	//Gamma-CT graph

	CLuminanceGrapher pLUM;
	CGammaGrapher pGAMMA;

	CColorTempGrapher pCT;

	if (isHDR)
	{
		pLUM.UpdateGraph(m_pDoc);	
		pLUM.m_graphCtrl.DrawGraphs(&MemDC, Rect);
	}
	else
	{
		pGAMMA.UpdateGraph(m_pDoc);	
		pGAMMA.m_graphCtrl.DrawGraphs(&MemDC, Rect);
	}

	pCT.UpdateGraph(m_pDoc);
	pCT.m_graphCtrl.DrawGraphs(&MemDC2, Rect);

	if (isHDR)
	{
		pCT.m_graphCtrl.SaveGraphs(&pLUM.m_graphCtrl, NULL, NULL, FALSE, 4);	
		draw_image2(pdf, "color\\temp.png", 6 + 300, HPDF_Page_GetHeight (page) - 320, "Correlated Color Temperature/EOTF");
	}
	else
	{
		pCT.m_graphCtrl.SaveGraphs(&pGAMMA.m_graphCtrl, NULL, NULL, FALSE, 2);	
		draw_image2(pdf, "color\\temp.png", 6 + 300, HPDF_Page_GetHeight (page) - 320, "Correlated Color Temperature/Gamma");
	}

	char * path;
	char filename1[255];
	path = getenv("APPDATA");
    strcpy(filename1, path);
    strcat(filename1, "\\");
    strcat(filename1, "color\\temp.png");

//CIE Chart
	CCIEChartGrapher pCIE;
	pCIE.SaveGraphFile(m_pDoc,CSize(dX,dY),filename1,2,95,TRUE);
	CColorReference cRef = GetColorReference();
	CString sName = cRef.standardName.c_str();
	draw_image2(pdf, "color\\temp.png", 6, HPDF_Page_GetHeight (page) - 320 - 230, "CIE Diagram "+sName);
	
//SATS/LUM Chart

	CSatLumGrapher pSat;
	CSatLumShiftGrapher pShift;
	pSat.UpdateGraph(m_pDoc);	
	pShift.UpdateGraph(m_pDoc);	
	pSat.m_graphCtrl.DrawGraphs(&MemDC, Rect);
	pShift.m_graphCtrl.DrawGraphs(&MemDC2, Rect);
	pSat.m_graphCtrl.SaveGraphs(&pShift.m_graphCtrl, &pShift.m_graphCtrl2, NULL, FALSE, 3);	

	draw_image2(pdf, "color\\temp.png", 6 + 300, HPDF_Page_GetHeight (page) - 320 - 230, "Saturation Sweep Luminance/Shifts");

//Color comparator
	CColor aColor, aReference;
	ColorRGB aMeasure, aRef;
	int i=0,j=0,k=0,mx=24,my=50;
	int nColors=GetConfig()->GetCColorsSize();
	int mi = nColors;
	double dE=0.0;
	char str2[10];
	if (GetConfig()->m_colorStandard != HDTVa && GetConfig()->m_colorStandard != HDTVb )
		YWhite = m_pDoc->GetMeasure()->GetPrimeWhite().GetY();
	else
		YWhite = m_pDoc->GetMeasure()->GetOnOffWhite().GetY();

	RefWhite = 1.0;
	if (GetConfig()->m_GammaOffsetType == 5)
	{
		if (GetConfig()->m_CCMode >= MASCIOR50 && GetConfig()->m_CCMode <= LG400017)	
			YWhite = m_pDoc->GetMeasure()->GetGray((m_pDoc->GetMeasure()->GetGrayScaleSize()-1)).GetY() ;
		else
		{
			RefWhite = YWhite / (tmWhite) ;
			YWhite = YWhite * 94.37844 / (tmWhite) ;
		}
	}

	int ri = 6;

	if (nColors > 24)
	{
		mx = 24;
		my = 33;
		ri = 6; //6x6
	}

	if (nColors > 36)
	{
		mx = 18;
		my = 33;
		ri = 8; //8x6
	}

	if (nColors > 48)
	{
		mx = 18;
		my = 25;
		ri = 8; //8x8
	}

	if (nColors > 64)
	{
		mx = 12;
		my = 25;
		ri = 12;
	}

	if (nColors > 96)
		mi = 96;
	CColor tmp;
	for ( i=0; i<mi; i++)
	{
		aColor = m_pDoc->GetMeasure()->GetCC24Sat(i);
		aReference = m_pDoc->GetMeasure()->GetRefCC24Sat(i);
		aRef = m_pDoc->GetMeasure()->GetRefCC24Sat(i).GetRGBValue(bRef);		

		if (aColor.isValid())
		{
			aMeasure = m_pDoc->GetMeasure()->GetCC24Sat(i).GetRGBValue(bRef);
			tmp = aColor;
			tmp.SetX(tmp.GetX() / YWhite);
			tmp.SetY(tmp.GetY() / YWhite);
			tmp.SetZ(tmp.GetZ() / YWhite);
			if (GetConfig()->m_GammaOffsetType == 5)
			{
				if (GetConfig()->m_CCMode >= MASCIOR50 && GetConfig()->m_CCMode <= LG400017)
				{
					aReference.SetX(aReference.GetX() * 100.);
					aReference.SetY(aReference.GetY() * 100.);
					aReference.SetZ(aReference.GetZ() * 100.);
					aRef[0] = aRef[0] * 100.;
					aRef[1] = aRef[1] * 100.;
					aRef[2] = aRef[2] * 100.;
				}
				else
				{
					aReference.SetX(aReference.GetX() * 105.95640);
					aReference.SetY(aReference.GetY() * 105.95640);
					aReference.SetZ(aReference.GetZ() * 105.95640);
					aRef[0] = aRef[0] * 105.95640;
					aRef[1] = aRef[1] * 105.95640;
					aRef[2] = aRef[2] * 105.95640;
				}
			}
			aMeasure[0]=pow((tmp.GetRGBValue(bRef)[0]), 1.0/2.22);
			aMeasure[1]=pow((tmp.GetRGBValue(bRef)[1]), 1.0/2.22);
			aMeasure[2]=pow((tmp.GetRGBValue(bRef)[2]), 1.0/2.22);
			dE = aColor.GetDeltaE(YWhite, aReference, RefWhite, bRef, GetConfig()->m_dE_form, false, GetConfig()->gw_Weight );
		}
		else
		{
			aMeasure[0] = 0.5;
			aMeasure[1] = 0.5;
			aMeasure[2] = 0.5;
		}
		sprintf(str2,"%.2f",dE);

		aRef[0]=pow(aRef[0],1.0/2.22);
		aRef[1]=pow(aRef[1],1.0/2.22);
		aRef[2]=pow(aRef[2],1.0/2.22);
		aMeasure[0] = min(max(aMeasure[0],0),1);
		aMeasure[1] = min(max(aMeasure[1],0),1);
		aMeasure[2] = min(max(aMeasure[2],0),1);
		aRef[0] = min(max(aRef[0],0),1);
		aRef[1] = min(max(aRef[1],0),1);
		aRef[2] = min(max(aRef[2],0),1);

		if (i !=0)
		{
			if ((i % ri) == 0)
			{
				k = 0;
				j++;
			}
			else
			{
				k++;
			}
		}

		HPDF_Page_SetRGBFill (page, (HPDF_REAL)aMeasure[0], (HPDF_REAL)aMeasure[1], (HPDF_REAL)aMeasure[2]);
		draw_rect (page, 10 + (mx * 2) * k, 20 + my * j, mx, my, 0, "");
		HPDF_Page_Fill(page);

		if (aColor.GetY() / YWhite > .5)
			HPDF_Page_SetRGBFill (page, 0, 0, 0);
		else
			HPDF_Page_SetRGBFill (page, (HPDF_REAL).9, (HPDF_REAL).9, (HPDF_REAL).9);
		HPDF_Page_BeginText (page);
	    HPDF_Page_MoveTextPos (page, 11 + (mx * 2) * k, 22 + my * j);
		HPDF_Page_SetFontAndSize (page, font2, 5);
		HPDF_Page_ShowText (page, str2);
		HPDF_Page_EndText (page);

		HPDF_Page_SetRGBFill (page, (HPDF_REAL)aRef[0], (HPDF_REAL)aRef[1], (HPDF_REAL)aRef[2]);
		draw_rect (page, 10 + (mx * 2) * k + mx, 20 + my * j, mx, my, 0, "");
		HPDF_Page_Fill(page);
	}
	
	HPDF_Page_SetRGBFill (page, 0, 0, 0);
	HPDF_Page_BeginText (page);
	HPDF_Page_MoveTextPos (page, 10, 10);
	HPDF_Page_SetFontAndSize (page, font2, 9);
	HPDF_Page_ShowText (page, "Color Checker Comparator");
	HPDF_Page_EndText (page);

	//page 2 if ref document open
	if (pDataRef && (pDataRef != m_pDoc))
	{
		pos = pDataRef -> GetFirstViewPosition ();
		pView = pDataRef -> GetNextView ( pos );
		current_mode = ((CMainView*)pView)->m_displayMode;
		((CMainView*)pView)->m_displayMode = 11;
		((CMainView*)pView)->UpdateAllGrids();
		((CMainView*)pView)->m_displayMode = 10;
		((CMainView*)pView)->UpdateAllGrids();
		((CMainView*)pView)->m_displayMode = 9;
		((CMainView*)pView)->UpdateAllGrids();
		((CMainView*)pView)->m_displayMode = 8;
		((CMainView*)pView)->UpdateAllGrids();
		((CMainView*)pView)->m_displayMode = 7;
		((CMainView*)pView)->UpdateAllGrids();
		((CMainView*)pView)->m_displayMode = 6;
		((CMainView*)pView)->UpdateAllGrids();
		((CMainView*)pView)->m_displayMode = 5;
		((CMainView*)pView)->UpdateAllGrids();
		((CMainView*)pView)->m_displayMode=current_mode;
		((CMainView*)pView)->UpdateAllGrids();

		dEavg_cc = ((CMainView*)pView)->dEavg_cc;
		dEavg_gs = ((CMainView*)pView)->dEavg_gs;
		dEmax_cc = ((CMainView*)pView)->dEmax_cc;
		dEmax_gs = ((CMainView*)pView)->dEmax_gs;
		dEavg_sr = ((CMainView*)pView)->dEavg_sr;
		dEmax_sr = ((CMainView*)pView)->dEmax_sr;
		dEavg_sg = ((CMainView*)pView)->dEavg_sg;
		dEmax_sg = ((CMainView*)pView)->dEmax_sg;
		dEavg_sb = ((CMainView*)pView)->dEavg_sb;
		dEmax_sb = ((CMainView*)pView)->dEmax_sb;
		dEavg_sy = ((CMainView*)pView)->dEavg_sy;
		dEmax_sy = ((CMainView*)pView)->dEmax_sy;
		dEavg_sc = ((CMainView*)pView)->dEavg_sc;
		dEmax_sc = ((CMainView*)pView)->dEmax_sc;
		dEavg_sm = ((CMainView*)pView)->dEavg_sm;
		dEmax_sm = ((CMainView*)pView)->dEmax_sm;
		White = pDataRef->GetMeasure()->GetOnOffWhite().GetY();
		Black = pDataRef->GetMeasure()->GetOnOffBlack().GetY();
		if (Black > 0)
			CR = White /Black;
		
		/* add a new page object. */
		page2 = HPDF_AddPage (pdf);
		HPDF_Page_SetSize (page2, HPDF_PAGE_SIZE_LETTER, HPDF_PAGE_PORTRAIT);
		HPDF_Page_SetTextLeading (page2, 14);

		HPDF_Page_BeginText (page2);
		HPDF_Page_SetFontAndSize (page2, font, 14);
		HPDF_Page_MoveTextPos (page2, HPDF_Page_GetWidth (page2) - 300, HPDF_Page_GetHeight (page2) - 20);
		HPDF_Page_ShowText (page2, "System data for file: "+pDataRef->GetTitle()+"[REF]");
		HPDF_Page_SetFontAndSize (page2, font, 12);
		HPDF_Page_ShowTextNextLine (page2, "Sensor: "+pDataRef->GetSensor()->GetName());
		if (pDataRef->GetGenerator()->GetName() == "View images")
		{
			CString dName;
			int d = GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_DEFAULT_MODE);
			switch (d)
			{
			case 0:
				dName = " [GDI]";
				break;
			case 3:
				dName = " [GDI - no background]";
				break;
			case 2:
				dName = " [madTPG]";
				break;
			case 4:
				dName = " [Chromecast]";
				break;
			case 5:
				dName = " [GDI - detached window]";
				break;
			}
			HPDF_Page_ShowTextNextLine (page2, "Generator: "+pDataRef->GetGenerator()->GetName()+dName);
		}
		else
			HPDF_Page_ShowTextNextLine (page2, "Generator: "+pDataRef->GetGenerator()->GetName());

		CString info=pDataRef->GetMeasure()->GetInfoString();
		int b1 = info.Find('\n', 0);
		CString line1 = info.Left(b1);
		HPDF_Page_ShowTextNextLine (page2, line1);
		info = info.Mid(b1+1);
		b1 = info.Find('\n', 0);
		line1 = info.Left(b1);
		HPDF_Page_ShowTextNextLine (page2, line1);
		info = info.Mid(b1+1);
		b1 = info.Find('\n', 0);
		line1 = info.Left(b1);
		HPDF_Page_ShowTextNextLine (page2, line1);
	    HPDF_Page_SetFontAndSize (page2, font2, 10);
		HPDF_Page_SetTextLeading (page2, 10);
		HPDF_Page_EndText (page2);

		HPDF_Page_SetLineWidth (page2, 2);
		HPDF_Page_SetRGBStroke (page2, (HPDF_REAL)0.5, (HPDF_REAL)0.5, (HPDF_REAL)0.7);
		HPDF_Page_SetLineCap (page2, HPDF_ROUND_END);
		draw_line (page2, 6, HPDF_Page_GetHeight (page2) - 95, "");

		HPDF_Page_SetLineWidth (page2, 5);
		HPDF_Page_SetRGBStroke (page2, (HPDF_REAL)0.8314, (HPDF_REAL)0.6863, (HPDF_REAL)0.2157);
		draw_rect (page2, 3, 3, HPDF_Page_GetWidth (page2) - 6, HPDF_Page_GetHeight (page2) - 5, 10, "");
		HPDF_Page_Stroke (page2);

		HPDF_Page_SetLineWidth (page2, 2);
		HPDF_Page_SetRGBStroke (page2, (HPDF_REAL)0.4, (HPDF_REAL)0.0, (HPDF_REAL)0.0);
		draw_rect (page2, 6 + 300, 20, 298, 200, 10, "Summary");
		HPDF_Page_Stroke (page2);
		
		if (White > 0 && Black < 0.000001)
			sprintf(str,"White: %.2f cd/m^2    Black: %.4f cd/m^2    CR: Infinite", White, Black);
		else
			sprintf(str,"White: %.2f cd/m^2    Black: %.4f cd/m^2    CR: %.0f:1", White, Black, CR);

		HPDF_Page_BeginText (page2);
		HPDF_Page_SetFontAndSize (page2, font2, 9);
		HPDF_Page_TextRect(page2,311,215,601,160,str,HPDF_TALIGN_CENTER,NULL);
		HPDF_Page_SetTextLeading (page2, 14);
		switch (GetConfig()->m_dE_form)
		{
			case 0:
			dEform = " [CIE76(uv)]";
			break;
			case 1:
			dEform = " [CIE76(ab)]";
			break;
			case 2:
			dEform = " [CIE94]";
			break;
			case 3:
			dEform = " [CIE2000]";
			break;
			case 4:
			dEform = " [CMC(1:1)]";
			break;
			case 5:
			dEform = " [CIE76(uv)]";
			break;
		}

		CString dEform2 = GetConfig()->m_dE_form==5?" [CIE2000]":dEform;
		dEform += GetConfig()->m_dE_gray==0?" [Relative Y]":(GetConfig ()->m_dE_gray == 1?" [Absolute Y w/gamma]":" [Absolute Y w/o gamma]");

		double nCount = pDataRef -> GetMeasure () -> GetGrayScaleSize ();
		double Gamma = GetConfig()->m_GammaAvg;
		double Offset;
		if (nCount > 0)
		{
			m_pDoc->ComputeGammaAndOffset(&Gamma, &Offset, 1, 1, nCount, false);
			sprintf(str,"Average measured gamma: %.2f, Color dE formula: "+dEform2,Gamma);
		}
		else
		{
			sprintf(str,"Average measured gamma: No data");
		}
		HPDF_Page_ShowTextNextLine (page2, str);

		if (dEavg_gs > 0)
			sprintf(str,"Grayscale dE: %.2f/%.2f", dEavg_gs, dEmax_gs);
		else
			sprintf(str,"Grayscale dE: No data");

		HPDF_Page_ShowTextNextLine (page2, str + dEform);

		YWhite = pDataRef->GetMeasure()->GetPrimeWhite().GetY();
		CColor rColor = pDataRef->GetMeasure()->GetRedPrimary();
		CColor gColor = pDataRef->GetMeasure()->GetGreenPrimary();
		CColor bColor = pDataRef->GetMeasure()->GetBluePrimary();
		CColor yColor = pDataRef->GetMeasure()->GetYellowSecondary();
		CColor cColor = pDataRef->GetMeasure()->GetCyanSecondary();
		CColor mColor = pDataRef->GetMeasure()->GetMagentaSecondary();
		double dEr=0,dEg=0,dEb=0,dEy=0,dEc=0,dEm=0;
		CColor NoDataColor;
		CColorReference cRef = GetColorReference();
		CColorReference  bRef = ((GetColorReference().m_standard == UHDTV3 || GetColorReference().m_standard == UHDTV4)?CColorReference(UHDTV2):(GetColorReference().m_standard == HDTVa || GetColorReference().m_standard == HDTVb)?CColorReference(HDTV):GetColorReference());
		tmWhite = getL_EOTF(0.5022283, NoDataColor, NoDataColor, GetConfig()->m_GammaRel, GetConfig()->m_Split, 5, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS) * 100.0;
		RefWhite = 1.0;

		if (GetConfig()->m_GammaOffsetType == 5)
		{
			if (cRef.m_standard == UHDTV2 || cRef.m_standard == HDTV || cRef.m_standard == UHDTV)
				RefWhite = YWhite / (tmWhite) ;
			else
			{
				RefWhite = YWhite / (tmWhite) ;					
				YWhite = YWhite * 94.37844 / (tmWhite) ;
			}
		}

		if (rColor.isValid())
		 dEr = rColor.GetDeltaE(YWhite, pDataRef->GetMeasure()->GetRefPrimary(0), RefWhite, bRef, GetConfig()->m_dE_form, false, GetConfig()->gw_Weight );
		if (gColor.isValid())
		 dEg = gColor.GetDeltaE(YWhite, pDataRef->GetMeasure()->GetRefPrimary(1), RefWhite, bRef, GetConfig()->m_dE_form, false, GetConfig()->gw_Weight );
		if (bColor.isValid())
		 dEb = bColor.GetDeltaE(YWhite, pDataRef->GetMeasure()->GetRefPrimary(2), RefWhite, bRef, GetConfig()->m_dE_form, false, GetConfig()->gw_Weight );
		if (yColor.isValid())
		 dEy = yColor.GetDeltaE(YWhite, pDataRef->GetMeasure()->GetRefSecondary(0), RefWhite, bRef, GetConfig()->m_dE_form, false, GetConfig()->gw_Weight );
		if (cColor.isValid())
		 dEc = cColor.GetDeltaE(YWhite, pDataRef->GetMeasure()->GetRefSecondary(1), RefWhite, bRef, GetConfig()->m_dE_form, false, GetConfig()->gw_Weight );
		if (mColor.isValid())
		 dEm = mColor.GetDeltaE(YWhite, pDataRef->GetMeasure()->GetRefSecondary(2), RefWhite, bRef, GetConfig()->m_dE_form, false, GetConfig()->gw_Weight );
	
		sprintf(str,"Primary dE:     Red %.2f, Green %.2f, Blue %.2f",dEr,dEg,dEb);
		HPDF_Page_ShowTextNextLine (page2, str);
		sprintf(str,"Secondary dE: Yellow %.2f, Cyan %.2f, Magenta %.2f",dEy,dEc,dEm);
		HPDF_Page_ShowTextNextLine (page2, str);
	

		dEform = "Color checker sequence";
		switch (GetConfig()->m_CCMode)
		{
		case 0:
			dEform += " [GCD Classic]";
			break;
		case 1:
			dEform += " [MCD Classic]";
			break;
		case 2:
			dEform += " [Pantone Skin]";
			break;
		case 3:
			dEform += " [CalMAN Classic]";
			break;
		case 4:
			dEform += " [CalMAN Skin]";
			break;
		case 5:
			dEform += " [Chromapure Skin]";
			break;
		case 6:
			dEform += " [CalMAN SG]";
			break;
		case 7:
			dEform += " [CalMAN 10 pt Lum.]";
			break;
		case 8:
			dEform += " [CalMAN 4 pt Lum.]";
			break;
		case 9:
			dEform += " [CalMAN 5 pt Lum.]";
			break;
		case 10:
			dEform += " [CalMAN 10 pt Lum.]";
			break;
		case 11:
			dEform += " [CalMAN 4 pt Sat.(100AMP)]";
			break;
		case 12:
			dEform += " [CalMAN 4 pt Sat.(75AMP)]";
			break;
		case 13:
			dEform += " [CalMAN 5 pt Sat.(100AMP)]";
			break;
		case 14:
			dEform += " [CalMAN 5 pt Sat.(75AMP)]";
			break;
		case 15:
			dEform += " [CalMAN 10 pt Sat.(100AMP)]";
			break;
		case 16:
			dEform += " [CalMAN 10 pt Sat.(75AMP)]";
			break;
		case 17:
			dEform += " [CalMAN near black]";
			break;
		case 18:
			dEform += " [CalMan dynamic range]";
			break;
		case 19:
			dEform += " [Random 250]";
			break;
		case 20:
			dEform += " [Random 500]";
			break;
		case 21:
			dEform += " [User]";
			break;
		}

		HPDF_Page_SetRGBFill(page2, (HPDF_REAL).1, 0, (HPDF_REAL).8);
		HPDF_Page_ShowTextNextLine (page2, dEform);
		HPDF_Page_SetGrayFill(page2, 0);

		if (dEavg_cc > 0)
			sprintf(str,"Colorchecker dE:          %.2f/%.2f", dEavg_cc, dEmax_cc);
		else
			sprintf(str,"Colorchecker dE:          No data");
		HPDF_Page_ShowTextNextLine (page2, str);

		if (dEavg_sr > 0)
			sprintf(str,"Red Saturation dE:        %.2f/%.2f", dEavg_sr, dEmax_sr);
		else
			sprintf(str,"Red Saturation dE:        No data");
		HPDF_Page_ShowTextNextLine (page2, str);

		if (dEavg_sg > 0)
			sprintf(str,"Green Saturation dE:     %.2f/%.2f", dEavg_sg, dEmax_sg);
		else
			sprintf(str,"Green Saturation dE:     No data");
		HPDF_Page_ShowTextNextLine (page2, str);

		if (dEavg_sb > 0)
			sprintf(str,"Blue Saturation dE:       %.2f/%.2f", dEavg_sb, dEmax_sb);
		else
			sprintf(str,"Blue Saturation dE:       No data");
		HPDF_Page_ShowTextNextLine (page2, str);

		if (dEavg_sy > 0)
			sprintf(str,"Yellow Saturation dE:    %.2f/%.2f", dEavg_sy, dEmax_sy);
		else
			sprintf(str,"Yellow Saturation dE:    No data");
		HPDF_Page_ShowTextNextLine (page2, str);

		if (dEavg_sc > 0)
			sprintf(str,"Cyan Saturation dE:      %.2f/%.2f", dEavg_sc, dEmax_sc);
		else
			sprintf(str,"Cyan Saturation dE:      No data");
		HPDF_Page_ShowTextNextLine (page2, str);

		if (dEavg_sm > 0)
			sprintf(str,"Magenta Saturation dE: %.2f/%.2f", dEavg_sm, dEmax_sm);
		else
			sprintf(str,"Magenta Saturation dE: No data");
		HPDF_Page_ShowTextNextLine (page2, str);
	
		sprintf(str,"dE values are avg/max");
		HPDF_Page_SetFontAndSize (page2, font, 6);
		HPDF_Page_ShowTextNextLine (page2, str);
		HPDF_Page_SetFontAndSize (page2, font2, 9);

		HPDF_Page_EndText (page2);

		draw_image (pdf, "color\\logo.png", 6, HPDF_Page_GetHeight (page2) - 89,
					"Calibration report for "+datetime+" [page 2]");
	//RGB graph
		CRGBGrapher pRGB;
		CRect Rect(0,0,dX,dY);
		pRGB.UpdateGraph(pDataRef);	
		CDC ScreenDC;
		ScreenDC.CreateDC ( "DISPLAY", NULL, NULL, NULL );
		CDC MemDC, MemDC2;
		MemDC.CreateCompatibleDC(&ScreenDC);
		MemDC2.CreateCompatibleDC(&ScreenDC);
		CBitmap Bmp;
		Bmp.CreateCompatibleBitmap(&ScreenDC, Rect.Width(),Rect.Height());
		ScreenDC.DeleteDC();
		MemDC.SelectObject(&Bmp);
		MemDC2.SelectObject(&Bmp);
		pRGB.m_graphCtrl.DrawGraphs(&MemDC, Rect);
		pRGB.m_graphCtrl.SaveGraphs(&pRGB.m_graphCtrl2, NULL, NULL, FALSE, 1);	
		draw_image2(pdf, "color\\temp.png", 6, HPDF_Page_GetHeight (page2) - 320, "Grayscale/Grayscale dE");

		//Gamma-CT graph
	
		CLuminanceGrapher pLUM;
		CGammaGrapher pGAMMA;

		CColorTempGrapher pCT;
		if (isHDR)
		{
			pLUM.UpdateGraph(pDataRef);	
			pLUM.m_graphCtrl.DrawGraphs(&MemDC, Rect);
		}
		else
		{
			pGAMMA.UpdateGraph(pDataRef);	
			pGAMMA.m_graphCtrl.DrawGraphs(&MemDC, Rect);
		}

		pCT.UpdateGraph(pDataRef);
		pCT.m_graphCtrl.DrawGraphs(&MemDC2, Rect);

		if (isHDR)
		{
			pCT.m_graphCtrl.SaveGraphs(&pLUM.m_graphCtrl, NULL, NULL, FALSE, 4);	
			draw_image2(pdf, "color\\temp.png", 6 + 300, HPDF_Page_GetHeight (page) - 320, "Correlated Color Temperature/EOTF");
		}
		else
		{
			pCT.m_graphCtrl.SaveGraphs(&pGAMMA.m_graphCtrl, NULL, NULL, FALSE, 2);	
			draw_image2(pdf, "color\\temp.png", 6 + 300, HPDF_Page_GetHeight (page) - 320, "Correlated Color Temperature/Gamma");
		}

	//CIE Chart
		char * path;
		char filename1[255];
		path = getenv("APPDATA");
		strcpy(filename1, path);
		strcat(filename1, "\\");
		strcat(filename1, "color\\temp.png");
		CCIEChartGrapher pCIE;
		pCIE.SaveGraphFile(pDataRef,CSize(dX,dY),filename1,2,95,TRUE);

		draw_image2(pdf, "color\\temp.png", 6, HPDF_Page_GetHeight (page2) - 320 - 230, "CIE Diagram "+sName);
	
	//SATS/LUM Chart

		CSatLumGrapher pSat;
		CSatLumShiftGrapher pShift;
		pSat.UpdateGraph(pDataRef);	
		pShift.UpdateGraph(pDataRef);	
		pSat.m_graphCtrl.DrawGraphs(&MemDC, Rect);
		pShift.m_graphCtrl.DrawGraphs(&MemDC2, Rect);
		pSat.m_graphCtrl.SaveGraphs(&pShift.m_graphCtrl, &pShift.m_graphCtrl2, NULL, FALSE, 3);	

		draw_image2(pdf, "color\\temp.png", 6 + 300, HPDF_Page_GetHeight (page2) - 320 - 230, "Saturation Sweep Luminance/Shifts");
	//Color comparator
		CColor aColor, aReference;
		ColorRGB aMeasure, aRef, WhiteRGB;
		i=0,j=0,k=0,mx=24,my=50;
		nColors=GetConfig()->GetCColorsSize();
		mi = nColors;
		dE=0.0;
		if (GetConfig()->m_colorStandard != HDTVa && GetConfig()->m_colorStandard != HDTVb )
		{
			WhiteRGB = pDataRef->GetMeasure()->GetPrimeWhite().GetRGBValue(GetColorReference());
			YWhite = pDataRef->GetMeasure()->GetPrimeWhite().GetY();
		}
		else
		{
			WhiteRGB = pDataRef->GetMeasure()->GetOnOffWhite().GetRGBValue(GetColorReference());
			YWhite = pDataRef->GetMeasure()->GetOnOffWhite().GetY();
		}

	if (GetConfig()->m_colorStandard != HDTVa && GetConfig()->m_colorStandard != HDTVb )
		YWhite = m_pDoc->GetMeasure()->GetPrimeWhite().GetY();
	else
		YWhite = m_pDoc->GetMeasure()->GetOnOffWhite().GetY();

		RefWhite = 1.0;
		if (GetConfig()->m_GammaOffsetType == 5)
		{
			if (GetConfig()->m_CCMode >= MASCIOR50 && GetConfig()->m_CCMode <= LG400017)	
				YWhite = m_pDoc->GetMeasure()->GetGray((m_pDoc->GetMeasure()->GetGrayScaleSize()-1)).GetY() ;
			else
			{
				RefWhite = YWhite / (tmWhite) ;
				YWhite = YWhite * 94.37844 / (tmWhite) ;
			}
		}
		ri = 6;

		if (nColors > 24)
		{
			mx = 24;
			my = 33;
			ri = 6; //6x6
		}

		if (nColors > 36)
		{
			mx = 18;
			my = 33;
			ri = 8; //8x6
		}

		if (nColors > 48)
		{
			mx = 18;
			my = 25;
			ri = 8; //8x8
		}

		if (nColors > 64)
		{
			mx = 12;
			my = 25;
			ri = 12;
		}

		if (nColors > 96)
			mi = 96;
	
		for ( i=0; i<mi; i++)
		{
			aColor = pDataRef->GetMeasure()->GetCC24Sat(i);
			aReference = pDataRef->GetMeasure()->GetRefCC24Sat(i);
			aRef = pDataRef->GetMeasure()->GetRefCC24Sat(i).GetRGBValue((cRef.m_standard==UHDTV3||cRef.m_standard==UHDTV4)?CColorReference(UHDTV2):GetColorReference());
			if (GetConfig()->m_GammaOffsetType == 5)
			{
				if (GetConfig()->m_CCMode >= MASCIOR50 && GetConfig()->m_CCMode <= LG400017)
				{
					aReference.SetX(aReference.GetX() * 100.);
					aReference.SetY(aReference.GetY() * 100.);
					aReference.SetZ(aReference.GetZ() * 100.);
					aRef[0] = aRef[0] * 100.;
					aRef[1] = aRef[1] * 100.;
					aRef[2] = aRef[2] * 100.;
				}
				else
				{
					aReference.SetX(aReference.GetX() * 105.95640);
					aReference.SetY(aReference.GetY() * 105.95640);
					aReference.SetZ(aReference.GetZ() * 105.95640);
					aRef[0] = aRef[0] * 105.95640;
					aRef[1] = aRef[1] * 105.95640;
					aRef[2] = aRef[2] * 105.95640;
				}
			}
			if (aColor.isValid())
			{
				aMeasure = pDataRef->GetMeasure()->GetCC24Sat(i).GetRGBValue(GetColorReference());
				tmp = aColor;
				tmp.SetX(tmp.GetX() / YWhite);
				tmp.SetY(tmp.GetY() / YWhite);
				tmp.SetZ(tmp.GetZ() / YWhite);
				aMeasure[0]=pow((tmp.GetRGBValue(GetColorReference())[0]), 1.0/2.22);
				aMeasure[1]=pow((tmp.GetRGBValue(GetColorReference())[1]), 1.0/2.22);
				aMeasure[2]=pow((tmp.GetRGBValue(GetColorReference())[2]), 1.0/2.22);
				dE = aColor.GetDeltaE(YWhite, aReference, RefWhite, bRef, GetConfig()->m_dE_form, false, GetConfig()->gw_Weight );
			}
			else
			{
				aMeasure[0] = 0.5;
				aMeasure[1] = 0.5;
				aMeasure[2] = 0.5;
			}
			sprintf(str2,"%.2f",dE);

			aRef[0]=pow(aRef[0],1.0/2.22);
			aRef[1]=pow(aRef[1],1.0/2.22);
			aRef[2]=pow(aRef[2],1.0/2.22);
			aMeasure[0] = min(max(aMeasure[0],0),1);
			aMeasure[1] = min(max(aMeasure[1],0),1);
			aMeasure[2] = min(max(aMeasure[2],0),1);
			aRef[0] = min(max(aRef[0],0),1);
			aRef[1] = min(max(aRef[1],0),1);
			aRef[2] = min(max(aRef[2],0),1);

			if (i !=0)
			{
				if ((i % ri) == 0)
				{
					k = 0;
					j++;
				}
				else
				{
					k++;
				}
			}

			HPDF_Page_SetRGBFill (page2, (HPDF_REAL)aMeasure[0], (HPDF_REAL)aMeasure[1], (HPDF_REAL)aMeasure[2]);
			draw_rect (page2, 10 + (mx * 2) * k, 20 + my * j, mx, my, 0, "");
			HPDF_Page_Fill(page2);

			if (aColor.GetY() / YWhite > .5)
				HPDF_Page_SetRGBFill (page2, 0, 0, 0);
			else
				HPDF_Page_SetRGBFill (page2, (HPDF_REAL).9, (HPDF_REAL).9, (HPDF_REAL).9);
			HPDF_Page_BeginText (page2);
			HPDF_Page_MoveTextPos (page2, 11 + (mx * 2) * k, 22 + my * j);
			HPDF_Page_SetFontAndSize (page2, font2, 5);
			HPDF_Page_ShowText (page2, str2);
			HPDF_Page_EndText (page2);

			HPDF_Page_SetRGBFill (page2, (HPDF_REAL)aRef[0], (HPDF_REAL)aRef[1], (HPDF_REAL)aRef[2]);
			draw_rect (page2, 10 + (mx * 2) * k + mx, 20 + my * j, mx, my, 0, "");
			HPDF_Page_Fill(page2);
		}
	
		HPDF_Page_SetRGBFill (page2, 0, 0, 0);
		HPDF_Page_BeginText (page2);
		HPDF_Page_MoveTextPos (page2, 10, 10);
		HPDF_Page_SetFontAndSize (page2, font2, 9);
		HPDF_Page_ShowText (page2, "Color Checker Comparator");
		HPDF_Page_EndText (page2);

		}
	MemDC.DeleteDC();
	MemDC2.DeleteDC();

	/* save the document to a file */
    HPDF_SaveToFile (pdf, afileName);

    /* clean up */
    HPDF_Free (pdf);
	GetConfig()->m_bWhiteBkgndOnFile=bk;
	return true;
}

bool CExport::SaveSheets()
{
	bool result;

	result=SaveGeneralSheet();
	result&=SaveGrayScaleSheet();
	result&=SavePrimariesSheet();
	result&=SaveCCSheet();
	result&=SaveSpectralSheet();

	if(!result)
	{
		CString	Msg, Title;
		Msg.LoadString ( IDS_ERREXPORT );
		Title.LoadString ( IDS_EXPORT );
		GetColorApp()->InMeasureMessageBox(Msg+"\n"+m_errorStr,Title,MB_OK);
	}
	return result;
}

bool CExport::SaveGeneralSheet()
{
	CRowArray Rows;
	CString tempString;
	bool result=true;
	int rowNb=1;
	
	CString SheetOrSeparator="GeneralSheet";
	CString aFileName;
	if(m_type == CSV)
	{
		aFileName=m_fileName+"."+SheetOrSeparator+".csv";
		SheetOrSeparator=m_separator;
	}
	else
		aFileName=m_fileName;


	// Fill general sheet
	CSpreadSheet generalSS(aFileName,SheetOrSeparator,m_doBackup);
	generalSS.BeginTransaction();
	Rows.RemoveAll();
	Rows.Add("Num");
	Rows.Add("Name");
	Rows.Add("Export Date");
	Rows.Add("Sensor");
	Rows.Add("Generator");
	Rows.Add("Infos");
	result&=generalSS.AddHeaders(Rows,true);
	rowNb++;

	CString numStr;
	if(m_doReplace)
		rowNb=m_numToReplace+1;
	else
		rowNb=generalSS.GetTotalRows()+1;

	numStr.Format("%d",rowNb-1);

	Rows.RemoveAll();
	Rows.Add(numStr);
	Rows.Add(m_pDoc->GetTitle());
	Rows.Add(CTime::GetCurrentTime().Format("%#c"));
	Rows.Add(m_pDoc->GetSensor()->GetName());
	Rows.Add(m_pDoc->GetGenerator()->GetName());
	CString info=m_pDoc->GetMeasure()->GetInfoString();
	if(m_type == CSV)
	{
		info.Replace('\r',' ');
		info.Replace('\n',' ');
	}
	Rows.Add(info);
	result&=generalSS.AddRow(Rows,rowNb,m_doReplace);
	
	result&=generalSS.Commit();
	if(!result)
		m_errorStr=generalSS.GetLastError();
	return result;
}

bool CExport::SaveGrayScaleSheet()
{
	CRowArray Rows;
	CString tempString;
	bool result=true;
	int rowNb=1;
	int i,j;

	BOOL		bIRE = m_pDoc->GetMeasure()->m_bIREScaleMode;

	CString SheetOrSeparator="GrayScaleSheet";
	CString aFileName;
	if(m_type == CSV)
	{
		aFileName=m_fileName+"."+SheetOrSeparator+".csv";
		SheetOrSeparator=m_separator;
	}
	else
		aFileName=m_fileName;

	// Fill grayScale sheet
	CSpreadSheet graySS(aFileName, SheetOrSeparator,m_doBackup);
	graySS.BeginTransaction();

	Rows.RemoveAll();
	Rows.Add("Measure");
	int size=m_pDoc->GetMeasure()->GetGrayScaleSize();
	for(j=0;j<size;j++)
	{
		tempString.Format("%d",j);
		Rows.Add(tempString,CRowArray::floatType);
	}
	result&=graySS.AddHeaders(Rows,true);

	if(m_doReplace)
		rowNb=(m_numToReplace-1)*9+2;
	else
		rowNb=graySS.GetTotalRows()+1;

	Rows.RemoveAll();
	Rows.Add(bIRE ? "IRE" : GetConfig()->m_PercentGray);
	for(j=0;j<size;j++)
	{
		Rows.Add((float)(ArrayIndexToGrayLevel ( j, size, GetConfig () -> m_bUseRoundDown, GetConfig () -> m_bUse10bit)));
	}
	result&=graySS.AddRow(Rows,rowNb,m_doReplace);
	rowNb++;

	for (i=0; i<3; i++)
	{
		Rows.RemoveAll();
		Rows.Add(legendXYZ[i]);
		for(j=0;j<size;j++)
			Rows.Add((float)m_pDoc->GetMeasure()->GetGray(j)[i]);
		result&=graySS.AddRow(Rows,rowNb,m_doReplace);
		rowNb++;
	}

	for (i=0; i<3; i++)
	{
		Rows.RemoveAll();
		Rows.Add(legendRGB[i]);
		for(j=0;j<size;j++)
			Rows.Add((float)m_pDoc->GetMeasure()->GetGray(j).GetRGBValue(GetColorReference())[i]);
		result&=graySS.AddRow(Rows,rowNb,m_doReplace);
		rowNb++;
	}

	Rows.RemoveAll();
	Rows.Add("ColorTemp");
	for(j=0;j<size;j++)
		Rows.Add((float)m_pDoc->GetMeasure()->GetGray(j).GetXYZValue().GetColorTemp(GetColorReference()));
	result&=graySS.AddRow(Rows,rowNb,m_doReplace);
	rowNb++;

	// Retrieve white reference, gamma and offset
	double		Gamma,Offset;
	CColor		refColor = GetColorReference().GetWhite();
    ColorXYZ aColor(m_pDoc->GetMeasure()->GetGray(i).GetXYZValue());
	if ( size && m_pDoc->GetMeasure()->GetGray(0).isValid() )
		m_pDoc->ComputeGammaAndOffset(&Gamma, &Offset, 3, 1, size, false);

	Rows.RemoveAll();
	Rows.Add("DeltaE");
	double YWhite = m_pDoc->GetMeasure()->GetGray(size-1)[1];
	for(j=0;j<size;j++)
	{
		// Determine Reference Y luminance for Delta E calculus
		if ( GetConfig ()->m_dE_gray > 0 || GetConfig ()->m_dE_form == 5 )
		{
		    double x = ArrayIndexToGrayLevel ( j, size, GetConfig () -> m_bUseRoundDown, GetConfig () -> m_bUse10bit );
            double valy;
    		CColor White = m_pDoc -> GetMeasure () -> GetGray ( size - 1 );
			CColor Black = m_pDoc->GetMeasure()->GetOnOffBlack();
			int mode = GetConfig()->m_GammaOffsetType;
			if (GetConfig()->m_colorStandard == sRGB) mode = 99;
			if (  (mode >= 4) )
			{
				double valx = GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig () -> m_bUse10bit);
				valy = getL_EOTF(valx, White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS);
			 }
			 else
			 {
				double valx=(GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig () -> m_bUse10bit)+Offset)/(1.0+Offset);
				valy=pow(valx, GetConfig()->m_useMeasuredGamma?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef));
				if (mode == 1) //black compensation target
					valy = (Black.GetY() + ( valy * ( YWhite - Black.GetY() ) )) / YWhite;
			 }

            ColorxyY tmpColor(GetColorReference().GetWhite());
			if (GetConfig()->m_GammaOffsetType == 5)
				tmpColor[2] = valy * 100. / YWhite;
			else
				tmpColor[2] = valy;

            if (GetConfig ()->m_dE_gray == 2 || GetConfig ()->m_dE_form == 5 )
                tmpColor[2] = aColor [ 1 ] / YWhite;
			refColor.SetxyYValue(tmpColor);
		}
		else
		{
			// Use actual gray luminance as correct reference (Delta E will check color only, not brightness)
			    YWhite = m_pDoc->GetMeasure()->GetGray(j) [ 1 ];
		}
		
		Rows.Add((float)m_pDoc->GetMeasure()->GetGray(j).GetDeltaE(YWhite, refColor, 1.0, GetColorReference(), 	GetConfig()->m_dE_form, true, GetConfig()->gw_Weight ));
	}
	result&=graySS.AddRow(Rows,rowNb,m_doReplace);
	rowNb++;

	result&=graySS.Commit();
	if(!result)
		m_errorStr=graySS.GetLastError();
	return result;
}

bool CExport::SavePrimariesSheet()
{
	CRowArray Rows;
	CString tempString;
	bool result=true;
	int rowNb=1;
	int i,j;

	double YWhite = m_pDoc->GetMeasure()->GetPrimeWhite().GetY();
	
	if (!YWhite)
		YWhite = m_pDoc->GetMeasure()->GetOnOffWhite().GetY();

	CColor rColor = m_pDoc->GetMeasure()->GetRedPrimary();
	CColor gColor = m_pDoc->GetMeasure()->GetGreenPrimary();
	CColor bColor = m_pDoc->GetMeasure()->GetBluePrimary();
	CColor yColor = m_pDoc->GetMeasure()->GetYellowSecondary();
	CColor cColor = m_pDoc->GetMeasure()->GetCyanSecondary();
	CColor mColor = m_pDoc->GetMeasure()->GetMagentaSecondary();
	
	double dEr=0,dEg=0,dEb=0,dEy=0,dEc=0,dEm=0;
	CColor NoDataColor;
	CColorReference cRef = GetColorReference();
	double tmWhite = getL_EOTF(0.5022283, NoDataColor, NoDataColor, GetConfig()->m_GammaRel, GetConfig()->m_Split, 5, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS) * 100.0;
	double RefWhite = 1.0;
	CColorReference  bRef = ((GetColorReference().m_standard == UHDTV3 || GetColorReference().m_standard == UHDTV4)?CColorReference(UHDTV2):(GetColorReference().m_standard == HDTVa || GetColorReference().m_standard == HDTVb)?CColorReference(HDTV):GetColorReference());

	if (GetConfig()->m_GammaOffsetType == 5)
	{
		if (cRef.m_standard == UHDTV2 || cRef.m_standard == HDTV || cRef.m_standard == UHDTV)
			RefWhite = YWhite / (tmWhite) ;
		else
		{
			RefWhite = YWhite / (tmWhite) ;					
			YWhite = YWhite * 94.37844 / (tmWhite) ;
		}
	}
	if (rColor.isValid())
	 dEr = rColor.GetDeltaE(YWhite, m_pDoc->GetMeasure()->GetRefPrimary(0), RefWhite, bRef, GetConfig()->m_dE_form, false, GetConfig()->gw_Weight );
	if (gColor.isValid())
	 dEg = gColor.GetDeltaE(YWhite, m_pDoc->GetMeasure()->GetRefPrimary(1), RefWhite, bRef, GetConfig()->m_dE_form, false, GetConfig()->gw_Weight );
	if (bColor.isValid())
	 dEb = bColor.GetDeltaE(YWhite, m_pDoc->GetMeasure()->GetRefPrimary(2), RefWhite, bRef, GetConfig()->m_dE_form, false, GetConfig()->gw_Weight );
	if (yColor.isValid())
	 dEy = yColor.GetDeltaE(YWhite, m_pDoc->GetMeasure()->GetRefSecondary(0), RefWhite, bRef, GetConfig()->m_dE_form, false, GetConfig()->gw_Weight );
	if (cColor.isValid())
	 dEc = cColor.GetDeltaE(YWhite, m_pDoc->GetMeasure()->GetRefSecondary(1), RefWhite, bRef, GetConfig()->m_dE_form, false, GetConfig()->gw_Weight );
	if (mColor.isValid())
	 dEm = mColor.GetDeltaE(YWhite, m_pDoc->GetMeasure()->GetRefSecondary(2), RefWhite, bRef, GetConfig()->m_dE_form, false, GetConfig()->gw_Weight );

	CString SheetOrSeparator="PrimariesSheet";
	CString aFileName;
	if(m_type == CSV)
	{
		aFileName=m_fileName+"."+SheetOrSeparator+".csv";
		SheetOrSeparator=m_separator;
	}
	else
		aFileName=m_fileName;

	// Fill primaries sheet
	CSpreadSheet primariesSS(aFileName, SheetOrSeparator,m_doBackup);
	primariesSS.BeginTransaction();

	Rows.RemoveAll();
	Rows.Add("Measure");
	for(j=0;j<6;j++)
	{
		Rows.Add(primariesName[j],CRowArray::floatType);
	}
	result&=primariesSS.AddHeaders(Rows,true);

	if(m_doReplace)
		rowNb=(m_numToReplace-1)*7+2;
	else
		rowNb=primariesSS.GetTotalRows()+1;

	for (i=0; i<3; i++)
	{
		Rows.RemoveAll();
		Rows.Add(legendXYZ[i]);
		for(j=0;j<3;j++)
			Rows.Add((float)m_pDoc->GetMeasure()->GetPrimary(j)[i]);
		for(j=0;j<3;j++)
			Rows.Add((float)m_pDoc->GetMeasure()->GetSecondary(j)[i]);
		result&=primariesSS.AddRow(Rows,rowNb,m_doReplace);
		rowNb++;
	}

	for (i=0; i<3; i++)
	{
		Rows.RemoveAll();
		Rows.Add(legendRGB[i]);
		for(j=0;j<3;j++)
			Rows.Add((float)m_pDoc->GetMeasure()->GetPrimary(j).GetRGBValue(bRef)[i]);
		for(j=0;j<3;j++)
			Rows.Add((float)m_pDoc->GetMeasure()->GetSecondary(j).GetRGBValue(bRef)[i]);
		result&=primariesSS.AddRow(Rows,rowNb,m_doReplace);
		rowNb++;
	}

	Rows.RemoveAll();
	Rows.Add("DeltaE");
	Rows.Add((float)dEr);
	Rows.Add((float)dEg);
	Rows.Add((float)dEb);
	Rows.Add((float)dEy);
	Rows.Add((float)dEc);
	Rows.Add((float)dEm);
	result&=primariesSS.AddRow(Rows,rowNb,m_doReplace);
	rowNb++;
	
	result&=primariesSS.Commit();
	if(!result)
		m_errorStr=primariesSS.GetLastError();
	return result;
}
bool CExport::SaveSpectralSheet()
{
	CRowArray Rows;
	bool result=true;
	int rowNb=1;
	int i,j,nWavs = 0;

	double YWhite = m_pDoc->GetMeasure()->GetOnOffWhite().GetY();
	CColor rColor = m_pDoc->GetMeasure()->GetRedPrimary();
	CColor gColor = m_pDoc->GetMeasure()->GetGreenPrimary();
	CColor bColor = m_pDoc->GetMeasure()->GetBluePrimary();
	CColor yColor = m_pDoc->GetMeasure()->GetYellowSecondary();
	CColor cColor = m_pDoc->GetMeasure()->GetCyanSecondary();
	CColor mColor = m_pDoc->GetMeasure()->GetMagentaSecondary();
	CSpectrum rSpec = rColor.GetSpectrum();
	CSpectrum gSpec = gColor.GetSpectrum();
	CSpectrum bSpec = bColor.GetSpectrum();
	CSpectrum wSpec = m_pDoc->GetMeasure()->GetOnOffWhite().GetSpectrum();
	bool hasSpectrum = rColor.HasSpectrum();
	if (hasSpectrum)
		nWavs = rSpec.GetRows();

	CString SheetOrSeparator="SpectralSheet";
	CString aFileName;
	if(m_type == CSV)
	{
		aFileName=m_fileName+"."+SheetOrSeparator+".csv";
		SheetOrSeparator=m_separator;
	}
	else
		aFileName=m_fileName;

	// Fill primaries sheet
	CSpreadSheet spectralSS(aFileName, SheetOrSeparator,m_doBackup);
	spectralSS.BeginTransaction();

	Rows.RemoveAll();
	Rows.Add("Wavelength {nm}",CRowArray::floatType);
	for (j=0;j<3;j++)
		Rows.Add(primariesName[j],CRowArray::floatType);
	Rows.Add("White",CRowArray::floatType);

	result&=spectralSS.AddHeaders(Rows,true);

	if(m_doReplace)
		rowNb = (m_numToReplace-1)*(nWavs)+2;
	else
		rowNb=spectralSS.GetTotalRows()+1;

	if (hasSpectrum)
	{
		for (i=0; i<nWavs; i++)
		{
			Rows.RemoveAll();
			Rows.Add((float)rSpec.m_WaveLengthMin+rSpec.m_BandWidth*i);
			Rows.Add((float)rSpec[i]);
			if (gColor.HasSpectrum())
				Rows.Add((float)gSpec[i]);
			if (bColor.HasSpectrum())
				Rows.Add((float)bSpec[i]);
			if (m_pDoc->GetMeasure()->GetOnOffWhite().HasSpectrum())
				Rows.Add((float)wSpec[i]);
			result&=spectralSS.AddRow(Rows,rowNb,m_doReplace);
			rowNb++;
		}		
	}

	result&=spectralSS.Commit();
	if(!result)
		m_errorStr=spectralSS.GetLastError();
	return result;
}


bool CExport::SaveCCSheet()
{
	CRowArray Rows;
	CString tempString;
	CColorReference cRef = GetColorReference();
	bool result=true;
	int rowNb=1;
	int i,j;
                    char*  PatName[96]={
                    "White",
                    "6J",
                    "5F",
                    "6I",
                    "6K",
                    "5G",
                    "6H",
                    "5H",
                    "7K",
                    "6G",
                    "5I",
                    "6F",
                    "8K",
                    "5J",
                    "Black",
                    "2B",
                    "2C",
                    "2D",
                    "2E",
                    "2F",
                    "2G",
                    "2H",
                    "2I",
                    "2J",
                    "2K",
                    "2L",
                    "2M",
                    "3B",
                    "3C",
                    "3D",
                    "3E",
                    "3F",
                    "3G",
                    "3H",
                    "3I",
                    "3J",
                    "3K",
                    "3L",
                    "3M",
                    "4B",
                    "4C",
                    "4D",
                    "4E",
                    "4F",
                    "4G",
                    "4H",
                    "4I",
                    "4J",
                    "4K",
                    "4L",
                    "4M",
                    "5B",
                    "5C",
                    "5D",
                    "5K",
                    "5L",
                    "5M",
                    "6B",
                    "6C",
                    "6D",
                    "6L",
                    "6M",
                    "7B",
                    "7C",
                    "7D",
                    "7E",
                    "7F",
                    "7G",
                    "7H",
                    "7I",
                    "7J",
                    "7L",
                    "7M",
                    "8B",
                    "8C",
                    "8D",
                    "8E",
                    "8F",
                    "8G",
                    "8H",
                    "8I",
                    "8J",
                    "8L",
                    "8M",
                    "9B",
                    "9C",
                    "9D",
                    "9E",
                    "9F",
                    "9G",
                    "9H",
                    "9I",
                    "9J",
                    "9K",
                    "9L",
                    "9M" };
                    char*  PatNameCMS[19]={
						"White",
						"Black",
						"2E",
						"2F",
						"2K",
						"5D",
						"7E",
						"7F",
						"7G",
						"7H",
						"7I",
						"7J",
						"8D",
						"8E",
						"8F",
						"8G",
						"8H",
						"8I",
						"8J" };
                    char*  PatNameCPS[19]={
						"White",
						"D7",
						"D8",
						"E7",
						"E8",
						"F7",
						"F8",
						"G7",
						"G8",
						"H7",
						"H8",
						"I7",
						"I8",
						"J7",
						"J8",
						"CP-Light",
						"CP-Dark",
						"Dark Skin",
						"Light Skin" };
                    char*  PatNameAXIS[71]={
						"Black",
						"White 10",
						"White 20",
						"White 30",
						"White 40",
						"White 50",
						"White 60",
						"White 70",
						"White 80",
						"White 90",
						"White 100",
						"Red 10",
						"Red 20",
						"Red 30",
						"Red 40",
						"Red 50",
						"Red 60",
						"Red 70",
						"Red 80",
						"Red 90",
						"Red 100",
						"Green 10",
						"Green 20",
						"Green 30",
						"Green 40",
						"Green 50",
						"Green 60",
						"Green 70",
						"Green 80",
						"Green 90",
						"Green 100",
						"Blue 10",
						"Blue 20",
						"Blue 30",
						"Blue 40",
						"Blue 50",
						"Blue 60",
						"Blue 70",
						"Blue 80",
						"Blue 90",
						"Blue 100", 
						"Cyan 10",
						"Cyan 20",
						"Cyan 30",
						"Cyan 40",
						"Cyan 50",
						"Cyan 60",
						"Cyan 70",
						"Cyan 80",
						"Cyan 90",
						"Cyan 100", 
						"Magenta 10",
						"Magenta 20",
						"Magenta 30",
						"Magenta 40",
						"Magenta 50",
						"Magenta 60",
						"Magenta 70",
						"Magenta 80",
						"Magenta 90",
						"Magenta 100", 
						"Yellow 10",
						"Yellow 20",
						"Yellow 30",
						"Yellow 40",
						"Yellow 50",
						"Yellow 60",
						"Yellow 70",
						"Yellow 80",
						"Yellow 90",
						"Yellow 100"
					};

	CString SheetOrSeparator="ColorCheckerSheet";
	CString aFileName;
	if(m_type == CSV)
	{
		aFileName=m_fileName+"."+SheetOrSeparator+".csv";
		SheetOrSeparator=m_separator;
	}
	else
		aFileName=m_fileName;

	// Fill colorchecker sheet
	CSpreadSheet colorcheckerSS(aFileName, SheetOrSeparator,m_doBackup);
	colorcheckerSS.BeginTransaction();

	Rows.RemoveAll();
	Rows.Add("Color");
	for (i=0;i<3;i++)
	{
		Rows.Add(legendRow1cc[i],CRowArray::floatType);
		Rows.Add(legendRow2cc[i],CRowArray::floatType);
	}
	Rows.Add("deltaE",CRowArray::floatType);
	result&=colorcheckerSS.AddHeaders(Rows,true);

	int size;
	BOOL isExtPat =( GetConfig()->m_CCMode == USER || GetConfig()->m_CCMode == CM10SAT || GetConfig()->m_CCMode == CM10SAT75 || GetConfig()->m_CCMode == CM5SAT || GetConfig()->m_CCMode == CM5SAT75 || GetConfig()->m_CCMode == CM4SAT || GetConfig()->m_CCMode == CM4SAT75 || GetConfig()->m_CCMode == CM4LUM || GetConfig()->m_CCMode == CM5LUM || GetConfig()->m_CCMode == CM10LUM || GetConfig()->m_CCMode == RANDOM250 || GetConfig()->m_CCMode == RANDOM500 || GetConfig()->m_CCMode == CM6NB || GetConfig()->m_CCMode == CMDNR || GetConfig()->m_CCMode == MASCIOR50);
	isExtPat = (isExtPat || GetConfig()->m_CCMode > 19);

	if (isExtPat)
		size = GetConfig()->GetCColorsSize();
    else
        size = GetConfig()->m_CCMode==CCSG?96:GetConfig()->m_CCMode==CMS||GetConfig()->m_CCMode==CPS?19:(GetConfig()->m_CCMode==AXIS?71:24);

	if(m_doReplace)
		rowNb=(m_numToReplace-1)*size+2;
	else
		rowNb=colorcheckerSS.GetTotalRows()+1;

	for(i=0;i<size;i++)
	{
		Rows.RemoveAll();
		if (GetConfig()->m_CCMode == CCSG)
        {
			Rows.Add(PatName[i]);
        } 
        else if (GetConfig()->m_CCMode == CMS)
        {
            Rows.Add(PatNameCMS[i]);
        }
        else if (GetConfig()->m_CCMode == CPS)
        {
            Rows.Add(PatNameCPS[i]);
        }
        else if (GetConfig()->m_CCMode == AXIS)
        {
            Rows.Add(PatNameAXIS[i]);
        }
        else if (isExtPat)
        {
            char aBuf[50];
			std::string name = GetConfig()->GetCColorsN(i);
            sprintf(aBuf,"%s", name.c_str());
            Rows.Add(aBuf);
        }
        else 
		{
			CString msg;
			switch ( i )
			{
				case 0:
						msg.LoadStringA(GetConfig()->m_CCMode == CMC?IDS_CC_1a:(GetConfig()->m_CCMode == SKIN?IDS_CC_1b:IDS_CC_1));
						break;
				case 1:
						msg.LoadStringA(GetConfig()->m_CCMode == CMC?IDS_CC_2a:(GetConfig()->m_CCMode == SKIN?IDS_CC_2b:IDS_CC_2));
						break;
				case 2:
						msg.LoadStringA(GetConfig()->m_CCMode == CMC?IDS_CC_3a:(GetConfig()->m_CCMode == SKIN?IDS_CC_3b:IDS_CC_3));
						break;
				case 3:
						msg.LoadStringA(GetConfig()->m_CCMode == CMC?IDS_CC_4a:(GetConfig()->m_CCMode == SKIN?IDS_CC_4b:IDS_CC_4));
						break;
				case 4:
						msg.LoadStringA(GetConfig()->m_CCMode == CMC?IDS_CC_5a:(GetConfig()->m_CCMode == SKIN?IDS_CC_5b:IDS_CC_5));
						break;
				case 5:
						msg.LoadStringA(GetConfig()->m_CCMode == CMC?IDS_CC_6a:(GetConfig()->m_CCMode == SKIN?IDS_CC_6b:IDS_CC_6));
						break;
				case 6:
						msg.LoadStringA(GetConfig()->m_CCMode == CMC?IDS_CC_7a:(GetConfig()->m_CCMode == SKIN?IDS_CC_7b:IDS_CC_7));
						break;
				case 7:
						msg.LoadStringA(GetConfig()->m_CCMode == CMC?IDS_CC_8a:(GetConfig()->m_CCMode == SKIN?IDS_CC_8b:IDS_CC_8));
						break;
				case 8:
						msg.LoadStringA(GetConfig()->m_CCMode == CMC?IDS_CC_9a:(GetConfig()->m_CCMode == SKIN?IDS_CC_9b:IDS_CC_9));
						break;
				case 9:
						msg.LoadStringA(GetConfig()->m_CCMode == CMC?IDS_CC_10a:(GetConfig()->m_CCMode == SKIN?IDS_CC_10b:IDS_CC_10));
						break;
				case 10:
						msg.LoadStringA(GetConfig()->m_CCMode == CMC?IDS_CC_11a:(GetConfig()->m_CCMode == SKIN?IDS_CC_11b:IDS_CC_11));
						break;
				case 11:
						msg.LoadStringA(GetConfig()->m_CCMode == CMC?IDS_CC_12a:(GetConfig()->m_CCMode == SKIN?IDS_CC_12b:IDS_CC_12));
						break;
				case 12:
						msg.LoadStringA(GetConfig()->m_CCMode == CMC?IDS_CC_13a:(GetConfig()->m_CCMode == SKIN?IDS_CC_13b:IDS_CC_13));
						break;
				case 13:
						msg.LoadStringA(GetConfig()->m_CCMode == CMC?IDS_CC_14a:(GetConfig()->m_CCMode == SKIN?IDS_CC_14b:IDS_CC_14));
						break;
				case 14:
						msg.LoadStringA(GetConfig()->m_CCMode == CMC?IDS_CC_15a:(GetConfig()->m_CCMode == SKIN?IDS_CC_15b:IDS_CC_15));
						break;
				case 15:
						msg.LoadStringA(GetConfig()->m_CCMode == CMC?IDS_CC_16a:(GetConfig()->m_CCMode == SKIN?IDS_CC_16b:IDS_CC_16));
						break;
				case 16:
						msg.LoadStringA(GetConfig()->m_CCMode == CMC?IDS_CC_17a:(GetConfig()->m_CCMode == SKIN?IDS_CC_17b:IDS_CC_17));
						break;
				case 17:
						msg.LoadStringA(GetConfig()->m_CCMode == CMC?IDS_CC_18a:(GetConfig()->m_CCMode == SKIN?IDS_CC_18b:IDS_CC_18));
						break;
				case 18:
						msg.LoadStringA(GetConfig()->m_CCMode == CMC?IDS_CC_19a:(GetConfig()->m_CCMode == SKIN?IDS_CC_19b:IDS_CC_19));
						break;
				case 19:
						msg.LoadStringA(GetConfig()->m_CCMode == CMC?IDS_CC_20a:(GetConfig()->m_CCMode == SKIN?IDS_CC_20b:IDS_CC_20));
						break;
				case 20:
						msg.LoadStringA(GetConfig()->m_CCMode == CMC?IDS_CC_21a:(GetConfig()->m_CCMode == SKIN?IDS_CC_21b:IDS_CC_21));
						break;
				case 21:
						msg.LoadStringA(GetConfig()->m_CCMode == CMC?IDS_CC_22a:(GetConfig()->m_CCMode == SKIN?IDS_CC_22b:IDS_CC_22));
						break;
				case 22:
						msg.LoadStringA(GetConfig()->m_CCMode == CMC?IDS_CC_23a:(GetConfig()->m_CCMode == SKIN?IDS_CC_23b:IDS_CC_23));
						break;
				case 23:
						msg.LoadStringA(GetConfig()->m_CCMode == CMC?IDS_CC_24a:(GetConfig()->m_CCMode == SKIN?IDS_CC_24b:IDS_CC_24));
						break;
            		 default:
				        msg.LoadStringA(IDS_CC_24a);
            }
			Rows.Add(msg);
		}
		
		for(j=0;j<3;j++)
		{
			if (m_pDoc->GetMeasure()->GetCC24Sat(i).isValid())
				Rows.Add((float)m_pDoc->GetMeasure()->GetCC24Sat(i).GetXYZValue()[j]);
			else
				Rows.Add(-1.0);
		}
		
		CColorReference  bRef = ((GetColorReference().m_standard == UHDTV3 || GetColorReference().m_standard == UHDTV4)?CColorReference(UHDTV2):(GetColorReference().m_standard == HDTVa || GetColorReference().m_standard == HDTVb)?CColorReference(HDTV):GetColorReference());

		for(j=0;j<3;j++)
		{
			if (m_pDoc->GetMeasure()->GetCC24Sat(i).isValid())
				Rows.Add((float)m_pDoc->GetMeasure()->GetCC24Sat(i).GetRGBValue(bRef)[j]);
			else
				Rows.Add(-1.0);
		}
		
		CColor aColor,aReference;
		double YWhite, RefWhite = 1.0;
		if (GetConfig()->m_colorStandard != HDTVa && GetConfig()->m_colorStandard != HDTVb )
			YWhite = m_pDoc->GetMeasure()->GetPrimeWhite().GetY();
		else
			YWhite = m_pDoc->GetMeasure()->GetOnOffWhite().GetY();

		CColor NoDataColor;
		double tmWhite = getL_EOTF(0.5022283, NoDataColor, NoDataColor, GetConfig()->m_GammaRel, GetConfig()->m_Split, 5, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS) * 100.0;
		if (GetConfig()->m_GammaOffsetType == 5)
		{
			if (GetConfig()->m_CCMode >= MASCIOR50 && GetConfig()->m_CCMode <= LG400017)	
				YWhite = m_pDoc->GetMeasure()->GetGray((m_pDoc->GetMeasure()->GetGrayScaleSize()-1)).GetY() ;
			else
			{
				RefWhite = YWhite / (tmWhite) ;
				YWhite = YWhite * 94.37844 / (tmWhite) ;
			}
		}

		aColor = m_pDoc->GetMeasure()->GetCC24Sat(i);
		aReference = m_pDoc->GetMeasure()->GetRefCC24Sat(i);
		if (GetConfig()->m_GammaOffsetType == 5)
		{
			if (GetConfig()->m_CCMode >= MASCIOR50 && GetConfig()->m_CCMode <= LG400017)
			{
				aReference.SetX(aReference.GetX() * 100.);
				aReference.SetY(aReference.GetY() * 100.);
				aReference.SetZ(aReference.GetZ() * 100.);
			}
			else
			{
				aReference.SetX(aReference.GetX() * 105.95640);
				aReference.SetY(aReference.GetY() * 105.95640);
				aReference.SetZ(aReference.GetZ() * 105.95640);
			}
		}

		if (aColor.isValid())
			Rows.Add(aColor.GetDeltaE(YWhite, aReference, RefWhite, bRef, GetConfig()->m_dE_form, false, GetConfig()->gw_Weight ));
		else
			Rows.Add(-1);

		result&=colorcheckerSS.AddRow(Rows,rowNb,m_doReplace);
		rowNb++;
	}


	result&=colorcheckerSS.Commit();
	if(!result)
		m_errorStr=colorcheckerSS.GetLastError();
	return result;
}
