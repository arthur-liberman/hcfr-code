// Class to read and write to Excel and text delimited spreadsheet
//
// Created by Yap Chun Wei
// December 2001
// 
// Version 1.1
// Updates: Fix bug in ReadRow() which prevent reading of single column spreadsheet

#ifndef CSPREADSHEET_H
#define CSPREADSHEET_H

#include <odbcinst.h>
#include <afxdb.h>

// Fx add
LPCTSTR dataTypeStringArray[3]={"char","float","date"};
LPCTSTR dataTypePreValueStringArray[3]={"'","","DATE("};
LPCTSTR dataTypePostValueStringArray[3]={"'","",")"};

class CRowArray : public CStringArray
{
	CUIntArray typeArray;

public:
	enum DataType 
	{
		charType=0,
		floatType=1,
		dateType=2,
	};

public:
//	CRowArray(CRowArray &aArray);
	INT_PTR Add(LPCTSTR newElement,DataType type=charType);
	INT_PTR Add(const CString& newElement,DataType type=charType);
	INT_PTR Add(float newElement);
	INT_PTR AddFromODBC(const CString& newElement, SWORD aSQLType);
	int Append( const CRowArray& src );
	void RemoveAll();
	void Copy(const CRowArray& src );
	CString GetFormatedValueString(int i, DataType type);

	LPCTSTR GetFormatString(int i) { return dataTypeStringArray[GetDataType(i)]; }
	DataType GetDataType(int i) { return (DataType)typeArray.GetAt(i); }
};

INT_PTR CRowArray::Add(LPCTSTR newElement, DataType type)
{
	CStringArray::Add(newElement);	
	return typeArray.Add(type);
}

INT_PTR CRowArray::Add(const CString& newElement, DataType type)
{
	CStringArray::Add(newElement);	
	return typeArray.Add(type);
}

INT_PTR CRowArray::AddFromODBC(const CString& newElement, SWORD aSQLType)
{
	CStringArray::Add(newElement);
	DataType type;
	switch(aSQLType)
	{
		case 8:
			type=CRowArray::floatType;
			break;
		default:
			type=CRowArray::charType;
	}			
	return typeArray.Add(type);
}

INT_PTR CRowArray::Add(float newElement)
{
	CString str;
	str.Format("%f",newElement);
	CStringArray::Add(str);	
	return typeArray.Add(CRowArray::floatType);
}

int CRowArray::Append( const CRowArray& src )
{
	typeArray.Append(src.typeArray);
	return CStringArray::Append(src);	
}

void CRowArray::RemoveAll()
{
	CStringArray::RemoveAll();	
	typeArray.RemoveAll();
}

void CRowArray::Copy(const CRowArray& src )
{
	CStringArray::Copy(src);	
	typeArray.Copy(src.typeArray);
}

CString CRowArray::GetFormatedValueString(int i, DataType type)
{
	CString val=GetAt(i);
	CString str=CString(dataTypePreValueStringArray[type])+GetAt(i)+CString(dataTypePostValueStringArray[type]);
	return str;
}
// end of fx add

class CSpreadSheet
{
public:
	CSpreadSheet(CString File, CString SheetOrSeparator, bool Backup = true); // Open spreadsheet for reading and writing
	~CSpreadSheet(); // Perform some cleanup functions
	bool AddHeaders(CRowArray &FieldNames, bool replace = false); // Add header row to spreadsheet
	bool DeleteSheet(); // Clear text delimited file content
	bool DeleteSheet(CString SheetName); // Clear entire Excel spreadsheet content. The sheet itself is not deleted
	bool AddRow(CRowArray &RowValues, long row = 0, bool replace = false); // Insert or replace a row into spreadsheet. Default is add new row. 
	bool AddCell(CString CellValue, CString column, long row = 0, bool Auto = true); // Replace or add a cell into Excel spreadsheet using header row or column alphabet. Default is add cell into new row. Set Auto to false if want to force column to be used as header name
	bool AddCell(CString CellValue, short column, long row = 0); // Replace or add a cell into spreadsheet using column number. Default is add cell into new row. 
	bool ReplaceRows(CRowArray &NewRowValues, CRowArray &OldRowValues); // Search and replace rows in Excel spreadsheet
	bool ReadRow(CRowArray &RowValues, long row = 0); // Read a row from spreadsheet. Default is read the next row
	bool ReadColumn(CStringArray &ColumnValues, CString column, bool Auto = true); // Read a column from Excel spreadsheet using header row or column alphabet. Set Auto to false if want to force column to be used as header name
	bool ReadColumn(CStringArray &ColumnValues, short column); // Read a column from spreadsheet using column number
	bool ReadCell (CString &CellValue, CString column, long row = 0, bool Auto = true); // Read a cell from Excel spreadsheet using header row or column alphabet. Default is read the next cell in next row. Set Auto to false if want to force column to be used as header name
	bool ReadCell (CString &CellValue, short column, long row = 0); // Read a cell from spreadsheet using column number. Default is read the next cell in next row.
	void BeginTransaction(); // Begin transaction
	bool Commit(); // Save changes to spreadsheet
	bool RollBack(); // Undo changes to spreadsheet
	bool Convert(CString SheetOrSeparator);
	inline void GetFieldNames (CStringArray &FieldNames) {FieldNames.RemoveAll(); FieldNames.Copy(m_aFieldNames);} // Get the header row from spreadsheet
	inline long GetTotalRows() {return m_dTotalRows;} // Get total number of rows in  spreadsheet
	inline short GetTotalColumns() {return m_dTotalColumns;} // Get total number of columns in  spreadsheet
	inline long GetCurrentRow() {return m_dCurrentRow;} // Get the currently selected row in  spreadsheet
	inline bool GetBackupStatus() {return m_bBackup;} // Get status of backup. True if backup is successful, False if spreadsheet is not backup
	inline bool GetTransactionStatus() {return m_bTransaction;} // Get status of Transaction. True if Transaction is started, False if Transaction is not started or has error in starting
	inline CString GetLastError() {return m_sLastError;} // Get last error message

private:
	bool Open(); // Open a text delimited file for reading or writing
	void GetExcelDriver(); // Get the name of the Excel-ODBC driver
	short CalculateColumnNumber(CString column, bool Auto); // Convert Excel column in alphabet into column number

	bool m_bAppend; // Internal flag to denote newly created spreadsheet or previously created spreadsheet
	bool m_bBackup; // Internal flag to denote status of Backup
	bool m_bExcel; // Internal flag to denote whether file is Excel spreadsheet or text delimited spreadsheet
	bool m_bTransaction; // Internal flag to denote status of Transaction

	long m_dCurrentRow; // Index of current row, starting from 1
	long m_dTotalRows; // Total number of rows in spreadsheet
	short m_dTotalColumns; // Total number of columns in Excel spreadsheet. Largest number of columns in text delimited spreadsheet

	CString m_sSql; // SQL statement to open Excel spreadsheet for reading
	CString m_sDsn; // DSN string to open Excel spreadsheet for reading and writing
	CString m_stempSql; // Temporary string for SQL statements or for use by functions
	CString m_stempString; // Temporary string for use by functions
	CString m_sSheetName; // Sheet name of Excel spreadsheet
	CString m_sExcelDriver; // Name of Excel Driver
	CString m_sFile; // Spreadsheet file name
	CString m_sSeparator; // Separator in text delimited spreadsheet
	CString m_sLastError; // Last error message

	CRowArray m_atempArray; // Temporary array for use by functions
	CRowArray m_aFieldNames; // Header row in spreadsheet
	CStringArray m_aRows; // Content of all the rows in spreadsheet

	CDatabase *m_Database; // Database variable for Excel spreadsheet
	CRecordset *m_rSheet; // Recordset for Excel spreadsheet
};

// Open spreadsheet for reading and writing
CSpreadSheet::CSpreadSheet(CString File, CString SheetOrSeparator, bool Backup) :
m_Database(NULL), m_rSheet(NULL), m_sFile(File),
m_dTotalRows(0), m_dTotalColumns(0), m_dCurrentRow(1),
m_bAppend(false), m_bBackup(Backup), m_bTransaction(false)
{
	// Detect whether file is an Excel spreadsheet or a text delimited file
	m_stempString = m_sFile.Right(4);
	m_stempString.MakeLower();
	if (m_stempString == ".xls") // File is an Excel spreadsheet
	{
		m_bExcel = true;
		m_sSheetName = SheetOrSeparator;
		m_sSeparator = ",;.?";
	}
	else // File is a text delimited file
	{
		m_bExcel = false;
		m_sSeparator = SheetOrSeparator;
	}

	if (m_bExcel) // If file is an Excel spreadsheet
	{
		m_Database = new CDatabase;
		GetExcelDriver();
		m_sDsn.Format("DRIVER={%s};DSN='';FIRSTROWHASNAMES=1;READONLY=FALSE;CREATE_DB=\"%s\";DBQ=%s", m_sExcelDriver, m_sFile, m_sFile);

		if (Open())
		{
			if (m_bBackup)
			{
				if ((m_bBackup) && (m_bAppend))
				{
					CString tempSheetName = m_sSheetName;
					m_sSheetName = "CSpreadSheetBackup";
					m_bAppend = false;
					if (!Commit())
					{
						m_bBackup = false;
					}
					m_bAppend = true;
					m_sSheetName = tempSheetName;
					m_dCurrentRow = 1;
				}
			}
		}
	}
	else // if file is a text delimited file
	{
		if (Open())
		{
			if ((m_bBackup) && (m_bAppend))
			{
				m_stempString = m_sFile;
				m_stempSql.Format("%s.bak", m_sFile);
				m_sFile = m_stempSql;
				if (!Commit())
				{
					m_bBackup = false;
				}
				m_sFile = m_stempString;
			}
		}
	}
}

// Perform some cleanup functions
CSpreadSheet::~CSpreadSheet()
{
	if (m_Database != NULL)
	{
		m_Database->Close();
		delete m_Database;
	}
}

// Add header row to spreadsheet
bool CSpreadSheet::AddHeaders(CRowArray &FieldNames, bool replace)
{
	if (m_bAppend) // Append to old Sheet
	{
		if (replace) // Replacing header row rather than adding new columns
		{
			if (!AddRow(FieldNames, 1, true))
			{
				return false;
			}
			else
			{
				return true;
			}
		}

		if (ReadRow(m_atempArray, 1)) // Add new columns
		{
			if (m_bExcel)
			{
				// Check for duplicate header row field
				for (int i = 0; i < FieldNames.GetSize(); i++)
				{
					for (int j = 0; j < m_atempArray.GetSize(); j++)
					{
						if (FieldNames.GetAt(i) == m_atempArray.GetAt(j))
						{
							m_sLastError.Format("Duplicate header row field:%s\n", FieldNames.GetAt(i));
							return false;
						}
					}
				}	
			}

			m_atempArray.Append(FieldNames);
			if (!AddRow(m_atempArray, 1, true))
			{
				m_sLastError = "Problems with adding headers\n";
				return false;
			}

			// Update largest number of columns if necessary
			if (m_atempArray.GetSize() > m_dTotalColumns)
			{
				m_dTotalColumns = m_atempArray.GetSize();
			}
			return true;
		}
		return false;				
	}
	else // New Sheet
	{
		m_dTotalColumns = FieldNames.GetSize();
		if (!AddRow(FieldNames, 1, true))
		{
			return false;
		}
		else
		{
			m_dTotalRows = 1;
			return true;
		}
	}
}

// Clear text delimited file content
bool CSpreadSheet::DeleteSheet()
{
	if (m_bExcel)
	{
		if (DeleteSheet(m_sSheetName))
		{
			return true;
		}
		else
		{
			m_sLastError = "Error deleting sheet\n";
			return false;
		}
	}
	else
	{
		m_aRows.RemoveAll();
		m_aFieldNames.RemoveAll();
		m_dTotalColumns = 0;
		m_dTotalRows = 0;
		if (!m_bTransaction)
		{
			Commit();			
		}
		m_bAppend = false; // Set flag to new sheet
		return true;		
	}
}

// Clear entire Excel spreadsheet content. The sheet itself is not deleted
bool CSpreadSheet::DeleteSheet(CString SheetName)
{
	if (m_bExcel) // If file is an Excel spreadsheet
	{
		// Delete sheet
		m_Database->OpenEx(m_sDsn, CDatabase::noOdbcDialog);
		SheetName = "[" + SheetName + "$A1:IV65536]";
		m_stempSql.Format ("DROP TABLE %s", SheetName);
		try
		{
			m_Database->ExecuteSQL(m_stempSql);
			m_Database->Close();
			m_aRows.RemoveAll();
			m_aFieldNames.RemoveAll();
			m_dTotalColumns = 0;
			m_dTotalRows = 0;
		}
		catch(CDBException *e)
		{
			m_sLastError = e->m_strError;
			m_Database->Close();
			return false;
		}
		return true;
	}
	else // if file is a text delimited file
	{
		return DeleteSheet();
	}
}

// Insert or replace a row into spreadsheet. 
// Default is add new row.
bool CSpreadSheet::AddRow(CRowArray &RowValues, long row, bool replace)
{
	long tempRow;
	
	if (row == 1)
	{
		if (m_bExcel) 
		{
			// Check for duplicate header row field for Excel spreadsheet
			for (int i = 0; i < RowValues.GetSize(); i++)
			{
				for (int j = 0; j < RowValues.GetSize(); j++)
				{
					if ((i != j) && (RowValues.GetAt(i) == RowValues.GetAt(j)))
					{
						m_sLastError.Format("Duplicate header row field:%s\n", RowValues.GetAt(i));
						return false;
					}
				}
			}
			
			// Check for reduced header row columns
			if (RowValues.GetSize() < m_dTotalColumns)
			{
				m_sLastError = "Number of columns in new header row cannot be less than the number of columns in previous header row";
				return false;
			}
			m_dTotalColumns = RowValues.GetSize();
		}

		// Update header row
		m_aFieldNames.RemoveAll();
		m_aFieldNames.Copy(RowValues);
	}
	else
	{
		if (m_bExcel)
		{
			if (m_dTotalColumns == 0)
			{
				m_sLastError = "No header row. Add header row first\n";
				return false;
			}
		}
	}

	if (m_bExcel) // For Excel spreadsheet
	{
		if (RowValues.GetSize() > m_aFieldNames.GetSize())
		{
			m_sLastError = "Number of columns to be added cannot be greater than the number of fields\n";
			return false;
		}
	}
	else // For text delimited spreadsheet
	{
		// Update largest number of columns if necessary
		if (RowValues.GetSize() > m_dTotalColumns)
		{
			m_dTotalColumns = RowValues.GetSize();
		}
	}

	// Convert row values
	m_stempString.Empty();
	for (int i = 0; i < RowValues.GetSize(); i++)
	{
		if (i != RowValues.GetSize()-1) // Not last column
		{
			m_stempSql.Format("\"%s\"%s", RowValues.GetAt(i), m_sSeparator);
			m_stempString += m_stempSql;
		}
		else // Last column
		{
			m_stempSql.Format("\"%s\"", RowValues.GetAt(i));
			m_stempString += m_stempSql;
		}
	}
	
	if (row)
	{
		if (row <= m_dTotalRows) // Not adding new rows
		{
			if (replace) // Replacing row
			{
				m_aRows.SetAt(row-1, m_stempString);
			}
			else // Inserting row
			{
				m_aRows.InsertAt(row-1, m_stempString);
				m_dTotalRows++;
			}

			if (!m_bTransaction)
			{
				Commit();
			}
			return true;
		}
		else // Adding new rows
		{
			// Insert null rows until specified row
			m_dCurrentRow = m_dTotalRows;
			m_stempSql.Empty();
			CString nullString;
			for (int i = 1; i <= m_dTotalColumns; i++)
			{
				if (i != m_dTotalColumns)
				{
					if (m_bExcel)
					{
						nullString.Format("\" \"%s", m_sSeparator);
					}
					else
					{
						nullString.Format("\"\"%s", m_sSeparator);
					}
					m_stempSql += nullString;
				}
				else
				{
					if (m_bExcel)
					{
						m_stempSql += "\" \"";
					}
					else
					{
						m_stempSql += "\"\"";
					}
				}
			}
			for (int j = m_dTotalRows + 1; j < row; j++)
			{
				m_dCurrentRow++;
				m_aRows.Add(m_stempSql);
			}
		}
	}
	else
	{
		tempRow = m_dCurrentRow;
		m_dCurrentRow = m_dTotalRows;
	}

	// Insert new row
	m_dCurrentRow++;
	m_aRows.Add(m_stempString);
	
	if (row > m_dTotalRows)
	{
		m_dTotalRows = row;
	}
	else if (!row)
	{
		m_dTotalRows = m_dCurrentRow;
		m_dCurrentRow = tempRow;
	}
	if (!m_bTransaction)
	{
		Commit();
	}
	return true;
}

// Replace or add a cell into Excel spreadsheet using header row or column alphabet. 
// Default is add cell into new row.
// Set Auto to false if want to force column to be used as header name
bool CSpreadSheet::AddCell(CString CellValue, CString column, long row, bool Auto)
{
	short columnIndex = CalculateColumnNumber(column, Auto);
	if (columnIndex == 0)
	{
		return false;
	}

	if (AddCell(CellValue, columnIndex, row))
	{
		return true;
	}
	return false;
}

// Replace or add a cell into spreadsheet using column number
// Default is add cell into new row.
bool CSpreadSheet::AddCell(CString CellValue, short column, long row)
{
	if (column == 0)
	{
		m_sLastError = "Column cannot be zero\n";
		return false;
	}

	long tempRow;

	if (m_bExcel) // For Excel spreadsheet
	{
		if (column > m_aFieldNames.GetSize() + 1)
		{
			m_sLastError = "Cell column to be added cannot be greater than the number of fields\n";
			return false;
		}
	}
	else // For text delimited spreadsheet
	{
		// Update largest number of columns if necessary
		if (column > m_dTotalColumns)
		{
			m_dTotalColumns = column;
		}
	}

	if (row)
	{
		if (row <= m_dTotalRows)
		{
			ReadRow(m_atempArray, row);
	
			// Change desired row
			m_atempArray.SetAtGrow(column-1, CellValue);

			if (row == 1)
			{
				if (m_bExcel) // Check for duplicate header row field
				{										
					for (int i = 0; i < m_atempArray.GetSize(); i++)
					{
						for (int j = 0; j < m_atempArray.GetSize(); j++)
						{
							if ((i != j) && (m_atempArray.GetAt(i) == m_atempArray.GetAt(j)))
							{
								m_sLastError.Format("Duplicate header row field:%s\n", m_atempArray.GetAt(i));
								return false;
							}
						}
					}
				}

				// Update header row
				m_aFieldNames.RemoveAll();
				m_aFieldNames.Copy(m_atempArray);
			}	

			if (!AddRow(m_atempArray, row, true))
			{
				return false;
			}

			if (!m_bTransaction)
			{
				Commit();
			}
			return true;
		}
		else
		{
			// Insert null rows until specified row
			m_dCurrentRow = m_dTotalRows;
			m_stempSql.Empty();
			CString nullString;
			for (int i = 1; i <= m_dTotalColumns; i++)
			{
				if (i != m_dTotalColumns)
				{
					if (m_bExcel)
					{
						nullString.Format("\" \"%s", m_sSeparator);
					}
					else
					{
						nullString.Format("\"\"%s", m_sSeparator);
					}
					m_stempSql += nullString;
				}
				else
				{
					if (m_bExcel)
					{
						m_stempSql += "\" \"";
					}
					else
					{
						m_stempSql += "\"\"";
					}
				}
			}
			for (int j = m_dTotalRows + 1; j < row; j++)
			{
				m_dCurrentRow++;
				m_aRows.Add(m_stempSql);
			}
		}
	}
	else
	{
		tempRow = m_dCurrentRow;
		m_dCurrentRow = m_dTotalRows;
	}

	// Insert cell
	m_dCurrentRow++;
	m_stempString.Empty();
	for (int j = 1; j <= m_dTotalColumns; j++)
	{
		if (j != m_dTotalColumns) // Not last column
		{
			if (j != column)
			{
				if (m_bExcel)
				{
					m_stempSql.Format("\" \"%s", m_sSeparator);
				}
				else
				{
					m_stempSql.Format("\"\"%s", m_sSeparator);
				}
				m_stempString += m_stempSql;
			}
			else
			{
				m_stempSql.Format("\"%s\"%s", CellValue, m_sSeparator);
				m_stempString += m_stempSql;
			}
		}
		else // Last column
		{
			if (j != column)
			{
				if (m_bExcel)
				{
					m_stempString += "\" \"";
				}
				else
				{
					m_stempString += "\"\"";
				}
			}
			else
			{
				m_stempSql.Format("\"%s\"", CellValue);
				m_stempString += m_stempSql;
			}
		}
	}	

	m_aRows.Add(m_stempString);
	
	if (row > m_dTotalRows)
	{
		m_dTotalRows = row;
	}
	else if (!row)
	{
		m_dTotalRows = m_dCurrentRow;
		m_dCurrentRow = tempRow;
	}
	if (!m_bTransaction)
	{
		Commit();
	}
	return true;
}

// Search and replace rows in Excel spreadsheet
bool CSpreadSheet::ReplaceRows(CRowArray &NewRowValues, CRowArray &OldRowValues)
{
	if (m_bExcel) // If file is an Excel spreadsheet
	{
		m_Database->OpenEx(m_sDsn, CDatabase::noOdbcDialog);
		m_stempSql.Format("UPDATE [%s] SET ", m_sSheetName);
		for (int i = 0; i < NewRowValues.GetSize(); i++)
		{
//			m_stempString.Format("[%s]='%s', ", m_aFieldNames.GetAt(i), NewRowValues.GetAt(i)); fx
			m_stempString="["+m_aFieldNames.GetAt(i)+"]="+NewRowValues.GetFormatedValueString(i,m_aFieldNames.GetDataType(i));
			m_stempSql = m_stempSql + m_stempString;
		}
		m_stempSql.Delete(m_stempSql.GetLength()-2, 2);
		m_stempSql = m_stempSql + " WHERE (";
		for (int j = 0; j < OldRowValues.GetSize()-1; j++)
		{
//			m_stempString.Format("[%s]='%s' AND ", m_aFieldNames.GetAt(j), OldRowValues.GetAt(j)); fx
			m_stempString="["+m_aFieldNames.GetAt(j)+"]="+OldRowValues.GetFormatedValueString(j,m_aFieldNames.GetDataType(j))+" AND ";
			m_stempSql = m_stempSql + m_stempString;
		}
		m_stempSql.Delete(m_stempSql.GetLength()-4, 5);
		m_stempSql += ")";

		try
		{
			m_Database->ExecuteSQL(m_stempSql);
			m_Database->Close();
			Open();
			return true;
		}
		catch(CDBException *e)
		{
			m_sLastError = e->m_strError;
			m_Database->Close();
			return false;
		}
	}
	else // if file is a text delimited file
	{
		m_sLastError = "Function not available for text delimited file\n";
		return false;
	}
}

// Read a row from spreadsheet. 
// Default is read the next row
bool CSpreadSheet::ReadRow(CRowArray &RowValues, long row)
{
	// Check if row entered is more than number of rows in sheet
	if (row <= m_aRows.GetSize())
	{
		if (row != 0)
		{
			m_dCurrentRow = row;
		}
		else if (m_dCurrentRow > m_aRows.GetSize())
		{
			return false;
		}
		// Read the desired row
		RowValues.RemoveAll();
		m_stempString = m_aRows.GetAt(m_dCurrentRow-1);
		m_stempString.Replace("'","''");	// Fx add
		m_dCurrentRow++;

		// Search for separator to split row
		int separatorPosition;
		m_stempSql.Format("\"%s\"", m_sSeparator);
		separatorPosition = m_stempString.Find(m_stempSql); // If separator is "?"
		if (separatorPosition != -1)
		{
			// Save columns
			int nCount = 0;
			int stringStartingPosition = 0;
			while (separatorPosition != -1)
			{
				nCount = separatorPosition - stringStartingPosition;
				RowValues.Add(m_stempString.Mid(stringStartingPosition, nCount));
				stringStartingPosition = separatorPosition + m_stempSql.GetLength();
				separatorPosition = m_stempString.Find(m_stempSql, stringStartingPosition);
			}
			nCount = m_stempString.GetLength() - stringStartingPosition;
			RowValues.Add(m_stempString.Mid(stringStartingPosition, nCount));

			// Remove quotes from first column
			m_stempString = RowValues.GetAt(0);
			m_stempString.Delete(0, 1);
			RowValues.SetAt(0, m_stempString);
			
			// Remove quotes from last column
			m_stempString = RowValues.GetAt(RowValues.GetSize()-1);
			m_stempString.Delete(m_stempString.GetLength()-1, 1);
			RowValues.SetAt(RowValues.GetSize()-1, m_stempString);

			return true;
		}
		else
		{
			// Save columns
			separatorPosition = m_stempString.Find(m_sSeparator); // if separator is ?
			if (separatorPosition != -1)
			{
				int nCount = 0;
				int stringStartingPosition = 0;
				while (separatorPosition != -1)
				{
					nCount = separatorPosition - stringStartingPosition;
					RowValues.Add(m_stempString.Mid(stringStartingPosition, nCount));
					stringStartingPosition = separatorPosition + m_sSeparator.GetLength();
					separatorPosition = m_stempString.Find(m_sSeparator, stringStartingPosition);
				}
				nCount = m_stempString.GetLength() - stringStartingPosition;
				RowValues.Add(m_stempString.Mid(stringStartingPosition, nCount));
				return true;
			}
			else	// Treat spreadsheet as having one column
			{
				// Remove opening and ending quotes if any
				int quoteBegPos = m_stempString.Find('\"');
				int quoteEndPos = m_stempString.ReverseFind('\"');
				if ((quoteBegPos == 0) && (quoteEndPos == m_stempString.GetLength()-1))
				{
					m_stempString.Delete(0, 1);
					m_stempString.Delete(m_stempString.GetLength()-1, 1);
				}

				RowValues.Add(m_stempString);
			}
		}
	}
	m_sLastError = "Desired row is greater than total number of rows in spreadsheet\n";
	return false; 

}

// Read a column from Excel spreadsheet using header row or column alphabet. 
// Set Auto to false if want to force column to be used as header name
bool CSpreadSheet::ReadColumn(CStringArray &ColumnValues, CString column, bool Auto)
{
	short columnIndex = CalculateColumnNumber(column, Auto);
	if (columnIndex == 0)
	{
		return false;
	}

	if (ReadColumn(ColumnValues, columnIndex))
	{
		return true;
	}
	return false;
}

// Read a column from spreadsheet using column number
bool CSpreadSheet::ReadColumn(CStringArray &ColumnValues, short column)
{
	if (column == 0)
	{
		m_sLastError = "Column cannot be zero\n";
		return false;
	}

	int tempRow = m_dCurrentRow;
	m_dCurrentRow = 1;
	ColumnValues.RemoveAll();
	for (int i = 1; i <= m_aRows.GetSize(); i++)
	{
		// Read each row
		if (ReadRow(m_atempArray, i))
		{
			// Get value of cell in desired column
			if (column <= m_atempArray.GetSize())
			{
				ColumnValues.Add(m_atempArray.GetAt(column-1));
			}
			else
			{
				ColumnValues.Add("");
			}
		}
		else
		{
			m_dCurrentRow = tempRow;
			m_sLastError = "Error reading row\n";
			return false;
		}
	}
	m_dCurrentRow = tempRow;
	return true;
}

// Read a cell from Excel spreadsheet using header row or column alphabet. 
// Default is read the next cell in next row. 
// Set Auto to false if want to force column to be used as header name
bool CSpreadSheet::ReadCell (CString &CellValue, CString column, long row, bool Auto)
{
	short columnIndex = CalculateColumnNumber(column, Auto);
	if (columnIndex == 0)
	{
		return false;
	}

	if (ReadCell(CellValue, columnIndex, row))
	{
		return true;
	}
	return false;
}

// Read a cell from spreadsheet using column number. 
// Default is read the next cell in next row.
bool CSpreadSheet::ReadCell (CString &CellValue, short column, long row)
{
	if (column == 0)
	{
		m_sLastError = "Column cannot be zero\n";
		return false;
	}

	int tempRow = m_dCurrentRow;
	if (row)
	{
		m_dCurrentRow = row;
	}
	if (ReadRow(m_atempArray, m_dCurrentRow))
	{
		// Get value of cell in desired column
		if (column <= m_atempArray.GetSize())
		{
			CellValue = m_atempArray.GetAt(column-1);
		}
		else
		{
			CellValue.Empty();
			m_dCurrentRow = tempRow;
			return false;
		}
		m_dCurrentRow = tempRow;
		return true;
	}
	m_dCurrentRow = tempRow;
	m_sLastError = "Error reading row\n";
	return false;
}

// Begin transaction
void CSpreadSheet::BeginTransaction()
{
	m_bTransaction = true;
}

// Save changes to spreadsheet
bool CSpreadSheet::Commit()
{
	if (m_bExcel) // If file is an Excel spreadsheet
	{
		m_Database->OpenEx(m_sDsn, CDatabase::noOdbcDialog);

		if (m_bAppend)
		{
			// Delete old sheet if it exists
			m_stempString= "[" + m_sSheetName + "$A1:IV65536]";
			m_stempSql.Format ("DROP TABLE %s", m_stempString);
			try
			{
				m_Database->ExecuteSQL(m_stempSql);
			}
			catch(CDBException *e)
			{
				m_sLastError = e->m_strError;
				m_Database->Close();
				return false;
			}
			
			// Create new sheet
			m_stempSql.Format("CREATE TABLE [%s$A1:IV65536] (", m_sSheetName);
			for (int j = 0; j < m_aFieldNames.GetSize(); j++)
			{
				m_stempSql = m_stempSql + "[" + m_aFieldNames.GetAt(j) +"] " + m_aFieldNames.GetFormatString(j) + ", ";
			}
			m_stempSql.Delete(m_stempSql.GetLength()-2, 2);
			m_stempSql += ")";
		}
		else
		{
			// Create new sheet
			m_stempSql.Format("CREATE TABLE [%s] (", m_sSheetName);
			for (int i = 0; i < m_aFieldNames.GetSize(); i++)
			{
				m_stempSql = m_stempSql + "[" + m_aFieldNames.GetAt(i) +"] " + m_aFieldNames.GetFormatString(i) + ", ";
			}
			m_stempSql.Delete(m_stempSql.GetLength()-2, 2);
			m_stempSql += ")";
		}

		try
		{
			m_Database->ExecuteSQL(m_stempSql);
			if (!m_bAppend)
			{
				m_dTotalColumns = m_aFieldNames.GetSize();
				m_bAppend = true;
			}
		}
		catch(CDBException *e)
		{
			m_sLastError = e->m_strError;
			m_Database->Close();
			return false;
		}

		// Save changed data
		for (int k = 1; k < m_dTotalRows; k++)
		{
			ReadRow(m_atempArray, k+1);

			// Create Insert SQL
			m_stempSql.Format("INSERT INTO [%s$A1:IV%d] (", m_sSheetName, k);
			for (int i = 0; i < m_atempArray.GetSize(); i++)
			{
				m_stempString.Format("[%s], ", m_aFieldNames.GetAt(i));
				m_stempSql = m_stempSql + m_stempString;
			}
			m_stempSql.Delete(m_stempSql.GetLength()-2, 2);
			m_stempSql += ") VALUES (";
			for (int j = 0; j < m_atempArray.GetSize(); j++)
			{
				m_stempString=m_atempArray.GetFormatedValueString(j,m_aFieldNames.GetDataType(j))+", ";
				m_stempSql = m_stempSql + m_stempString;
			}
			m_stempSql.Delete(m_stempSql.GetLength()-2, 2);
			m_stempSql += ")";

			// Add row
			try
			{
				m_Database->ExecuteSQL(m_stempSql);
			}
			catch(CDBException *e)
			{
				m_sLastError = e->m_strError;
				m_Database->Close();
				return false;
			}
		}
		m_Database->Close();
		m_bTransaction = false;
		return true;
	}
	else // if file is a text delimited file
	{
		try
		{
			CFile *File = NULL;
			File = new CFile(m_sFile, CFile::modeCreate | CFile::modeWrite  | CFile::shareDenyNone);
			if (File != NULL)
			{
				CArchive *Archive = NULL;
				Archive = new CArchive(File, CArchive::store);
				if (Archive != NULL)
				{
					for (int i = 0; i < m_aRows.GetSize(); i++)
					{
						Archive->WriteString(m_aRows.GetAt(i));
						Archive->WriteString("\r\n");
					}
					delete Archive;
					delete File;
					m_bTransaction = false;
					return true;
				}
				delete File;
			}
		}
		catch(...)
		{
		}
		m_sLastError = "Error writing file\n";
		return false;
	}
}

// Undo changes to spreadsheet
bool CSpreadSheet::RollBack()
{
	if (Open())
	{
		m_bTransaction = false;
		return true;
	}
	m_sLastError = "Error in returning to previous state\n";
	return false;
}

bool CSpreadSheet::Convert(CString SheetOrSeparator)
{
	// Prepare file
	m_stempString = m_sFile;
	m_stempString.Delete(m_stempString.GetLength()-4, 4);
	if (m_bExcel) // If file is an Excel spreadsheet
	{
		m_stempString += ".csv";
		CSpreadSheet tempSheet(m_stempString, SheetOrSeparator, false);
		
		// Stop convert if text delimited file exists
		if (tempSheet.GetTotalColumns() != 0)
		{
			return false;
		}

		tempSheet.BeginTransaction();

		for (int i = 1; i <= m_dTotalRows; i++)
		{
			if (!ReadRow(m_atempArray, i))
			{
				return false;
			}
			if (!tempSheet.AddRow(m_atempArray, i))
			{
				return false;
			}
		}
		if (!tempSheet.Commit())
		{
			return false;
		}
		return true;
	}
	else // if file is a text delimited file
	{
		m_stempString += ".xls";
		CSpreadSheet tempSheet(m_stempString, SheetOrSeparator, false);

		// Stop convert if Excel file exists
		if (tempSheet.GetTotalColumns() != 0)
		{
			return false;
		}

		GetFieldNames(m_atempArray);

		// Check for duplicate header row field
		bool duplicate = false;
		for (int i = 0; i < m_atempArray.GetSize(); i++)
		{
			for (int j = 0; j < m_atempArray.GetSize(); j++)
			{
				if ((i != j) && (m_atempArray.GetAt(i) == m_atempArray.GetAt(j)))
				{
					m_sLastError.Format("Duplicate header row field:%s\n", m_atempArray.GetAt(i));
					duplicate = true;
				}
			}
		}

		if (duplicate) // Create dummy header row
		{
			m_atempArray.RemoveAll();
			for (int k = 1; k <= m_dTotalColumns; k++)
			{
				m_stempString.Format("%d", k);
				m_atempArray.Add(m_stempString);
			}

			if (!tempSheet.AddHeaders(m_atempArray))
			{
				return false;
			}

			for (int l = 1; l <= m_dTotalRows; l++)
			{
				if (!ReadRow(m_atempArray, l))
				{
					return false;
				}
				if (!tempSheet.AddRow(m_atempArray, l+1))
				{
					return false;
				}
			}
			return true;
		}
		else
		{
			if (!tempSheet.AddHeaders(m_atempArray))
			{
				return false;
			}

			for (int l = 2; l <= m_dTotalRows; l++)
			{
				if (!ReadRow(m_atempArray, l))
				{
					return false;
				}
				if (!tempSheet.AddRow(m_atempArray, l))
				{
					return false;
				}
			}
			return true;
		}
	}
}

// Open a text delimited file for reading or writing
bool CSpreadSheet::Open()
{
	if (m_bExcel) // If file is an Excel spreadsheet
	{
		m_Database->OpenEx(m_sDsn, CDatabase::noOdbcDialog);

		// Open Sheet
		m_rSheet = new CRecordset( m_Database );
		m_sSql.Format("SELECT * FROM [%s$A1:IV65536]", m_sSheetName);
		try
		{
			m_rSheet->Open(CRecordset::forwardOnly, m_sSql, CRecordset::readOnly);
		}
		catch(...)
		{
			delete m_rSheet;
			m_rSheet = NULL;
			m_Database->Close();
			return false;
		}

		// Get number of columns
		m_dTotalColumns = m_rSheet->m_nResultCols;

		if (m_dTotalColumns != 0)
		{
			m_aRows.RemoveAll();
			m_stempString.Empty();
			m_bAppend = true;
			m_dTotalRows++; // Keep count of total number of rows
			
			// Get field names i.e header row
			for (int i = 0; i < m_dTotalColumns; i++)
			{
				m_stempSql = m_rSheet->m_rgODBCFieldInfos[i].m_strName;
				m_aFieldNames.AddFromODBC(m_stempSql,m_rSheet->m_rgODBCFieldInfos[i].m_nSQLType);

				// Join up all the columns into a string
				if (i != m_dTotalColumns-1) // Not last column
				{
					m_stempString = m_stempString + "\"" + m_stempSql + "\"" + m_sSeparator;
				}
				else // Last column
				{	
					m_stempString = m_stempString + "\"" + m_stempSql + "\"";
				}				
			}
			
			// Store the header row as the first row in memory
			m_aRows.Add(m_stempString);

			// Read and store the rest of the rows in memory
			while (!m_rSheet->IsEOF())
			{
				m_dTotalRows++; // Keep count of total number of rows
				try
				{
					// Get all the columns in a row
					m_stempString.Empty();
					for (short column = 0; column < m_dTotalColumns; column++)
					{
						m_rSheet->GetFieldValue(column, m_stempSql);

						// Join up all the columns into a string
						if (column != m_dTotalColumns-1) // Not last column
						{
							m_stempString = m_stempString + "\"" + m_stempSql + "\"" + m_sSeparator;
						}
						else // Last column
						{	
							m_stempString = m_stempString + "\"" + m_stempSql + "\"";
						}
					}

					// Store the obtained row in memory
					m_aRows.Add(m_stempString);
					m_rSheet->MoveNext();
				}
				catch (...)
				{
					m_sLastError = "Error reading row\n";
					delete m_rSheet;
					m_rSheet = NULL;
					m_Database->Close();
					return false;
				}
			}		
		}
		
		m_rSheet->Close();
		delete m_rSheet;
		m_rSheet = NULL;
		m_Database->Close();
		m_dCurrentRow = 1;
		return true;
	}
	else // if file is a text delimited file
	{
		try
		{
			CFile *File = NULL;
			File = new CFile(m_sFile, CFile::modeRead | CFile::shareDenyNone);
			if (File != NULL)
			{
				CArchive *Archive = NULL;
				Archive = new CArchive(File, CArchive::load);
				if (Archive != NULL)
				{
					m_aRows.RemoveAll();
					// Read and store all rows in memory
					while(Archive->ReadString(m_stempString))
					{
						m_aRows.Add(m_stempString);
					}
					ReadRow(m_aFieldNames, 1); // Get field names i.e header row
					delete Archive;
					delete File;

					// Get total number of rows
					m_dTotalRows = m_aRows.GetSize();

					// Get the largest number of columns
					for (int i = 0; i < m_aRows.GetSize(); i++)
					{
						ReadRow(m_atempArray, i);
						if (m_atempArray.GetSize() > m_dTotalColumns)
						{
							m_dTotalColumns = m_atempArray.GetSize();
						}
					}

					if (m_dTotalColumns != 0)
					{
						m_bAppend = true;
					}
					return true;
				}
				delete File;
			}
		}
		catch(...)
		{
			m_sLastError = "Error in opening file\n";
			return false;
		}
		m_sLastError = "Error in opening file\n";
		return false;
	}
}

// Convert Excel column in alphabet into column number
short CSpreadSheet::CalculateColumnNumber(CString column, bool Auto)
{
	if (Auto)
	{
		int firstLetter, secondLetter;
		column.MakeUpper();

		if (column.GetLength() == 1)
		{
			firstLetter = column.GetAt(0);
			return (firstLetter - 65 + 1); // 65 is A in ascii
		}
		else if (column.GetLength() == 2)
		{
			firstLetter = column.GetAt(0);
			secondLetter = column.GetAt(1);
			return ((firstLetter - 65 + 1)*26 + (secondLetter - 65 + 1)); // 65 is A in ascii
		}
	}

	// Check if it is a valid field name
	for (int i = 0; i < m_aFieldNames.GetSize(); i++)
	{
		if (!column.Compare(m_aFieldNames.GetAt(i)))
		{
			return (i + 1);
		}
	}
	m_sLastError = "Invalid field name or column alphabet\n";
	return 0;	
}

// Get the name of the Excel-ODBC driver
void CSpreadSheet::GetExcelDriver()
{
	char szBuf[2001];
	WORD cbBufMax = 2000;
	WORD cbBufOut;
	char *pszBuf = szBuf;

	// Get the names of the installed drivers ("odbcinst.h" has to be included )
	if(!SQLGetInstalledDrivers(szBuf,cbBufMax,& cbBufOut))
	{
		m_sExcelDriver = "";
	}
	
	// Search for the driver...
	do
	{
		if( strstr( pszBuf, "Excel" ) != 0 )
		{
			// Found !
			m_sExcelDriver = CString( pszBuf );
			break;
		}
		pszBuf = strchr( pszBuf, '\0' ) + 1;
	}
	while( pszBuf[1] != '\0' );
}

#endif