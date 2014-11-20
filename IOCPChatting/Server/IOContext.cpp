#include <WinSock2.h>
#include "IOContext.h"

IOContext::IOContext()
	:buf(nullptr), recv(false)
{
	ZeroMemory(&overlapped, sizeof(WSAOVERLAPPED));
}

IOContext::~IOContext()
{
	if (buf)
		delete[] buf;
}
