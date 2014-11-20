#pragma once

#include <vector>

struct Client;

class ClientManager
{
public:
	static ClientManager& GetInstance();

	~ClientManager();

	void AddClient(Client* const client);
	void CloseClient(Client* const client);
	void CloseAllClients();

	void SendAll(char* const buf, const int len);
	std::vector<char*> ListNickname();

private:
	std::vector<Client*> clientSocketVec;

};