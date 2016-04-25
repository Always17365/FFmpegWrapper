#include "stdafx.h"
#include "ScreenCapture.h"

CScreenCapture::CScreenCapture(void)
{
	m_dataCb = NULL;
	m_argCb = NULL;
	m_bmp = NULL;
	m_pRGB = NULL;
	m_dcMem = NULL;
	m_hOldObj = NULL;
	memset(&m_bmpInfo,0,sizeof m_bmpInfo);
	m_wdc = CreateDC("DISPLAY", NULL, NULL, NULL); 
}


CScreenCapture::~CScreenCapture(void)
{
	if(m_hOldObj&&m_dcMem)
	{
		SelectObject(m_dcMem,m_hOldObj);
		m_hOldObj = NULL;
	}
	if(m_bmp)
	{
		DeleteObject(m_bmp);
		m_bmp = NULL;
	}
	if(m_dcMem)
	{
		DeleteDC(m_dcMem);
		m_dcMem = NULL;
	}
	if(m_wdc)
	{
		DeleteDC(m_wdc);
		m_wdc = NULL;
	}
}

bool CScreenCapture::Setup( int width, int height, int nFps, int bltMode )
{
	m_nDelay = 1000/nFps;
	m_nBltMode = bltMode;

	m_bmpInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);  
	m_bmpInfo.bmiHeader.biWidth = width;  
	m_bmpInfo.bmiHeader.biHeight = height;  
	m_bmpInfo.bmiHeader.biPlanes = 1;  
	m_bmpInfo.bmiHeader.biBitCount = 24;
	m_nRGB = width * height * 3;
	m_dcMem = CreateCompatibleDC(m_wdc);
	m_bmp = CreateDIBSection(m_dcMem,&m_bmpInfo,DIB_RGB_COLORS,reinterpret_cast<VOID **>(&m_pRGB),NULL,0);
	if(bltMode)
		SetStretchBltMode(m_dcMem, COLORONCOLOR);
	m_hOldObj = SelectObject(m_dcMem, m_bmp);
	return TRUE;
}

#include <atlimage.h>
BOOL Save2File(HBITMAP bmp,LPCTSTR path)
{
	BOOL bRet = FALSE;
	CImage image;
	image.Attach(bmp);
	bRet = SUCCEEDED(image.Save(path));
	image.Detach();
	return bRet;
}

/* �ɵ�ץͼ��ʽ
HDC hdcScreen, hMemDC;   
HBITMAP hBitmap, hOldBitmap;   
//����һ����Ļ�豸������� 
hdcScreen = CreateDC("DISPLAY", NULL, NULL, NULL); 
hMemDC = CreateCompatibleDC(hdcScreen); 
//����һ������Ļ�豸����������ݡ���������ڴ��Ĵ��ڵ�����ȴ��λͼ 
hBitmap = CreateCompatibleBitmap(hdcScreen, nWidth, nHeight); 
// ����λͼѡ���ڴ��豸�������� 
hOldBitmap =(HBITMAP)SelectObject(hMemDC, hBitmap); 
// ����Ļ�豸�����������ڴ��豸�������� 
BitBlt(hMemDC, 0, 0, nWidth, nHeight, hdcScreen,rectCapture.left
	,rectCapture.top,SRCCOPY); 
//ȡ��������ڴ��Ĵ�����Ļλͼ�ľ�� 
hBitmap =(HBITMAP)SelectObject(hMemDC, hOldBitmap); 
DeleteDC(hdcScreen); 
DeleteDC(hMemDC); 
*/
void CScreenCapture::OnCapture()
{
	if(!m_pRGB||!m_dataCb)
		return;
	// ��ȡ�κ�DC�Ĵ�С
	//int iWidth = GetDeviceCaps(m_wdc,HORZRES);  
	//int iHeight = GetDeviceCaps(m_wdc,VERTRES);
	int sw = GetSystemMetrics ( SM_CXSCREEN ); 
	int sh = GetSystemMetrics ( SM_CYSCREEN ); 
	// ����붯̬����Ӧ�ֱ��ʵı仯,����WM_DISPLAYCHANGE��Ϣ. 
	BOOL bDone = m_nBltMode?BitBlt(m_dcMem, 0,0,GetWidth(), GetHeight(), m_wdc, 0, 0, SRCCOPY)
		:StretchBlt(m_dcMem, 0,0,GetWidth(),GetHeight(), m_wdc, 0, 0, sw, sh, SRCCOPY);
	if(bDone)
	{
#if JPG_DUMP
		static int nCount = 0;
		if(nCount++%15==0)
		{
			TCHAR path[MAX_PATH];
			sprintf(path, "D:\\test\\%d.jpg", nCount);
			Save2File(m_bmp, path);
		}
#endif
		if(m_dataCb)
			(*m_dataCb)(m_pRGB, GetWidth(), GetHeight(), m_argCb);
	}
}

void CScreenCapture::RlsRGB()
{
	if(m_pRGB){
		delete [] m_pRGB;
		m_pRGB = NULL;
	}
	m_nRGB = 0;
}

void CScreenCapture::SetDataCallBack( DataCallBack pCb, LPVOID arg )
{
	m_dataCb = pCb;
	m_argCb = arg;
}

DWORD CScreenCapture::CaptureThread(CMyThread* pThread)
{
	CScreenCapture* pCapture = (CScreenCapture*)pThread->GetArg();
	while(pThread->Delay(pCapture->GetDelay())){
		pCapture->OnCapture();
	}
	return 0;
}

bool CScreenCapture::Start()
{
	if(m_capThread.IsAlive()){
		printf("already start call stopFirst");
		return false;
	}
	return m_capThread.Start(&CaptureThread, this);
}

bool CScreenCapture::Stop()
{
	return m_capThread.Close();
}

bool CScreenCapture::TakeSnap( LPCTSTR bmpPath )
{
	if(!(m_nRGB&&m_pRGB))
		return false;
	//Bimap file header in order to write bmp file  
	HANDLE hFile = CreateFile(bmpPath,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);  
	if(hFile)
	{
		OnCapture();
		BITMAPFILEHEADER bmFileHeader = {0};  
		bmFileHeader.bfType = 0x4d42;  //bmp    
		bmFileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);  
		bmFileHeader.bfSize = bmFileHeader.bfOffBits + m_nRGB; ///3=(24 / 8)  

		DWORD dwWrite = 0;  
		WriteFile(hFile,&bmFileHeader,sizeof(BITMAPFILEHEADER),&dwWrite,NULL);  
		WriteFile(hFile,&m_bmpInfo.bmiHeader, sizeof(BITMAPINFOHEADER),&dwWrite,NULL);  
		WriteFile(hFile, m_pRGB, m_nRGB,&dwWrite,NULL);  
		CloseHandle(hFile); 
		return true;
	}
	return false;
}
