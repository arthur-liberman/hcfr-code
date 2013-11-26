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

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

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

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

char *legendXYZ[3]={"X","Y","Z"};
char *legendRGB[3]={"R","G","B"};
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
	char *ext[3]={"xls","csv","icc"};
	char *filter[3]={"Excel files (*.xls)|*.xls||","CSV files (*.cvs)|*.csv||","ICC files (*.icc)|*.icc||"};
	CString	Msg, Title;

	CFileDialog fileSaveDialog( FALSE, ext[(int)m_type], NULL, OFN_HIDEREADONLY, filter[(int)m_type]);
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

	CFile testFile;
	BOOL testFileRes=testFile.Open(fileSaveDialog.GetPathName(),CFile::shareDenyNone|CFile::modeRead);

	if(testFileRes)
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
			if(MessageBox(NULL,Msg,Title,MB_YESNO)!=IDYES)
				return false;
		}
	}

	return SaveSheets();
}

bool CExport::SaveSheets()
{
	bool result;

	result=SaveGeneralSheet();
	result&=SaveGrayScaleSheet();
	result&=SavePrimariesSheet();

	if(!result)
	{
		CString	Msg, Title;
		Msg.LoadString ( IDS_ERREXPORT );
		Title.LoadString ( IDS_EXPORT );
		MessageBox(NULL,Msg+"\n"+m_errorStr,Title,MB_OK);
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
		Rows.Add((float)(ArrayIndexToGrayLevel ( j, size, GetConfig () -> m_bUseRoundDown)));
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
		m_pDoc->ComputeGammaAndOffset(&Gamma, &Offset, 3, 1, size);

	Rows.RemoveAll();
	Rows.Add("DeltaE");
	double YWhite = m_pDoc->GetMeasure()->GetGray(size-1)[1];
	for(j=0;j<size;j++)
	{
		// Determine Reference Y luminance for Delta E calculus
		if ( GetConfig ()->m_dE_gray > 0 || GetConfig ()->m_dE_form == 5 )
		{
		    double x = ArrayIndexToGrayLevel ( j, size, GetConfig () -> m_bUseRoundDown );
            double valy;
            if (GetConfig()->m_GammaOffsetType == 4)
			{
            //BT.1886 L = a(max[(V + b),0])^2.4
	            double maxL = m_pDoc->GetMeasure()->GetGray(size-1).GetY();
		        double minL = m_pDoc->GetMeasure()->GetGray(0).GetY();
			    double a = pow ( ( pow (maxL,1.0/2.4 ) - pow ( minL,1.0/2.4 ) ),2.4 );
				double b = ( pow ( minL,1.0/2.4 ) ) / ( pow (maxL,1.0/2.4 ) - pow ( minL,1.0/2.4 ) );
				double valx = GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown);
				valy = ( a * pow ( (valx + b)<0?0:(valx+b), 2.4 ) ) / maxL ;
			 }
			 else
			 {
				double valx=(GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown)+Offset)/(1.0+Offset);
				valy=pow(valx, GetConfig()->m_GammaRef);
			 }

            ColorxyY tmpColor(GetColorReference().GetWhite());
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
		rowNb=(m_numToReplace-1)*6+2;
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
			Rows.Add((float)m_pDoc->GetMeasure()->GetPrimary(j).GetRGBValue(GetColorReference())[i]);
		for(j=0;j<3;j++)
			Rows.Add((float)m_pDoc->GetMeasure()->GetSecondary(j).GetRGBValue(GetColorReference())[i]);
		result&=primariesSS.AddRow(Rows,rowNb,m_doReplace);
		rowNb++;
	}

	result&=primariesSS.Commit();
	if(!result)
		m_errorStr=primariesSS.GetLastError();
	return result;
}
