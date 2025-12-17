/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kbrauer <kbrauer@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/10 15:18:12 by kbrauer           #+#    #+#             */
/*   Updated: 2025/12/12 10:00:00 by kbrauer          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <map>
#include <poll.h>
#include <netinet/in.h>

class Client;
class Channel;

class Server {
private:
    int serverSocket;
    int port;
    std::string password;
    std::string serverName;
    
    std::vector<struct pollfd> pollFds;
    std::map<int, Client*> clients;
    std::map<std::string, Channel*> channels;
    
    bool isRunning;

    // Private helper methods
    void setupServerSocket();
    void acceptNewClient();
    void handleClientData(int clientFd);
    void handleClientWrite(int clientFd);
    void removeClient(int clientFd);
    void parseCommand(Client* client, const std::string& message);
    
    // Validation helpers
    bool isValidNickname(const std::string& nick) const;
    bool isValidChannelName(const std::string& name) const;
    
    // IRC command handlers
    void handlePass(Client* client, const std::vector<std::string>& tokens);
    void handleNick(Client* client, const std::vector<std::string>& tokens);
    void handleUser(Client* client, const std::vector<std::string>& tokens);
    void handleJoin(Client* client, const std::vector<std::string>& tokens);
    void handlePart(Client* client, const std::vector<std::string>& tokens);
    void handlePrivmsg(Client* client, const std::vector<std::string>& tokens);
    void handleKick(Client* client, const std::vector<std::string>& tokens);
    void handleInvite(Client* client, const std::vector<std::string>& tokens);
    void handleTopic(Client* client, const std::vector<std::string>& tokens);
    void handleMode(Client* client, const std::vector<std::string>& tokens);
    void handleQuit(Client* client, const std::vector<std::string>& tokens);
    void handlePing(Client* client, const std::vector<std::string>& tokens);
    
    void tryCompleteRegistration(Client* client);
    void updatePollEvents(int fd, short events);
    void flushAllPendingWrites();
    void removeMarkedClients();
    void cleanupEmptyChannels();
    
public:
    Server(int port, const std::string& password);
    ~Server();
    
    void start();
    void stop();
    
    // Getters
    const std::string& getPassword() const { return password; }
    const std::string& getServerName() const { return serverName; }
    Client* getClientByNickname(const std::string& nickname);
    Channel* getChannel(const std::string& channelName);
    Channel* createChannel(const std::string& channelName, Client* creator);
};

#endif
