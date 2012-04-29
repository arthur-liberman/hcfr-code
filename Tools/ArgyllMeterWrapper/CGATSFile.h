/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2012 HCFR Project.  All rights reserved.
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
//      John Adcock
//      Ian C 
/////////////////////////////////////////////////////////////////////////////

// CGATSFile.h: Wrapper for the argyll CGATS type
//
//////////////////////////////////////////////////////////////////////

#if !defined(CGATSFILE_HEADER_INCLUDED)
#define CGATS_HEADER_INCLUDED

#include <string>
#include <vector>
#include <stdarg.h>

struct _cgats;
typedef _cgats cgats;

class CGATSFile
{
public:
	CGATSFile(void);
	CGATSFile(const CGATSFile &c);
	~CGATSFile(void);

	bool operator==(const CGATSFile& s) const;
    CGATSFile& operator=(const CGATSFile& s);

    const char* getPath() const;
	bool Read(const std::string& samplePath);
	bool FindKeyword(const std::string& keyWord, const int tableNum, int& index) const;
	bool FindField(const std::string& field, const int tableNum, int& index) const;
	void AllowAnySignature();
	std::string getErrorMessage() const;

	int getNumberOfTables() const;
	int getNumberOfDataSets(const int tableNum) const;
	int getNumberOfKeywords(const int tableNum) const;
	int getNumberOfFields(const int tablenum) const;

	int getIntKeywordValue(const int tableNum, const int index) const;
	double getDoubleKeywordValue(const int tableNum, const int index) const;
	std::string getStringKeywordValue(const int tableNum, const int index) const;

	double getDoubleFieldValue(const int tableNum, const int row, const int col) const;

private:
	cgats* m_CGATS;
	std::string m_Path;

};

#endif
