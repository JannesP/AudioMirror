#pragma once
#include "Globals.h"

class RingBuffer
{
private:
	KSPIN_LOCK* m_BufferLock;
	KIRQL m_SpinLockIrql;
	BYTE* m_Buffer;
	BYTE* m_AlignBuffer;
	SIZE_T m_BufferLength;
	SIZE_T m_nByteAlign;
	BOOL m_IsFilling;

	ULONGLONG m_LinearBufferReadPosition;
	ULONGLONG m_LinearBufferWritePosition;
	SIZE_T m_nByteAlignBufferCount;

	NTSTATUS PutInternal(BYTE * pBytes, SIZE_T count);
public:
	RingBuffer();
	~RingBuffer();

	NTSTATUS Init(SIZE_T bufferSize, SIZE_T nByteAlign);
	/*
		Puts the given bytes into the buffer.
	*/
	NTSTATUS Put(_In_ BYTE* pBytes, _In_ SIZE_T count);
	/*
		Takes bytes out of the buffer and puts them into the target address.
	*/
	NTSTATUS Take(_In_ BYTE* pTarget, _In_ SIZE_T count, _Outptr_opt_ SIZE_T* readCount);

	SIZE_T GetSize();

	SIZE_T GetAvailableBytes();

	void Clear();
};

