/*
This file was created by Paul Barvinko (pbarvinko@yahoo.com).
This file is distributed "as is", e.g. there are no warranties 
and obligations and you could use it in your applications on your
own risk. 
Your comments and questions are welcome.
If using in your applications, please mention author in credits for your app.
*/

#include "../../stdafx.h"
#include "gen_prop_dlgs.h"
#include "graphwnd.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CAxisPropertyPage property page

IMPLEMENT_DYNCREATE(CAxisPropertyPage, CPropertyPage)

CAxisPropertyPage::CAxisPropertyPage() : CPropertyPage(CAxisPropertyPage::IDD)
{
	//{{AFX_DATA_INIT(CAxisPropertyPage)
	m_x_precision = 0;
	m_x_title = _T("");
	m_x_uom = _T("");
	m_y_precision = 0;
	m_y_title = _T("");
	m_y_uom = _T("");
	//}}AFX_DATA_INIT
}

CAxisPropertyPage::~CAxisPropertyPage()
{
}

void CAxisPropertyPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAxisPropertyPage)
	DDX_Text(pDX, IDC_XAXIS_PRECISION, m_x_precision);
	DDV_MinMaxLong(pDX, m_x_precision, 0, 10);
	DDX_Text(pDX, IDC_XTITLE, m_x_title);
	DDX_Text(pDX, IDC_XUOM, m_x_uom);
	DDX_Text(pDX, IDC_YAXIS_PRECISION, m_y_precision);
	DDV_MinMaxLong(pDX, m_y_precision, 0, 10);
	DDX_Text(pDX, IDC_YTITLE, m_y_title);
	DDX_Text(pDX, IDC_YUOM, m_y_uom);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CAxisPropertyPage, CPropertyPage)
	//{{AFX_MSG_MAP(CAxisPropertyPage)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAxisPropertyPage message handlers


/////////////////////////////////////////////////////////////////////////////

void CGraphPropsComboBox::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
    //get pointer to the main graph class
    CGraphicsPropertyPage* parent_ptr = dynamic_cast<CGraphicsPropertyPage*>(GetParent());
    ASSERT(parent_ptr != NULL);
    if (parent_ptr != NULL)
    {
	int index = lpDrawItemStruct->itemID;
	if (index >=0)
	{
	    CGraphProps* graph_props = parent_ptr->grprops.GetAt(index);
	    ASSERT(graph_props != NULL);
	    DrawComboItem(graph_props->GetGraphColor(), graph_props->GetTitle(), lpDrawItemStruct);
	};
    };
}

// CGraphicsPropertyPage property page

IMPLEMENT_DYNCREATE(CGraphicsPropertyPage, CPropertyPage)

CGraphicsPropertyPage::CGraphicsPropertyPage() : CPropertyPage(CGraphicsPropertyPage::IDD)
{
    ASSERT(FALSE);  //should never be called
}

CGraphicsPropertyPage::CGraphicsPropertyPage(CGraphWnd* main_wnd) : CPropertyPage(CGraphicsPropertyPage::IDD)
{
	//{{AFX_DATA_INIT(CGraphicsPropertyPage)
	m_graph_title = _T("");
	//}}AFX_DATA_INIT

    //fill array of graph properties
    if (main_wnd != NULL && main_wnd->GetGraphCount() != 0)
    {
	//fill new array with data
	int index;
	CGraphProps* grp;
	CGraphProps* grprop = main_wnd->GetFirstGraph(&index);
	while (grprop!=NULL)
	{
	    grp = new CGraphProps(grprop, FALSE);
	    grp->SetIndex(index);
	    grprops.Add(grp);
	    grprop = main_wnd->GetNextGraph(&index);
	};
    };
}

CGraphicsPropertyPage::~CGraphicsPropertyPage()
{
    for (int i=0; i<grprops.GetSize(); i++) delete grprops.GetAt(i);
}

void CGraphicsPropertyPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CGraphicsPropertyPage)
	DDX_Control(pDX, IDC_GRAPH_CHOICE, m_graph_combo);
	DDX_Text(pDX, IDC_GRAPH_TITLE, m_graph_title);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CGraphicsPropertyPage, CPropertyPage)
	//{{AFX_MSG_MAP(CGraphicsPropertyPage)
	ON_BN_CLICKED(IDC_GRAPH_CH_COLOR, OnGraphChColor)
	ON_BN_CLICKED(IDC_GRAPH_CH_TITLE, OnGraphChTitle)
	ON_BN_CLICKED(IDC_GRAPH_HIDESHOW, OnGraphHideshow)
	ON_BN_CLICKED(IDC_GRAPHS_SHOWALL, OnGraphsShowall)
	ON_CBN_SELCHANGE(IDC_GRAPH_CHOICE, OnSelchangeGraphChoice)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGraphicsPropertyPage message handlers
BOOL CGraphicsPropertyPage::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();
	
	//fill graph combobox
	for (int i=0; i<grprops.GetSize(); i++)
	{
	    m_graph_combo.AddString(_T(""));
	};
	m_graph_combo.SetCurSel(0);
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CGraphicsPropertyPage::OnGraphChColor() 
{
    int index = m_graph_combo.GetCurSel();
    if (index == -1) return;
    CGraphProps* grprop = grprops.GetAt(index);
    CColorDialog dlg(grprop->GetGraphColor());
    if (dlg.DoModal() == IDOK)
    {
	grprop->SetGraphColor(dlg.GetColor());
	m_graph_combo.RedrawWindow();
    };
}

void CGraphicsPropertyPage::OnGraphChTitle() 
{
    int index = m_graph_combo.GetCurSel();
    if (index == -1) return;
    CGraphProps* grprop = grprops.GetAt(index);
    CGraphTitlePrompt dlg;
    dlg.m_title = grprop->GetTitle();
    if (dlg.DoModal() == IDOK)
    {
	grprop->SetTitle(dlg.m_title);
	m_graph_combo.RedrawWindow();
	m_graph_title = grprop->GetTitle();
	UpdateData(FALSE);
    };
}

void CGraphicsPropertyPage::OnGraphHideshow() 
{
    int index = m_graph_combo.GetCurSel();
    if (index == -1) return;
    CGraphProps* grprop = grprops.GetAt(index);
    grprop->SetVisible(!grprop->IsVisible());
    SetHideShowAttr(index);
}

void CGraphicsPropertyPage::OnGraphsShowall() 
{
    int index = m_graph_combo.GetCurSel();
    if (index == -1) return;
    CGraphProps* grprop;
    for (int i=0; i<grprops.GetSize(); i++)
    {
	grprop = grprops.GetAt(i);
	grprop->SetVisible(TRUE);
    };
    SetHideShowAttr(index);
}

void CGraphicsPropertyPage::OnSelchangeGraphChoice() 
{
    SetActiveSel(m_graph_combo.GetCurSel());
}

void CGraphicsPropertyPage::SetActiveSel(int selnum)
{
    if (selnum<0 || selnum>=grprops.GetSize()) return;
    m_graph_combo.SetCurSel(selnum);
    SetHideShowAttr(selnum);
    CGraphProps* grprop = grprops.GetAt(selnum);
    m_graph_title = grprop->GetTitle();
    UpdateData(FALSE);
}

void CGraphicsPropertyPage::SetHideShowAttr(int selnum)
{
    if (selnum == -1) return;
    CGraphProps* grprop = grprops.GetAt(selnum);
    CWnd* wnd = GetDlgItem(IDC_GRAPH_HIDESHOW);
    if (wnd == NULL) return;
    if (grprop->IsVisible()) 
    {
	wnd->SetWindowText(_T("Hide"));
    } else
    {
	wnd->SetWindowText(_T("Show"));
    };
}
/////////////////////////////////////////////////////////////////////////////
// CGraphTitlePrompt dialog


CGraphTitlePrompt::CGraphTitlePrompt(CWnd* pParent /*=NULL*/)
	: CDialog(CGraphTitlePrompt::IDD, pParent)
{
	//{{AFX_DATA_INIT(CGraphTitlePrompt)
	m_title = _T("");
	//}}AFX_DATA_INIT
}


void CGraphTitlePrompt::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CGraphTitlePrompt)
	DDX_Text(pDX, IDC_TITLE_STRING, m_title);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CGraphTitlePrompt, CDialog)
	//{{AFX_MSG_MAP(CGraphTitlePrompt)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGraphTitlePrompt message handlers


