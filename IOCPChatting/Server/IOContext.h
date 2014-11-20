#pragma once

struct IOContext
{
	IOContext();
	~IOContext();

	WSAOVERLAPPED overlapped;
	char* buf;
	bool recv;

};