#if !defined(BITMAPTOOLS_H_INCLUDED_)
#define BITMAPTOOLS_H_INCLUDED_

extern void DrawTransparentBitmap(HDC hdc, HBITMAP hBitmap, short xStart, short yStart, COLORREF cTransparentColor);
extern HBITMAP ScaleBitmap( HDC hDC, HBITMAP hBmp, WORD wNewWidth, WORD wNewHeight, BOOL fastScale );
extern HBITMAP ShrinkBitmap (HBITMAP a,WORD bx,WORD by);


#endif