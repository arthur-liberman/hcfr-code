/*
This file was created by Paul Barvinko (pbarvinko@yahoo.com).
This file is distributed "as is", e.g. there are no warranties 
and obligations and you could use it in your applications on your
own risk. 
Your comments and questions are welcome.
If using in your applications, please mention author in credits for your app.
*/
#ifndef _F2DOFF_H_
#define _F2DOFF_H_

class FOffScreen
{
    public:
	FOffScreen(int cx, int cy);
	~FOffScreen();
	// access to members
	BYTE* GetBits(){return Dib;};
	int GetWidth(){return Width;};
	int GetHeight(){return Height;};
	// DC from offscreens
	HDC GetDibDC();
	CDC* GetDibCDC();
	//Draw capabilities
	void DrawDib(HDC dc, CRect r);
	void SetMemoryPoint(int x, int y, BYTE value);
	void DrawCross(int x, int y, int size, BYTE color);

	// resize options
	void ResizeDib(int cx, int cy, BOOL clear=TRUE, char ch=0);

	// Fill options
	void FillBits(char ch=0);
	void SetBits(LPVOID src, int count);

	//misc
	void ShiftDibData(int cx, int cy);
	BOOL IsDIBReady(){return IsReady;};
	void SetReady(BOOL Flag){IsReady=Flag;};
    protected:
	BYTE* Dib;
	int Width;
	int Height;
	BITMAPINFO* fbmi;
	HDC DibDC;
	CDC *DibCDC;
	HBITMAP DibBMP, oldDibBMP;
	BOOL IsReady;

	void CreateDib(BOOL clear=TRUE, char ch=0);
	void KillDib();
};

#endif
