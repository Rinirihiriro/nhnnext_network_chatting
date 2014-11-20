#pragma once

#define BUF_SIZE 256

void Send(const SOCKET s, char* const buf, const int len);
void Recv(const SOCKET s);

void ErrorLog(const int errorCode);
void Log(char* const str);
