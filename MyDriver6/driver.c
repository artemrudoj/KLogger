#include "ntddk.h"
#include "kLogger.h"
#define TEST_TIMERS_NUMBER 1
static PKTIMER timer_test[TEST_TIMERS_NUMBER];
static PRKDPC dpc_obj[TEST_TIMERS_NUMBER];
kLogger *logger;

VOID timer_dpc_test_routine(
	_In_     PKDPC	Dpc,
	_In_opt_ PVOID  DeferredContext,
	_In_opt_ PVOID  SystemArgument1,
	_In_opt_ PVOID  SystemArgument2);


VOID KloggerTestUnload(PDRIVER_OBJECT DriverObject);

void stop_klogger_test()
{
	DbgPrint("KLogger: stop test\n");

	if (!dpc_obj[0] || !timer_test[0]) {
		DbgPrint("KLogger: start was failed\n");
		return;
	}

	KeFlushQueuedDpcs();

	for (unsigned i = 0; i < TEST_TIMERS_NUMBER; i++) {
		KeCancelTimer(timer_test[i]);
		freeMemory(timer_test[i]);
		freeMemory(dpc_obj[i]);
	}

	DbgPrint("KLogger: stop test complete\n");
}


VOID KloggerTestUnload( PDRIVER_OBJECT DriverObject)
{
	DbgPrint("KLogger: start DriverUnload\n");
	destroyKLogger(logger);
	stop_klogger_test();
	DbgPrint("KLogger: DriverUnload completed\n");
}

NTSTATUS  DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pusRegPath) {
	NTSTATUS Status;
	HANDLE hEvent;
	OBJECT_ATTRIBUTES oa;
	UNICODE_STRING us;
	PVOID pEvent;
	
	pDriverObject->DriverUnload = KloggerTestUnload;

	logger = initLogger(1024);

	if (!logger) {
		DbgPrint("KLogger: Driver Enrty - error creating klogger\n");
		return STATUS_FAILED_DRIVER_ENTRY;
	}

	LARGE_INTEGER timeout;
	LONG period;

	DbgPrint("KLogger: start test\n");

	for (unsigned i = 0; i < TEST_TIMERS_NUMBER; i++) {
		timer_test[i] = allocateMemory(sizeof(KTIMER));
		if (!timer_test[i]) {
			for (unsigned j = 0; j < i; j++) {
				freeMemory(timer_test[j]);
				timer_test[j] = NULL;
			}
			DbgPrint("KLogger: start test - error allocate memory for test timer\n");
			return;
		}
	}

	for (unsigned i = 0; i < TEST_TIMERS_NUMBER; i++) {
		dpc_obj[i] = allocateMemory(sizeof(KDPC));
		if (!dpc_obj[i]) {
			for (unsigned j = 0; j < i; j++) {
				freeMemory(dpc_obj[j]);
				dpc_obj[j] = NULL;
			}
			for (unsigned j = 0; j < TEST_TIMERS_NUMBER; j++) {
				freeMemory(timer_test[j]);
				timer_test[j] = NULL;
			}
			DbgPrint("KLogger: start test - error allocate memory for dpc object\n");
			return;
		}
	}

	timeout.QuadPart = -1000000LL;	// 100ms, because time in 100ns format
	period = 1000; // 1s, because time in 1ms format

	for (unsigned i = 0; i < TEST_TIMERS_NUMBER; i++) {
		KeInitializeTimer(timer_test[i]);
		KeInitializeDpc(dpc_obj[i], timer_dpc_test_routine, klog);
		KeSetTimerEx(timer_test[i], timeout, period, dpc_obj[i]);
	}

	DbgPrint("KLogger: start test completed\n");

	return STATUS_SUCCESS;
}

VOID timer_dpc_test_routine(
	_In_     PKDPC	Dpc,
	_In_opt_ PVOID  DeferredContext,
	_In_opt_ PVOID  SystemArgument1,
	_In_opt_ PVOID  SystemArgument2)
{
	static unsigned counter = 0;

	if (counter % 2) {
		KIRQL cur_irql;
		static const char* msg = "message from high level\r\n";


		KeRaiseIrql(HIGH_LEVEL, &cur_irql);
		klog(logger, msg);
		DbgPrint("KLogger: test write high");
		KeLowerIrql(cur_irql);
	}
	else {
		static const char* msg = "message from dpc level\r\n";
		size_t msg_size = strlen(msg);
		size_t ret_size;

		klog(logger, msg);
		DbgPrint("KLogger: test write dpc");
	}

	counter++;
}