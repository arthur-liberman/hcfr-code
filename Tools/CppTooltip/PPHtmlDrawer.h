#ifndef _PPHTMLDRAWER_H
#define _PPHTMLDRAWER_H

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// PPHtmlDrawer.h : header file
//

#include "PPDrawManager.h"
#include <vector>
#include <map>

/////////////////////////////////////////////////////////////////////////////
// CPPHtmlDrawer window

class CPPHtmlDrawer/* : public CObject*/
{
// Construction
public:
	CPPHtmlDrawer();

// Attributes
public:

// Operations
public:
	//Drawing methods
	void  Draw(CDC * pDC, CString & strHtml, CPoint ptTopLeft);
	CSize PrepareOutput(CDC * pDC, CString & strHtml); //Prepares to draw the HTML string
	void  DrawPreparedOutput(CDC * pDC, CString & strHtml, CRect rect);

	void  EnableEscapeSequences(BOOL bEnable = TRUE);
	
	//Shadow of the image
	void SetImageShadow(int nOffsetX, int nOffsetY, BYTE nDarkenPercent = 50, BOOL bGradient = TRUE, int nDepthX = 7, int nDepthY = 7);

	CString GetResCommandPrompt(UINT nID, UINT nNumParam = 0);

	//Functions for the styles
	void SetCssStyles(LPCTSTR lpszCssString = NULL); //Sets the CSS styles
	void SetCssStyles(DWORD dwIdCssString, LPCTSTR lpszPathDll = NULL); //Sets the CSS styles
	LPCTSTR GetCssStyles(); //Returns the current CSS styles

	void OnLButtonDown(CPoint & ptClient);
	BOOL OnSetCursor(CPoint & ptClient);

	void SetHyperlinkCursor(HCURSOR hCursor = NULL); //Sets the cursor to be displayed when moving the mouse over a link. Specifying NULL will cause the control to display its default 'hand' cursor.
	HCURSOR GetHyperlinkCursor() const; //Returns the current link cursor.

	void SetCallbackHyperlink(HWND hWnd, UINT nMessage, LPARAM lParam = 0); //Sets the callback message: "Mouse over the link".
	void SetCallbackRepaint(HWND hWnd, UINT nMessage, LPARAM lParam = 0); //Sets the callback message: "Please repaint me".

	//Functions for images
	void SetImageList(UINT nIdBitmap, int cx, int cy, int nCount, COLORREF crMask = RGB(255, 0, 255));
	void SetImageList(HBITMAP hBitmap, int cx, int cy, int nCount, COLORREF crMask = RGB(255, 0, 255));

	void LoadResourceDll(LPCTSTR lpszPathDll, DWORD dwFlags = 0); //Sets the path to the resource's DLL
	void SetResourceDll(HINSTANCE hInstDll = NULL); //Sets the handle of the loaded resource's DLL

	CPPDrawManager * GetDrawManager();

	static short GetVersionI()		{return 0x10;}
	static LPCTSTR GetVersionC()	{return (LPCTSTR)_T("1.0");}
	
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPPHtmlDrawer)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CPPHtmlDrawer();

protected:
	enum{	ALIGN_LEFT = 0,
			ALIGN_CENTER,
			ALIGN_RIGHT
		};
	
	enum{	ALIGN_TOP = 0,
			ALIGN_VCENTER,
			ALIGN_BOTTOM,
			ALIGN_BASELINE
		};

	enum{	LINK_NONE = 0,
			LINK_HREF,
			LINK_MESSAGE
		};

	enum{	TEXT_TRANSFORM_NONE = 0,
			TEXT_TRANSFORM_UPPERCASE,
			TEXT_TRANSFORM_LOWERCASE,
			TEXT_TRANSFORM_CAPITALIZE
		};

	enum{	BORDER_STYLE_NONE = 0,
			BORDER_STYLE_SOLID,
			BORDER_STYLE_DOTTED,
			BORDER_STYLE_DASHED,
			BORDER_STYLE_DOUBLE,
		};



#pragma pack(1)
	typedef struct _STRUCT_HYPERLINK
	{
		CRect rcArea;		// The hot rect of the hyperlink
		int nTypeLink;		// The type of the hyperlink
		int nIndexLink;		// The index of the hyperlink
		CString sHyperlink; // The hyperlink
	} STRUCT_HYPERLINK;
#pragma pack()
	
#pragma pack(1)
	typedef struct _STRUCT_CHANGESTYLE 
	{
		CString strTag;		//The name of the last opened tag
		
		//Font
		int  nSizeFont;		//The height of the logic font
		int	 nWeightFont;	//The weight of the logic font
		BOOL bItalicFont;	//Is italic logic font?
		BOOL bUnderlineFont;//Is underline logic font?
		BOOL bStrikeOutFont;//Is strikeout logic font?
		BOOL bOverlineFont; //Is overline logic font?
		CString sFaceFont;  //The face name of the logic font
		
		//Color		
		COLORREF crText;	//The foreground color 
		COLORREF crBkgnd;	//The background color (also begin for the gradient)
		COLORREF crBorderLight;	//The border color
		COLORREF crBorderDark;	//The border color
		COLORREF crMidBkgnd;//The middle background color
		COLORREF crEndBkgnd;//The end background color

		//Fill
		int  nBkMode;		//The background mode for the text (TRANSPARENT, OPAQUE)
		int  nFillBkgnd;	//The fill effect of the background
		CString strNameResBk;

		//Align
		int  nHorzAlign;	//The horizontal align
		int  nVertAlign;	//The vertical align
		
		//Border
		int  nBorderStyle;	//The border style
		int  nBorderWidth;	//The width of the border

		//Text
		int  nTextTransform;//Transformation of the text (NONE, UPPERCASE, LOWERCASE, CAPITALIZE)

		//Margins
		int nMarginLeft;
		int nMarginRight;
		int nMarginTop;
		int nMarginBottom;
		
		//Padding
		int nPaddingLeft;
		int nPaddingRight;
		int nPaddingTop;
		int nPaddingBottom;
		
		//Hyperlink
		int  nTypeLink;		//The type of the link (NONE, HREF, MESSAGE)
		CString sHyperlink; //The additional parameter for the link
	} STRUCT_CHANGESTYLE; 
#pragma pack()
	
#pragma pack(1)
	typedef struct _STRUCT_IMAGE
	{
		int			nIndexImageList;//image's index of the image list
		int			nIdRes;			//ID resource from app
		int			nIdDll;			//ID resource from dll
		int			nHandle;		//handle of the resource
		int			cx;				//horizontal size of image
		int			cy;				//vertical size of image
		int			nWidth;			//width of image
		int			nHeight;		//height of image
		UINT		nStyles;		//styles of image
		UINT		nHotStyles;		//hot styles of image
		BOOL        bUseMask;		//
		BOOL		bPercentWidth;
		BOOL		bPercentHeight;
		COLORREF	crMask;			//color of mask
		CString		strSrcFile;		//path on the source file
		CString     strPathDll;		//path on the resource dll
	} STRUCT_IMAGE;
#pragma pack()
	
#pragma pack(1)
	typedef struct _STRUCT_CALLBACK
	{
		HWND		hWnd;			/* Дескриптор окна, принимающего сообщение */
		UINT		nMessage;		// Message identifier
		WPARAM		wParam;
		LPARAM		lParam;
	} STRUCT_CALLBACK;
#pragma pack()

#pragma pack(1)
	typedef struct _STRUCT_CELLPARAM
	{
//		int  rowspan;
//		int  colspan;
		UINT uBorder;
		int  iFillBkgnd;
		UINT uMarginCx;
		UINT uMarginCy;
		UINT uBorderStyle;
		UINT uVAlign;
		UINT uAlign;
		COLORREF clrBorder;
		COLORREF clrBkgnd;
		COLORREF clrMidBkgnd;
		COLORREF clrEndBkgnd;
	} STRUCT_CELLPARAM;
#pragma pack()

#pragma pack(1)
	typedef struct _STRUCT_HTMLLINE
	{
		int  nWidthLine;
		int  nHeightLine;
		int  nDescentLine;
		int  nAddPercentWidth;
		int  nHorzAlign;
	} STRUCT_HTMLLINE;
#pragma pack()

#pragma pack(1)
	typedef struct _STRUCT_CELL
	{
		int   nColSpan; //-1 = Cell isn't used, >0 - How much columns was spaned
		int   nRowSpan; //-1 = Cell isn't used, >0 - How much rows was spaned
		CSize szText;
		CSize szCell;
	} STRUCT_CELL;
#pragma pack()

	STRUCT_CALLBACK	m_csCallbackRepaint; //Callback for repaint HTML drawer
	STRUCT_CALLBACK	m_csCallbackLink; //Callback for hyperlink message
	STRUCT_CHANGESTYLE m_defStyle;
	STRUCT_HTMLLINE m_hline;

	CPPDrawManager m_drawmanager;
	
	//Values of the system context
	CImageList m_ImageList;
	CSize m_szImageList;

	HINSTANCE m_hInstDll;
	BOOL m_bFreeInstDll;

	HCURSOR m_hLinkCursor;
	CFont * m_pOldFont;
	int m_nOldBkMode;
	COLORREF m_crOldText;
	COLORREF m_crOldBk;
//	COLORREF m_clrShadow;

	CSize m_szOutput; // Output size
	int   m_nCurLine;   // The current drawing line
	int   m_nCurTableIndex; //The current index of the table
	int   m_nNumCurTable; //The number of the current table
	CRect m_rect; //
//	int m_nLineHeight; //The height of the current line
//	int m_nLineDescent;
	int m_nHoverIndexLink; //The index of the link under the mouse
	int m_nCurIndexLink;
	BOOL m_bLastValueIsPercent;
	BOOL m_bEnableEscapeSequences; // 

	//Shadow of the image
	BOOL m_bGradientShadow;
	SIZE m_szOffsetShadow;
	SIZE m_szDepthShadow;
	BYTE m_nDarkenShadow;
	COLORREF m_crShadow;

	TEXTMETRIC m_tm;

	LOGFONT m_lfDefault; //Default font
	CFont m_font;
	CDC * m_pDC;

	//Wrapper string
	CString m_strPrefix; //Prefix string 
	CString m_strPostfix; //Postfix string
	CString m_strCssStyles;

	//Vectors
	typedef std::vector<STRUCT_HTMLLINE> vecHtmlLine;
	vecHtmlLine m_arrHtmlLine;

	//Vector of the stack
	typedef std::vector<STRUCT_CHANGESTYLE> arrStack;
	arrStack m_arrStack;

	//Vector of the hyperlinks
	typedef std::vector<STRUCT_HYPERLINK> arrLink;
	arrLink m_arrLinks;

	//Map of the colors by name
	typedef std::map<CString, COLORREF> mapColors;
	typedef std::map<CString, COLORREF>::iterator iterMapColors;
	mapColors m_mapColors;

	//Map of the styles
	typedef std::map<CString, CString> mapStyles;
	typedef std::map<CString, CString>::iterator iter_mapStyles;
	mapStyles m_mapStyles;

	//Cells of Table
	typedef std::vector<STRUCT_CELL> vecCol;
	typedef std::vector<vecCol> vecTable;
	vecTable m_arrTable;

	//Dimensions of the columns and rows
	typedef std::vector<int> vecSize;
	typedef std::vector<vecSize> vecRowCol;
	vecRowCol m_arrTableSizes;

protected:
	//The resource's methods
	HICON GetIconFromResources(DWORD dwID, CSize szIcon = CSize(0, 0)) const; //Load an icon from the app resources
	HICON GetIconFromFile(LPCTSTR lpszPath, CSize szIcon = CSize(0, 0)) const; //Load an icon from the file
	HICON GetIconFromDll(DWORD dwID, CSize szIcon = CSize(0, 0), LPCTSTR lpszPathDll = NULL) const; //Load an icon from the dll resources
	HBITMAP GetBitmapFromResources(DWORD dwID) const; //Load a bitmap from the app resources
	HBITMAP GetBitmapFromFile(LPCTSTR lpszPath) const; //Load a bitmap from the file
	HBITMAP GetBitmapFromDll(DWORD dwID, LPCTSTR lpszPathDll = NULL) const; //Load a bitmap from the dll resources
	CString GetStringFromResource(DWORD dwID) const; //Load a string from the app resources
	CString GetStringFromDll(DWORD dwID, LPCTSTR lpszPathDll = NULL) const; //Load a string from the dll resources

	//The drawing methods
	CSize DrawHtml (CDC * pDC, CString & str, CRect & rect, BOOL bCalc = TRUE); //Draws the HTML text on device context or gets the size of the output area.
	CSize DrawHtmlTable (CDC * pDC, CString & str, CRect & rect, BOOL bCalc = TRUE); //Draws the HTML table on device context or gets the size of the output area.
	CSize DrawHtmlString (CDC * pDC, CString & str, CRect & rect, BOOL bCalc = TRUE); //Draws the HTML string on device context or gets the size of the output area.
	
//public:
	//The methods
	void SetDefaultCssStyles();
	void SetDefaultCursor();
	LPLOGFONT GetSystemToolTipFont() const; //Gets the system logfont

	CString SearchBodyOfTag(CString & str, CString & strTag, int & nIndex); //Search a tag's body
	CString SearchPropertyOfTag(CString & str, int & nIndex); //Search a name or a property of a tag
	CSize AnalyseCellParam(CString & strTag, _STRUCT_CHANGESTYLE & cs);
	void  AnalyseImageParam(CString & strTag, _STRUCT_IMAGE & si);
	BOOL  IsImageWithShadow(_STRUCT_IMAGE & si);

	//Functions for hyperlink
	int PtInHyperlink(CPoint pt);
	void JumpToHyperlink(int nLink);
	void StoreHyperlinkArea(CRect rArea);
	HINSTANCE GotoURL(LPCTSTR url, int showcmd = SW_SHOW);
	LONG GetRegKey(HKEY key, LPCTSTR subkey, LPTSTR retdata);

	//Functions for notify
	void CallbackOnClickHyperlink(LPCTSTR sLink);
	void CallbackOnRepaint(int nIndexLink);

	//Running tag
	void   HorizontalAlign(CPoint & ptCur);
	CPoint VerticalAlignText(CPoint pt, int nHeight, BOOL bCalc);
	CPoint VerticalAlignImage(CPoint pt, int nHeight, BOOL bCalc);
	void UpdateContext(CDC * pDC);
	BOOL StoreRestoreStyle(BOOL bRestore);
	void Tag_NewLine(CPoint & ptCur, int nNum, BOOL bCalc);
	void Tag_Tabulation(CPoint & ptCur, int nNum);
	void InitNewLine(CPoint & pt, BOOL bCalc);

	void SelectNewHtmlStyle(LPCTSTR lpszNameStyle, STRUCT_CHANGESTYLE & cs);

	CSize GetTableDimensions(CString str); //Gets dimensions of the table
	void  SearchEndOfTable(CString & str, int & nIndex); //Searching end of the table

protected:
	//Functions for the map of the styles
	void SetTableOfColors();
	void SetColorName(LPCTSTR lpszColorName, COLORREF color);
	COLORREF GetColorByName(LPCTSTR lpszColorName, COLORREF crDefColor = RGB(0, 0, 0));

	BOOL SearchTag(CString & str, int & nIndex, CString strTag);
	BOOL GetIndexNextAlphaNum(CString & str, int & nIndex, BOOL bArithmetic = FALSE);
	BOOL GetBeginParameter(CString & str, int & nIndex, TCHAR chSeparator = _T(':'));
	TCHAR GetIndexNextChars(CString & str, int & nIndex, CString strChars);
	TCHAR GetIndexNextNoChars(CString & str, int & nIndex, CString strChars);
	CString GetParameterString(CString & str, int & nIndex, TCHAR chBeginParam = _T(':'), CString strSeparators = _T(";"));
	CString GetNameOfTag(CString & str, int & nIndex);

	//Functions for the map of the styles
	LPCTSTR GetTextStyle(LPCTSTR lpszStyleName);
	void SetTextStyle(LPCTSTR lpszStyleName, LPCTSTR lpszStyleValue);
	void RemoveTextStyle(LPCTSTR lpszStyleName);
	void AddToTextStyle(LPCTSTR lpszStyleName, LPCTSTR lpszAddStyle);
	void UnpackTextStyle(CString strStyle, _STRUCT_CHANGESTYLE & cs);

	//Functions for analyzing parameters
	void SetDefaultStyles(_STRUCT_CHANGESTYLE & cs);
	BOOL GetStyleFontStyle(CString & str, BOOL bDefault);
	int  GetStyleFontWeight(CString & str, int nDefault);
	int  GetStyleHorzAlign(CString & str, int nDefault);
	int  GetStyleVertAlign(CString & str, int nDefault);
	COLORREF GetStyleColor(CString & str, COLORREF crDefault);
	int  GetStyleTextTransform(CString & str, int nDefault);
	CString GetStyleString(CString str, CString strDefault);
	void GetStyleFontShortForm(CString & str);
	UINT GetStyleImageShortForm(CString & str);
	int GetStyleBkgndEffect(CString & str, int nDefault);
	
	void StyleTextDecoration(CString & str, _STRUCT_CHANGESTYLE & cs);
	int StyleBorderWidth(CString & str, int Default);
	int StyleBorder(CString & str, int nDefault);

	//Get
	int GetLengthUnit(CString & str, int nDefault, BOOL bFont = FALSE);
	BOOL IsPercentableValue(CString & str);
	int GetTableWidth(CString & str, int nClientWidth, int nMinWidth, BOOL bSet = FALSE);

	//Drawing
	void DrawBackgroundImage(CDC * pDC, int nDestX, int nDestY, int nWidth, int nHeight, CString strNameImage);
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif
