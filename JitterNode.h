#ifndef __RTP_JITTER_NODE_H___ 
#define __RTP_JITTER_NODE_H___ 

#include "rtp.h"


class Entry : public RTP_DataFrame
{
  public:
    Entry * next;
    Entry * prev;
    PTimeInterval tick;
	operator =(RTP_DataFrame& frame)
	{
		*(RTP_DataFrame*)this=frame;
	}
};

class RTP_JitterBufferAnalyser;
/****************************************
�Լ��޸ĵĶ���������,��Ҫ���ڷ���������Ƶ����,�ṩ�ķ�����
�ڲ�ʵ����Ҫ�������������,�����ݶ�ȡ��д����ٶ�ƽ����Ƶ���Ķ���
����Ӧ�Ķ�����������
�̰߳�ȫ��WriteData\ReadData�ӿ�
Ŀǰֱֻ��һ��ReadData��ȡ����
����:�����������ת����ƵӦ��������ӳ�Ϊ����,�����ټ��붶�������������ӳ�
���,�����޸ļ򵥵Ĵ洢�ṹ,����֧��1��д���ζ���,�Խ�ʡ�ڴ�ʹ��
****************************************/
class JitterNode
{
public:

	JitterNode(unsigned minJitterDelay,unsigned maxJitterDelay);
	virtual ~JitterNode();

	void SetDelay(unsigned minJitterDelay, unsigned maxJitterDelay);

	virtual BOOL WriteData(
		RTP_DataFrame& frame
	);

	virtual BOOL ReadData(
	  DWORD timestamp,        /// Timestamp to read from buffer.
	  RTP_DataFrame & frame   /// Frame read from the RTP session
	);
	
	void UseImmediateReduction(BOOL state) { doJitterReductionImmediately = state; }

	/**Get current delay for jitter buffer.//��ǰJitter Buffer���ӳ�
	   */
	DWORD GetJitterTime() const { return currentJitterTime; }

	/**Get total number received packets too late to go into jitter buffer.
	  */
	DWORD GetPacketsTooLate() const { return packetsTooLate; }

	/**Get total number received packets that overran the jitter buffer.
	  */
	DWORD GetBufferOverruns() const { return bufferOverruns; }

	/**Get maximum consecutive marker bits before buffer starts to ignore them.
	  */
	DWORD GetMaxConsecutiveMarkerBits() const { return maxConsecutiveMarkerBits; }

	/**Set maximum consecutive marker bits before buffer starts to ignore them.
	  */
	void SetMaxConsecutiveMarkerBits(DWORD max) { maxConsecutiveMarkerBits = max; }
protected:
	PINDEX        bufferSize;
	DWORD         minJitterTime;
	DWORD         maxJitterTime;
	DWORD         maxConsecutiveMarkerBits;

	unsigned currentDepth;		//oldestFrame����ĳ���
	DWORD    currentJitterTime; //current delay for jitter buffer
	DWORD    packetsTooLate;
	unsigned bufferOverruns;
	unsigned consecutiveBufferOverruns;
	DWORD    consecutiveMarkerBits;
	PTimeInterval    consecutiveEarlyPacketStartTime;
	DWORD    lastWriteTimestamp;
	PTimeInterval lastWriteTick;
	DWORD    jitterCalc;
	DWORD    targetJitterTime;
	unsigned jitterCalcPacketCount;
	BOOL     doJitterReductionImmediately;
	BOOL     doneFreeTrash;

	Entry * oldestFrame;
	Entry * newestFrame;
	Entry * freeFrames;
	Entry * currentWriteFrame;

	PMutex bufferMutex;
	BOOL   preBuffering;
	BOOL   doneFirstWrite;

	RTP_JitterBufferAnalyser * analyser;
};

#endif