#include <WinSock2.h>
#include <string.h>
#include "Client.h"

Client::Client() :sock(INVALID_SOCKET), entered(false)
{
	strcpy_s(nickname, "Nonamed");
}

Client::Client(SOCKET s) :sock(s), entered(false)
{
	strcpy_s(nickname, "Nonamed");
}
