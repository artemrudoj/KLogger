#pragma once
#define bool int
#define true 1
#define false 0
#define KERNEL

#include <Wdm.h>
#include <Ntddk.h>

#ifdef KERNEL
#define PRINT(msg, ...) DbgPrint(msg, __VA_ARGS__)
#else
#define PRINT(msg, ...) printf(msg, __VA_ARGS__)
#endif
typedef struct _Locker {
	KSPIN_LOCK mutex;
	KIRQL current_irql;
} Locker;

typedef struct _FileHandler {
	HANDLE hFile;
} FileHandler;

typedef struct _RingBuffer {
	char* start;
	char* bufferForFlushingThread;
	int  size;
	char* pointerToFirstNoFlushedByte;
	char* pointerToNextToWriteByte;
	char  countOfNonFlushedBytes;
	Locker locker;
	FileHandler  filehandler;
} RingBuffer;

typedef struct _ShouldWrite {
	int toEndBytes;
	int fromBegginningBytes;
} ShouldWrite;



typedef struct _LogEntry {
	bool isReady;
	int size;
}LogEntry;

int sizeOfLogEntryHeader();


bool initFileHandler(FileHandler *locker);
bool initLocker(Locker *locker);
RingBuffer* initRingBuffer(int size);
void destroyLocker(Locker *locker);
void destroyFileHandler(FileHandler *locker);
void destroyRingBuffer(RingBuffer *ringBuffer);
void loging(RingBuffer *ringBuffer, char* string);
void flush(RingBuffer *ringBuffer);
int  getSizeOfString(char *string);
bool lockForLog(RingBuffer *ringBuffer);
bool unlockForLog(RingBuffer *ringBuffer);
void* tryToReservPointers(RingBuffer * ringBuffer, int size, ShouldWrite *sizes);
void saveToBuffer(RingBuffer *ringBuffer, char* strat, char *string, ShouldWrite *sizes);
void* prepareBufferForFlush(RingBuffer *ringBuffer, int *sumLength);
bool shouldFlush(RingBuffer *ringBuffer);
void notifyForFlush(RingBuffer *ringBuffer);
bool writeToFile(FileHandler *handler, int* start, int length);
int calculateCurrentFreeSpace(RingBuffer *buffer);
void* allocateMemory(int size);
void freeMemory(void *pointer);
bool isEnoughtForHeader(RingBuffer *buffer, char* pointer);