#include <stdio.h>
#include <WinSock2.h>

#include "Global.h"

#include "Lock.h"
#include "IOContext.h"

void Send(const SOCKET s, char* const buf, const int len)
{
	IOContext* context = new IOContext();

	WSABUF wsa_buf;
	wsa_buf.buf = buf;
	wsa_buf.len = len;

	if (WSASend(s, &wsa_buf, 1, NULL, 0, &context->overlapped, nullptr) == SOCKET_ERROR)
	{
		int error = WSAGetLastError();
		if (error != WSA_IO_PENDING)
		{
			ErrorLog(error);
		}
	}
}

void Recv(const SOCKET s)
{
	static DWORD flags = 0;

	IOContext* context = new IOContext();
	context->buf = new char[BUF_SIZE];
	context->recv = true;

	WSABUF wsa_buf;
	wsa_buf.buf = context->buf;
	wsa_buf.len = BUF_SIZE;

	if (WSARecv(s, &wsa_buf, 1, NULL, &flags, &context->overlapped, nullptr) == SOCKET_ERROR)
	{
		int error = WSAGetLastError();
		if (error != WSA_IO_PENDING)
		{
			ErrorLog(error);
		}
	}
}


void ErrorLog(const int errorCode)
{
	Lock _lock;
	char* buf = nullptr;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		nullptr,
		errorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR)&buf,
		0,
		nullptr);
	fputs("ERROR: ", stderr);
	fputs(buf, stderr);
}


void Log(char* const str)
{
	Lock _lock;
	puts(str);
}

