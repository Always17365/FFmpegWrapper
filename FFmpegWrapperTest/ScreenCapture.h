#pragma once
#include "mythread.h"

/*
���ݻص�����
@param pRgb RGB����
@param width ���
@param height �߶�
@param arg ����
*/
typedef void (*DataCallBack)(BYTE* pRgb, int width, int height, LPVOID arg);

/*
��Ļ��׽��
ʹ�÷�ʽ��
---
CScreenCapture sCap;
sCap.SetDataCallBack
sCap.Setup
sCap.Start
---
DataCallBack(rgb,widht, height);
---
sCap.Stop
*/
class CScreenCapture
{
public:
	CScreenCapture(void);
	~CScreenCapture(void);

	/*
	���òɼ�����
	@param width ���
	@param height �߶�
	@param nFps ֡��
	@param bltMode ����ģʽ 1 ���죬0 BitBlt
	@return BOOL �ɹ��򷵻�true
	*/
	bool Setup(int width, int height, int nFps, int bltMode);
	/*
	�������ݻص�
	@param pCb �ص�����
	@param arg ����
	*/
	void SetDataCallBack(DataCallBack pCb, LPVOID arg);
	/// ���زɼ��ӳ�
	int GetDelay(){return m_nDelay;}
	/// ��ʼ��׽
	bool Start();
	/// ֹͣ��׽
	bool Stop();

	/*
	ץͼ���������BMP
	CScreenCapture video;
	video.Setup(width, height, nFps, 1);
	video.TakeSnap("D:\\abc.bmp");
	@param bmpPath bmp·��
	@return bool �ɹ�����true
	*/
	bool TakeSnap(LPCTSTR bmpPath);
	/// ��ȡĿ�Ŀ��
	int GetWidth(){return m_bmpInfo.bmiHeader.biWidth;}
	/// ��ȡĿ�ĸ߶�
	int GetHeight(){return m_bmpInfo.bmiHeader.biHeight;}
	/// ��׽�߳�
	static DWORD CaptureThread(CMyThread* pThread);
protected:
	void RlsRGB();
	void OnCapture();
	BITMAPINFO m_bmpInfo;
	/// �ڴ�Bitmap
	HBITMAP m_bmp;
	/// �ɵ�BMP����
	HGDIOBJ m_hOldObj;
	/// �ڴ�DC
	HDC m_dcMem;
	/// ��ĻDc
	HDC m_wdc;
	/// ÿ֡�ӳ� 1000/fps
	int m_nDelay;
	/// ����ģʽ
	int m_nBltMode;
	/// �ɼ��߳�
	CMyThread m_capThread;
	/// RGB������
	BYTE* m_pRGB;
	/// RGB��������С
	int m_nRGB;

	/// ���ݻص�����
	DataCallBack m_dataCb;
	/// �ص�����
	LPVOID m_argCb;
};

