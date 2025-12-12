/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kbrauer <kbrauer@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/10 15:18:12 by kbrauer           #+#    #+#             */
/*   Updated: 2025/12/10 15:18:14 by kbrauer          ###   ########.fr       */
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
    int serverSocket;              // Main socket for accepting connections
    int port;                      // Port number
    std::string password;          // Server password
    
    std::vector<struct pollfd> pollFds;  // Array of file descriptors to monitor
    std::map<int, Client*> clients;      // Map: socket fd -> Client object
    std::map<std::string, Channel*> channels; // Map: channel name -> Channel
    
    bool isRunning;                // Server running flag

    // Private helper methods
    void setupServerSocket();      // Initialize and bind server socket
    void acceptNewClient();        // Accept incoming connection
    void handleClientData(int clientFd);  // Process client messages
    void removeClient(int clientFd);      // Clean up disconnected client
    void parseCommand(Client* client, const std::string& message);
    
    // IRC command handlers
    void handlePass(Client* client, const std::vector<std::string>& tokens);
    void handleNick(Client* client, const std::vector<std::string>& tokens);
    void handleUser(Client* client, const std::vector<std::string>& tokens);
    void handleJoin(Client* client, const std::vector<std::string>& tokens);
    void handlePrivmsg(Client* client, const std::vector<std::string>& tokens);
    void handleKick(Client* client, const std::vector<std::string>& tokens);
    void handleInvite(Client* client, const std::vector<std::string>& tokens);
    void handleTopic(Client* client, const std::vector<std::string>& tokens);
    void handleMode(Client* client, const std::vector<std::string>& tokens);
    
    void tryCompleteRegistration(Client* client);
    
public:
    Server(int port, const std::string& password);
    ~Server();
    
    void start();                  // Main server loop
    void stop();                   // Stop server
    
    // Getters
    const std::string& getPassword() const { return password; }
    Client* getClientByNickname(const std::string& nickname);
    Channel* getChannel(const std::string& channelName);
    Channel* createChannel(const std::string& channelName, Client* creator);
};

#endif