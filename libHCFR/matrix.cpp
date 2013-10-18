//------------------------------------
//   Implementation of Matrix Class
//------------------------------------

#include <math.h>
#include "endianness.h"
#include "matrix.h"
#include "Exceptions.h"
using namespace std;

//appends an identity matrix to the right of the calling object
//used in finding the inverse
//the user probably doesn't need access to this
Matrix& Matrix::RightAppendIdentity()
{
    Matrix temp;
    temp = Matrix(0.0, m_nRows, (2 * m_nCols));

    for(int i=0; i<m_nRows; ++i) 
    {
        for(int j=0; j<m_nCols; ++j) 
        {
            temp.m_pData[i * temp.m_nCols + j] = m_pData[i* m_nCols + j];
        }
    }
    for(int q=0; q<m_nRows; ++q) 
    {
        temp.m_pData[q * temp.m_nCols + m_nCols + q] = 1;
    }

    *this = temp;

    return *this;
}

//removes an identity matrix from the left of the calling object
//used in finding the inverse
//the user probably doesn't need access to this
Matrix& Matrix::LeftRemoveIdentity()
{
    Matrix temp;
    temp = Matrix(0.0, m_nRows, (m_nCols / 2));

    for(int i=0; i<m_nRows; ++i)
    {
        for(int j=0; j<m_nCols/2; ++j)
        {
            temp.m_pData[i * temp.m_nCols + j] = m_pData[i* m_nCols + m_nCols/2+j];
        }
    }

    *this = temp;

    return *this;
}

//standard constructor
//Data = Null; rows = columns = 0
Matrix::Matrix()
{
	m_nCols = m_nRows = 0;
}

//constructor with explicit values
//of the matrix, rows = 'Rows', columns = 'Cols'
//all spots of matrix = InitVal
Matrix::Matrix(double InitVal, int Rows, int Cols) :
    m_nRows(Rows),
    m_nCols(Cols),
    m_pData(Rows * Cols, InitVal)
{
}

//constructor with explicit values
//of the matrix, rows = 'Rows', columns = 'Cols'
//all spots of matrix are the spots of 'Data', going row-by-row
Matrix::Matrix(double* Data, int Rows, int Cols) :
    m_nRows(Rows),
    m_nCols(Cols),
    m_pData(Rows * Cols)
{
    for(int i=0; i<m_nRows; ++i)
    {
        for(int j=0; j<m_nCols; ++j) 
        {
            m_pData[i * m_nCols + j] = Data[i*m_nRows+j];
        }
    }
}

//constructor with explicit values
//of the matrix, rows = 'Rows', columns = 'Cols'
//spots in matrix correspond to the spots in the double pointer-pointer
Matrix::Matrix(double** Data, int Rows, int Cols) :
    m_nRows(Rows),
    m_nCols(Cols),
    m_pData(Rows * Cols)
{
    for(int i=0; i<m_nRows; ++i)
    {
        for(int j=0; j<m_nCols; ++j) 
        {
            m_pData[i * m_nCols + j] = Data[i][j];
        }
    }
}

//constructor with explicit values
//of matrix, rows = 'Rows', columns = 'Cols'
//all spots of the matrix are filled by calling 'Func' with the same paramaters as indexes
// ie MatrixObj[2][3] = Func(2, 3),...
Matrix::Matrix(double (* Func)(double, double), int Rows, int Cols) :
    m_nRows(Rows),
    m_nCols(Cols),
    m_pData(Rows * Cols)
{
    for(int i=0; i<m_nRows; ++i)
    {
        for(int j=0; j<m_nCols; ++j) 
        {
            m_pData[i * m_nCols + j] = Func(i, j);
        }
    }
}

Matrix::Matrix(ifstream &theFile) :
    m_nRows(0),
    m_nCols(0)
{
    this->readFromFile(theFile);
}

void Matrix::readFromFile(ifstream &theFile)
{
    uint32_t header1, header2, version;
    uint32_t nbColumns, nbRows;

    // les détrompeurs
    theFile.read((char*)&header1, 4);
    header1 = littleEndianUint32ToHost(header1);
    theFile.read((char*)&header2, 4);
    header2 = littleEndianUint32ToHost(header2);

    if (header1 != 0x434d6174 || header2 != 0x72697843)
    {
        cout << "LibHCFR : CHCFile::readFile : matrix header not recognised, maybe a shift ..." << endl;
        throw Exception("Invalid file format");
    }      

    // le numéro de version de la classe Matrix (c'est toujours 1)
    theFile.read((char*)&version, 4);
    version = littleEndianUint32ToHost(version);

    // la taille de la matrix
    theFile.read((char*)&nbColumns, 4);
    m_nCols = littleEndianUint32ToHost(nbColumns);
    theFile.read((char*)&nbRows, 4);
    m_nRows = littleEndianUint32ToHost(nbRows);

    // on crée l'espace de stockage  
    m_pData.resize(m_nRows * m_nCols);

    // puis on le remplie
    for (int columnIndex = 0; columnIndex < m_nCols; columnIndex++)
    {
        for (int rowIndex = 0; rowIndex < m_nRows; rowIndex++)
        {
            double value;
            theFile.read((char*)&value, 8);
            m_pData[rowIndex * m_nCols + columnIndex] = littleEndianDoubleToHost(value);
        }
    }
}

//destructor
Matrix::~Matrix()
{
}

//add two matrices
Matrix Matrix::operator +(const Matrix& obj) const
{
    Matrix temp(*this);
    temp += obj;
    return temp;
}

//subtract two matrices
Matrix Matrix::operator -(const Matrix& obj) const
{
    Matrix temp(*this);
    temp -= obj;
    return temp;
}

//multiply two matrices
Matrix Matrix::operator *(const Matrix& obj) const
{
    if(m_nCols != obj.m_nRows)
    {
        cout << "Matrix exception : Mismatched matrices in multiplication" << endl;
        throw new MatrixException("Mismatched matrices in multiplication");
    }

    Matrix temp(0.0, m_nRows, obj.m_nCols);

    for(int i=0; i<temp.m_nRows; ++i) 
    {
        for(int j=0; j<temp.m_nCols; ++j) 
        {
            double sum(0);
            for(int q=0; q<m_nCols; ++q) 
            {
                double prod = m_pData[i * m_nCols + q] * obj.m_pData[q * obj.m_nCols + j];
                sum += prod;
            }
            temp.m_pData[i * temp.m_nCols + j] = sum;
        }
    }

    return temp;
}

//multiply a matrix by a double scalar
Matrix Matrix::operator *(const double _d) const
{
    Matrix temp(*this);
    temp *= _d;
    return temp;
}

//divide a matrix by another matrix
Matrix Matrix::operator /(const Matrix& obj) const
{
    return(*this * obj.GetInverse());
}

//divide a matrix by a double scalar
Matrix Matrix::operator /(const double _d) const
{
    Matrix temp(*this);
    temp /= _d;
    return temp;
}

//add two matrices and assign
Matrix& Matrix::operator +=(const Matrix& obj)
{
    if((m_nRows != obj.m_nRows) || (m_nCols != obj.m_nCols))
    {
        cout << "Matrix exception : Mismatched matrices in addition" << endl;
        throw new MatrixException("Mismatched matrices in addition");
    }

    for(int i=0; i< m_nRows * m_nCols; ++i)
    {
        m_pData[i] += obj.m_pData[i];
    }
    return *this;
}

//subtract two matrices and assign
Matrix& Matrix::operator -=(const Matrix& obj)
{
    if((m_nRows != obj.m_nRows) || (m_nCols != obj.m_nCols))
    {
        cout << "Matrix exception : Mismatched matrices in subtraction" << endl;
        throw new MatrixException("Mismatched matrices in subtraction");
    }

    for(int i=0; i< m_nRows * m_nCols; ++i)
    {
        m_pData[i] -= obj.m_pData[i];
    }
    return *this;
}

//multiply two matrices and assign
Matrix& Matrix::operator *=(const Matrix& obj)
{
    return(*this = *this * obj);
}

//multiply a matrix by a double scalar and assign
Matrix& Matrix::operator *=(const double _d)
{
    for(int i=0; i< m_nRows * m_nCols; ++i)
    {
        m_pData[i] *= _d;
    }
    return *this;
}

//divide a matrix by a matrix and assign
Matrix& Matrix::operator /=(const Matrix& obj)
{
	return(*this = *this / obj);
}

//divide a matrix by a double scalar and assign
Matrix& Matrix::operator /=(const double _d)
{
    for(int i=0; i< m_nRows * m_nCols; ++i)
    {
        m_pData[i] /= _d;
    }
    return *this;
}

//inversion operator
//returns the inverse of the calling matrix
Matrix Matrix::operator ~() const
{
    return GetInverse();
}

//equality operator
bool Matrix::operator ==(const Matrix& obj) const
{
    if(m_nRows != obj.m_nRows) return false;
    if(m_nCols != obj.m_nCols) return false;

    for(int i=0; i<m_nRows * m_nCols; ++i) 
    {
        if(m_pData[i]!= obj.m_pData[i])
        {
            return false;
        }
    }
    return true;
}

//inequality operator
bool Matrix::operator !=(const Matrix& obj) const
{
    return !(operator==(obj));
}

//indexing operator
//remember, a zero (0) element DOES exist in this matrix, although not in a real matrix
double* Matrix::operator [](const int _i)
{
    if((_i >= m_nRows) || (_i < 0))
    {
        cout << "Matrix exception : index " << _i << " out of bounds in [] operator" << endl;
        throw new MatrixException("Out of bound index");
    }

    return &m_pData[_i * m_nCols];
}

//another indexing operator, but takes both rows and columns
//remember, a zero (0) element DOES exist in this matrix, although not in a real matrix
double& Matrix::operator ()(const int _i, const int _j)
{
    if((_i >= m_nRows) || (_j >= m_nCols))
    {
        cout << "Matrix exception : index " << _i << " * " << _j << " out of bounds in () operator" << endl;
        throw new MatrixException("Out of bound index");
    }
    if((_i < 0) || (_j < 0))
    {
        cout << "Matrix exception : index " << _i << " * " << _j << " out of bounds in () operator" << endl;
        throw new MatrixException("Out of bound index");
    }

    return m_pData[_i * m_nCols + _j];
}

//another indexing operator, but takes both rows and columns
//remember, a zero (0) element DOES exist in this matrix, although not in a real matrix
const double& Matrix::operator ()(const int _i, const int _j) const
{
    if((_i >= m_nRows) || (_j >= m_nCols))
    {
        cout << "Matrix exception : index " << _i << " * " << _j << " out of bounds in () operator" << endl;
        throw new MatrixException("Out of bound index");
    }
    if((_i < 0) || (_j < 0))
    {
        cout << "Matrix exception : index " << _i << " * " << _j << " out of bounds in () operator" << endl;
        throw new MatrixException("Out of bound index");
    }

    return m_pData[_i * m_nCols + _j];
}

//returns true if the calling matrix is an identity matrix
bool Matrix::IsIdentity() const
{
    if(m_nCols != m_nRows) return false;

    for(int i=0; i<m_nRows; ++i) 
    {
        for(int j=0; j<m_nCols; ++j) 
        {
            if(i == j && m_pData[i * m_nCols + j] != 1.0)
            {
                return false;
            }
            if(i != j && m_pData[i * m_nCols + j] != 0.0) 
            {
                return false;
            }
        }
    }
    return true;
}

//returns true if every element in the calling matrix is zero (0)
bool Matrix::IsEmpty() const
{
    for(int i=0; i<m_nCols; ++i) 
    {
        for(int j=0; j<m_nRows; ++j) 
        {
            if(m_pData[i * m_nCols + j] != 0.0) 
            {
                return false;
            }
        }
    }
    return true;
}

//finds determinant of the matrix
double Matrix::Determinant() const
{
    if(m_nRows != m_nCols)
    {
        cout << "Matrix exception : determinant on a non-square matrix" << endl;
        throw new MatrixException("Determinant on a non-square matrix");
    }
  
    double sum = 0;

    if(m_nRows == 2) 
    {
        return((m_pData[0 * m_nCols + 0] * m_pData[1 * m_nCols + 1]) - (m_pData[1 * m_nCols + 0] * m_pData[0 * m_nCols + 1]));
    }

	for(int q=0; q<m_nCols; ++q) {
		Matrix NewMinor = GetMinor(0, q);
		sum += (pow(-1.0, (double)q)*(m_pData[0 * m_nCols + q]*NewMinor.Determinant()));
	}

	return sum;
}

//returns number of rows of the matrix
int Matrix::GetRows() const
{
	return m_nRows;
}

//returns number of columns of the matrix
int Matrix::GetColumns() const
{
	return m_nCols;
}

//returns the inverse of the calling matrix
Matrix Matrix::GetInverse() const
{
    if(m_nRows != m_nCols)
    {
        cout << "Matrix exception : getInverse on a non-square matrix" << endl;
        throw new MatrixException("GetInverse : not a square matrix");
    }

    Matrix temp(*this);
    temp.RightAppendIdentity();
    temp.RREF();
    temp.LeftRemoveIdentity();

    return temp;
}

//turns the calling matrix into its inverse
Matrix& Matrix::Invert()
{
	*this = this->GetInverse();

	return *this;
}

//add 'SourceRow' by scale of 'factor' (default is one) to 'DestRow'
Matrix& Matrix::AddRows(const int SourceRow, const int DestRow, const double factor)
{
    for(int j=0; j<m_nCols; ++j) 
    {
        m_pData[DestRow * m_nCols + j] += (m_pData[SourceRow * m_nCols + j] * factor);
    }

    return *this;
}

Matrix& Matrix::DivideRow(const int Row, const double _d)
{
    if(_d == 0)
    {
        cout << "Matrix exception : DivideRow : division by zero" << endl;
        throw new MatrixException("DivideRow : division by zero");
    }
    for(int j=0; j<m_nCols; ++j) 
    {
        m_pData[Row * m_nCols + j] /= _d;
    }

    return *this;
}

//puts the matrix into row-echelon form
Matrix& Matrix::REF()
{
    for(int i=0; i<m_nRows; ++i)
    {
        for(int j=i+1; j<m_nRows; ++j) 
        {
            AddRows(i, j, -m_pData[j * m_nCols + i]/m_pData[i * m_nCols + i]);
        }
        DivideRow(i, m_pData[i * m_nCols + i]);
    }

    return *this;
}

//puts the matrix into reduced row-echelon form
Matrix& Matrix::RREF()
{
    REF();

    for(int i=m_nRows-1; i>=0; --i) 
    {
        for(int j=i-1; j>=0; --j) 
        {
            AddRows(i, j, -m_pData[j * m_nCols + i]/m_pData[i * m_nCols + i]);
        }
        DivideRow(i, m_pData[i * m_nCols + i]);
    }

    return *this;
}


//returns minor around spot Row,Col
Matrix Matrix::GetMinor(const int RowSpot, const int ColSpot) const
{
    Matrix temp(0.0, m_nRows-1, m_nCols-1);

    for(int i=0, k=0; i<m_nRows;)
    {
        if(i == RowSpot) 
        {
            ++i;
            continue;
        }
        for(int j=0, l=0; j<m_nCols; )
        {
            if(j == ColSpot) 
            {
                ++j;
                continue;
            }
            temp.m_pData[k * temp.m_nCols + l] = m_pData[i * m_nCols + j];
            ++l;
            ++j;
        }
        ++i;
        ++k;
    }
    return temp;
}

//returns the transposition of the calling matrix
Matrix Matrix::GetTransposed() const
{
    Matrix temp(0.0, m_nCols, m_nRows);

    for(int i=0; i<m_nRows; ++i) 
    {
        for(int j=0; j<m_nCols; ++j) 
        {
            temp.m_pData[j * temp.m_nCols + i] = m_pData[i * m_nCols + j];
        }
    }

    return temp;
}

//transposes the calling matrix
Matrix& Matrix::Transpose()
{
	*this = this->GetTransposed();

	return *this;
}


//CMAC = Concatenate Matrix As Columns
//concatenates matrix 'obj' on to the bottom of the calling matrix
Matrix& Matrix::CMAC(const Matrix& obj)
{
    if(m_nRows != obj.m_nRows)
    {
        cout << "Matrix exception : Mismatched matrices in column concatenation" << endl;
        throw new MatrixException("Mismatched matrices in column concatenation");
    }

    Matrix temp(0.0, m_nRows, (m_nCols + obj.m_nCols));

    for(int i=0; i<m_nRows; ++i)
    {
        for(int j=0; j<m_nCols; ++j) 
        {
            temp.m_pData[i * temp.m_nCols + j] = m_pData[i * m_nCols + j];
        }
        for(int j=0; j<obj.m_nCols; ++j)
        {
            temp.m_pData[i * temp.m_nCols + m_nCols + j] = obj.m_pData[i * obj.m_nCols + j];
        }
    }

    *this = temp;

    return *this;
}

//a way to display the matrix
//formatted
void Matrix::Display() const
{
    for(int i=0; i<m_nRows; ++i) 
    {
        for(int j=0; j<m_nCols; ++j) 
        {
            cout.width(15);
            cout << m_pData[i * m_nCols + j];
        }
        cout << endl;
    }
}

//another way to display the matrix
//can output to a stream other than cout, which is the default
//formatted
void Matrix::Output(ostream& ostr) const
{
    ostr << "[";
    for(int i=0; i<m_nRows; ++i) 
    {
        for(int j=0; j<m_nCols; ++j) 
        {
            ostr << "[" << m_pData[i + m_nCols + j] << "]";
        }
        if(m_nRows-i-1)
        {
            cout << "\n ";
        }
    }
    ostr << "]\n";
}

//input from a file
void Matrix::Read(ifstream& istr)
{
    char str[6];

    istr >> str >> m_nRows;                     //"Rows: "
    istr >> str >> m_nCols;                     //"Cols: "
    istr >> str;                                //"Data\n"
    m_pData.resize(m_nRows * m_nCols);
    for(int i=0; i<m_nRows; ++i)
    {
        for(int j=0; j<m_nCols; ++j)
        {
            istr >> m_pData[i * m_nCols + j];
        }
    }
}

//output to a file
void Matrix::Write(ofstream& ostr) const
{
    ostr << "Rows: " << m_nRows << '\n';
    ostr << "Cols: " << m_nCols << '\n';
    ostr << "Data:\n";
    for(int i=0; i<m_nRows; ++i)
    {
        for(int j=0; j<m_nCols; ++j)
        {
            ostr << m_pData[i * m_nCols + j] << '\n';
        }
    }
}

//returns an identity matrix of size Diagonal
Matrix Matrix::IdentityMatrix(int Diagonal)
{
    Matrix temp(0.0, Diagonal, Diagonal);

    for(int q=0; q<Diagonal; ++q)
    {
        temp.m_pData[q * temp.m_nCols + q] = 1.0;
    }

    return temp;
}

//uses Matrix::IdentityMatrix to get an identity matrix of size Diagonal
Matrix IdentityMatrix(int Diagonal)
{
    return Matrix::IdentityMatrix(Diagonal);
}

//easily display the matrix, usually to the console
//formatted output
ostream& operator <<(ostream& ostr, const Matrix& obj)
{
    obj.Output(ostr);
    return ostr;
}

#ifdef LIBHCFR_HAS_MFC
void Matrix::Serialize(CArchive& archive)
{
    // Fx: be aware that matrices are serialized by column, row not row, column as usual 
    //		(needed to preserve compatibility with old matrix class :( )

    if (archive.IsStoring())
    {
        // writing the matrix
        // write the object header first so we can correctly recognise the object type "CMatrixC"
        long	header1 = 0x434d6174 ;
        long	header2 = 0x72697843 ;
        int		version = 1 ;						// serialization version format number

        archive << header1 ;
        archive << header2 ;
        archive << version ;						// version number of object type

        // now write out the actual matrix
        archive << GetColumns() ;
        archive << GetRows() ;
        // this could be done with a archive.Write(m_pData, sizeof(double) * m_NumColumns * m_NumRows)
        // for efficiency (dont forget its a flat array). Not done here for clarity
        for (int i = 0 ; i < m_nCols ; ++i)
        {
            for (int j = 0 ; j < m_nRows ; ++j)
            {
                archive << m_pData[j * m_nCols + i] ;
            }
        }
        // done!
    }
    else
    {
        // reading the matrix
        // read the object header first so we can correctly recognise the object type "CMatrixC"
        long	header1 = 0 ;
        long	header2 = 0 ;
        int		version = 0 ;				// serialization version format

        archive >> header1 ;
        archive >> header2 ;
        if (header1 != 0x434d6174 || header2 != 0x72697843)
        {
        // incorrect header, cannot load it
            AfxThrowArchiveException(CArchiveException::badClass, NULL) ;
        }
        archive >> version ;				// version number of object type
        ASSERT(version == 1) ;				// only file format number so far

        // now write out the actual matrix
        int		nCols ;
        int		nRows ;
        double	value ;
        archive >> nCols ;
        archive >> nRows ;
        Matrix loading(0.0, nRows, nCols) ;
        for (int i = 0 ; i < nCols ; ++i)
        {
            for (int j = 0 ; j < nRows ; ++j)
            {
                archive >> value ;
                loading(j,i)=value;
            }
        }
        *this = loading ;					// copy the data into ourselves
        // done!
    }
}
#endif //#ifdef LIBHCFR_HAS_MFC
