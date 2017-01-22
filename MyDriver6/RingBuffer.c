#include "RingBuffer.h"
#include <stdlib.h>
#include <Wdm.h>

#define LOG_FILE_NAME  L"log.txt"


RingBuffer* initRingBuffer(int size) {
	RingBuffer* MainRingBuffer = (RingBuffer *)allocateMemory(sizeof(RingBuffer));
	if (MainRingBuffer == NULL) {
		PRINT("MainRingBuffer = NULL");
		return false;
	}
	MainRingBuffer->size = size;
	MainRingBuffer->start = allocateMemory(size);
	if (MainRingBuffer->start == NULL) {
		PRINT("MainRingBuffer->start = NULL");
		goto free_buf;
	}
	MainRingBuffer->pointerToFirstNoFlushedByte = MainRingBuffer->start;
	MainRingBuffer->pointerToNextToWriteByte = MainRingBuffer->start;

	if (!initFileHandler(&MainRingBuffer->filehandler)) {
		PRINT("initFileHandler incorrect");
		goto free_buf;
	}
	if (!initLocker(&MainRingBuffer->locker)) {
		PRINT("initLocker incorrect");
		goto free_buf;
	}
	MainRingBuffer->bufferForFlushingThread = allocateMemory(2*size);
	if (MainRingBuffer->bufferForFlushingThread == NULL) {
		PRINT("MainRingBuffer->start = NULL");
		goto free_buf;
	}
	
	return MainRingBuffer;
free_buf:
	freeMemory(MainRingBuffer->start);
free_main_buf:
	freeMemory(MainRingBuffer);
	return NULL;
}

void loging(RingBuffer *ringBuffer, char * string) {
	lockForLog(ringBuffer);
	int stringLength = getSizeOfString(string);
	ShouldWrite sizes;
	char *startAddress = tryToReservPointers(ringBuffer, stringLength, &sizes);
	while (startAddress == NULL) {
		startAddress = tryToReservPointers(ringBuffer, stringLength, &sizes);
	}
	LogEntry* logEntry = (LogEntry* )startAddress;
	
	logEntry->isReady = false;
	logEntry->size = stringLength;
	
	unlockForLog(ringBuffer);
	saveToBuffer(ringBuffer, startAddress, string, &sizes);
	logEntry->isReady = true;
	
	if (shouldFlush(ringBuffer)) {
		notifyForFlush(ringBuffer);
	}
}

void flush(RingBuffer *ringBuffer) {
	int length;
	void * lastNonFlushedPointer = prepareBufferForFlush(ringBuffer, &length);
	if (lastNonFlushedPointer != NULL) {
		writeToFile(&ringBuffer->filehandler, ringBuffer->bufferForFlushingThread, length);
		ringBuffer->pointerToFirstNoFlushedByte = lastNonFlushedPointer;
	}
}

bool writeToFile(FileHandler *handler, int* start, int length) {
	NTSTATUS ret;
	IO_STATUS_BLOCK IoStatusBlock;
	ret = ZwWriteFile(handler->hFile,
		NULL,	// event (optional)
		NULL,
		NULL,
		&IoStatusBlock,
		(PVOID)start,
		length,
		NULL,	// byte offset (in a file)
		NULL);	// key
	if (STATUS_SUCCESS != ret) {
		PRINT("ERROR [write_to_file]: ZwWriteFile");
		return false;
	}
	return true;
}

bool isEnoughtForHeader(RingBuffer *buffer, char * pointer) {
	if (buffer->start + buffer->size - pointer > sizeOfLogEntryHeader()) {
		return true;
	} else {
		return false;
	}
}
//
 void* prepareBufferForFlush(RingBuffer *ringBuffer,  int *sumLength) {
	 // nothing to flush
	 if (ringBuffer->pointerToFirstNoFlushedByte == ringBuffer->pointerToNextToWriteByte) {
		 return NULL;
	 }
	 LogEntry *iterator;
	 char *data;
	 int headerSize = sizeOfLogEntryHeader();
	 if (isEnoughtForHeader(ringBuffer, ringBuffer->pointerToFirstNoFlushedByte)) {
		 iterator = ringBuffer->pointerToFirstNoFlushedByte;
	 } else {
		 iterator = ringBuffer->start;
	 }
	 *sumLength = 0;
	 char *tmpBuffer = ringBuffer->bufferForFlushingThread;
	 void *firstToNonFlushed = iterator;
	 while (iterator->isReady == 1) {
		 (*sumLength) += iterator->size;
		 iterator->isReady = false;
		 data = (char *)iterator + headerSize;
		 int sizeToEnd = (ringBuffer->start + ringBuffer->size) - data;
		 int diff = sizeToEnd - iterator->size;
		 if (diff >= 0) {
			 memcpy(tmpBuffer, data, iterator->size);
			 tmpBuffer += iterator->size;
			 iterator = (char *)iterator + headerSize + iterator->size;
		 } else {
			 if (sizeToEnd != 0) {
				 memcpy(tmpBuffer, data, sizeToEnd);
			 }
			 memcpy(tmpBuffer + sizeToEnd, ringBuffer->start,  -diff);
			 tmpBuffer += iterator->size;
			 iterator = (char *)ringBuffer->start - diff;
		 }
		 firstToNonFlushed = iterator;
		 if (!isEnoughtForHeader(ringBuffer, iterator)) {
			 iterator = ringBuffer->start;
		 }
	 }
	 return firstToNonFlushed;
}

 bool initLocker(Locker *locker) {
	 KeInitializeSpinLock(
		 locker->mutex
	 );
	 return true;
 }

 bool initFileHandler(FileHandler *locker) {
	 UNICODE_STRING     uniName;
	 OBJECT_ATTRIBUTES  objAttr;
	 NTSTATUS ntstatus;
	 IO_STATUS_BLOCK    ioStatusBlock;	// the caller can determine the cause of the failure by checking this value

	 RtlInitUnicodeString(&uniName, LOG_FILE_NAME);  // or L"\\SystemRoot\\example.txt"
	 InitializeObjectAttributes(&objAttr, &uniName,
		 OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
		 NULL, NULL);


	 ntstatus = ZwCreateFile(&locker->hFile,		// file handle
		 GENERIC_WRITE,						// DesiredAccess
		 &objAttr, &ioStatusBlock, NULL,
		 FILE_ATTRIBUTE_NORMAL,				// file attributes
		 FILE_SHARE_READ,					// SHARE READ ACCESS (!)
		 FILE_OVERWRITE_IF,					// Specifies the action to perform if the file does or does not exist
		 FILE_SYNCHRONOUS_IO_NONALERT,
		 NULL, 0);
	 if (STATUS_SUCCESS != ntstatus) {
		 return false;
	 }
	 return true;
 }

 bool lockForLog(RingBuffer *ringBuffer) {
	 KeAcquireSpinLock(
		 ringBuffer->locker.mutex,
		 &ringBuffer->locker.current_irql
	 );
	 return true;
 }

 bool unlockForLog(RingBuffer *ringBuffer) {
	 KeReleaseSpinLock(
		 ringBuffer->locker.mutex,
		 ringBuffer->locker.current_irql
	 );
	 return true;
 }

 void notifyForFlush(RingBuffer *ringBuffer) {
	 return;
 }

 bool shouldFlush(RingBuffer *ringBuffer) {
	 return false;
 }

 //should consider header
 void saveToBuffer(RingBuffer *ringBuffer, char * startAddress, char *string, ShouldWrite *sizes) {
	 int headerSize = sizeOfLogEntryHeader();
	 // all on begin of buffer
	 if (sizes->toEndBytes == 0) {
		 memcpy(ringBuffer->start + headerSize, string, sizes->fromBegginningBytes - headerSize);
	 } else if (sizes->toEndBytes == headerSize){
		 //only header on the end of buffer
		 memcpy(ringBuffer->start, string, sizes->fromBegginningBytes);
	 } else {
		 //only header and some of raw data on the end
		 sizes->toEndBytes -= headerSize;
		 char *tmpPointer = (char *)startAddress;
		 tmpPointer += headerSize;
		 memcpy(tmpPointer, string, sizes->toEndBytes);
		 if (sizes->fromBegginningBytes != 0) {
			 memcpy(ringBuffer->start, string + sizes->toEndBytes, sizes->fromBegginningBytes);
		 }
	 }
 }

 void * tryToReservPointers(RingBuffer * ringBuffer, int size, ShouldWrite *sizes) {
	 int currentAvaliableSize = calculateCurrentFreeSpace(ringBuffer);
	 int logEntryHeaderLength = sizeOfLogEntryHeader();

	 int restToEnd = (ringBuffer->start + ringBuffer->size) - ringBuffer->pointerToNextToWriteByte;
	 int additionalLengthForFragmentation;
     // header always full on end of the line otherwise to begin
	 if (restToEnd < logEntryHeaderLength) {
		 additionalLengthForFragmentation = restToEnd;
	 } else {
		 additionalLengthForFragmentation = 0;
	 }
	 if (currentAvaliableSize < size + logEntryHeaderLength + additionalLengthForFragmentation) {
		 return NULL;
	 }
	 if (restToEnd < logEntryHeaderLength) {
		 ringBuffer->pointerToNextToWriteByte = ringBuffer->start;
	 }
	 int fullSizeWithHeader = size + logEntryHeaderLength;
	 int diff = (ringBuffer->start + ringBuffer->size) - (ringBuffer->pointerToNextToWriteByte + fullSizeWithHeader);
	 void * oldStartToWrite = ringBuffer->pointerToNextToWriteByte;
	 if (diff >= 0) {
		 ringBuffer->pointerToNextToWriteByte += fullSizeWithHeader;
		 sizes->toEndBytes = fullSizeWithHeader;
		 sizes->fromBegginningBytes = 0;
	 } else {
		 sizes->toEndBytes = restToEnd;
		 sizes->fromBegginningBytes = fullSizeWithHeader - restToEnd;
		 ringBuffer->pointerToNextToWriteByte = ringBuffer->start + sizes->fromBegginningBytes;
	 }
	 //todo should sync
	 if (!isEnoughtForHeader(ringBuffer, ringBuffer->pointerToFirstNoFlushedByte)) {
		 ringBuffer->pointerToFirstNoFlushedByte = ringBuffer->start;
	 }
	 return oldStartToWrite;
 }

 int calculateCurrentFreeSpace(RingBuffer *ringBuffer) {
	 int diff = ringBuffer->pointerToNextToWriteByte - ringBuffer->pointerToFirstNoFlushedByte;
	 if (diff >= 0) {
		 return ringBuffer->size - diff;
	 } else {
		 return -diff;
	 }
 }

 int  getSizeOfString(char *string) {
	 return strlen(string);
 }

 void* allocateMemory(int size) {
	return ExAllocatePoolWithTag(
		 _In_ NonPagedPool,			// driver might access this mem while it is running at IRQL > APC_LEVEL
		 _In_ size,
		 _In_ 'Tag1'					// TODO: should use a unique pool tag to help debuggers and verifiers identify the code path
	 );
 }

 void freeMemory(void *pointer) {
	 ExFreePoolWithTag(
		 _In_ pointer,
		 _In_ 'Tag1'
	 );
 }

 int sizeOfLogEntryHeader() {
	 return sizeof(bool) + sizeof(int);
 }

 void destroyRingBuffer(RingBuffer *MainRingBuffer) {
	 destroyFileHandler(&MainRingBuffer->filehandler);
	 destroyLocker(&MainRingBuffer->locker);
	 freeMemory(MainRingBuffer->bufferForFlushingThread);
	 freeMemory(MainRingBuffer->start);
	 freeMemory(MainRingBuffer);
 }

 void destroyLocker(Locker *locker) {
	// CloseHandle(locker->mutex);
 }

 void destroyFileHandler(FileHandler *locker) {
	 ZwClose(locker->hFile);
 }