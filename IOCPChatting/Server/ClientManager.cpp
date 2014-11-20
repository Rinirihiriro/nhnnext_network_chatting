#include <WinSock2.h>

#include "ClientManager.h"
#include "Client.h"

#include "Lock.h"

#include "Global.h"

ClientManager& ClientManager::GetInstance()
{
	static ClientManager instance;
	return instance;
}

ClientManager::~ClientManager()
{
	CloseAllClients();
}

void ClientManager::AddClient(Client* const client)
{
	if (!client) return;
	Lock _lock;
	clientSocketVec.push_back(client);
}

void ClientManager::CloseClient(Client* const client)
{
	if (!client) return;
	Lock _lock;
	closesocket(client->sock);
	clientSocketVec.erase(
		std::find(clientSocketVec.begin(), clientSocketVec.end(), client));
	delete client;
}

void ClientManager::CloseAllClients()
{
	Lock _lock;
	for (Client* client : clientSocketVec)
	{
		closesocket(client->sock);
		delete client;
	}
	clientSocketVec.clear();
}

void ClientManager::SendAll(char* const buf, const int len)
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

std::vector<char*> ClientManager::ListNickname()
{
	std::vector<char*> out;
	for (Client* client : clientSocketVec)
	{
		if (!client->entered) continue;
		out.push_back(client->nickname);
	}
	return out;
}