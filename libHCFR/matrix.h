//-------------------------------
//   Interface of Matrix Class
//-------------------------------

#ifndef _MATRIX_H_
#define _MATRIX_H_

#include <iostream>
#include <fstream>
using namespace std;

class Matrix
{
private:
	double** m_pData;                                                  //the actual data
	int m_nCols;                                                       //number of columns
	int m_nRows;                                                       //number of rows


	//private functions

	Matrix& RightAppendIdentity();
	Matrix& LeftRemoveIdentity();
public:
	//constructors and destructor

	Matrix();
	Matrix(double InitVal, int Rows, int Cols);
	Matrix(double* Data, int Rows, int Cols);
	Matrix(double** Data, int Rows, int Cols);
	Matrix(double (* Func)(double, double), int Rows, int Cols);
	Matrix(const Matrix& obj);
  Matrix(ifstream &theFile);
  void readFromFile(ifstream &theFile);
	~Matrix();


	//operators

	Matrix& operator +(const Matrix& obj) const;
	Matrix& operator -(const Matrix& obj) const;
	Matrix& operator *(const Matrix& obj) const;
	Matrix& operator *(const double _d) const;
	Matrix& operator *(const int _i) const;
	Matrix& operator /(const Matrix& obj) const;
	Matrix& operator /(const double _d) const;
	Matrix& operator /(const int _i) const;
	Matrix& operator +=(const Matrix& obj);
	Matrix& operator -=(const Matrix& obj);
	Matrix& operator *=(const Matrix& obj);
	Matrix& operator *=(const double _d);
	Matrix& operator *=(const int _i);
	Matrix& operator /=(const Matrix& obj);
	Matrix& operator /=(const double _d);
	Matrix& operator /=(const int _i);
	Matrix& operator =(const Matrix& obj);
	Matrix& operator ~() const;
	bool operator ==(const Matrix& obj) const;
	bool operator !=(const Matrix& obj) const;
	double* operator [](const int _i) const;
	double& operator ()(const int _i, const int _j) const;

	//other non-static functions, no specific order, but generally by return-type
	// and generally grouped by similar function

	bool IsIdentity() const;
	bool IsEmpty() const;

	double Determinant() const;

	double SumAll() const;
	double SumAllSquared() const;
	double SumRow(const int Row) const;
	double SumColumn(const int Col) const;
	double SumRowSquared(const int Row) const;
	double SumColumnSquared(const int Col) const;

	double GetMax() const;
	double GetMin() const;
	double GetRowMax(const int Row) const;
	double GetRowMin(const int Row) const;
	double GetColumnMax(const int Col) const;
	double GetColumnMin(const int Col) const;
	double GetRange() const;
	double GetRowRange(const int Row) const;
	double GetColumnRange(const int Col) const;

	double* GetDataOneDimen() const;
	double** GetDataTwoDimen() const;

	int GetRows() const;
	int GetColumns() const;

	Matrix& Clear();
	Matrix& ClearRow(const int Row);
	Matrix& ClearColumn(const int Col);

	Matrix& Fill(const double _d);
	Matrix& FillRow(const int Row, const double _d);
	Matrix& FillColumn(const int Col, const double _d);

	Matrix& GetInverse() const;
	Matrix& Invert();

	Matrix& AddRows(const int SourceRow, const int DestRow, const double factor = 1);
	Matrix& MultiplyRow(const int Row, const double _d);
	Matrix& DivideRow(const int Row, const double _d);
	Matrix& AddColumns(const int SourceCol, const int DestCol, const double factor = 1);
	Matrix& MultiplyColumn(const int Col, const double _d);
	Matrix& DivideColumn(const int Col, const double _d);

	Matrix& REF();
	Matrix& RREF();
	Matrix& GetREF() const;
	Matrix& GetRREF() const;

	Matrix& GetMinor(const int RowSpot, const int ColSpot) const;
	Matrix* GetMinorNew(const int RowSpot, const int ColSpot) const;
	Matrix& GetSubMatrix(const int RowSpot, const int ColSpot, const int RowLen, const int ColLen) const;
	Matrix& SetSubMatrix(const int RowSpot, const int ColSpot, const int RowLen, const int ColLen);

	Matrix& SwapRows(const int Row1, const int Row2);
	Matrix& SwapCols(const int Col1, const int Col2);

	Matrix& GetTransposed() const;
	Matrix& Transpose();

	Matrix& GetNumericRange(double& Min, double& Max) const;
	Matrix& GetNumericRangeOfRow(double& Min, double& Max, const int Row) const;
	Matrix& GetNumericRangeOfColumn(double& Min, double& Max, const int Col) const;

	Matrix& CMAR(const Matrix& obj);
	Matrix& CMAC(const Matrix& obj);
	Matrix& GetCMAR(const Matrix& obj) const;
	Matrix& GetCMAC(const Matrix& obj) const;

	Matrix& ConcatenateRow(const double* RowData);
	Matrix& ConcatenateColumn(const double* ColumnData);
	Matrix& SpliceInRow(const double* RowData, const int RowSpot);
	Matrix& SpliceInColumn(const double* ColumnData, const int ColumnSpot);
	Matrix& RemoveRow(const int Row);
	Matrix& RemoveColumn(const int Column);

	Matrix& SetValuesFromFunction(double (* Func)(double, double));

	Matrix& SortAscend();
	Matrix& SortDescend();

	Matrix& GetNormalized(const double Min, const double Max) const;
	Matrix& Normalize(const double Min, const double Max);

	Matrix& GetCovariant() const;
	Matrix& MakeCovariant();

	void Display() const;
	void Output(ostream& ostr = cout) const;
	void Input(istream& istr = cin);
	void Read(ifstream& istr);
	void Write(ofstream& ostr) const;


	//static functions

	static Matrix& IdentityMatrix(int Diagonal);
};

Matrix& IdentityMatrix(int Diagonal);

ostream& operator <<(ostream& ostr, const Matrix& obj);

/*
Une classe MatrixException contenant simplement un descriptif de l'erreur.
Une instance de cette classe sera envoyÈe en tant qu'exception si une opÈration
invalide est effectuÈe (division par zÈro, discriminant de matrice non carrÈe ...)
*/
class MatrixException
{
private:
  const char* message;
  
public:
  MatrixException(const char* errorMessage) {message = errorMessage;}
  const char* getMessage() {return message;}
};



#endif /* def _MATRIX_H_ */