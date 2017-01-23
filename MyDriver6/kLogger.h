#include <stdlib.h>
#include <ntddk.h>
#include "RingBuffer.h"

typedef struct _kLogger
{
	RingBuffer* ringBuffer;
	PKTHREAD thread_flush;
	KEVENT event_flush;
	PKTIMER timer_flush;
	BOOLEAN stop_working;
} kLogger;

kLogger* initLogger(int size);
void destroyKLogger(kLogger* klogger);
void klog(kLogger* loger, char* buffer);
void destroyThreadFlush(kLogger* klogger);