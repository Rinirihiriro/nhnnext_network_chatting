#pragma once

#include "QueueBuffer.h"
#include "Global.h"

#define NICK_LEN 32

struct Client
{
	Client();
	Client(SOCKET s);

	SOCKET sock;
	char nickname[NICK_LEN];
	QueueBuffer<BUF_SIZE> queue;
	bool entered;
};