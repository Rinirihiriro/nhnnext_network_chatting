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

#define THREAD_NUM 2
#define BUF_SIZE 256
#define NICK_LEN 32

// #define TEST

char* g_Manual[] = {
	"  %help - Show this page",
	"  %cn ~ - Change Nickname to ~",
	"  %ls   - List users",
};

/*
	Lock
*/

CRITICAL_SECTION globalCriticalSection;

struct Lock
{
	Lock() { EnterCriticalSection(&globalCriticalSection); }
	~Lock() { LeaveCriticalSection(&globalCriticalSection); }
};

/*
	IO Context
*/

struct IOContext
{
	IOContext()
		:buf(nullptr), recv(false)
	{
		ZeroMemory(&overlapped, sizeof(WSAOVERLAPPED));
	}

	~IOContext()
	{
		if (buf)
			delete[] buf;
	}

	WSAOVERLAPPED overlapped;
	char* buf;
	bool recv;

};

/*
	Send & Recv Wrapper
*/

void Send(const SOCKET s, char* const buf, const int len);
void Recv(const SOCKET s);

/*
	PC Queue
*/

template <int size>
class PCQueue
{
public:
	PCQueue()
		:itemSize(0)
	{
	}
	~PCQueue()
	{
	}

	void Produce(char* const buf, const int len)
	{
		Lock _lock;
		int remainSize = size - itemSize;
		if (remainSize >= len)
		{
			memcpy_s(buffer + itemSize, remainSize, buf, len);
			itemSize += len;
		}
	}

	void Consume(char* const buf, const int len)
	{
		Lock _lock;
		if (itemSize >= len)
		{
			memcpy_s(buf, len, buffer, len);
			memmove_s(buffer, size, buffer + len, itemSize - len);
			itemSize -= len;
		}
	}

	void Peek(char* const buf, const int len)
	{
		if (itemSize >= len)
		{
			memcpy_s(buf, len, buffer, len);
		}
	}

	int GetItemSize() const
	{
		Lock _lock;
		return itemSize;
	}

private:
	char buffer[size];
	int itemSize;
};

/*
	Client & Manager
*/

struct Client
{
	Client() :sock(INVALID_SOCKET), entered(false)
	{
		strcpy_s(nickname, "Nonamed");
	}

	Client(SOCKET s) :sock(s), entered(false)
	{
		strcpy_s(nickname, "Nonamed");
	}

	SOCKET sock;
	char nickname[NICK_LEN];
	PCQueue<BUF_SIZE> queue;
	bool entered;
};

class ClientManager
{
public:
	static ClientManager& GetInstance()
	{
		static ClientManager instance;
		return instance;
	}

	~ClientManager()
	{
		CloseAllClients();
	}

	void AddClient(Client* const client)
	{
		if (!client) return;
		Lock _lock;
		clientSocketVec.push_back(client);
	}

	void CloseClient(Client* const client)
	{
		if (!client) return;
		Lock _lock;
		closesocket(client->sock);
		clientSocketVec.erase(
			std::find(clientSocketVec.begin(), clientSocketVec.end(), client));
		delete client;
	}
	
	void CloseAllClients()
	{
		Lock _lock;
		for (Client* client : clientSocketVec)
		{
			closesocket(client->sock);
			delete client;
		}
		clientSocketVec.clear();
	}
	
	void SendAll(char* const buf, const int len)
	{
		if (!buf || len < 1) return;
		Lock _lock;
		for (Client* client : clientSocketVec)
		{
			if (!client->entered) continue;
			char l = len;
			Send(client->sock, &l, 1);
			Send(client->sock, buf, len);
		}
	}

	std::vector<char*> ListNickname()
	{
		std::vector<char*> out;
		for (Client* client : clientSocketVec)
		{
			if (!client->entered) continue;
			out.push_back(client->nickname);
		}
		return out;
	}

private:
	std::vector<Client*> clientSocketVec;

};

void ErrorLog(const int errorCode);
void Log(char* const str);

void IOCPThread(HANDLE hCP);

/*
	Main
*/

void main(const int argc, const char * const * const argv)
{
	int error;
	unsigned short port;
	std::unique_ptr<std::thread> threads[THREAD_NUM];

	InitializeCriticalSection(&globalCriticalSection);


	if (argc != 2)
	{
#ifndef TEST
		printf("USAGE : %s <PORT>", argv[0]);
		return;
#else
		port = 12345;
#endif
	}
	else
	{
		port = atoi(argv[1]);
	}

	Log("Starting the server...");

	WSAData wsadata = { 0, };
	if (WSAStartup(MAKEWORD(2, 2), &wsadata))
	{
		error = WSAGetLastError();
		ErrorLog(error);
		return;
	}

	HANDLE hCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, THREAD_NUM);
	for (int i = 0; i < THREAD_NUM; ++i)
	{
		threads[i].reset(new std::thread(IOCPThread, hCP));
	}

	SOCKET listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED);
	if (listenSocket == INVALID_SOCKET)
	{
		error = WSAGetLastError();
		ErrorLog(error);
		return;
	}

	char option = 1;
	if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) == SOCKET_ERROR)
	{
		error = WSAGetLastError();
		ErrorLog(error);
		return;
	}

	sockaddr_in serverAddr = { 0, };
	serverAddr.sin_family = PF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons(port);
	if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		error = WSAGetLastError();
		ErrorLog(error);
		return;
	}

	if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		error = WSAGetLastError();
		ErrorLog(error);
		return;
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
			error = WSAGetLastError();
			ErrorLog(error);
			continue;
		}

		Client* client = new Client(clientSocket);
		IOContext* context = new IOContext();
		context->recv = true;

		if (CreateIoCompletionPort((HANDLE)clientSocket, hCP, (ULONG_PTR)client, 0) != hCP)
		{
			error = GetLastError();
			ErrorLog(error);
			continue;
		}

		Log("Client connected");

		ClientManager::GetInstance().AddClient(client);

		Recv(clientSocket);
	}

	CloseHandle(hCP);
	closesocket(listenSocket);
	WSACleanup();
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
