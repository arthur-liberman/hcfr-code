/* 
 *  Filename:    EditEx.h
 *  Description: CEditEx class definition
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


#pragma once
#include <stack>

// CEditEx

class CEditEx : public CEdit
{
	// commands

	// abstract base class
	class CEditExCommand
	{
	public:
		CEditExCommand(CEditEx* const pEditControl) : m_pEditControl(pEditControl) {}
		virtual ~CEditExCommand() {}

		void Undo() { DoUndo(); }
		void Execute() { DoExecute(); }

	protected:
		virtual void DoUndo() = 0;
		virtual void DoExecute() = 0;

		CEditEx* const m_pEditControl;
	};

	class CInsertTextCommand : public CEditExCommand
	{
	public:
		CInsertTextCommand(CEditEx* const pEditControl, const CString& text);
	protected:
		virtual void DoUndo();
		virtual void DoExecute();
	private:
		CString m_text;
		int m_nStart;
	};

	class CDeleteSelectionCommand : public CEditExCommand
	{
	public:
		enum CursorOnUndo {
			Start,
			End,
			Selection
		};

		CDeleteSelectionCommand(CEditEx* const pEditControl, CursorOnUndo cursorOnUndo);
	protected:
		virtual void DoUndo();
		virtual void DoExecute();
	private:
		int m_nStart;
		int m_nEnd;
		CString m_textDeleted;
		CursorOnUndo m_cursorOnUndo;
	};

	class CDeleteCharacterCommand : public CEditExCommand
	{
	public:
		CDeleteCharacterCommand(CEditEx* pEditControl, bool isBackspace);
	protected:
		virtual void DoUndo();
		virtual void DoExecute();
	private:
		TCHAR m_charDeleted;
		int m_nStart;
		bool m_isBackspace;
	};

	class CReplaceSelectionCommand : public CEditExCommand
	{
	public:
		CReplaceSelectionCommand(CEditEx* const pEditControl, const CString& text);
		virtual ~CReplaceSelectionCommand();
	protected:
		virtual void DoUndo();
		virtual void DoExecute();
	private:
		CDeleteSelectionCommand* m_pDeleteCommand;
		CInsertTextCommand* m_pInsertCommand;
	};

	class CSetTextCommand : public CEditExCommand
	{
	public:
		CSetTextCommand(CEditEx* const pEditControl);
		virtual ~CSetTextCommand();
	protected:
		virtual void DoUndo();
		virtual void DoExecute();
	private:
		CString m_textReplaced;
	};

	// command history

	class CCommandHistory
	{
	public:
		CCommandHistory();
		~CCommandHistory();

		void AddCommand(CEditExCommand* pCommand);
		bool Undo();
		bool Redo();

		bool CanUndo() const;
		bool CanRedo() const;

	private:
		typedef std::stack<CEditExCommand*> CommandStack;

		CommandStack m_undoCommands;
		CommandStack m_redoCommands;

		void DestroyCommands(CommandStack& stack);
		void DestroyUndoCommands();
		void DestroyRedoCommands();
	};

	// CEditEx definition

	DECLARE_DYNAMIC(CEditEx)

public:
	CEditEx();
	virtual ~CEditEx();

public:
	bool IsMultiLine() const;
	bool IsSelectionEmpty() const;
	int GetCursorPosition() const;
	CString GetSelectionText() const;

	void ReplaceSel(LPCTSTR lpszNewText, BOOL bCanUndo = FALSE);

	BOOL Undo();
	BOOL Redo();
	BOOL CanUndo() const;
	BOOL CanRedo() const;

	// command pattern receiver methods
	void InsertText(const CString& textToInsert, int nStart);
	void InsertChar(TCHAR charToInsert, int nStart);
	void DeleteText(int nStart, int nLen);

public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);

protected:
	DECLARE_MESSAGE_MAP()

public:
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnEnterIdle(UINT nWhy, CWnd* pWho);

private:
	void CreateInsertTextCommand(const CString& newText);
	BOOL PreTranslateKeyDownMessage(WPARAM wParam);

	BOOL CreatePasteCommand();
	BOOL DoDelete();
	BOOL DoBackspace();
	void DoUndo();

	enum CharCategory
	{
		enumSpace,
		enumPunctuation,
		enumAlphaNum
	};

	void DeleteFromTheBeginning();
	void DeleteToTheEnd();
	void DeleteSelectedChar();
	void BackSelectedChar();

	CharCategory GetCharCategory(TCHAR ch);
	void ExtendSelectionToStartOfCharacterBlock(int nStart, int nEnd);
	void ExtendSelectionToEndOfCharacterBlock(int nStart, int nEnd);
	void DeleteFromTheBeginningOfWord();
	void DeleteToTheBeginningOfNextWord();

	void UpdateContextMenuItems(CWnd* pWnd);
	UINT FindMenuPos(CMenu* pMenu, UINT myID);
	void AppendKeyboardShortcuts(CMenu* pMenu, UINT id, LPCTSTR shortcut);

	CCommandHistory m_commandHistory;
	bool m_contextMenuShownFirstTime;
};
