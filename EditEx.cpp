/* 
 *  Filename:    EditEx.h
 *  Description: CEditEx class implementation
 *  Copyright:   Julijan Šribar, 2011-2014
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the author(s) be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include "stdafx.h"
#include "EditEx.h"

//////////////////////////////////////////////////////////////////////////
// CEditEx::CInsertTextCommand

CEditEx::CInsertTextCommand::CInsertTextCommand(CEditEx* const pEditControl, const CString& text) 
	: CEditExCommand(pEditControl)
	, m_text(text)
{
	int nEnd;
	pEditControl->GetSel(m_nStart, nEnd);
}

void CEditEx::CInsertTextCommand::DoUndo()
{
	m_pEditControl->DeleteText(m_nStart, m_text.GetLength());
	m_pEditControl->SetSel(m_nStart, m_nStart);
}

void CEditEx::CInsertTextCommand::DoExecute()
{
	m_pEditControl->InsertText(m_text, m_nStart);
	int nEnd = m_nStart + m_text.GetLength();
	m_pEditControl->SetSel(nEnd, nEnd);
}

//////////////////////////////////////////////////////////////////////////
// CEditEx::CDeleteSelectionCommand

CEditEx::CDeleteSelectionCommand::CDeleteSelectionCommand(CEditEx* const pEditControl, CursorOnUndo cursorOnUndo) 
	: CEditExCommand(pEditControl)
	, m_textDeleted(pEditControl->GetSelectionText())
{
	m_pEditControl->GetSel(m_nStart, m_nEnd);
	m_cursorOnUndo = cursorOnUndo;
}

void CEditEx::CDeleteSelectionCommand::DoUndo()
{
	m_pEditControl->InsertText(m_textDeleted, m_nStart);
	switch (m_cursorOnUndo)
	{
	case Selection:
		m_pEditControl->SetSel(m_nStart, m_nEnd);
		break;
	case End:
		m_pEditControl->SetSel(m_nEnd, m_nEnd);
		break;
	case Start:
		m_pEditControl->SetSel(m_nStart, m_nStart);
		break;
	default:
		ASSERT(FALSE);
		break;
	}
}

void CEditEx::CDeleteSelectionCommand::DoExecute()
{
	m_pEditControl->DeleteText(m_nStart, m_nEnd - m_nStart);
	m_pEditControl->SetSel(m_nStart, m_nStart);
}

//////////////////////////////////////////////////////////////////////////
// CEditEx::CDeleteCharacterCommand

CEditEx::CDeleteCharacterCommand::CDeleteCharacterCommand(CEditEx* pEditControl, bool isBackspace) 
	: CEditExCommand(pEditControl)
	, m_isBackspace(isBackspace)
{
	ASSERT(m_pEditControl->IsSelectionEmpty());
	m_pEditControl->GetSel(m_nStart, m_nStart);
	CString text;
	m_pEditControl->GetWindowText(text);
	if (m_isBackspace)
		--m_nStart;
	m_charDeleted = text[m_nStart];
}

void CEditEx::CDeleteCharacterCommand::DoUndo()
{
	m_pEditControl->InsertChar(m_charDeleted, m_nStart);
	if (m_isBackspace)
		m_pEditControl->SetSel(m_nStart + 1, m_nStart + 1);
	else
		m_pEditControl->SetSel(m_nStart, m_nStart);
}

void CEditEx::CDeleteCharacterCommand::DoExecute()
{
	m_pEditControl->DeleteText(m_nStart, 1);
	m_pEditControl->SetSel(m_nStart, m_nStart);
}

//////////////////////////////////////////////////////////////////////////
// CEditEx::CReplaceSelectionCommand

CEditEx::CReplaceSelectionCommand::CReplaceSelectionCommand(CEditEx* const pEditControl, const CString& text) 
	: CEditExCommand(pEditControl)
{
	ASSERT(pEditControl->IsSelectionEmpty() == false);
	m_pDeleteCommand = new CDeleteSelectionCommand(pEditControl, CDeleteSelectionCommand::Selection);
	m_pInsertCommand = new CInsertTextCommand(pEditControl, text);
	ASSERT(m_pDeleteCommand != NULL);
	ASSERT(m_pInsertCommand != NULL);
}

CEditEx::CReplaceSelectionCommand::~CReplaceSelectionCommand()
{
	delete m_pDeleteCommand;
	delete m_pInsertCommand;
}

void CEditEx::CReplaceSelectionCommand::DoUndo()
{
	m_pInsertCommand->Undo();
	m_pDeleteCommand->Undo();
}

void CEditEx::CReplaceSelectionCommand::DoExecute()
{
	m_pDeleteCommand->Execute();
	m_pInsertCommand->Execute();
}

//////////////////////////////////////////////////////////////////////////
// CEditEx::CSetTextCommand

CEditEx::CSetTextCommand::CSetTextCommand(CEditEx* const pEditControl) 
	: CEditExCommand(pEditControl)
{
	pEditControl->GetWindowText(m_textReplaced);
}

CEditEx::CSetTextCommand::~CSetTextCommand()
{
}

void CEditEx::CSetTextCommand::DoUndo()
{
	DoExecute();
}

void CEditEx::CSetTextCommand::DoExecute()
{
	CString currentText;
	m_pEditControl->GetWindowText(currentText);
	m_pEditControl->SendMessage(WM_SETTEXT, (WPARAM)m_pEditControl->m_hWnd, (LPARAM)(m_textReplaced.GetBuffer()));
	m_pEditControl->SetSel(0, -1);
	m_textReplaced = currentText;
}

//////////////////////////////////////////////////////////////////////////
// CEditEx::CCommandHistory

CEditEx::CCommandHistory::CCommandHistory()
{
}

CEditEx::CCommandHistory::~CCommandHistory()
{
	DestroyUndoCommands();
	DestroyRedoCommands();
}

void CEditEx::CCommandHistory::AddCommand(CEditExCommand* pCommand)
{
	m_undoCommands.push(pCommand);
	DestroyRedoCommands();
}

bool CEditEx::CCommandHistory::Undo()
{
	if (!CanUndo())
		return false;
	CEditExCommand* pCommand = m_undoCommands.top();
	pCommand->Undo();
	m_undoCommands.pop();
	m_redoCommands.push(pCommand);
	return true;
}

bool CEditEx::CCommandHistory::Redo()
{
	if (!CanRedo())
		return false;
	CEditExCommand* pCommand = m_redoCommands.top();
	pCommand->Execute();
	m_redoCommands.pop();
	m_undoCommands.push(pCommand);
	return true;
}

bool CEditEx::CCommandHistory::CanUndo() const
{
	return m_undoCommands.empty() == false;
}

bool CEditEx::CCommandHistory::CanRedo() const
{
	return m_redoCommands.empty() == false;
}

void CEditEx::CCommandHistory::DestroyCommands(CommandStack& stack)
{
	while (!stack.empty())
	{
		CEditExCommand* pCommand = stack.top();
		delete pCommand;
		stack.pop();
	}
}

void CEditEx::CCommandHistory::DestroyUndoCommands()
{
	DestroyCommands(m_undoCommands);
}

void CEditEx::CCommandHistory::DestroyRedoCommands()
{
	DestroyCommands(m_redoCommands);
}


//////////////////////////////////////////////////////////////////////////
// CEditEx implementation

IMPLEMENT_DYNAMIC(CEditEx, CEdit)

CEditEx::CEditEx()
{
	m_contextMenuShownFirstTime = false;
}

CEditEx::~CEditEx()
{
}

// public methods

inline bool CEditEx::IsMultiLine() const
{
	return (GetStyle() & ES_MULTILINE) == ES_MULTILINE;
}

inline bool CEditEx::IsSelectionEmpty() const
{
	int nStart, nEnd;
	GetSel(nStart, nEnd);
	return nEnd == nStart;
}

int CEditEx::GetCursorPosition() const
{
	ASSERT(IsSelectionEmpty());
	return GetSel() & 0xFFFF;
}

CString CEditEx::GetSelectionText() const 
{
	CString text;
	GetWindowText(text);
	int nStart, nEnd;
	GetSel(nStart, nEnd);
	return text.Mid(nStart, nEnd - nStart);
}

void CEditEx::ReplaceSel(LPCTSTR lpszNewText, BOOL bCanUndo /*= FALSE*/)
{
	if (bCanUndo)
		CreateInsertTextCommand(lpszNewText);
	CEdit::ReplaceSel(lpszNewText, bCanUndo);
}

BOOL CEditEx::Undo()
{
	return m_commandHistory.Undo();
}

BOOL CEditEx::Redo()
{
	return m_commandHistory.Redo();
}

BOOL CEditEx::CanUndo() const
{
	return m_commandHistory.CanUndo();
}

BOOL CEditEx::CanRedo() const
{
	return m_commandHistory.CanRedo();
}

// command pattern receiver methods

void CEditEx::InsertText(const CString& textToInsert, int nStart)
{
	ASSERT(nStart <= GetWindowTextLength());
	CString text;
	GetWindowText(text);
	text.Insert(nStart, textToInsert);
	SendMessage(WM_SETTEXT, (WPARAM)m_hWnd, (LPARAM)text.GetString());
}

void CEditEx::InsertChar(TCHAR charToInsert, int nStart)
{
	ASSERT(nStart <= GetWindowTextLength());
	CString text;
	GetWindowText(text);
	text.Insert(nStart, charToInsert);
	SendMessage(WM_SETTEXT, (WPARAM)m_hWnd, (LPARAM)text.GetString());
}

void CEditEx::DeleteText(int nStart, int nLen)
{
	ASSERT(nLen > 0);
	ASSERT(GetWindowTextLength() > 0);
	CString text;
	GetWindowText(text);
	text.Delete(nStart, nLen);
	SendMessage(WM_SETTEXT, (WPARAM)m_hWnd, (LPARAM)text.GetString());
}

// CEditEx overrides

// intercept commands to add them to the history
BOOL CEditEx::PreTranslateMessage(MSG* pMsg)
{
	switch (pMsg->message)
	{
		// Omardris
		int i;
	case WM_CHAR:
		i = pMsg->wParam;
		wchar_t wChar;
		if (i > 31 || i == 13) {
			wChar = pMsg->wParam;
		}
		//int nCount = wParam & 0xFF;
		if (iswprint(wChar))
		{			
			CString newText(wChar, 1);
			//CreateInsertTextCommand(newText);
			if (IsSelectionEmpty()) {
				CEditExCommand* pCommand = new CInsertTextCommand(this, newText);
				pCommand->Execute();
			}
			else {
				CEditExCommand* pCommand = new CReplaceSelectionCommand(this, newText);
				pCommand->Execute();
			}
			Invalidate();
			//UpdateWindow();
			return true;
		}
		// Omardris

		break;

	case WM_KEYDOWN: // Non-System_key (without ALT)
		if (PreTranslateKeyDownMessage(pMsg->wParam) == TRUE) {
			//Invalidate();
			return TRUE;
		}
		break;
		/*
	case WM_SYSCHAR: // bedeutet, dass auch Alt-Taste mit gedrückt wurde
		// Alt + Back
		if (pMsg->wParam == VK_BACK)
		{
			// for single-line Alt + Back generates WM_UNDO message but not for multiline!
			// Therefore we need to capture this keyboard shortcut for multiline control
			if (IsMultiLine())
				DoUndo();
			return TRUE;
		}
		break;
		*/
	}
	return CEdit::PreTranslateMessage(pMsg);
}

// intercept commands to add them to the history
LRESULT CEditEx::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CHAR:
		{
			wchar_t wChar = static_cast<wchar_t>(wParam);
			int nCount = lParam & 0xFF;
			if (iswprint(wChar))
			{
				CString newText(wChar, nCount);
				CreateInsertTextCommand(newText);
			}
			// special case for Unicode control characters inserted from the context menu
			else if (nCount == 0)
			{
				CString newText(wChar);
				CreateInsertTextCommand(newText);
			}
		}
		break;
	case WM_PASTE:
		CreatePasteCommand();
		break;
	case WM_CUT:
	case WM_CLEAR: // delete command from context menu
		if (!IsSelectionEmpty())
			m_commandHistory.AddCommand(new CDeleteSelectionCommand(this, CDeleteSelectionCommand::Selection));
		break;
	case WM_UNDO:
		DoUndo();
		return TRUE; // we did the undo and shouldn't pass the message to base class
	case ID_EDIT_REDO:
		Redo();
		return TRUE;
	case WM_SETTEXT:
		// add event to history only if control is already visible (to prevent 
		// initial call of DDX_Text to be added to command history) and 
		// if message has been sent from outside the control (i.e. wParam is NULL)
		if (IsWindowVisible() && (HWND)wParam != m_hWnd)
			m_commandHistory.AddCommand(new CSetTextCommand(this));
		break;
	}
	return CEdit::WindowProc(message, wParam, lParam);
}


BEGIN_MESSAGE_MAP(CEditEx, CEdit)
	ON_WM_CONTEXTMENU()
	ON_WM_ENTERIDLE()
END_MESSAGE_MAP()

// CEditEx message handlers

// sets the flag used to update context menu state
void CEditEx::OnContextMenu(CWnd* pWnd, CPoint point)
{
	m_contextMenuShownFirstTime = true;
	CEdit::OnContextMenu(pWnd, point);
}

void CEditEx::OnEnterIdle(UINT nWhy, CWnd* pWho)
{
	CEdit::OnEnterIdle(nWhy, pWho);
	if (nWhy == MSGF_MENU)
	{
		// update context menu only first time it is displayed
		if (m_contextMenuShownFirstTime)
		{
			m_contextMenuShownFirstTime = false;
			UpdateContextMenuItems(pWho);
		}
	}
}

// CEditEx private methods

BOOL CEditEx::PreTranslateKeyDownMessage(WPARAM wParam) // eigentliche Tastenabfrage
{
	switch (wParam) 
	{
	case _T('A'):
		// Ctrl + 'A'
		if ((GetKeyState(VK_CONTROL) & 0x8000) != 0)
		{
			SetSel(0, -1);
			return TRUE;
		}
		break;
	case _T('Z'):
		// Ctrl + 'Z'
		if ((GetKeyState(VK_CONTROL) & 0x8000) != 0)
		{
			Undo();
			return TRUE;
		}
		break;
	case _T('Y'):
		// Ctrl + 'Y'
		if ((GetKeyState(VK_CONTROL) & 0x8000) != 0)
		{
			Redo();
			return TRUE;
		}
		break;
	case VK_BACK:
		return DoBackspace();
	case VK_DELETE:
		return DoDelete();
	}

	return FALSE;
}

void CEditEx::CreateInsertTextCommand(const CString& newText)
{
	if (IsSelectionEmpty())
		m_commandHistory.AddCommand(new CInsertTextCommand(this, newText));
	else
		m_commandHistory.AddCommand(new CReplaceSelectionCommand(this, newText));
}

BOOL CEditEx::CreatePasteCommand()
{
	if (!OpenClipboard())
		return FALSE;

#ifdef UNICODE
	HANDLE hglb = ::GetClipboardData(CF_UNICODETEXT);
#else
	HANDLE hglb = ::GetClipboardData(CF_TEXT);
#endif
	if (hglb == NULL)
	{
		CloseClipboard(); 
		return FALSE;
	}
	LPTSTR pText = reinterpret_cast<LPTSTR>(::GlobalLock(hglb));
	if (pText != NULL)
	{
		CreateInsertTextCommand(pText);
		::GlobalUnlock(hglb);
	}
	CloseClipboard(); 
	return TRUE;
}

BOOL CEditEx::DoDelete()
{
	if ((GetKeyState(VK_CONTROL) & 0x8000) != 0)
	{
		// Ctrl + Delete + Shift
		if ((GetKeyState(VK_SHIFT) & 0x8000) != 0)
			DeleteToTheEnd();
		// Ctrl + Delete
		else
			DeleteToTheBeginningOfNextWord();
		return TRUE;
	}
	// simple delete
	DeleteSelectedChar();
	return TRUE;
	// macht eigentlich keinen Sinn mehr ??
	if (IsSelectionEmpty() == false)
		m_commandHistory.AddCommand(new CDeleteSelectionCommand(this, CDeleteSelectionCommand::Selection));
	else
	{
		if (GetCursorPosition() < GetWindowTextLength())
			m_commandHistory.AddCommand(new CDeleteCharacterCommand(this, false));
	}
	return FALSE; // Delete-Taste wird weitergereicht
	
}

BOOL CEditEx::DoBackspace()
{
	if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) 
	{
		if ((GetKeyState(VK_SHIFT) & 0x8000) != 0)
			// Ctrl + Shift + Back
			DeleteFromTheBeginning();
		else
			// Ctrl + Back
			DeleteFromTheBeginningOfWord();
		return TRUE;
	}
	// plain Back
	BackSelectedChar();
	return TRUE;
	// macht eigentlich keinen Sinn mehr ??
	if (IsSelectionEmpty() == false)
		m_commandHistory.AddCommand(new CDeleteSelectionCommand(this, CDeleteSelectionCommand::Selection));
	else
	{
		if (GetCursorPosition() > 0)
			m_commandHistory.AddCommand(new CDeleteCharacterCommand(this, true));
	}
	return FALSE; // BackSpace taste wird weitergereicht
}

void CEditEx::DoUndo()
{
	// Alt + Shift + Back also generates WM_UNDO so we must distinguish it to execute redo operation
	if ((GetKeyState(VK_BACK) & 0x8000) & (GetKeyState(VK_SHIFT) & 0x8000))
	{
		ASSERT(GetKeyState(VK_MENU) & 0x8000);
		Redo();
	}
	else
		Undo();
}

// Deletes characters from the start of the content up
// to the end of selection
void CEditEx::DeleteFromTheBeginning()
{
	int nStart, nEnd;
	GetSel(nStart, nEnd);
	if (nEnd == 0)
		return;
	SetSel(0, nEnd);
	CEditExCommand* pCommand = new CDeleteSelectionCommand(this, CDeleteSelectionCommand::End);
	m_commandHistory.AddCommand(pCommand);
	pCommand->Execute();
}

// Deletes characters from the start of the selection 
// to the end of content
void CEditEx::DeleteToTheEnd()
{
	int nStart, nEnd;
	GetSel(nStart, nEnd);
	nEnd = GetWindowTextLength();
	if (nStart == nEnd)
		return;
	SetSel(nStart, nEnd);
	CEditExCommand* pCommand = new CDeleteSelectionCommand(this, CDeleteSelectionCommand::Start);
	m_commandHistory.AddCommand(pCommand);
	pCommand->Execute();
}

// Deletes selected character
void CEditEx::DeleteSelectedChar()
{
	int nStart, nEnd;
	CString cTemp;
	GetSel(nStart, nEnd);
	if (nStart == GetWindowTextLength()) // Cursor ganz am Ende
		return;
	if (nStart == nEnd) // Keine mehrfach Selektion - nur 1 Char löschen
	SetSel(nStart, nStart+1);
	
	CEditExCommand* pCommand = new CDeleteSelectionCommand(this, CDeleteSelectionCommand::Start);
	m_commandHistory.AddCommand(pCommand);
	pCommand->Execute();
}

// BachSpace selected character
void CEditEx::BackSelectedChar()
{
	int nStart, nEnd;
	GetSel(nStart, nEnd);
	if (nStart == 0) // Cursor ganz am Anfag
		return;
	if (nStart == nEnd) // Keine mehrfach Selektion - nur 1 Char löschen
		SetSel(nStart-1, nStart);
	CEditExCommand* pCommand = new CDeleteSelectionCommand(this, CDeleteSelectionCommand::Start);
	m_commandHistory.AddCommand(pCommand);
	pCommand->Execute();
}


// Gets character category
CEditEx::CharCategory CEditEx::GetCharCategory(TCHAR ch)
{
	if (::_istspace(ch))
		return enumSpace;
	if (::_istpunct(ch))
		return enumPunctuation;
	return enumAlphaNum;
}

// Extends start of selection to include characters of the same category
void CEditEx::ExtendSelectionToStartOfCharacterBlock(int nStart, int nEnd) 
{
	ASSERT(nStart > 0);
	ASSERT(nEnd >= nEnd);
	CString text;
	GetWindowText(text);
	// skip trailing whitespaces
	while (--nStart > 0)
	{
		if (!_istspace(text[nStart]))
			break;
	}
	// find start of block with the same character category
	CharCategory blockCharCategory = GetCharCategory(text[nStart]);
	while (nStart > 0)
	{
		--nStart;
		if (blockCharCategory != GetCharCategory(text[nStart]))
		{
			++nStart;
			break;
		}
	}
	ASSERT(nStart >= 0 && nEnd >= nEnd);
	SetSel(nStart, nEnd, TRUE);
}

// Extends end of selection to include characters of the same category
void CEditEx::ExtendSelectionToEndOfCharacterBlock(int nStart, int nEnd) 
{
	CString text;
	GetWindowText(text);
	CharCategory blockCharCategory = GetCharCategory(text[nEnd]);
	int len = text.GetLength();
	while (++nEnd < len) 
	{
		if (blockCharCategory != GetCharCategory(text[nEnd]))
			break;
	}
	// skip trailing whitespaces
	while (nEnd < len) 
	{
		if (!_istspace(text[nEnd]))
			break;
		++nEnd;
	}
	ASSERT(nEnd >= nStart && nEnd <= len);
	SetSel(nStart, nEnd, TRUE);
}

// Deletes characters from the beginning of the block
// consisting of characters of the same category as
// the first character in the selection
void CEditEx::DeleteFromTheBeginningOfWord()
{
	int nStart, nEnd;
	GetSel(nStart, nEnd);
	ASSERT(nStart <= nEnd);
	if (nStart == 0)
		return;
	ExtendSelectionToStartOfCharacterBlock(nStart, nEnd);
	CEditExCommand* pCommand = new CDeleteSelectionCommand(this, CDeleteSelectionCommand::End);
	m_commandHistory.AddCommand(pCommand);
	pCommand->Execute();
}

// Deletes characters to the end of the block
// consisting of characters of the same category as
// the last character in the selection
void CEditEx::DeleteToTheBeginningOfNextWord()
{
	int nStart, nEnd;
	GetSel(nStart, nEnd);
	ASSERT(nStart <= nEnd);
	if (GetWindowTextLength() <= nEnd)
		return;
	ExtendSelectionToEndOfCharacterBlock(nStart, nEnd);
	CEditExCommand* pCommand = new CDeleteSelectionCommand(this, CDeleteSelectionCommand::Start);
	m_commandHistory.AddCommand(pCommand);
	pCommand->Execute();
}

// updates Undo menu item state, adds Redo command and
// displays keyboard shortcuts
void CEditEx::UpdateContextMenuItems(CWnd* pWnd)
{
	MENUBARINFO mbi = {0};
	mbi.cbSize = sizeof(MENUBARINFO);
	::GetMenuBarInfo(pWnd->m_hWnd, OBJID_CLIENT, 0, &mbi);
	ASSERT(::IsMenu(mbi.hMenu));
	HMENU hMenu = mbi.hMenu;
	CMenu* pMenu = CMenu::FromHandle(hMenu);
	if (m_commandHistory.CanUndo())
		pMenu->EnableMenuItem(WM_UNDO, MF_BYCOMMAND | MF_ENABLED);
	else
		pMenu->EnableMenuItem(WM_UNDO, MF_BYCOMMAND | MF_GRAYED);

	int pos = FindMenuPos(pMenu, WM_UNDO);
	if (pos == -1)
		return;

	static TCHAR* strRedo = _T("&Redo");
	MENUITEMINFO mii;
	mii.cbSize = sizeof(MENUITEMINFO);
	mii.fMask = MIIM_ID | MIIM_STATE | MIIM_STRING;
	mii.fType = MFT_STRING;
	mii.fState = m_commandHistory.CanRedo() ? MF_ENABLED : MF_DISABLED;
	mii.wID = ID_EDIT_REDO;
	mii.dwTypeData = strRedo;
	mii.cch = _tclen(strRedo);
	VERIFY(pMenu->InsertMenuItem(pos + 1, &mii, TRUE));

	AppendKeyboardShortcuts(pMenu, WM_UNDO, _T("Ctrl+Z"));
	AppendKeyboardShortcuts(pMenu, ID_EDIT_REDO, _T("Ctrl+Y"));
	AppendKeyboardShortcuts(pMenu, WM_CUT, _T("Ctrl+X"));
	AppendKeyboardShortcuts(pMenu, WM_COPY, _T("Ctrl+C"));
	AppendKeyboardShortcuts(pMenu, WM_PASTE, _T("Ctrl+V"));
	AppendKeyboardShortcuts(pMenu, WM_CLEAR, _T("Del"));
	AppendKeyboardShortcuts(pMenu, EM_SETSEL, _T("Ctrl+A"));
}

UINT CEditEx::FindMenuPos(CMenu* pMenu, UINT myID)
{
	for (UINT pos = pMenu->GetMenuItemCount() - 1; pos >= 0; --pos)
	{
		MENUITEMINFO mii;
		mii.cbSize = sizeof(MENUITEMINFO);
		mii.fMask = MIIM_ID;
		if (pMenu->GetMenuItemInfo(pos, &mii, TRUE) == FALSE)
			return -1;

		if (mii.wID == myID)
			return pos;
	}
	return -1;
}

void CEditEx::AppendKeyboardShortcuts(CMenu* pMenu, UINT id, LPCTSTR shortcut)
{
	CString caption;
	if (pMenu->GetMenuString(id, caption, MF_BYCOMMAND) == 0)
		return;

	caption.AppendFormat(_T("\t%s"), shortcut);
	MENUITEMINFO mii;
	mii.cbSize = sizeof(MENUITEMINFO);
	mii.fMask = MIIM_STRING;
	mii.fType = MFT_STRING;
	mii.dwTypeData = caption.GetBuffer();
	mii.cch = caption.GetLength();
	pMenu->SetMenuItemInfo(id, &mii);
}
