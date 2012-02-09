/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2008 Association Homecinema Francophone.  All rights reserved.
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
//  Benoit SEGUIN
/////////////////////////////////////////////////////////////////////////////
// IRProfilePropPage.cpp : implementation file
//

#include "stdafx.h"
#include "colorhcfr.h"
#include "IRProfilePropPage.h"
#include "KiGenerator.h"
#include <io.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CIRProfilePropPage property page

IMPLEMENT_DYNCREATE(CIRProfilePropPage, CDialog)

CIRProfilePropPage::CIRProfilePropPage() : CDialog(CIRProfilePropPage::IDD)
{
	//{{AFX_DATA_INIT(CIRProfilePropPage)
	m_LatencyNav = 0;
	m_LatencyNext = 0;
	m_LatencyValid = 0;
	m_Name = _T("");
	//}}AFX_DATA_INIT

	m_CodeNext = _T("");
	m_CodeOk = _T("");
	m_CodeDown = _T("");
	m_CodeRight = _T("");
	m_pGenerator = NULL;
	m_comPort = _T("USB");
	m_bUpdateExisting = FALSE;
}

CIRProfilePropPage::~CIRProfilePropPage()
{
}


void CIRProfilePropPage::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CIRProfilePropPage)
	DDX_Control(pDX, IDC_EDIT_CODE_IR, m_Edit_IR);
	DDX_Control(pDX, IDC_COMBO_CODE_NAME, m_ComboCodeName);
	DDX_Text(pDX, IDC_EDIT_LATENCY_NAV, m_LatencyNav);
	DDV_MinMaxInt(pDX, m_LatencyNav, 0, 5000);
	DDX_Text(pDX, IDC_EDIT_LATENCY_NEXT, m_LatencyNext);
	DDV_MinMaxInt(pDX, m_LatencyNext, 0, 5000);
	DDX_Text(pDX, IDC_EDIT_LATENCY_VALID, m_LatencyValid);
	DDV_MinMaxInt(pDX, m_LatencyValid, 0, 5000);
	DDX_Text(pDX, IDC_IR_PROFILE_NAME, m_Name);
	DDV_MaxChars(pDX, m_Name, 15);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CIRProfilePropPage, CDialog)
	//{{AFX_MSG_MAP(CIRProfilePropPage)
	ON_BN_CLICKED(IDC_ACQUIRE, OnAcquireIRCode)
	ON_EN_CHANGE(IDC_EDIT_CODE_IR, OnChangeEditCodeIr)
	ON_CBN_SELCHANGE(IDC_COMBO_CODE_NAME, OnSelchangeComboCodeName)
	ON_BN_CLICKED(IDC_SEND_IR_CODE, OnSendIrCode)
	ON_BN_CLICKED(IDC_TEST_IR_VALID, OnTestIrValid)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CIRProfilePropPage message handlers

void CIRProfilePropPage::OnAcquireIRCode() 
{
	BOOL	bOk = FALSE;
	CString	OldComPort, Msg;

	UpdateData ( TRUE );

	if ( m_pGenerator )
	{
		OldComPort = m_pGenerator -> m_comPort;
		m_pGenerator -> m_comPort = m_comPort;

		if ( m_pGenerator -> InitRealComPort () )
		{
			CWaitCursor	wait;

			CString str = m_pGenerator -> AcquireIRCode ();

			if ( ! str.IsEmpty () )
			{
				m_Edit_IR.SetWindowText ( str );
				OnSelchangeComboCodeName ();
				bOk = TRUE;
			}
		}

		m_pGenerator -> m_comPort = OldComPort;

		if ( bOk )
			Msg.LoadString ( IDS_CODERECEIVED );
		else
			Msg.LoadString ( IDS_NOCODERECEIVED );
		
		AfxMessageBox ( Msg );
	}
}

void CIRProfilePropPage::OnChangeEditCodeIr() 
{
	switch ( m_nCurrentSel )
	{
		case 0:	// Next
			 m_Edit_IR.GetWindowText ( m_CodeNext );
			 break;

		case 1: // Ok
			 m_Edit_IR.GetWindowText ( m_CodeOk );
			 break;

		case 2: // Down
			 m_Edit_IR.GetWindowText ( m_CodeDown );
			 break;

		case 3:	// Right
			 m_Edit_IR.GetWindowText ( m_CodeRight );
			 break;
	}
}


BOOL CIRProfilePropPage::OnInitDialog() 
{
	CDialog::OnInitDialog();
	// TODO: Add extra initialization here
	
	if ( m_bUpdateExisting )
		m_OriginalName = m_Name;
	else
		m_OriginalName.Empty ();

	if (m_Name != "")
	{
		CString str = GetConfig () -> m_ApplicationPath;
		str += m_IRProfile.GetIhcFileSubDir();
		GetConfig () -> EnsurePathExists ( str );
		str += '\\';
		str += m_Name;
		str +=".ihc";
		m_fileName = str;

		if((_access( m_fileName, 0 )) != -1 )
		{
			CFile IhcFile ( str, CFile::modeRead | CFile::shareDenyNone );
			CArchive ar ( & IhcFile, CArchive::load );
			m_IRProfile.Serialize(ar);
		}
		else
			m_IRProfile.Clear();
	}
	else
		m_IRProfile.Clear();
	
	m_CodeNext=m_IRProfile.m_NextCode;
	m_CodeOk=m_IRProfile.m_OkCode;
	m_CodeDown=m_IRProfile.m_DownCode;
	m_CodeRight=m_IRProfile.m_RightCode;
	m_LatencyNav = m_IRProfile.m_NavInMenuLatency;
	m_LatencyNext = m_IRProfile.m_ValidMenuLatency;
	m_LatencyValid = m_IRProfile.m_NextChapterLatency;
			
	m_nCurrentSel = -1;
	m_ComboCodeName.SetCurSel ( 0 );
	m_nCurrentSel = 0;
	m_Edit_IR.SetWindowText ( m_CodeNext );

	UpdateData(FALSE);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
void CIRProfilePropPage::OnSelchangeComboCodeName() 
{
	switch ( m_nCurrentSel )
	{
		case 0:	// Next
			 m_Edit_IR.GetWindowText ( m_CodeNext );
			 break;

		case 1: // Ok
			 m_Edit_IR.GetWindowText ( m_CodeOk );
			 break;

		case 2: // Down
			 m_Edit_IR.GetWindowText ( m_CodeDown );
			 break;

		case 3:	// Right
			 m_Edit_IR.GetWindowText ( m_CodeRight );
			 break;
	}

	m_nCurrentSel = m_ComboCodeName.GetCurSel ();
	switch ( m_nCurrentSel )
	{
		case 0:	// Next
			 m_Edit_IR.SetWindowText ( m_CodeNext );
			 break;

		case 1: // Ok
			 m_Edit_IR.SetWindowText ( m_CodeOk );
			 break;

		case 2: // Down
			 m_Edit_IR.SetWindowText ( m_CodeDown );
			 break;

		case 3:	// Right
			 m_Edit_IR.SetWindowText ( m_CodeRight );
			 break;
	}
	
}


void CIRProfilePropPage::OnSendIrCode() 
{
	BOOL	bOk = FALSE;
	CString	OldComPort;
	CString	CurrentCode;
	CString	Msg;
	BYTE	BinCode [ 512 ];
	int		BinCodeLength;
	char	mStr[255];

	UpdateData ( TRUE );

	if ( m_pGenerator )
	{
		OldComPort = m_pGenerator -> m_comPort;
		m_pGenerator -> m_comPort = m_comPort;

		if ( m_pGenerator -> InitRealComPort () )
		{
			CWaitCursor	wait;

			m_Edit_IR.GetWindowText ( CurrentCode );
			m_IRProfile.BuildBinaryIRCode ( BinCode, & BinCodeLength, CurrentCode );

			bOk = m_pGenerator -> SendIRCode (500, BinCode, BinCodeLength, mStr);
		}

		m_pGenerator -> m_comPort = OldComPort;

		if ( bOk )
			Msg.LoadString ( IDS_CODESENT );
		else
			Msg.LoadString ( IDS_CODENOTSENT );
		
		AfxMessageBox ( Msg );
	}
}


void CIRProfilePropPage::OnTestIrValid() 
{
	CString		Code, Msg;

	UpdateData ( TRUE );

	m_Edit_IR.GetWindowText ( Code );

	if ( ! Code.IsEmpty () )
	{
		int		i, j;
		int		k = 0, k2 = 0, k3 = 0;
		char	c;
		WORD	w = 0;
		WORD	wHead = 0;
		WORD	wLen1 = 0;
		WORD	wLen2 = 0;

		for ( i = 0, j = 0; i < Code.GetLength (); i ++ )
		{
			c = Code [i];
			
			if ( c == ' ' || c == '\t' )
			{
				// Separator
			}
			else if ( isxdigit ( c ) )
			{
				// Hexadecimal digit
				w <<= 4;
				
				if ( isdigit (c) )
					w += (WORD) c - (WORD) '0';
				else
					w += (WORD) toupper(c) - (WORD) 'A' + 10;

				j ++;

				if ( j % 4 == 0 )
				{
					if ( j == 4 )
					{
						k = i - 3;
						wHead = w;
					}
					else if ( j == 12 )
					{
						k2 = i - 3;
						wLen1 = w;
					}
					else if ( j == 16 )
					{
						k3 = i - 3;
						wLen2 = w;
					}
					w = 0;
				}
			}
			else
			{
				// Not a recognized digit
				m_Edit_IR.SetSel ( i, i + 1 );
				Msg.LoadString ( IDS_INVALIDCODE1 );
				AfxMessageBox ( Msg );
				m_Edit_IR.SetFocus ();
				return;
			}
		}

		// Check validity
		if ( j % 4 != 0 )
		{
			// Wrong number of digits or bad header
			m_Edit_IR.SetSel ( i, i );
			Msg.LoadString ( IDS_INVALIDCODE2 );
			AfxMessageBox ( Msg );
			m_Edit_IR.SetFocus ();
			return;
		}
		else if ( wHead != 0x0000 && wHead != 0x5000 && wHead != 0x6000 )
		{
			m_Edit_IR.SetSel ( k, k + 4 );
			Msg.LoadString ( IDS_INVALIDCODE3 );
			AfxMessageBox ( Msg );
			m_Edit_IR.SetFocus ();
			return;
		}
		else if ( wHead == 0x5000 || wHead == 0x6000 )
		{
			if ( j != 24 )
			{
				// Wrong number of codes
				m_Edit_IR.SetSel ( i, i );
				Msg.LoadString ( IDS_INVALIDCODE4 );
				AfxMessageBox ( Msg );
				m_Edit_IR.SetFocus ();
				return;
			}
		}
		else
		{
			ASSERT ( wHead == 0x0000 );

			if ( wLen1 > 255 || wLen2 > 255 || ( wLen2 + wLen1 ) > 196 )
			{
				// Too long code
				m_Edit_IR.SetSel ( k2, k3 + 4 );
				Msg.LoadString ( IDS_INVALIDCODE5 );
				AfxMessageBox ( Msg );
				m_Edit_IR.SetFocus ();
				return;
			}
			else
			{
				if ( j != ( ( wLen2 + wLen1 ) * 8 ) + 16 )
				{
					// Not the right number of words
					m_Edit_IR.SetSel ( i, i );
					if ( j < ( ( wLen2 + wLen1 ) * 8 ) + 16 )
					{
						Msg.LoadString ( IDS_INVALIDCODE6 );
						AfxMessageBox ( Msg );
					}
					else
					{
						Msg.LoadString ( IDS_INVALIDCODE7 );
						AfxMessageBox ( Msg );
					}
					m_Edit_IR.SetFocus ();
					return;
				}
			}
		}
	
		Msg.LoadString ( IDS_CODEOK );
		AfxMessageBox ( Msg );
	}
	else
	{
		Msg.LoadString ( IDS_NOCODE );
		AfxMessageBox ( Msg );
	}
}


void CIRProfilePropPage::OnOK() 
{
	// TODO: Add your specialized code here and/or call the base class
	UpdateData(TRUE);

	m_IRProfile.m_NextCode = m_CodeNext;
	m_IRProfile.m_OkCode = m_CodeOk;
	m_IRProfile.m_DownCode = m_CodeDown;
	m_IRProfile.m_RightCode = m_CodeRight;
	m_IRProfile.m_ReadOnly = FALSE;
	m_IRProfile.m_NavInMenuLatency = m_LatencyNav;
	m_IRProfile.m_ValidMenuLatency = m_LatencyNext;
	m_IRProfile.m_NextChapterLatency = m_LatencyValid;

	CString	Msg,Title;
	if (m_Name =="")
		AfxMessageBox(IDS_NOIRPROFILENAME,MB_ICONERROR | MB_OK);
	else
	{
		CString str = GetConfig () -> m_ApplicationPath;
		str += m_IRProfile.GetIhcFileSubDir();
		GetConfig () -> EnsurePathExists ( str );
		str += '\\';
		str += m_Name;
		str +=".ihc";
		m_fileName = str;

		if( ( ! m_bUpdateExisting || m_OriginalName != m_Name ) && (_access( m_fileName, 0 )) != -1 )
		{
			CString str1, str2;
			str2.LoadString (IDS_IRPROFILEEXIST);
			str1.Format (str2,m_Name);
		
			if (AfxMessageBox(str1, MB_YESNO | MB_ICONQUESTION) == IDYES)
			{
				CFile IhcFile ( str, CFile::modeCreate | CFile::modeWrite );
				CArchive ar ( & IhcFile, CArchive::store );
				m_IRProfile.Serialize(ar);
				CDialog::OnOK();
			}
		}	
		else
		{
			CFile IhcFile ( str, CFile::modeCreate | CFile::modeWrite );
			CArchive ar ( & IhcFile, CArchive::store );
			m_IRProfile.Serialize(ar);
			CDialog::OnOK();
		}
	}
}