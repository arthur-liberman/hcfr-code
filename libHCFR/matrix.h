//-------------------------------
//   Interface of Matrix Class
//-------------------------------

#ifndef _MATRIX_H_
#define _MATRIX_H_

#include "libHCFR_Config.h"

#ifdef LIBHCFR_HAS_MFC
#include <afxwin.h>         // MFC core and standard components
#endif
#include <iostream>
#include <fstream>
#include <vector>
using namespace std;

class Matrix
{
private:
	vector<double> m_pData;                                            //the actual data
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
    Matrix(ifstream &theFile);
    void readFromFile(ifstream &theFile);
	virtual ~Matrix();


	//operators

	Matrix operator +(const Matrix& obj) const;
	Matrix operator -(const Matrix& obj) const;
	Matrix operator *(const Matrix& obj) const;
	Matrix operator *(const double _d) const;
	Matrix operator /(const Matrix& obj) const;
	Matrix operator /(const double _d) const;
	Matrix& operator +=(const Matrix& obj);
	Matrix& operator -=(const Matrix& obj);
	Matrix& operator *=(const Matrix& obj);
	Matrix& operator *=(const double _d);
	Matrix& operator /=(const Matrix& obj);
	Matrix& operator /=(const double _d);
	Matrix operator ~() const;
	bool operator ==(const Matrix& obj) const;
	bool operator !=(const Matrix& obj) const;
	double* operator [](const int _i);
	double& operator ()(const int _i, const int _j);
	const double* operator [](const int _i) const;
	const double& operator ()(const int _i, const int _j) const;

	//other non-static functions, no specific order, but generally by return-type
	// and generally grouped by similar function

	bool IsIdentity() const;
	bool IsEmpty() const;

	double Determinant() const;

	int GetRows() const;
	int GetColumns() const;

	Matrix GetInverse() const;
	Matrix& Invert();

	Matrix GetMinor(const int RowSpot, const int ColSpot) const;

	Matrix GetTransposed() const;
	Matrix& Transpose();

	Matrix& CMAC(const Matrix& obj);

	void Display() const;
	void Output(ostream& ostr = cout) const;
	void Input(istream& istr = cin);
	void Read(ifstream& istr);
	void Write(ofstream& ostr) const;

#ifdef LIBHCFR_HAS_MFC
    void Serialize(CArchive& archive);
#endif

	//static functions

	static Matrix IdentityMatrix(int Diagonal);
private:
    Matrix& AddRows(const int SourceRow, const int DestRow, const double factor);
    Matrix& DivideRow(const int Row, const double factor);
    Matrix& REF();
    Matrix& RREF();

};

Matrix IdentityMatrix(int Diagonal);

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