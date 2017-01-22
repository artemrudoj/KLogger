#include <stdlib.h>
#include <ntddk.h>
#include "RingBuffer.h"

typedef struct _kLogger
{
	RingBuffer* ringBuffer;
	HANDLE file_handle;
	void* buffer_flush;
	PKTHREAD thread_flush;
	KEVENT event_flush;
	PKTIMER timer_flush;
	BOOLEAN stop_working;
} kLogger;

kLogger* initLogger(int size);
void klog(kLogger* loger, char* buffer);