#include "StdAfx.h"
#include "VersionInfoFromFile.h"

VersionInfoFromFile::VersionInfoFromFile()
{
    // get name of executable
    std::vector<TCHAR> moduleName(_MAX_PATH);
    GetModuleFileName( NULL, &moduleName[0], _MAX_PATH);
    setupVersionInfoForFile(&moduleName[0]);
}

VersionInfoFromFile::VersionInfoFromFile(const std::string& fileName)
{
}

VersionInfoFromFile::~VersionInfoFromFile(void)
{
}

void VersionInfoFromFile::setupVersionInfoForFile(LPCTSTR fileName)
{
    DWORD dwHandle;

    // Get the size of the version information.
    DWORD verSize = GetFileVersionInfoSize(fileName, &dwHandle);

    m_buf.resize(verSize + 1);

    BOOL res = GetFileVersionInfo(fileName, NULL,  verSize, &m_buf[0]);
    if(!res)
    {
        m_buf.clear();
    }
}

bool VersionInfoFromFile::getString(const std::string& partialString, std::string& result) const
{
    result.clear();

    if(m_buf.empty())
    {
        return false;
    }

    struct LANGANDCODEPAGE 
    {
        WORD wLanguage;
        WORD wCodePage;
    } *lpTranslate;
    UINT wSize;

    BOOL retVal = VerQueryValue(&m_buf[0], _T("\\VarFileInfo\\Translation" ), (LPVOID*)&lpTranslate, &wSize);
    if (retVal == 0 || wSize == 0)
    {
        return false;
    }

    // Read the file description for each language and code page.
    std::vector<TCHAR> subBlock(30 + partialString.size());
    TCHAR* resultBuffer;
    for( size_t i(0); i < (wSize/sizeof(struct LANGANDCODEPAGE)); ++i)
    {
        wsprintf( &subBlock[0], _T("\\StringFileInfo\\%04x%04x\\%s"), lpTranslate[i].wLanguage, lpTranslate[i].wCodePage, partialString.c_str());
        retVal = VerQueryValue(&m_buf[0], &subBlock[0], (LPVOID*)&resultBuffer, &wSize);
        if (retVal)
        {
            result = resultBuffer;
            return true;
        }
    }
    return false;
}

bool VersionInfoFromFile::getProductName(std::string& valueToSet) const
{
    return getString(_T("ProductName"), valueToSet);
}

bool VersionInfoFromFile::getCompanyName(std::string& valueToSet) const
{
    return getString(_T("CompanyName"), valueToSet);
}

bool VersionInfoFromFile::getLegalCopyright(std::string& valueToSet) const
{
    return getString(_T("LegalCopyright"), valueToSet);
}

bool VersionInfoFromFile::getComments(std::string& valueToSet) const
{
    return getString(_T("Comments"), valueToSet);
}

bool VersionInfoFromFile::getProductVersion(std::string& valueToSet) const
{
    return getString(_T("ProductVersion"), valueToSet);
}

#pragma comment(lib, "version.lib")
