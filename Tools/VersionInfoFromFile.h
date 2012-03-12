#pragma once

#include <vector>
#include <string>

class VersionInfoFromFile
{
public:
    explicit VersionInfoFromFile();
    explicit VersionInfoFromFile(const std::string& fileName);
    ~VersionInfoFromFile(void);
    bool getProductName(std::string& valueToSet) const;
    bool getCompanyName(std::string& valueToSet) const;
    bool getLegalCopyright(std::string& valueToSet) const;
    bool getComments(std::string& valueToSet) const;
    bool getProductVersion(std::string& valueToSet) const;
private:
    void setupVersionInfoForFile(LPCTSTR fileName);
    bool getString(const std::string& partialString, std::string& result) const;
    std::vector<BYTE> m_buf;
};
