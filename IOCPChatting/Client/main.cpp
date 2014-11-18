/*
	Chatting Client
*/

#pragma comment(lib, "ws2_32.lib")

#include <stdio.h>
#include <WinSock2.h>
#include <Windows.h>
#include <conio.h>

#include <thread>

#define BUF_SIZE 256
#define NICK_LEN 32
#define LINE_NUM 32

#define COL  100
#define LINE (LINE_NUM + 1)

// #define TEST

void gotoxy(int x, int y)
{
	COORD pos = { x, y };
	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
}

void hidecursor()
{
	CONSOLE_CURSOR_INFO info = {100, FALSE};
	SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
}

void ErrorMessage(const int errorCode)
{
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



CRITICAL_SECTION globalCriticalSection;

struct Lock
{
	Lock() { EnterCriticalSection(&globalCriticalSection); }
	~Lock() { LeaveCriticalSection(&globalCriticalSection); }
};

int Send(SOCKET s, char* buf, int len);
int Recv(SOCKET s, char* buf, int len);

//
char* chatLog[LINE_NUM] = {0};
int chatLogNum = 0;
char typping[BUF_SIZE] = {0};
int typpingIndex = 0;
int typpingStart = 0;
bool chattingOver = false;
//

void UpdateScreen();
void ChangeNickname(char* nickname);

void SendThread(SOCKET sock);
void RecvThread(SOCKET sock);



void main(const int argc, const char * const * const argv)
{
	int error;
	unsigned short port;
	const char* ip;

	InitializeCriticalSection(&globalCriticalSection);

	if (argc != 2)
	{
#ifndef TEST
		printf("USAGE : %s <IP> <PORT>", argv[0]);
		return;
#else
		port = 12345;
		ip = "127.0.0.1";
#endif
	}
	else
	{
		ip = argv[1];
		port = atoi(argv[2]);
	}

	WSAData wsadata = { 0, };
	if (WSAStartup(MAKEWORD(2, 2), &wsadata))
	{
		error = WSAGetLastError();
		ErrorMessage(error);
		return;
	}

	SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (clientSocket == INVALID_SOCKET)
	{
		error = WSAGetLastError();
		ErrorMessage(error);
		return;
	}

	sockaddr_in serverAddr = { 0, };
	serverAddr.sin_family = PF_INET;
	serverAddr.sin_addr.s_addr = inet_addr(ip);
	serverAddr.sin_port = htons(port);
	if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		error = WSAGetLastError();
		ErrorMessage(error);
		return;
	}

	char len;
	char nickname[NICK_LEN];
	puts("Write your nickname :");
	gets_s(nickname);
	len = strlen(nickname) + 4;
	Send(clientSocket, &len, 1);
	Send(clientSocket, "%et", 3);
	Send(clientSocket, nickname, strlen(nickname)+1);

	char mode[BUF_SIZE];
	sprintf_s(mode, "MODE CON COLS=%d LINES=%d", COL, LINE);
	system(mode);
	hidecursor();

	ChangeNickname(nickname);

	std::thread sendthread(SendThread, clientSocket);
	std::thread recvthread(RecvThread, clientSocket);

	sendthread.join();
	recvthread.join();

	for (int i = 0; i < chatLogNum; ++i)
		delete[] chatLog[i];

	closesocket(clientSocket);

	WSACleanup();

	DeleteCriticalSection(&globalCriticalSection);
}

void PushChatLog(char* const buf)
{
	if (chatLogNum >= LINE_NUM)
	{
		delete[] chatLog[0];
		for (int i = 0; i < LINE_NUM - 1; ++i)
		{
			chatLog[i] = chatLog[i + 1];
		}
		--chatLogNum;
	}

	chatLog[chatLogNum] = new char[BUF_SIZE];
	memcpy_s(chatLog[chatLogNum], BUF_SIZE, buf, BUF_SIZE);
	++chatLogNum;
	UpdateScreen();
}

void UpdateScreen()
{
	Lock _lock;
	for (int i = 0; i < chatLogNum; ++i)
	{
		gotoxy(0, i);
		printf("%99c", ' ');
		gotoxy(0, i);
		puts(chatLog[i]);
	}
}

void ChangeNickname(char* nickname)
{
	Lock _lock;
	typpingStart = strlen(nickname) + 5;
	gotoxy(0, LINE_NUM);
	printf("%99c", ' ');
	gotoxy(0, LINE_NUM);
	printf("[ %s ]", nickname);
}

void SendThread(SOCKET sock)
{
	while (!chattingOver)
	{
		int input = _getch();
		if (input == 13)
		{
			if (typpingIndex > 0)
			{
				char len = typpingIndex + 1;
				if (Send(sock, &len, 1) == SOCKET_ERROR)
					break;
				if (Send(sock, typping, len) == SOCKET_ERROR)
					break;

				typpingIndex = 0;

				if (strncmp(typping, "%cn ", 4) == 0)
					ChangeNickname(typping + 4);

				{
					Lock _lock;
					gotoxy(typpingStart, LINE_NUM);
					for (int i = typpingStart; i < COL - 1; ++i)
						putc(' ', stdout);
				}

			}
		}
		else if (input == 8)
		{
			if (typpingIndex > 0)
			{
				typping[--typpingIndex] = '\0';
				{
					Lock _lock;
					gotoxy(typpingStart + typpingIndex, LINE_NUM);
					putc(' ', stdout);
				}
			}
		}
		else if (input != 0 && typpingStart + typpingIndex < COL - 1)
		{
			{
				Lock _lock;
				gotoxy(typpingStart + typpingIndex, LINE_NUM);
				putc(input, stdout);
			}
			typping[typpingIndex++] = input;
			typping[typpingIndex] = '\0';
		}
	}
	chattingOver = true;
}

void RecvThread(SOCKET sock)
{
	int result;
	char len;
	char buf[BUF_SIZE];
	while (!chattingOver)
	{
		result = Recv(sock, &len, 1);
		if (result == 0 || result == SOCKET_ERROR)
			break;

		result = Recv(sock, buf, len);
		if (result == 0 || result == SOCKET_ERROR)
			break;

		PushChatLog(buf);
	}
	if (result == 0)
		PushChatLog("** Server closed **");
	chattingOver = true;
}



int Send(SOCKET s, char* buf, int len)
{
	int result;
	int sentBytes = 0;
	while (sentBytes < len)
	{
		result = send(s, buf + sentBytes, len - sentBytes, 0);
		if (result == SOCKET_ERROR)
			return result;
		else if (result == 0)
			break;
		sentBytes += result;
	}
	return sentBytes;
}

int Recv(SOCKET s, char* buf, int len)
{
	int result;
	int recivedBytes = 0;
	while (recivedBytes < len)
	{
		result = recv(s, buf + recivedBytes, len - recivedBytes, 0);
		if (result == SOCKET_ERROR)
			return result;
		else if (result == 0)
			break;
		recivedBytes += result;
	}
	return recivedBytes;
}
