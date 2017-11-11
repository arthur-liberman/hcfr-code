// CWebUpdate.h : header file
//
#include <Wininet.h>

#ifndef _CWEBUPDATE_H
#define _CWEBUPDATE_H

/////////////////////////////////////////////////////////////////////////////
// CWebUpdate definitions

class CWebUpdate
{
// Construction
public:
	CWebUpdate();   // Standard constructor

	// Base directory, specify true to use the exe's path
	void SetLocalDirectory(LPCSTR pathTo, bool generate);
	CString GetLocalDirectory() { return localDir; }

	// Remote URLs/dirs
	void SetUpdateFileURL(LPCSTR url) { updateURL = url; }
	void SetRemoteURL(LPCSTR url) { remoteURL = url; }
	CString GetRemoteURL() { return remoteURL; }
	CString GetUpdateFileURL() { return updateURL; }

	// Give back some information
	int GetNumberSuccessful() { return numSuccess; }
	int GetNumberDifferent() { return numDifferent; }
	int GetNumberMissing() { return numMissing; }
	CString GetSuccessfulAt(int i) { return successfulFiles.GetAt(i); }
	CString GetDifferentAt(int i) { return differentFiles.GetAt(i); }
	CString GetMissingAt(int i) { return missingFiles.GetAt(i); }

	// Download missing files
	bool DownloadMissing(int i);
	bool DownloadDifferent(int i);

	// Do the update
	bool DoUpdateCheck();

// Implementation
protected:
	CString    remoteURL;
	CString    updateURL;
	CString    localDir;
	CString    lastError;

	CStringArray    successfulFiles;
	int             numSuccess;
	CStringArray    differentFiles;
	int             numDifferent;
	CStringArray    missingFiles;
	int             numMissing;

private:
	CString DoSHA1Hash(LPCSTR filePath);
};

#endif
