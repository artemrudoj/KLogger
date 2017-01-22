#include "kLogger.h"
VOID flush_routine(PVOID context);
void init_events(kLogger* klog);
void init_thread_flush(kLogger* logger);

kLogger* initLogger(int size) {
	kLogger* logger = (kLogger *)allocateMemory(sizeof(kLogger *));

	if (!logger) {
		DbgPrint("KLogger: init error - allocate memory for klogger\n");
		return NULL;
	}

	logger->ringBuffer = initRingBuffer(size);
	if (!logger->ringBuffer) {
		DbgPrint("KLogger: init error - create ring buffer\n");
		goto err_rbcreate;
	}

	logger->stop_working = FALSE;

	init_events(logger);

	init_thread_flush(logger);
	if (!logger->thread_flush) {
		DbgPrint("KLogger: init error - create thread\n");
		goto err_openfile;
	}

	return (void*)logger;

err_openfile:
	destroyRingBuffer(logger->ringBuffer);
err_rbcreate:
	freeMemory(logger);
	return NULL;


}
void klog(kLogger* loger, char* buffer) {

	if (loger->stop_working)
		return ;
	loging(loger->ringBuffer, buffer);
}



void init_thread_flush(kLogger* logger)
{
	HANDLE flushing_thread = NULL;
	NTSTATUS status;

	if (!logger)
		return;

	status = PsCreateSystemThread(
		&flushing_thread,
		THREAD_ALL_ACCESS,
		NULL,
		NULL,
		NULL,
		flush_routine,
		(PVOID)logger);

	if (!NT_SUCCESS(status)) {
		DbgPrint("KLogger: flush thread - error creating\n");
		return;
	}

	ObReferenceObjectByHandle(
		flushing_thread,
		THREAD_ALL_ACCESS,
		NULL,
		KernelMode,
		(PVOID *)&logger->thread_flush,
		NULL);

	ZwClose(flushing_thread);
}

VOID flush_routine(PVOID context)
{
	kLogger* klog = (kLogger*)context;
	PVOID handles[1];
	LARGE_INTEGER timeout;

	DbgPrint("KLogger: flust thread - enter routine\n");

	handles[0] = (PVOID)&klog->event_flush;
	timeout.QuadPart = -100000000LL; // 10 sec, because time in 100ns format

	while (!klog->stop_working) {
		NTSTATUS status = KeWaitForMultipleObjects(
			1,
			handles,
			WaitAny,
			Executive,
			KernelMode,
			TRUE,
			&timeout,
			NULL);

		if (status == STATUS_TIMEOUT)
			DbgPrint("KLogger: flust thread - timer event\n");
		flush(klog->ringBuffer);


		KeClearEvent(&klog->event_flush);
	}

	DbgPrint("KLogger: flust thread - exit routine\n");

	PsTerminateSystemThread(STATUS_SUCCESS);
}

void init_events(kLogger* klog)
{
	if (!klog)
		return;

	KeInitializeEvent(&klog->event_flush, NotificationEvent, FALSE);
}

void event_callback(void* context)
{
	kLogger* klog;

	if (!context)
		return;

	DbgPrint("KLogger: callback handled\n");
	klog = (kLogger *)context;
	KeSetEvent(&klog->event_flush, 0, FALSE);
}

void destroy_events(kLogger* klog)
{
	ZwClose(&klog->event_flush);
}
