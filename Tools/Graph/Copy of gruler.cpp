/*
This file was created by Paul Barvinko (pbarvinko@yahoo.com).
This file is distributed "as is", e.g. there are no warranties 
and obligations and you could use it in your applications on your
own risk. 
Your comments and questions are welcome.
If using in your applications, please mention author in credits for your app.
*/
// Changed from ruler.cpp (created by Pasha)
// fruler.cpp : implementation file
//

#include "../../stdafx.h"
#include "gruler.h"
#include "afxtempl.h"
//

//all followinf sizes are in logical units

// FX mod
// const double FRulerWidth = 1/6.0;    

// const double FFontSize = FRulerWidth/1.13;
// const double FBigMarkSize = FRulerWidth/4;
// const double FSmallMarkSize = FRulerWidth/4;

const double FRulerWidth = 1/5.0;    

const double FFontSize = FRulerWidth/1.3;
const double FBigMarkSize = FRulerWidth/4;
const double FSmallMarkSize = FRulerWidth/4;

// End of FX mod

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// FRuler

IMPLEMENT_DYNCREATE(FRuler, CWnd);

FRuler::FRuler() : CWnd()
{
	MinValue=1.0f; MaxValue=100.0f;
	MainFontRec.lfHeight = 14;
	MainFontRec.lfWidth = 8;
	MainFontRec.lfEscapement = 0;
	MainFontRec.lfOrientation = 0;
	MainFontRec.lfWeight = 0;
	MainFontRec.lfItalic = 0;
	MainFontRec.lfUnderline = 0;
	MainFontRec.lfStrikeOut = 0;
	MainFontRec.lfCharSet = ANSI_CHARSET;
	MainFontRec.lfOutPrecision = OUT_TT_PRECIS;
	MainFontRec.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	MainFontRec.lfQuality = PROOF_QUALITY;
	MainFontRec.lfPitchAndFamily =FF_ROMAN | VARIABLE_PITCH;
	_tcscpy(MainFontRec.lfFaceName, _T(""));
	sUnit = _T("");
}

FRuler::~FRuler()
{
}

void FRuler::SetMinMax(double minv, double maxv, BOOL bRedraw)
{
	MinValue = minv;
	MaxValue = maxv;
	if (bRedraw) RedrawWindow();
}

BEGIN_MESSAGE_MAP(FRuler, CWnd)
	//{{AFX_MSG_MAP(FRuler)
	ON_WM_CREATE()
	ON_WM_MOUSEMOVE()
	ON_WM_PAINT()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


int FRuler::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	EnableToolTips(TRUE);
	//create tool tip object
	tooltip.Create(this, TTS_ALWAYSTIP);
	tooltip.Activate(TRUE);
	tooltip.AddTool(this, _T(""));
  tooltip.SendMessage(TTM_SETMAXTIPWIDTH, 0, SHRT_MAX);
  tooltip.SendMessage(TTM_SETDELAYTIME, TTDT_AUTOPOP, SHRT_MAX);
  tooltip.SendMessage(TTM_SETDELAYTIME, TTDT_INITIAL, 100);
  tooltip.SendMessage(TTM_SETDELAYTIME, TTDT_RESHOW, 100);
	return 0;
}

BOOL FRuler::PreTranslateMessage(MSG* pMsg) 
{
	tooltip.RelayEvent(pMsg);
	return CWnd::PreTranslateMessage(pMsg);
}

void FRuler::SetNewTitles(char* newUOM, char* newTitle)
{
	sUnit = newUOM;
	sTitle = newTitle;
	tooltip.UpdateTipText(sTitle, this);
}

void FRuler::OnMouseMove(UINT nFlags, CPoint point) 
{
	tooltip.Activate(TRUE);
	CWnd::OnMouseMove(nFlags, point);
}

/////////////////////////////////////////////////////////////////////////////
// FRuler drawing

/////////////////////////////////////////////////////////////////////////////
// FRuler diagnostics

#ifdef _DEBUG
void FRuler::AssertValid() const
{
	CWnd::AssertValid();
}

void FRuler::Dump(CDumpContext& dc) const
{
	CWnd::Dump(dc);
}
#endif //_DEBUG

void FRuler::OnPaint()
{
    CRect R;
    GetClientRect( &R);
    CPaintDC PaintDC(this);
    DrawRuler(&PaintDC, R);
}

int FRuler::ConvertLogPixToRealPix(CDC* dest_dc, double log_pix_num, BOOL b_x_axis)
{
    int index = LOGPIXELSY;
    if (b_x_axis)
    {
	index = LOGPIXELSX;
    };
    int log_pix = GetDeviceCaps(dest_dc->m_hAttribDC, index);
    int res = (int)(log_pix_num*log_pix);
    if (res == 0) res = 1;
    return res;
}

/////////////////////////////////////////////////////////////////////////////
// FHRuler

IMPLEMENT_DYNAMIC(FHRuler, FRuler);

//FHRuler object
FHRuler::FHRuler() : FRuler()
{
}

int FHRuler::Width(CDC* dest_dc)
{
    return ConvertLogPixToRealPix(dest_dc, FRulerWidth, FALSE);
}

int FHRuler::GetNMax(CDC* dc_to_draw, CRect rect_to_draw)
{
	int MinShift = Width(dc_to_draw);
	int MaxShift=0;

	CString s;
	s = _T("9.99 ");

	TEXTMETRIC TM;
	int Nmax;
	tagSIZE Sz;
	CFont* newfont;
	newfont=new CFont();
	MainFontRec.lfEscapement = 0;
	MainFontRec.lfOrientation = 0;
	MainFontRec.lfHeight = ConvertLogPixToRealPix(dc_to_draw, FFontSize, FALSE);
	MainFontRec.lfWidth = 0;
	newfont->CreateFontIndirect(&MainFontRec);

	CFont* OldFont=(CFont*)dc_to_draw->SelectObject(newfont);
	dc_to_draw->GetTextMetrics(&TM);
	GetTextExtentPoint32(dc_to_draw->m_hDC,LPCTSTR(s),s.GetLength(),&Sz);

	double p;
	p=(double)(rect_to_draw.right-rect_to_draw.left-MinShift-MaxShift)/Sz.cx;
	if (p==(int)p) Nmax=(int)p; else Nmax=(int)p+1;

	dc_to_draw->SelectObject(OldFont);

	delete newfont;
	return Nmax;
}

void FHRuler::DrawRuler(CDC* dc_to_draw, CRect rect_to_draw)
{
	int ruler_width = Width(dc_to_draw);
	int MinShift = ruler_width;
	int MaxShift=0;
			// Filling Rectangle
	CPen* OldPen = (CPen*)dc_to_draw->SelectStockObject(NULL_PEN);
	CBrush* OldBrush = (CBrush*) dc_to_draw->SelectStockObject(LTGRAY_BRUSH);
	rect_to_draw.right += 1;
	rect_to_draw.bottom += 1;
	dc_to_draw->Rectangle(rect_to_draw);
	rect_to_draw.right-=2;
	rect_to_draw.bottom-=2;
	dc_to_draw->SelectStockObject(BLACK_PEN);
	dc_to_draw->MoveTo(rect_to_draw.left+MinShift,rect_to_draw.bottom);
	dc_to_draw->LineTo(rect_to_draw.right,rect_to_draw.bottom);
	dc_to_draw->LineTo(rect_to_draw.right,rect_to_draw.top);
	dc_to_draw->SelectStockObject(WHITE_PEN);
	dc_to_draw->LineTo(rect_to_draw.left+MinShift,rect_to_draw.top);
	dc_to_draw->LineTo(rect_to_draw.left+MinShift,rect_to_draw.bottom);
	if (rect_to_draw.bottom>=rect_to_draw.top+ruler_width-1 && rect_to_draw.right-rect_to_draw.left>MinShift+MaxShift+5)
	{
		 // Counting grid value
		CString s;
		s = _T("9.99 ");
		CString UnitName = sUnit+_T("-10");

		TEXTMETRIC TM;
		int Nmax;
		tagSIZE Sz,UnitNameSize; 
		CFont* newfont;
		newfont=new CFont();
		MainFontRec.lfEscapement = 0;
		MainFontRec.lfOrientation = 0;
		MainFontRec.lfHeight = ConvertLogPixToRealPix(dc_to_draw, FFontSize, FALSE);
		MainFontRec.lfWidth = 0;
		newfont->CreateFontIndirect(&MainFontRec);
		CFont* OldFont=(CFont*)dc_to_draw->SelectObject(newfont);
		dc_to_draw->GetTextMetrics(&TM);
		GetTextExtentPoint32(dc_to_draw->m_hAttribDC,LPCTSTR(s),s.GetLength(),&Sz);
		GetTextExtentPoint32(dc_to_draw->m_hAttribDC,LPCTSTR(UnitName),UnitName.GetLength(),&UnitNameSize);
		UnitNameSize.cx += ConvertLogPixToRealPix(dc_to_draw, FRulerWidth/10, FALSE);

		CCoordinates coord(rect_to_draw.left+MinShift, rect_to_draw.right-MaxShift, MinValue, MaxValue);

		Nmax = GetNMax(dc_to_draw, rect_to_draw);
		double Delta;//=(float)CountGrid(minv,maxv,Nmax);
		double* wp;
		double* sp;
		int count, maxexp;
		coord.DivideInterval(Nmax, &maxexp, &Delta, &count, &sp, &wp);
		// drawing grid points
		int CurrentX;
		double CurrentValue;
		//char ch[4];
		dc_to_draw->SelectStockObject(BLACK_PEN);
		dc_to_draw->SetBkMode( TRANSPARENT);
		for (int i=0; i<count; i++)
		{
			CurrentX = (int)sp[i];
			CurrentValue = wp[i];
			double v = CurrentValue*exp(-maxexp*log(10));
			//big marks
			dc_to_draw->MoveTo(CurrentX,rect_to_draw.bottom);	
			dc_to_draw->LineTo(CurrentX,rect_to_draw.bottom-ConvertLogPixToRealPix(dc_to_draw, FBigMarkSize, FALSE));
			//write text above big marks
// Fx mod
//			s.Format(_T("%.1f"), v);
			s.Format(_T("%.0f"), CurrentValue);
			GetTextExtentPoint32(dc_to_draw->m_hAttribDC,LPCTSTR(s),s.GetLength(),&Sz);
//			if (CurrentX-(Sz.cx >> 1)>rect_to_draw.left+MinShift  && CurrentX+(Sz.cx >> 1)< rect_to_draw.right-UnitNameSize.cx)
// End of fx mod 			
				dc_to_draw->TextOut(CurrentX-(Sz.cx >> 1),rect_to_draw.bottom-(int)(TM.tmHeight*1.1),s);
			//little marks
			v = (CurrentValue*exp(-maxexp*log(10))-Delta/2)*exp(maxexp*log(10));
			int x = (int)coord.WtoX(v);
			if (coord.IsPointInRangeX(x))
			{
				dc_to_draw->MoveTo(x, rect_to_draw.bottom);	
				dc_to_draw->LineTo(x, rect_to_draw.bottom-ConvertLogPixToRealPix(dc_to_draw, FSmallMarkSize, FALSE));
			};
			v = (CurrentValue*exp(-maxexp*log(10))+Delta/2)*exp(maxexp*log(10));
			x = (int)coord.WtoX(v);
			if (coord.IsPointInRangeX(x))
			{
				dc_to_draw->MoveTo(x, rect_to_draw.bottom);	
				dc_to_draw->LineTo(x, rect_to_draw.bottom-ConvertLogPixToRealPix(dc_to_draw, FSmallMarkSize, FALSE));
			};
		};		
		//  Draw current measuring unit
		if (maxexp!=0) 
		{
			s.Format(_T("%d"), maxexp);
			UnitName = sUnit+_T("*")+s;
		} else UnitName = sUnit;
//Fx mod
//		dc_to_draw->TextOut(rect_to_draw.right-(UnitNameSize.cx+1),rect_to_draw.bottom-(int)(TM.tmHeight*1.1),UnitName);
// End of fx mod

		if (sp!=NULL) delete sp;
		if (wp!=NULL) delete wp;

		dc_to_draw->SelectObject(OldFont);
		delete newfont;
	};
}

BEGIN_MESSAGE_MAP(FHRuler, FRuler)
	//{{AFX_MSG_MAP(FRuler)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// FVRuler

IMPLEMENT_DYNAMIC(FVRuler, FRuler);

FVRuler::FVRuler() : FRuler()
{
}

int FVRuler::GetNMax(CDC* dc_to_draw, CRect rect_to_draw)
{
	int MinShift = Width(dc_to_draw);
	int MaxShift=0;

	CString s;
	s = _T("9.99 ");

	TEXTMETRIC TM;
	int Nmax;
	tagSIZE Sz;
	CFont* newfont;
	newfont=new CFont();
	MainFontRec.lfEscapement = 900;
	MainFontRec.lfOrientation = 900;
	MainFontRec.lfHeight = ConvertLogPixToRealPix(dc_to_draw, FFontSize, TRUE);
	MainFontRec.lfWidth = 0;
	newfont->CreateFontIndirect(&MainFontRec);

	CFont* OldFont=(CFont*)dc_to_draw->SelectObject(newfont);
	dc_to_draw->GetTextMetrics(&TM);
	GetTextExtentPoint32(dc_to_draw->m_hDC,LPCTSTR(s),s.GetLength(),&Sz);

	double p;
	p=(double)(rect_to_draw.bottom-rect_to_draw.top-MinShift-MaxShift)/Sz.cx;
	if (p==(int)p) Nmax=(int)p; else Nmax=(int)p+1;

	dc_to_draw->SelectObject(OldFont);

	delete newfont;

	return Nmax;
}

void FVRuler::DrawRuler(CDC* dc_to_draw, CRect rect_to_draw)
{
	int ruler_width = Width(dc_to_draw);
	int MinShift = ruler_width;
	int MaxShift=0;

	CPen* OldPen = (CPen*)dc_to_draw->SelectStockObject(NULL_PEN);
	CBrush* OldBrush = (CBrush*) dc_to_draw->SelectStockObject(LTGRAY_BRUSH);
	rect_to_draw.right+=1;
	rect_to_draw.bottom+=1;
	dc_to_draw->Rectangle(rect_to_draw);
	rect_to_draw.right-=2;
	rect_to_draw.bottom-=2;
	dc_to_draw->SelectStockObject(BLACK_PEN);
	dc_to_draw->MoveTo(rect_to_draw.left,rect_to_draw.bottom);
	dc_to_draw->LineTo(rect_to_draw.right,rect_to_draw.bottom);
	dc_to_draw->LineTo(rect_to_draw.right,rect_to_draw.top+MinShift);
	dc_to_draw->SelectStockObject(WHITE_PEN);
	dc_to_draw->LineTo(rect_to_draw.left,rect_to_draw.top+MinShift);
	dc_to_draw->LineTo(rect_to_draw.left,rect_to_draw.bottom);
	if (rect_to_draw.bottom-rect_to_draw.top>MinShift+MaxShift+5 && rect_to_draw.right>=rect_to_draw.left+ruler_width-1)
	{
		// Counting grid value
		CString s;
		s = _T("9.99 ");
		CString UnitName = sUnit+_T("-10");
		TEXTMETRIC TM;
		int Nmax;
		tagSIZE Sz,UnitNameSize;
		CFont* newfont;
		newfont=new CFont();
		MainFontRec.lfEscapement = 900;
		MainFontRec.lfOrientation = 900;
		MainFontRec.lfHeight = ConvertLogPixToRealPix(dc_to_draw, FFontSize, TRUE);
		MainFontRec.lfWidth = 0;
		newfont->CreateFontIndirect(&MainFontRec);
		CFont* OldFont=(CFont*)dc_to_draw->SelectObject(newfont);
		dc_to_draw->GetTextMetrics(&TM);
		GetTextExtentPoint32(dc_to_draw->m_hAttribDC,LPCTSTR(s),s.GetLength(),&Sz);
		GetTextExtentPoint32(dc_to_draw->m_hAttribDC,LPCTSTR(UnitName),UnitName.GetLength(),&UnitNameSize);
		UnitNameSize.cx += ConvertLogPixToRealPix(dc_to_draw, FRulerWidth/10, TRUE);

		CCoordinates coord(rect_to_draw.bottom-MaxShift, rect_to_draw.top+MinShift, MinValue, MaxValue);

		Nmax = GetNMax(dc_to_draw, rect_to_draw);
		double Delta;//=(float)CountGrid(minv,maxv,Nmax);
		double* wp;
		double* sp;
		int count, maxexp;
		coord.DivideInterval(Nmax, &maxexp, &Delta, &count, &sp, &wp);

		  // drawing grid points
		int CurrentY;
		double CurrentValue;
		dc_to_draw->SelectStockObject(BLACK_PEN);
		dc_to_draw->SetBkMode( TRANSPARENT);
		for (int i=0; i<count; i++)
		{
			CurrentY = (int)sp[i];
			CurrentValue = wp[i];
			double v = CurrentValue*exp(-maxexp*log(10));
			//big marks
			dc_to_draw->MoveTo(rect_to_draw.right,CurrentY);
			dc_to_draw->LineTo(rect_to_draw.right-ConvertLogPixToRealPix(dc_to_draw, FBigMarkSize, TRUE),CurrentY);
			//write text above big marks
// Fx mod
//			s.Format(_T("%.1f"), v);
			s.Format(_T("%.0f"), CurrentValue);
			GetTextExtentPoint32(dc_to_draw->m_hAttribDC,LPCTSTR(s),s.GetLength(),&Sz);
//			if (CurrentY+(Sz.cx >> 1)<rect_to_draw.bottom && CurrentY-(Sz.cx >> 1)>rect_to_draw.top+MinShift+UnitNameSize.cx)
// End of fx mod 			
			{
			    dc_to_draw->TextOut(rect_to_draw.right-TM.tmHeight,CurrentY+(Sz.cx >> 1),s);
			};
			//little marks
			v = (CurrentValue*exp(-maxexp*log(10))-Delta/2)*exp(maxexp*log(10));
			int x = (int)coord.WtoX(v);
			if (coord.IsPointInRangeX(x))
			{
				dc_to_draw->MoveTo(rect_to_draw.right,x);
				dc_to_draw->LineTo(rect_to_draw.right-ConvertLogPixToRealPix(dc_to_draw, FSmallMarkSize, TRUE),x);
			};
			v = (CurrentValue*exp(-maxexp*log(10))+Delta/2)*exp(maxexp*log(10));
			x = (int)coord.WtoX(v);
			if (coord.IsPointInRangeX(x))
			{
				dc_to_draw->MoveTo(rect_to_draw.right,x);
				dc_to_draw->LineTo(rect_to_draw.right-ConvertLogPixToRealPix(dc_to_draw, FSmallMarkSize, TRUE),x);
			};
		};		
		//  Draw current measuring unit
		if (maxexp!=0) 
		{
			s.Format(_T("%d"), maxexp);
			UnitName = sUnit+_T("*")+s;
		} else UnitName = sUnit;
//Fx mod
//		dc_to_draw->TextOut(rect_to_draw.right-TM.tmHeight,rect_to_draw.top+ruler_width+(UnitNameSize.cx+1),UnitName);
// End of fx mod

		if (sp!=NULL) delete sp;
		if (wp!=NULL) delete wp;

		dc_to_draw->SelectObject(OldFont);
		delete newfont;
	};
 }

int FVRuler::Width(CDC* dest_dc)
{
    return ConvertLogPixToRealPix(dest_dc, FRulerWidth, TRUE);
}

BEGIN_MESSAGE_MAP(FVRuler, FRuler)
	//{{AFX_MSG_MAP(FRuler)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

