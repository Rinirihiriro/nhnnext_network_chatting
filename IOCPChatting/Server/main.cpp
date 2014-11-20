/*
	IOCP Chatting Server
*/

#pragma comment(lib, "ws2_32.lib")

#include <stdio.h>
#include <WinSock2.h>
#include <Windows.h>

#include <thread>
#include <memory>
#include <vector>


#include "Lock.h"
#include "QueueBuffer.h"

#include "IOContext.h"

#include "Global.h"

#include "ClientManager.h"
#include "Client.h"

#define THREAD_NUM 2

// #define TEST

extern CRITICAL_SECTION globalCriticalSection;

char* g_Manual[] = {
	"  %help - Show this page",
	"  %cn ~ - Change Nickname to ~",
	"  %ls   - List users",
};

void IOCPThread(HANDLE hCP);

/*
	Main
*/

void main(const int argc, const char * const * const argv)
{
	unsigned short port;

	std::unique_ptr<std::thread> threads[THREAD_NUM];

	WSAData wsadata = { 0, };
	sockaddr_in serverAddr = { 0, };

	InitializeCriticalSection(&globalCriticalSection);

	if (argc != 2)
	{
#ifndef TEST
		printf("USAGE : %s <PORT>", argv[0]);
		goto delcs;
#else
		port = 12345;
#endif
	}
	else
	{
		port = atoi(argv[1]);
	}

	Log("Starting the server...");

	if (WSAStartup(MAKEWORD(2, 2), &wsadata))
	{
		ErrorLog(WSAGetLastError());
		goto delcs;
	}

	HANDLE hCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, THREAD_NUM);
	if (hCP == NULL)
	{
		ErrorLog(WSAGetLastError());
		goto wsaclean;
	}

	for (int i = 0; i < THREAD_NUM; ++i)
	{
		threads[i].reset(new std::thread(IOCPThread, hCP));
	}

	SOCKET listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED);
	if (listenSocket == INVALID_SOCKET)
	{
		ErrorLog(WSAGetLastError());
		goto closecp;
	}

	char option = 1;
	if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) == SOCKET_ERROR)
	{
		ErrorLog(WSAGetLastError());
		goto closesock;
	}

	serverAddr.sin_family = PF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons(port);
	if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		ErrorLog(WSAGetLastError());
		goto closesock;
	}

	if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		ErrorLog(WSAGetLastError());
		goto closesock;
	}

	Log("Server started.");

	DWORD flags = 0;

	while (true)
	{
		sockaddr_in clientAddr = { 0, };
		int addrLen = sizeof(clientAddr);

		Log("Waiting for clients...");
		SOCKET clientSocket = accept(listenSocket, (sockaddr*)&clientAddr, &addrLen);
		if (clientSocket == INVALID_SOCKET)
		{
			ErrorLog(WSAGetLastError());
			continue;
		}

		Client* client = new Client(clientSocket);
		IOContext* context = new IOContext();
		context->recv = true;

		if (CreateIoCompletionPort((HANDLE)clientSocket, hCP, (ULONG_PTR)client, 0) != hCP)
		{
			ErrorLog(GetLastError());
			closesocket(clientSocket);
			continue;
		}

		Log("Client connected");

		ClientManager::GetInstance().AddClient(client);

		Recv(clientSocket);
	}

closesock:
	closesocket(listenSocket);
	
closecp:
	CloseHandle(hCP);
	
wsaclean:
	WSACleanup();

delcs:
	DeleteCriticalSection(&globalCriticalSection);

}

/*
	IOCP Thread
*/

void IOCPThread(HANDLE hCP)
{
	DWORD flags = 0;

	int error;

	while (true)
	{
		DWORD byteTransferred;
		Client* client;
		IOContext* context;
		char buf[BUF_SIZE + NICK_LEN + 5];

		if (!GetQueuedCompletionStatus(hCP, &byteTransferred, (PULONG_PTR)&client, (LPOVERLAPPED*)&context, INFINITE))
		{
			error = GetLastError();
			if (error != ERROR_NETNAME_DELETED)
			{
				ErrorLog(error);
				break;
			}
		}

		if (context->recv)
		{
			buf[0] = '\0';
			if (byteTransferred == 0)
			{
				sprintf_s(buf, "*** %s has left the room. ***", client->nickname);
				ClientManager::GetInstance().CloseClient(client);
			}
			else
			{
				client->queue.Produce(context->buf, byteTransferred);

				char len = 0;
				client->queue.Peek(&len, 1);
				while (len <= client->queue.GetItemSize() - 1)
				{
					char line[BUF_SIZE];
					client->queue.Consume(&len, 1);
					client->queue.Consume(line, len);
					if (!client->entered)
					{
						if (strncmp(line, "%et", 3) == 0)
						{
							client->entered = true;
							strcpy_s(client->nickname, line + 3);
							sprintf_s(buf, "*** %s entered the room. ***", client->nickname);
						}
					}
					else
					{
						if (strncmp(line, "%help", 5) == 0)
						{
							for (char* str : g_Manual)
							{
								char len = strlen(str) + 1;
								Send(client->sock, &len, 1);
								Send(client->sock, str, len);
							}
						}
						else if (strncmp(line, "%cn ", 4) == 0)
						{
							sprintf_s(buf, "*** %s changed nickname to %s. ***", client->nickname, line + 4);
							strcpy_s(client->nickname, line + 4);
						}
						else if (strcmp(line, "%ls") == 0)
						{
							char len;
							auto nicknames = ClientManager::GetInstance().ListNickname();
							for (char* nickname : nicknames)
							{
								len = strlen(nickname) + 1;
								Send(client->sock, &len, 1);
								Send(client->sock, nickname, len);
							}
						}
						else
						{
							sprintf_s(buf, "[ %s ] %s", client->nickname, line);
						}
					}
					len = 0;
					client->queue.Peek(&len, 1);
				}

				Recv(client->sock);
			}

			if (buf[0] != '\0')
			{
				Log(buf);
				ClientManager::GetInstance().SendAll(buf, strlen(buf) + 1);
			}
		}

		delete context;
	}
}
