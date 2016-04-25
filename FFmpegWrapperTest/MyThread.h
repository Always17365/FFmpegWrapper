#pragma once

class CMyThread;
/// �̴߳�����(д��������ֹ�ô�)
typedef DWORD (*ThreadFunc)(CMyThread* pThread);

/// �̷߳�װ��
class CMyThread
{
public:
	CMyThread(void);
	~CMyThread(void);

	/*
	�����߳�
	@param func �̴߳����� 
	@param arg ����ָ��
	@return BOOL �ɹ������򷵻�TRUE
	*/
	BOOL Start(ThreadFunc func, LPVOID arg);
	/*
	�ر��̣߳����ȴ��˳�
	@param timeOut �ȴ���ʱ�����ڳ�ʱʱ���ڻ�δ�Ƴ���ǿ���˳��߳�
	*/
	BOOL Close(int timeOut=INFINITE);
	/// ÿ��ѭ������delayʱ��
	BOOL WaitTimeOut(int delay);
	/*
	�ȴ�ĳ���¼��Ĳ���
	@param hEvent �¼�
	@param timeOut �ȴ�ʱ��
	@return int 
	@retval 0 �߳��˳�
	@retval 1 �¼�����
	@retval -1 �ȴ���ʱ
	*/
	int WaitEvent(HANDLE hEvent, int timeOut=INFINITE);
	/// ����delay���м������
	BOOL Delay(int delay);
	/// ���ز���
	LPVOID GetArg(){return m_pArg;}
	/*
	�ж��߳��Ƿ����
	@param timeOut ���Ϊ0�����ж��߳��Ƿ���ڣ�
	�������Ӧ�ò���жϣ�������m_dwLastTime��timeOutֵ���ж��߳��Ƿ񻹻��ţ����Ƕ���ĳ���ط�
	@return BOOL �����򷵻�TRUE������FALSE
	*/
	BOOL IsAlive(int timeOut=0);
protected:
	// ������start�����arg
	LPVOID m_pArg;
	/// ����dealy��WaitTimeOut����
	DWORD m_dwLastTime;
	/// �߳�ID
	DWORD m_dwThreadId;
	/// �߳̾��
	HANDLE m_hThread;
	/// �˳��¼�(����Delay��WaitTimeOut��Close���ж��˳�)
	HANDLE m_hStopEvent;
};

