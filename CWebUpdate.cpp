// CWebUpdate.cpp : implementation file
//

#include "stdafx.h"
#include "CWebUpdate.h"

#include "sha1.h" 

#include <process.h>
#include "atlbase.h"

#include <sys/stat.h>

#pragma comment( lib, "wininet" )

// Global variables
CString remoteFile;
CString localFile;
HRESULT dloadResult;

// Thread function, downloads a file
//
void downloadFile(void* nullData)
{
	dloadResult = URLDownloadToFile(NULL, remoteFile, localFile, NULL, NULL);   

	return;
}

/////////////////////////////////////////////////////////////////////////////
// CWebUpdate class



CWebUpdate::CWebUpdate()
{
	// Nothing to see here
}



// Internal function, do the SHA-1 hash
//
CString CWebUpdate::DoSHA1Hash(LPCSTR filePath)
{
	sha1_ctx m_sha1;

	CString tempHash;
	FILE *fileToHash = NULL;

	unsigned char fileBuf[16000];
	unsigned long lenRead = 0;

	// The outputted hash
	CString outHash;

	// Temporary working buffers
	unsigned char* tempOut = new unsigned char[256];

	sha1_begin(&m_sha1);

	fileToHash = fopen(filePath, "rb");
	
	do
	{
		lenRead = fread(fileBuf, 1, 16000, fileToHash);
		if(lenRead != 0)
		{
			sha1_hash(fileBuf, lenRead, &m_sha1);
		}
	} while (lenRead == 16000);

	fclose(fileToHash); fileToHash = NULL;

	sha1_end(tempOut, &m_sha1);

	for (int i = 0 ; i < 20 ; i++)
	{
		char tmp[3];
		_itoa(tempOut[i], tmp, 16);
		if (strlen(tmp) == 1)
		{
			tmp[1] = tmp[0];
			tmp[0] = '0';
			tmp[2] = '\0';
		}
		tempHash += tmp;
	}

	delete[] tempOut;

	outHash = tempHash;

	return outHash;
}



// Set the local directory to download to
//
void CWebUpdate::SetLocalDirectory(LPCSTR pathTo, bool generate)
{
	if (generate)
	{
		char blankStr[MAX_PATH] = "";
		CString path;

		GetModuleFileName(0, blankStr, sizeof(blankStr) - 1);

		path = blankStr;
		path = path.Left(path.ReverseFind('\\'));
		localDir = path;
	}

	else
	{
		localDir = pathTo;
	}
}



// Perform the actual update
//
bool CWebUpdate::DoUpdateCheck()
{
	// Reset previous attributes
	numMissing = 0;
	numDifferent = 0;
	numSuccess = 0;

	missingFiles.RemoveAll();
	differentFiles.RemoveAll();
	successfulFiles.RemoveAll();

	// First of all, try and retrieve the update file
	remoteFile = updateURL;
	CString path;
	path = getenv("APPDATA");

	localFile = path + "\\color\\CheckUpdate.txt";
	// Download
	HANDLE	dloadHandle = (HANDLE)_beginthread(downloadFile, 0, (void*)"");
	
	// Wait for it to finish
	//	AtlWaitWithMessageLoop(dloadHandle); //replace with routine below to prevent hang if

	DWORD dwInitialTimeOutMilliseconds = 0;
	DWORD dwIterateTimeOutMilliseconds = 500;
	DWORD dwRet=0;

	MSG msg={0};

	dwRet = ::WaitForSingleObject(dloadHandle, dwInitialTimeOutMilliseconds);

	if(dwRet == WAIT_OBJECT_0)
	{
		// The object is already signalled.
	}
	else
	{
		while(true)
		{
		// There are one or more window message available. Dispatch them.
			while(::PeekMessage(&msg,NULL,NULL,NULL,PM_REMOVE))
			{
				::TranslateMessage(&msg);
				::DispatchMessage(&msg);
				dwRet = ::WaitForSingleObject(dloadHandle, 0);
				if(dwRet == WAIT_OBJECT_0)
				{
					// The object is already signalled.

					break;
				}
			}
			// Now we’ve dispatched all the messages in the queue.
			// Use MsgWaitForMultipleObjects to either tell us there are
			// more messages to dispatch, or that our object has been signalled.
			dwRet = ::MsgWaitForMultipleObjects(1, &dloadHandle, FALSE,
			dwIterateTimeOutMilliseconds, QS_ALLINPUT);
			if(dwRet == WAIT_OBJECT_0)
			{
			// The event was signaled.
				break;
			}
		}
	}


	// The download failed, return false
	if (!SUCCEEDED(dloadResult))
		return false;

	// It downloaded, now we parse it
	// Read it line by line and work out what's what
	CStdioFile loadFile(localFile, CFile::modeRead | CFile::typeText);
	CString curLine;
	
	while(loadFile.ReadString(curLine))
	{
		CString fileTo;
		AfxExtractSubString(fileTo, curLine, 0, ' ');

		CString fileHash;
		AfxExtractSubString(fileHash, curLine, 1, ' ');

		// So the path = ..
		CString pathTo = localDir + "\\" + fileTo;

		struct stat checkFile;
		if(stat(pathTo, &checkFile) == 0)
		{
			// It exists, but is it the same file?
			CString verifyFile = DoSHA1Hash(pathTo);

			// Now compare the hashes
			if (verifyFile == fileHash)
			{
				// The files are the same, no worries
				numSuccess++;
				successfulFiles.Add(fileTo);
			}

			else
			{
				// The files are different
				numDifferent++;
				differentFiles.Add(fileTo);
			}
		}

		else
		{
			// The files doesn't exist at all
			numMissing++;
			missingFiles.Add(fileTo);
		}
	}

	loadFile.Close();
	DeleteFile(localFile);
	return true;
}



// Download a missing file
//
bool CWebUpdate::DownloadMissing(int i)
{
	// Attempt to download the file
//	remoteFile = remoteURL + "/" + missingFiles.GetAt(i);
	remoteFile = remoteURL + "/HCFRSetup.exe";
//	localFile = localDir + "\\" + missingFiles.GetAt(i);
	localFile = localDir + "\\HCFRSetup.exe";

	HANDLE dloadHandle = (HANDLE)_beginthread(downloadFile, 0, (void*)"");

	AtlWaitWithMessageLoop(dloadHandle);

	if (!SUCCEEDED(dloadResult))
		return false;

	else
		return true;
}



// Download an updated file
//
bool CWebUpdate::DownloadDifferent(int i)
{
	// Attempt to download the file
	//	remoteFile = remoteURL + "/" + differentFiles.GetAt(i);
	remoteFile = remoteURL + "/HCFRSetup.exe";
	//	localFile = localDir + "\\" + differentFiles.GetAt(i);

	CString path;
	path = getenv("APPDATA");

	localFile = path + "\\color\\HCFRSetup.exe";

	HANDLE dloadHandle = (HANDLE)_beginthread(downloadFile, 0, (void*)"");

	AtlWaitWithMessageLoop(dloadHandle);

	if (!SUCCEEDED(dloadResult))
		return false;

	else
		return true;
}