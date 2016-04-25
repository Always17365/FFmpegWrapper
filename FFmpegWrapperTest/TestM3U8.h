#pragma once
#include "ScreenCapture.h"
#include "Soundin.h"
#include "../FFmpegWrapper/AutoLock.h"
#include <vector>
/// ��Ƭ��
struct SegItem
{
	/// URL·��
	TCHAR url[MAX_PATH];
	/// ����ʱ��
	float duration;
	/// �������һ���л������Ƿ�仯
	bool bChange;
};

/// M3U8�ļ��и���
class CTestM3U8
{
public:
	CTestM3U8(void);
	~CTestM3U8(void);

	/// ��Ƶ�ɼ��豸
	CSoundIn audio;
	/// ��Ļ��׽�豸
	CScreenCapture video;
	/// ��ǰTS�ļ�
	FFmpegEncoder* m_ptsMux;

	/// ����M3U8�ļ���һ֡Tick
	DWORD m_dwFistTick;
	/// ��һ��Ƭʱ��
	DWORD m_dwLastSeg;
	/// hls�汾��
	int m_hlsVer;
	/// ��Ƭ����ʱ��
	int m_nSegTime;
	/// �����Ƭ��[�������ɳ������޵�M3U8�ļ�]
	int m_nMaxSeg;
	/// ֱ��ģ��FALSE�����β���
	bool m_bLive;
	/// URLǰ׺
	std::string m_szUrlPrefix;
	/// ��ǰƬ��������0����
	int m_nSegIdx;
	/// �ļ�����������·��
	std::string m_szFileName;
	/// ���ش洢Ŀ¼
	std::string m_szFileDir;
	/// ��Ƭ�б�
	std::vector<SegItem> m_vecSegs;
	/// �߼���
	Mutex m_lock;
	/// �����Ƭ������������
	bool AddSegMent(DWORD tick);
	/// ����M3U8�����ļ�
	bool updateIndex();
	int nFrame;
public:
	static void AudioCallBack(BYTE* pcm, int length, LPVOID arg);
	static void VideoCallBack(BYTE* pRgb, int width, int height, LPVOID arg);
	/*
	������Ƭ����
	@param nSegTime ÿ����Ƭ��С
	@param nMaxSeg ��Ƭ����
	@param bLive �Ƿ�ʵʱģʽ
	@return bool ����true
	*/
	bool SetSegment(int nSegTime, int nMaxSeg, bool bLive);
	/*
	����
	@param vParam ��Ƶ����
	@param aParam ��Ƶ����
	@param path ���ش��M3U8·��
	@param preUrl ���緢��URL,��Ϊ��
	@param bSource �Ƿ����ɼ�
	@return bool �ɹ��򷵻�true
	*/
	bool Start(FFmpegVideoParam& vParam, FFmpegAudioParam& aParam, 
		LPCTSTR path, LPCTSTR preUrl=NULL, bool bSource=true);
	/// �رղɼ���M3U8�ļ�
	bool Close();

	/*
	���벢д����Ƶ
	@param pcm PCM����
	@param length ����
	*/
	void OnAudio( BYTE* pcm, int length );
	/*
	���벢д����Ƶ
	@param pRgb RGB���� 
	@param length ����
	*/
	void OnVideo( BYTE* pRgb, int length );
};

