#include "screen_shot.h"
#include <stdio.h>
#include <windows.h>
#include <gdiplus.h>
#include <time.h>

#pragma comment(lib, "Gdiplus")

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	using namespace Gdiplus;
	UINT  num = 0;
	UINT  size = 0;

	ImageCodecInfo* pImageCodecInfo = NULL;

	GetImageEncodersSize(&num, &size);
	if (size == 0)
		return -1;

	pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
	if (pImageCodecInfo == NULL)
		return -1;

	GetImageEncoders(num, size, pImageCodecInfo);
	for (UINT j = 0; j < num; ++j)
	{
		if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;
		}
	}
	free(pImageCodecInfo);
	return 0;
}

void gdiscreen() {
	using namespace Gdiplus;
	IStream* istream;
	HRESULT res = CreateStreamOnHGlobal(NULL, true, &istream);
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
	{
		HDC scrdc, memdc;
		HBITMAP membit;
		scrdc = ::GetDC(0);
		int Height = GetSystemMetrics(SM_CYSCREEN);
		int Width = GetSystemMetrics(SM_CXSCREEN);
		memdc = CreateCompatibleDC(scrdc);
		membit = CreateCompatibleBitmap(scrdc, Width, Height);
		HBITMAP hOldBitmap = (HBITMAP)SelectObject(memdc, membit);
		BitBlt(memdc, 0, 0, Width, Height, scrdc, 0, 0, SRCCOPY);

		Gdiplus::Bitmap bitmap(membit, NULL);
		CLSID clsid;
		GetEncoderClsid(L"image/jpeg", &clsid);
		bitmap.Save(L"screen.jpeg", &clsid, NULL); // To save the jpeg to a file
		//bitmap.Save(istream, &clsid, NULL);

		// Create a bitmap from the stream and save it to make sure the stream has the image
//		Gdiplus::Bitmap bmp(istream, NULL);
//		bmp.Save(L"t1est.jpeg", &clsid, NULL);             
		// END

		delete& clsid;
		DeleteObject(memdc);
		DeleteObject(membit);
		::ReleaseDC(0, scrdc);
	}
	GdiplusShutdown(gdiplusToken);
}

