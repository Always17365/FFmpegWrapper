#include "JitterNode.h"

///////////////////////////////////////
//RTP_JitterBuffer::JitterNode
///////////////////////////////////////
JitterNode::JitterNode(unsigned minJitterDelay,
                             unsigned maxJitterDelay)
{

  // Jitter buffer is a queue of frames waiting for playback, a list of
  // free frames, and a couple of place holders for the frame that is
  // currently beeing read from the RTP transport or written to the codec.
  // ������Jitter ��Buffer ��һ��˫��Ķ��нṹ�����ڻ�����������ݡ�
  
  oldestFrame = newestFrame = currentWriteFrame = NULL;
  
  // Calculate the maximum amount of timestamp units for the jitter buffer
  minJitterTime = minJitterDelay;
  maxJitterTime = maxJitterDelay;
  currentJitterTime = minJitterDelay;
  targetJitterTime = currentJitterTime;

  // Calculate number of frames to allocate, we make the assumption that the
  // smallest packet we can possibly get is 5ms long (assuming audio 8kHz unit).
  bufferSize = maxJitterTime/5+1;	//maxJitterTime����ȷ��bufferSize

  // Nothing in the buffer so far
  currentDepth = 0;
  packetsTooLate = 0;
  bufferOverruns = 0;
  consecutiveBufferOverruns = 0;
  maxConsecutiveMarkerBits = 10;
  consecutiveMarkerBits = 0;
  consecutiveEarlyPacketStartTime = 0;
  doJitterReductionImmediately = FALSE;
  doneFreeTrash = FALSE;

  lastWriteTimestamp = 0;
  lastWriteTick = 0;
  jitterCalc = 0;
  jitterCalcPacketCount = 0;

  preBuffering = TRUE;
  doneFirstWrite = FALSE;

  // Allocate the frames and put them all into the free list
  freeFrames = new Entry;
  freeFrames->next = freeFrames->prev = NULL;

  for (PINDEX i = 0; i < bufferSize; i++) {
    Entry * frame = new Entry;
    frame->prev = NULL;
    frame->next = freeFrames;
    freeFrames->prev = frame;
    freeFrames = frame;
  }

  PTRACE(2, "RTP\tJitter buffer created:"
            " size=" << bufferSize <<
            " delay=" << minJitterTime << '-' << maxJitterTime << '/' << currentJitterTime <<
            " obj=" << this);

#if PTRACING && !defined(NO_ANALYSER)
  analyser = new RTP_JitterBufferAnalyser;
#else
  analyser = NULL;
#endif
}
	
JitterNode::~JitterNode()
{
  bufferMutex.Wait();

  // Free up all the memory allocated
  //���������һ����Ԫ
  while (oldestFrame != NULL) {
    Entry * frame = oldestFrame;
    oldestFrame = oldestFrame->next;
    delete frame;
  }

  while (freeFrames != NULL) {
    Entry * frame = freeFrames;
    freeFrames = freeFrames->next;
    delete frame;
  }
  //��Ԫ
  delete currentWriteFrame;

  bufferMutex.Signal();

#if PTRACING && !defined(NO_ANALYSER)
  PTRACE(5, "Jitter buffer analysis: size=" << bufferSize
         << " time=" << currentJitterTime << '\n' << *analyser);
  delete analyser;
#endif
}
	  
BOOL JitterNode::ReadData(DWORD timestamp,RTP_DataFrame & frame)
{

  // Ĭ�Ϸ���һ����֡(�羲��)
  frame.SetPayloadSize(0);

  PWaitAndSignal mutex(bufferMutex);

  //�ͷ�д����������֡(��:�����Ż�free list),
  //Ȼ�������־(parking spot)
  if (currentWriteFrame != NULL) 
  {
    // Move frame from current to free list
    currentWriteFrame->next = freeFrames;
    if (freeFrames != NULL)
      freeFrames->prev = currentWriteFrame;
    freeFrames = currentWriteFrame;

    currentWriteFrame = NULL;
  }

  /*
    ȡ����һ��д�����������һ֡,�Ӷ�����ɵ�λ����һ������,�����ʱ���������Ļ�
	and parks it in the special member,���Բ�����Mutex,��Ϊ������д���߳�Ҳ���Լ�
	��Ӧ��(ʹ��Buffer,��������)
   */
  if (oldestFrame == NULL) 
  {
    /*No data to play! We ran the buffer down to empty, restart buffer by
      setting flag that will fill it again before returning any data.
     */
	//û�����ݶ�
    preBuffering = TRUE;
    currentJitterTime = targetJitterTime;
    
	
#if PTRACING && !defined(NO_ANALYSER)
    analyser->Out(0, currentDepth, "Empty");
#endif
    return TRUE;
  }
 
  DWORD oldestTimestamp = oldestFrame->GetTimestamp();
  DWORD newestTimestamp = newestFrame->GetTimestamp();

  /*
  ����л���(due to silence in the buffer)����Jitter Buffer��С 
  Take it 
  */

  if (targetJitterTime < currentJitterTime &&
      (newestTimestamp - oldestTimestamp) < currentJitterTime) 
  {
	//����currentJitterTime,ʹ������Ϊ�����������ֵ
	//<<<<<<
    currentJitterTime = max( targetJitterTime > (newestTimestamp - oldestTimestamp));
	//<<<<<<
    PTRACE(3, "RTP\tJitter buffer size decreased to " << currentJitterTime);
  }

  /* ���oldestFrame����Ҫ�Ļ���,��ʹ����;����ȷ��д�߳������(CPU�ܵò�����)
     
	 �����Ƿ��Ѿ����Զ�������ݰ�:
	 1. �����ɵ�frame������Ҫ�������(timestamp)��Ҫ��,��ʹ����
	 2. ���������ʱ��,ȷ��д�߳�û�����
	 3. ���Jitter�е���ɺ�����֡���ʱ����,����󻺴�Ļ���,������η�����
	 �ɵ�֡,ʹWrite�̸߳���
  */

  if (preBuffering) //preBuffering���ɶ������и�ֵ
  {
    // Reset jitter baseline��׼��
	// (should be handled by GetMarker() condition, but just in case...)
    lastWriteTimestamp = 0;
    lastWriteTick = 0;

    // Check for requesting something that already exceeds the maximum time,
    // or have filled the jitter buffer, not filling if this is so

    // �����ɵ�֡��û��BUFF�д�����ʱ��,�򲻷����κζ���
    if ((PTimer::Tick() - oldestFrame->tick) < currentJitterTime / 2) //�����ɵ�֡��û�б��ӳ��㹻ʱ��
	{
#if PTRACING && !defined(NO_ANALYSER)
      analyser->Out(oldestTimestamp, currentDepth, "PreBuf");
#endif
      return TRUE;
    }

    preBuffering = FALSE;
  }


  //Handle short silence bursts(����, ը��) in the middle of the buffer
  // - if we think we're getting marker bit information, use that
  BOOL shortSilence = FALSE;
  if (consecutiveMarkerBits < maxConsecutiveMarkerBits) 
  {  
	  //����˵���Ǿ�����ĵ�һ��Ԫ��,�����ӳ�һ����ʱ�䣬��Ϸ�ſ���
     if( oldestFrame->GetMarker() && (PTimer::Tick() - oldestFrame->tick) < currentJitterTime/2 )
        shortSilence = TRUE;
  }
  else if (timestamp<oldestTimestamp && timestamp>(newestTimestamp-currentJitterTime))
    shortSilence = TRUE; //�����ݰ��ѱ�����
  /*
  ===============================================
  ^	    ^										^
  |tsp  |oldestFrame							|newestFrame
  */
  if (shortSilence) 
  {
    // It is not yet time for something in the buffer
#if PTRACING && !defined(NO_ANALYSER)
    analyser->Out(oldestTimestamp, currentDepth, "Wait");
#endif
    lastWriteTimestamp = 0;
    lastWriteTick = 0;
    return TRUE;
  }

  // Detatch oldest packet from the list, put into parking space
  currentDepth--;
#if PTRACING && !defined(NO_ANALYSER)
  analyser->Out(oldestTimestamp,currentDepth,timestamp>=oldestTimestamp?"":"Late");
#endif
  currentWriteFrame = oldestFrame;
  oldestFrame = currentWriteFrame->next;
  currentWriteFrame->next = NULL;
 
  // Calculate the jitter contribution(����) of this frame

  // �Ծ�����ĵ�һ����������ʱ�Ӷ�������
  if (currentWriteFrame->GetMarker())
  { 
    lastWriteTimestamp = 0;
    lastWriteTick = 0;
  }

  if (lastWriteTimestamp != 0 && lastWriteTick !=0) 
  { 
	//��GetMarker�Ĵ���
    int thisJitter = 0;

    if (currentWriteFrame->GetTimestamp() < lastWriteTimestamp) 
	  {
      //Not too sure how to handle this situation...
      thisJitter = 0;
    }
    else if (currentWriteFrame->tick < lastWriteTick) 
	  {
      //Not too sure how to handle this situation either!
      thisJitter = 0;
    }
    else 
	  { //thisJitterӦ����(���紫���)ʱ�Ӷ���
      thisJitter = (currentWriteFrame->tick - lastWriteTick) +
                   lastWriteTimestamp - currentWriteFrame->GetTimestamp();
    }

    if (thisJitter < 0)//����ֵ 
		thisJitter *=(-1);
    thisJitter *=2; //currentJitterTime needs to be at least TWICE the maximum jitter

    if (thisJitter > (int) currentJitterTime * LOWER_JITTER_MAX_PCNT / 100) 
	{ 
	//ʱ�Ӷ����Ƚϴ�(ֹͣ˵����,����ֹͣ��˵��Ӧ����GetMark��,Ҫ�����Ƕ���)
	//<<<<<<
      targetJitterTime = currentJitterTime;//�������¸�ֵ������С
      PTRACE(3, "RTP\tJitter buffer target realigned to current jitter buffer");
      consecutiveEarlyPacketStartTime = PTimer::Tick();//����һ��ʱ�����
      jitterCalcPacketCount = 0;			//����ֵΪ0
      jitterCalc = 0;
	//<<<<<<
    }
    else 
	{ //���ʱ�Ӷ����ȵ�ǰС
      if (thisJitter > (int) jitterCalc)
        jitterCalc = thisJitter; //ȡ��� jitterCalc=MAX(jitterCalc,thisJitter)

      jitterCalcPacketCount++;   //���������ĸ���

      //�����������ǳ������õ�targetJitterTime���� ,�͵�����
	  //����ǰ������,�ⲻ����ʹ��targetJitterTime����currentJitterTime
      if (thisJitter > (int) targetJitterTime * LOWER_JITTER_MAX_PCNT / 100) 
	  { //targetJitterTime���(Ϊ��һ������׼��)
	  //<<<<<<
        targetJitterTime = thisJitter * 100 / LOWER_JITTER_MAX_PCNT;
        PTRACE(3, "RTP\tJitter buffer target size increased to " << targetJitterTime);
	  //<<<<<<
      }

    }
  }

  lastWriteTimestamp = currentWriteFrame->GetTimestamp();
  lastWriteTick = currentWriteFrame->tick;


  if (oldestFrame == NULL)
    newestFrame = NULL;
  else 
  {
    oldestFrame->prev = NULL;

    // If exceeded current jitter buffer time delay: 
    if ((newestTimestamp - currentWriteFrame->GetTimestamp()) > currentJitterTime) 
	{
      PTRACE(4, "RTP\tJitter buffer length exceeded");
      consecutiveEarlyPacketStartTime = PTimer::Tick();
      jitterCalcPacketCount = 0;
      jitterCalc = 0;
      lastWriteTimestamp = 0;
      lastWriteTick = 0;
      
      // If we haven't yet written a frame, we get one free overrun
      if (!doneFirstWrite) //������ǻ�û��д���κ�һ֡
	  { 
        PTRACE(4, "RTP\tJitter buffer length exceed was prior to first write. Not increasing buffer size");
        while ((newestTimestamp - currentWriteFrame->GetTimestamp()) > currentJitterTime) 
		{//wastedFrame�˷�֡(Ӧ�ó�ȥ��֡) 
          Entry * wastedFrame = currentWriteFrame;
          currentWriteFrame = oldestFrame;
          oldestFrame = oldestFrame->next;
          currentDepth--;

          currentWriteFrame->next = NULL; 
		  //current WriteFrame should never be able to be NULL
          
          wastedFrame->next = freeFrames;
          if (freeFrames != NULL)
            freeFrames->prev = wastedFrame;
          freeFrames = wastedFrame;

          if (oldestFrame == NULL) 
		  {
            newestFrame = NULL;
            break;
          }

          oldestFrame->prev = NULL;
        }
        
        doneFirstWrite = TRUE;
        frame = *currentWriteFrame;
        return TRUE;
      }


      // See if exceeded maximum jitter buffer time delay, waste them if so
	  // ����(���ǻ������)
      while ((newestFrame->GetTimestamp() - currentWriteFrame->GetTimestamp()) > maxJitterTime) 
	  {
        PTRACE(4, "RTP\tJitter buffer oldest packet ("
               << oldestFrame->GetTimestamp() << " < "
               << (newestTimestamp - maxJitterTime)
               << ") too late, throwing away");
		//<<<<<<
          currentJitterTime = maxJitterTime;
		//<<<<<<
          //Throw away the oldest frame and move everything up
          Entry * wastedFrame = currentWriteFrame;	//�����ͳ���һ��
          currentWriteFrame = oldestFrame;			//���ƶ�����һ��
          oldestFrame = oldestFrame->next;			//oldestFrame�ټ���һ��
          currentDepth--;

          currentWriteFrame->next = NULL; 
		  //currentWriteFrame should never be able to be NULL
          
          wastedFrame->next = freeFrames;
          if (freeFrames != NULL)
            freeFrames->prev = wastedFrame;
          freeFrames = wastedFrame;

          if (oldestFrame == NULL) 
		  {
            newestFrame = NULL;
            break;
          }

      }

	// Now change the jitter time to cope with the new size
    // unless already set to maxJitterTime
	//<<<<<<
      if (newestTimestamp - currentWriteFrame->GetTimestamp() > currentJitterTime) 
          currentJitterTime = newestTimestamp - currentWriteFrame->GetTimestamp();

      targetJitterTime = currentJitterTime;
	//<<<<<<
      PTRACE(3, "RTP\tJitter buffer size increased to " << currentJitterTime);
    }
  }

  //<<<<<<
  if ((PTimer::Tick() - consecutiveEarlyPacketStartTime) > DECREASE_JITTER_PERIOD &&
       jitterCalcPacketCount >= DECREASE_JITTER_MIN_PACKETS)
  {//��ʱ����targetJitterTime
    jitterCalc = jitterCalc * 100 / LOWER_JITTER_MAX_PCNT;
    if (jitterCalc < targetJitterTime / 2) 
		jitterCalc = targetJitterTime / 2;
    if (jitterCalc < minJitterTime) 
		jitterCalc = minJitterTime;
    targetJitterTime = jitterCalc;//<<<<<<
    PTRACE(3, "RTP\tJitter buffer target size decreased to " << targetJitterTime);
    jitterCalc = 0;
    jitterCalcPacketCount = 0;
    consecutiveEarlyPacketStartTime = PTimer::Tick();
  }
  //<<<<<<

  /* If using immediate jitter reduction (rather than waiting for silence opportunities)
  then trash oldest frames as necessary to reduce the size of the jitter buffer */
  if (targetJitterTime < currentJitterTime &&
      doJitterReductionImmediately && newestFrame != NULL) 
  {
    while ((newestFrame->GetTimestamp() - currentWriteFrame->GetTimestamp()) > targetJitterTime)
	{
      // Throw away the newest entries
      Entry * wastedFrame = newestFrame;
      newestFrame = newestFrame->prev;
      if (newestFrame != NULL)
          newestFrame->next = NULL;
      wastedFrame->prev = NULL;

      // Put thrown away frame on free list
      wastedFrame->next = freeFrames;
      if (freeFrames != NULL)
        freeFrames->prev = wastedFrame;
      freeFrames = wastedFrame;

      // Reset jitter calculation baseline
      lastWriteTimestamp = 0;
      lastWriteTick = 0;

      currentDepth--;
      if (newestFrame == NULL) 
      {
          oldestFrame = NULL;
          break;
      }
    }

    currentJitterTime = targetJitterTime;//<<<<<<
    PTRACE(3, "RTP\tJitter buffer size decreased to " << currentJitterTime );

  }

  doneFirstWrite = TRUE;
  frame = *currentWriteFrame;
  return true;
}

void JitterNode::SetDelay(unsigned int minJitterDelay, unsigned int maxJitterDelay)
{
  bufferMutex.Wait();

  minJitterTime = minJitterDelay;
  maxJitterTime = maxJitterDelay;
  currentJitterTime = minJitterDelay;
  targetJitterTime = currentJitterTime;

  PINDEX newBufferSize = maxJitterTime/5+1;
  while (bufferSize < newBufferSize) 
  {
    Entry * frame = new Entry;
    frame->prev = NULL;
    frame->next = freeFrames;
    freeFrames->prev = frame;
    freeFrames = frame;
    bufferSize++;
  }

  packetsTooLate = 0;
  bufferOverruns = 0;
  consecutiveBufferOverruns = 0;
  consecutiveMarkerBits = 0;
  consecutiveEarlyPacketStartTime = 0;

  preBuffering = TRUE;

  PTRACE(2, "RTP\tJitter buffer restarted:"
              " size=" << bufferSize <<
              " delay=" << minJitterTime << '-' << maxJitterTime << '/' << currentJitterTime);

  bufferMutex.Signal();
}

BOOL JitterNode::WriteData(RTP_DataFrame& frame)
{
	PWaitAndSignal mutex(bufferMutex);
    BOOL markerWarning = FALSE;

//<<==��currentReadFrame�Ĺ��̣��ȵ�freeFrames�ң�Ȼ��oldestFrame��

    // Get the next free frame available for use for reading from the RTP
    // transport. Place it into a parking spot.
    Entry * currentReadFrame;
    if (freeFrames != NULL) 
	{
      // Take the next free frame and make it the current for reading
      currentReadFrame = freeFrames;
      freeFrames = freeFrames->next;
      if (freeFrames != NULL)
        freeFrames->prev = NULL;//˫���ѭ������

      PTRACE_IF(2, consecutiveBufferOverruns > 1,
                "RTP\tJitter buffer full, threw away "
                << consecutiveBufferOverruns << " oldest frames");
      consecutiveBufferOverruns = 0;
    }
    else 
	{
      // We have a full jitter buffer, need a new frame so take the oldest one
      currentReadFrame = oldestFrame;
      oldestFrame = oldestFrame->next;
      if (oldestFrame != NULL)
        oldestFrame->prev = NULL;

      currentDepth--;	//currentDepth��ָoldestFrame list�Ĵ�С
      bufferOverruns++; //����Overrun
      consecutiveBufferOverruns++;
	//<<==
      if (consecutiveBufferOverruns > MAX_BUFFER_OVERRUNS) 
	  {
        PTRACE(2, "RTP\tJitter buffer continuously(������)full, throwing away entire buffer.");
        freeFrames = oldestFrame;
        oldestFrame = newestFrame = NULL;
        preBuffering = TRUE;//����Ԥ����
      }
      else 
	  {
        PTRACE_IF(2, consecutiveBufferOverruns == 1,
                  "RTP\tJitter buffer full, throwing away oldest frame ("
                  << currentReadFrame->GetTimestamp() << ')');
      }
	//<<==
    }
//<<==END ��currentReadFrame
	//�ҵ�currentReadFrame
	*currentReadFrame=frame;
    currentReadFrame->next = NULL;
	currentReadFrame->tick = PTimer::Tick();
	
	//�����Ա�־
    if (consecutiveMarkerBits < maxConsecutiveMarkerBits) 
	{
      if (currentReadFrame->GetMarker()) 
	  {
        PTRACE(3, "RTP\tReceived start of talk burst: " << currentReadFrame->GetTimestamp());
        //preBuffering = TRUE;
        consecutiveMarkerBits++;
      }
      else
        consecutiveMarkerBits = 0;
    }
    else 
	{ //consecutiveMarkerBits>=maxConsecutiveMarkerBits
      if (currentReadFrame->GetMarker())
        currentReadFrame->SetMarker(FALSE);
      if (!markerWarning && (consecutiveMarkerBits == maxConsecutiveMarkerBits)) 
	  {
        markerWarning = TRUE;
        PTRACE(3, "RTP\tEvery packet has Marker bit, ignoring them from this client!");
      }
    }
    
    
#if PTRACING && !defined(NO_ANALYSER)
	//currentDepth��ǰoldest Frame�ĳ��� =>=����Ԥ����
    analyser->In(currentReadFrame->GetTimestamp(), currentDepth, preBuffering ? "PreBuf" : "");
#endif

    // Queue the frame for playing by the thread at other end of jitter buffe

    // Have been reading a frame, put it into the queue now, at correct position
    if (newestFrame == NULL)
      oldestFrame = newestFrame = currentReadFrame; // Was empty
    else 
	{
      DWORD time = currentReadFrame->GetTimestamp();
		/*											
		============================================
		^											^
		|oldestFrame								|newestFrame
		*/
      if (time > newestFrame->GetTimestamp()) {
        // Is newer than newst, put at that end of queue
        currentReadFrame->prev = newestFrame;
        newestFrame->next = currentReadFrame;
        newestFrame = currentReadFrame;
      }
      else if (time <= oldestFrame->GetTimestamp()) {
        // Is older than the oldest, put at that end of queue
        currentReadFrame->next = oldestFrame;
        oldestFrame->prev = currentReadFrame;
        oldestFrame = currentReadFrame;
      }
      else {
        // Somewhere in between, locate its position
        Entry * frame = newestFrame->prev;
        while (time < frame->GetTimestamp())
          frame = frame->prev;

        currentReadFrame->prev = frame;
        currentReadFrame->next = frame->next;
        frame->next->prev = currentReadFrame;
        frame->next = currentReadFrame;
      }
    }

  currentDepth++;
  return true;
}
