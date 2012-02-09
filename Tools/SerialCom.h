#if !defined(AFX_SerialCom_H__0DCE7CC1_2426_4BDF_9AFC_410B32D9FE74__INCLUDED_)
#define AFX_SerialCom_H__0DCE7CC1_2426_4BDF_9AFC_410B32D9FE74__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// SerialCom.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CSerialCom window
//////////////////////////////////////////////////////////////////////
// SerialCom.h: implementation of the CSerialCom class.

// Written by Shibu K.V (shibukv@erdcitvm.org)
// Copyright (c) 2002
//
// To use CSerialCom, follow these steps:
//   - Copy the files SerialCom.h & SerialCom.cpp and paste it in your
//		Projects Directory.
//   - Take Project tab from your VC++ IDE,Take Add to Project->Files.Select files 
//		SerialCom.h & SerialCom.cpp and click ok
//	 -	Add the line #include "SerialCom.h" at the top of your Dialog's Header File.
//   - Create an instance of CSerialCom in your dialogs header File.Say
//		CSerialCom port;
 
// Warning: this code hasn't been subject to a heavy testing, so
// use it on your own risk. The author accepts no liability for the 
// possible damage caused by this code.
//
// Version 1.0  7 Sept 2002.

//////////////////////////////////////////////////////////////////////

class CSerialCom : public CWnd
{
// Construction
public:
	CSerialCom();

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSerialCom)
	//}}AFX_VIRTUAL

// Implementation
public:
	void ClosePort();
	BOOL ReadByte(BYTE &resp);
	BOOL WriteByte(BYTE bybyte);
	BOOL OpenPort(CString portname);
	BOOL SetCommunicationTimeouts(DWORD ReadIntervalTimeout,DWORD ReadTotalTimeoutMultiplier,DWORD ReadTotalTimeoutConstant,DWORD WriteTotalTimeoutMultiplier,DWORD WriteTotalTimeoutConstant);
	BOOL ConfigurePort(DWORD BaudRate,BYTE ByteSize,DWORD fParity,BYTE  Parity,BYTE StopBits);
	HANDLE hComm;
	DCB      m_dcb;
	COMMTIMEOUTS m_CommTimeouts;
	BOOL     m_bPortReady;
	BOOL     bWriteRC;
	BOOL     bReadRC;
	DWORD iBytesWritten;
	DWORD iBytesRead;
	DWORD dwBytesRead;
	virtual ~CSerialCom();

	// Generated message map functions
protected:
	//{{AFX_MSG(CSerialCom)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SerialCom_H__0DCE7CC1_2426_4BDF_9AFC_410B32D9FE74__INCLUDED_)
