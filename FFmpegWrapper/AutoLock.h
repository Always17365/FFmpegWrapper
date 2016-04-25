#include <Windows.h>
/// �ٽ���
class Mutex
{	
	// Constructor
public:
	/// ���캯��
	Mutex()
	{ ::InitializeCriticalSection(&m_sect); }

	// Attributes
public:
	/// ת�ͺ���
	operator CRITICAL_SECTION*()
	{ return (CRITICAL_SECTION*) &m_sect; }
	CRITICAL_SECTION m_sect;	///< �ٽ���

	// Operations
public:
	/// ����
	BOOL Unlock()
	{ ::LeaveCriticalSection(&m_sect); return TRUE; }
	/// ����
	BOOL Lock()
	{ ::EnterCriticalSection(&m_sect); return TRUE; }
	/// ����
	BOOL Lock(DWORD dwTimeout)
	{ return Lock(); }

	// Implementation
	/// ��������
	virtual ~Mutex()
	{ ::DeleteCriticalSection(&m_sect); }
};
/// �Զ��ӽ���
class AutoLock
{
public:
	/// ���캯�� �Զ�����
	AutoLock(Mutex& s):sec(&s)
	{
		sec && sec->Lock();	
	}
	AutoLock(Mutex* s):sec(s)
	{
		sec && sec->Lock();	
	}
	/// �������� �Զ�����
	~AutoLock()
	{
		sec && sec->Unlock();
	}
protected:
	Mutex* sec;	///< ������
};