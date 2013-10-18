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
//    John Adcock
//    Ian C
/////////////////////////////////////////////////////////////////////////////

// CGATSFile.cpp: Wrapper for the argyll CGATS type
//
//////////////////////////////////////////////////////////////////////


#include "ArgyllMeterWrapper.h"
#include "CGATSFile.h"
#include <stdexcept>

#define SALONEINSTLIB
#define ENABLE_USB
#define ENABLE_FAST_SERIAL
#if defined(_MSC_VER)
#pragma warning(disable:4200)
#include <winsock.h>
#endif
#include "numsup.h"
#include "xspect.h"
#include "cgats.h"
#undef SALONEINSTLIB



CGATSFile::CGATSFile(void) :
    m_CGATS(NULL)
{
    if ((m_CGATS = new_cgats()) == NULL)
    {
        throw std::logic_error("Error allocating data structure for cgats file");
    }
}


CGATSFile::CGATSFile(const CGATSFile &c) :
    m_CGATS(NULL)
{
	if ((m_CGATS = new_cgats()) == NULL)
	{
		throw std::logic_error("Error allocating data structure for cgats file");
	}

	if (!c.m_Path.empty() && !Read(c.m_Path))
	{
		std::string errorMessage = "Error reading CGATS file ";
		errorMessage += c.m_Path;
		throw std::logic_error(errorMessage);
	}
}


CGATSFile::~CGATSFile(void)
{
	if (m_CGATS != NULL)
    {
        m_CGATS->del(m_CGATS);
    }
}


bool CGATSFile::operator==(const CGATSFile& c) const
{
	if (m_Path == c.m_Path)
	{
		return true;
	}
	else
	{
		return false;
	}
}


CGATSFile& CGATSFile::operator=(const CGATSFile& c)
{
    if (this != &c)
	{
		if (!Read(c.m_Path))
		{
			std::string errorMessage = "Error reading CGATS file ";
			errorMessage += c.m_Path;
			throw std::logic_error(errorMessage);
		}
	}
	return *this;
}


const char* CGATSFile::getPath() const
{
	return m_Path.c_str();
}


bool CGATSFile::Read(const std::string& samplePath)
{
	if (!samplePath.empty())
	{
		AllowAnySignature();

        if (m_CGATS->read_name(m_CGATS, (char *)samplePath.c_str()))
        {
            return false;
        }
		else
		{
			m_Path = samplePath;
			return true;
		}
    }
    else
    {
        return false;
    }
}


bool CGATSFile::FindKeyword(const std::string& keyWord, const int tableNum, int& index) const
{
	if (getNumberOfTables() == 0)
	{
		return false;
	}
	else if ((index = m_CGATS->find_kword(m_CGATS, tableNum, keyWord.c_str())) < 0)
	{
		return false;
	}
	else
	{
		return true;
	}
}


bool CGATSFile::FindField(const std::string& field, const int tableNum, int& index) const
{
	if (getNumberOfTables() == 0)
	{
		return false;
	}
	else if ((index = m_CGATS->find_field(m_CGATS, tableNum, field.c_str())) < 0)
	{
		return false;
	}
	else
	{
		return true;
	}
}


void CGATSFile::AllowAnySignature()
{
	m_CGATS->add_other(m_CGATS, "");
}


int CGATSFile::getNumberOfTables() const
{
	if (m_CGATS != NULL)
	{
		return m_CGATS->ntables;
	}
	else
	{
		return 0;
	}
}


std::string CGATSFile::getErrorMessage() const
{
	if (m_CGATS != NULL)
	{
		return m_CGATS->err;
	}
	else
	{
		return "";
	}
}

int CGATSFile::getNumberOfDataSets(const int tableNum) const
{
	int numTables = getNumberOfTables();

	if (numTables > 0 && tableNum < numTables)
	{
		return m_CGATS->t[0].nsets;
	}
	else
	{
		return 0;
	}
}


int CGATSFile::getNumberOfKeywords(const int tableNum) const
{
	int numTables = getNumberOfTables();

	if (numTables > 0 && tableNum < numTables)
	{
		return m_CGATS->t[0].nkwords;
	}
	else
	{
		return 0;
	}
}


int CGATSFile::getNumberOfFields(const int tableNum) const
{
	int numTables = getNumberOfTables();

	if (numTables > 0 && tableNum < numTables)
	{
		return m_CGATS->t[0].nfields;
	}
	else
	{
		return 0;
	}
}


int CGATSFile::getIntKeywordValue(const int tableNum, const int index) const
{
	int numTables = getNumberOfTables();

	if (numTables > 0 && tableNum < numTables)
	{
		return atoi(m_CGATS->t[tableNum].kdata[index]);
	}
	else
	{
		return 0;
	}
}


double CGATSFile::getDoubleKeywordValue(const int tableNum, const int index) const
{
	int numTables = getNumberOfTables();

	if (numTables > 0 && tableNum < numTables)
	{
		return atof(m_CGATS->t[tableNum].kdata[index]);
	}
	else
	{
		return 0;
	}
}

std::string CGATSFile::getStringKeywordValue(const int tableNum, const int index) const
{
	int numTables = getNumberOfTables();

	if (numTables > 0 && tableNum < numTables)
	{
		std::string s;
		s = (char *)m_CGATS->t[tableNum].kdata[index];
		return s;
	}
	else
	{
		return 0;
	}
}

double CGATSFile::getDoubleFieldValue(const int tableNum, const int row, const int col) const
{
	int numTables = getNumberOfTables();

	if (numTables > 0 && tableNum < numTables)
	{
		return *((double *)m_CGATS->t[tableNum].fdata[row][col]);
	}
	else
	{
		return 0;
	}
}