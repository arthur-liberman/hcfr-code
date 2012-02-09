/*
This file was created by Paul Barvinko (pbarvinko@yahoo.com).
This file is distributed "as is", e.g. there are no warranties 
and obligations and you could use it in your applications on your
own risk. 
Your comments and questions are welcome.
If using in your applications, please mention author in credits for your app.
*/
#include "../../stdafx.h"
#include "f2doff.h"

#define DWordAlign(x) ((x+3)&~3)

FOffScreen::FOffScreen(int cx, int cy)
{
 Width=cx; Height=cy;
 // create BITMAPINFO structure
 fbmi=(BITMAPINFO*)new char[sizeof(BITMAPINFOHEADER)];
 fbmi->bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
 fbmi->bmiHeader.biWidth=cx;
 fbmi->bmiHeader.biHeight=cy;
 fbmi->bmiHeader.biPlanes=1;
// fbmi->bmiHeader.biBitCount=16;
 fbmi->bmiHeader.biBitCount=24;		// FX mod
 fbmi->bmiHeader.biCompression=0;
 fbmi->bmiHeader.biSizeImage=0;
 fbmi->bmiHeader.biXPelsPerMeter=0;
 fbmi->bmiHeader.biYPelsPerMeter=0;
 fbmi->bmiHeader.biClrUsed=0;
 fbmi->bmiHeader.biClrImportant=0;

 Dib=NULL;
 DibCDC=NULL;

 CreateDib();
}

FOffScreen::~FOffScreen()
{
 KillDib();
 delete fbmi;
}

void FOffScreen::KillDib()
{
 if (Dib!=NULL)
 {
  ::SelectObject(DibDC, oldDibBMP);
  ::DeleteObject(DibBMP);
  ::DeleteDC(DibDC);
  delete DibCDC;
  DibCDC=NULL;
  Dib=NULL;
 };
}

void FOffScreen::CreateDib(BOOL clear, char ch)
{
 KillDib();

 HDC sdc=::GetDC(0);
 DibDC=::CreateCompatibleDC(sdc);
 ::ReleaseDC(0,sdc);
 LPVOID lp;
 DibBMP=::CreateDIBSection(DibDC,fbmi,DIB_PAL_COLORS,&lp,NULL,NULL);
 DWORD dw = GetLastError();
 Dib=(BYTE*)lp;
 if (Dib==NULL)
 {
//    ASSERT(FALSE);
//  AfxMessageBox(TEXT("There is no memory for offscreens!"));
    ::DeleteDC(DibDC);
    DibDC = NULL;
  return;
 };
 oldDibBMP=(HBITMAP)::SelectObject(DibDC, DibBMP);
 DibCDC=new CDC();
 DibCDC->Attach(DibDC);
 if (clear) FillBits(ch);
 IsReady=FALSE;
}

void FOffScreen::ResizeDib(int cx, int cy, BOOL clear, char ch)
{
 if (cx!=Width || cy!=Height)
 {
  Width=cx; Height=cy;
  fbmi->bmiHeader.biWidth=cx;
  fbmi->bmiHeader.biHeight=cy;
  CreateDib(clear, ch);
 } else
 {
  if (clear) FillBits(ch);
 };
}

void FOffScreen::FillBits(char ch)
{
 if (Dib!=NULL)
  memset(Dib, ch, Height*DWordAlign(Width));
}

void FOffScreen::SetBits(LPVOID src, int count)
{
 ASSERT(count<=Height*DWordAlign(Width));
 LPVOID lp=(LPVOID)Dib;
 count>>=2;
 _asm
 {
  push ds;
  pop  es;
  mov  edi, dword ptr [lp];
  mov  esi, dword ptr [src];
  cld;
  mov ecx, count;
  rep movsd;
 };
/*
 if (count>Height*DWordAlign(Width)) return;
 memcpy((LPVOID)Dib, src, count);
*/
}

HDC FOffScreen::GetDibDC()
{
 if (Dib!=NULL) return DibDC; else return 0;
 return 0;
}

CDC* FOffScreen::GetDibCDC()
{
 if (Dib!=NULL) return DibCDC; else return NULL;
 return NULL;
}

void FOffScreen::DrawDib(HDC dc, CRect r)
{
 if (Dib!=NULL)
 {
  SetDIBitsToDevice(dc,r.left,r.top,r.Width(),
  r.Height(),r.left,fbmi->bmiHeader.biHeight-r.bottom ,0 ,fbmi->bmiHeader.biHeight , Dib,
    fbmi,DIB_RGB_COLORS);
 };
}

void FOffScreen::SetMemoryPoint(int x, int y, BYTE value)
{
 if (Dib==NULL) return;
 if (x<0 || y<0 || x>=Width || y>=Height) return;
 Dib[(Height-y-1)*DWordAlign(Width)+x]=value;
}

void FOffScreen::DrawCross(int x, int y, int size, BYTE color)
{
 if (Dib==NULL) return;
 int i;
 for (i=y-size; i<=y+size; i++) SetMemoryPoint(x, i, color);
 for (i=x-size; i<=x+size; i++) SetMemoryPoint(i, y, color);
}

void FOffScreen::ShiftDibData(int cx, int cy)
{
 if (Dib==NULL) return;
 LPVOID lp1, lp2;
 if (cx!=0)
 { // Shift data with x coord
  if (abs(cx)<Width)
  {
   for (int i=0; i<Height; i++)
   {
    lp1=(LPVOID)((BYTE*)Dib+i*DWordAlign(Width));
    lp2=(LPVOID)((BYTE*)Dib+i*DWordAlign(Width)+abs(cx));
    if (cx<0) memcpy(lp2, lp1, Width-abs(cx));
     else memcpy(lp1, lp2, Width-abs(cx));
   };
  };
 };
 if (cy!=0)
 { // Shift data with x coord
  if (abs(cy)<Height)
  {
   if (cy<0)
   { // shift data down
    for (int i=0; i<Height-abs(cy); i++)
    {
     lp2=(LPVOID)((BYTE*)Dib+i*DWordAlign(Width));
     lp1=(LPVOID)((BYTE*)Dib+(i+abs(cy))*DWordAlign(Width));
     memcpy(lp2, lp1, Width);
    };
   } else
   {
    for (int i=0; i<Height-abs(cy); i++)
    {
     lp2=(LPVOID)((BYTE*)Dib+(Height-i-1)*DWordAlign(Width));
     lp1=(LPVOID)((BYTE*)Dib+(Height-i-abs(cy)-1)*DWordAlign(Width));
     memcpy(lp2, lp1, Width);
    };
   };
  };
 };
}
