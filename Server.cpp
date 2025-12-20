/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mvolgger <mvolgger@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/19 18:03:52 by mvolgger          #+#    #+#             */
/*   Updated: 2025/12/20 14:18:22 by mvolgger         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */



#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <cerrno>
#include <algorithm>


Server::Server(int port, const std::string& password) 
    : serverSocket(-1), 
      port(port), 
      password(password), 
      serverName("ircserv"),
      isRunning(false) {
}

Server::~Server() {
    for (std::map<int, Client*>::iterator it = clients.begin(); it != clients.end(); ++it) {
        delete it->second;
    }
    
    for (std::map<std::string, Channel*>::iterator it = channels.begin(); it != channels.end(); ++it) {
        delete it->second;
    }
    
    if (serverSocket != -1) {
        close(serverSocket);
    }
}

/*
Sockets:    a software construct that wraps a combination of a protocol (TCP/UDP),
            an IP address, and a port number = OS takes that combo to route msg
            appropriately.
            Sockets operate on the transport layer (OSI Model).

TCP:        Data arrives in order and without dups, without lossing data (reliable).

Sockets
Lifecycle:  starts with creation of listening socket `socket()` which is bound to a
            specific IP address and port `bind()`. `listen()` wains for the incoming
            client connections `connect()` and once a client initiates the connection
            the server accepts it `accept()`, creating a new socket instance that is
            dedicated to that specif. client. And the OG socket `listen()` continus
            to listen for new requests.
            That's how server handles many diff clients concurrently, each with its
            own socket. Usually handled with multy threading, but we handle it with
            Non-blocking/async I/O - allowing a single thread to manage 1000 of open
            sockets concurently (acchieved using sys calls eg. `poll()`). It notifies
            the app only when specific socket are ready for writing or reading.

*/
void Server::setupServerSocket() {
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        throw std::runtime_error("Failed to create socket");
    }
    
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(serverSocket);
        throw std::runtime_error("Failed to set socket options");
    }
    
    if (fcntl(serverSocket, F_SETFL, O_NONBLOCK) < 0) {
        close(serverSocket);
        throw std::runtime_error("Failed to set non-blocking mode");
    }
    
    struct sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);
    
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        close(serverSocket);
        throw std::runtime_error("Failed to bind socket");
    }
    
    if (listen(serverSocket, 10) < 0) {
        close(serverSocket);
        throw std::runtime_error("Failed to listen on socket");
    }
    
    struct pollfd serverPollFd;
    serverPollFd.fd = serverSocket;
    serverPollFd.events = POLLIN;
    serverPollFd.revents = 0;
    pollFds.push_back(serverPollFd);
    
    std::cout << "Server listening on port " << port << std::endl;
}

void Server::acceptNewClient() {
    struct sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);
    
    int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
    
    if (clientSocket < 0) {
        if (errno != EWOULDBLOCK && errno != EAGAIN) {
            std::cerr << "Error accepting client" << std::endl;
        }
        return;
    }
    
    if (fcntl(clientSocket, F_SETFL, O_NONBLOCK) < 0) {
        std::cerr << "Failed to set client socket to non-blocking" << std::endl;
        close(clientSocket);
        return;
    }
    
    Client* newClient = new Client(clientSocket);
    
    // Set client hostname from connection address
    char hostStr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(clientAddr.sin_addr), hostStr, INET_ADDRSTRLEN);
    newClient->setHostname(hostStr);
    
    clients[clientSocket] = newClient;
    
    struct pollfd clientPollFd;
    clientPollFd.fd = clientSocket;
    clientPollFd.events = POLLIN;
    clientPollFd.revents = 0;
    pollFds.push_back(clientPollFd);
    
    std::cout << "New client connected: fd " << clientSocket 
              << " from " << hostStr << std::endl;
}

void Server::start() {
    setupServerSocket();
    isRunning = true;
    
    std::cout << "Server started. Waiting for connections..." << std::endl;
    
    while (isRunning) {
        int pollCount = poll(&pollFds[0], pollFds.size(), -1);
        
        if (pollCount < 0) {
            if (errno == EINTR) {
                continue;  // interrupted by signal, just retry
            }
            std::cerr << "Poll error" << std::endl;
            break;
        }
        
        // handle events
        for (size_t i = pollFds.size(); i > 0; i--) {
            size_t idx = i - 1;
            
            if (pollFds[idx].revents == 0) {
                continue;
            }
            
            if (pollFds[idx].fd == serverSocket) {
                if (pollFds[idx].revents & POLLIN) {
                    acceptNewClient();
                }
            } else {
                if (pollFds[idx].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                    removeClient(pollFds[idx].fd);
                    continue;
                }
                
                if (pollFds[idx].revents & POLLIN) {
                    handleClientData(pollFds[idx].fd);
                }
                
                if (pollFds[idx].revents & POLLOUT) {
                    handleClientWrite(pollFds[idx].fd);
                }
            }
        }
        
        removeMarkedClients();
    }
}

void Server::stop() {
    isRunning = false;
}

const std::string& Server::getPassword() const {
    return password;
}

const std::string& Server::getServerName() const {
    return serverName;
}

// client data handling
void Server::handleClientData(int clientFd) {
    Client* client = clients[clientFd];
    if (!client || client->isMarkedForRemoval()) 
        return;
    
    char buffer[512];
    std::memset(buffer, 0, sizeof(buffer));
    
    ssize_t bytesRead = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytesRead <= 0) {
        if (bytesRead == 0) {
            std::cout << "Client " << clientFd << " disconnected" << std::endl;
        } else if (errno != EWOULDBLOCK && errno != EAGAIN) {
            std::cerr << "Error reading from client" << clientFd << std::endl;
        } else {
            return;
        }
        removeClient(clientFd);
        return;
    }
    
    client->getInputBuffer() += std::string(buffer, bytesRead);
    
    std::string& inputBuffer = client->getInputBuffer();
    size_t pos;
    
    while ((pos = inputBuffer.find('\n')) != std::string::npos) {
        std::string line = inputBuffer.substr(0, pos);
        inputBuffer.erase(0, pos + 1);
        
        //REMOVE \n, only needed to find the end of cmd
        if (!line.empty() && line[line.size() - 1] == '\r') {
            line.erase(line.size() - 1);
        }
        
        if (line.empty()) continue;
        
        std::cout << "Received from " << clientFd << ": " << line << std::endl;
        parseCommand(client, line);
    }
    
    // check if client has data to send after processing
    if (client->hasDataToSend()) {
        updatePollEvents(clientFd, POLLIN | POLLOUT);
    }
}

void Server::handleClientWrite(int clientFd) {
    Client* client = clients[clientFd];
    if (!client || client->isMarkedForRemoval()) 
        return;
    
    if (client->sendOutputBuffer()) {
        // after sending data remove POLLOUT
        updatePollEvents(clientFd, POLLIN);
    }
}

void Server::updatePollEvents(int fd, short events) {
    for (size_t i = 0; i < pollFds.size(); i++) {
        if (pollFds[i].fd == fd) {
            pollFds[i].events = events;
            return;
        }
    }
}

// update poll events for all clients that have data waiting to be sent
void Server::sendAllData() {
    for (std::map<int, Client*>::iterator it = clients.begin(); 
         it != clients.end(); ++it) {
        if (it->second->hasDataToSend()) {
            updatePollEvents(it->first, POLLIN | POLLOUT);
        }
    }
}

void Server::parseCommand(Client* client, const std::string& message) {
    std::vector<std::string> tokens;
    std::istringstream iss(message);
    std::string token;
    
    while (iss >> token) {
        if (token[0] == ':' && tokens.size() > 0) {
            std::string rest = token.substr(1);
            std::string remainder;
            if (std::getline(iss, remainder)) {
                rest += remainder;
            }
            tokens.push_back(rest);
            break;
        }
        tokens.push_back(token);
    }
    
    if (tokens.empty()) return;
    
    std::string command = tokens[0];
    for (size_t i = 0; i < command.length(); i++) {
        command[i] = std::toupper(command[i]);
    }
    
    if (command == "PASS") {
        cmdPass(client, tokens);
    } else if (command == "NICK") {
        cmdNick(client, tokens);
    } else if (command == "USER") {
        cmdUser(client, tokens);
    } else if (command == "JOIN") {
        cmdJoin(client, tokens);
    } else if (command == "PART") {
        cmdPart(client, tokens);
    } else if (command == "PRIVMSG") {
        cmdPrivmsg(client, tokens);
    } else if (command == "KICK") {
        cmdKick(client, tokens);
    } else if (command == "INVITE") {
        cmdInvite(client, tokens);
    } else if (command == "TOPIC") {
        cmdTopic(client, tokens);
    } else if (command == "MODE") {
        cmdMode(client, tokens);
    } else if (command == "QUIT") {
        cmdQuit(client, tokens);
    } else if (command == "PING") {
        cmdPing(client, tokens);
    } else {
        if (client->getRegistered()) {
            client->queueMessage("421 " + client->getNickname() + " " + command + " :Unknown command");
        }
    }
    sendAllData();
}

// Remove clients that were marked for removal (e.g., after QUIT)
void Server::removeMarkedClients() {
    std::vector<int> toRemove;
    
    for (std::map<int, Client*>::iterator it = clients.begin(); 
         it != clients.end(); ++it) {
        if (it->second->isMarkedForRemoval()) {
            it->second->sendOutputBuffer();
            toRemove.push_back(it->first);
        }
    }
    
    for (size_t i = 0; i < toRemove.size(); i++) {
        removeClient(toRemove[i]);
    }
}

bool Server::isValidNickname(const std::string& nick) const {
    if (nick.empty() || nick.length() > 9) {
        return false;
    }
    
    // first character must be a letter or special char
    char first = nick[0];
    if (!std::isalpha(first) && first != '[' && first != ']' && 
        first != '\\' && first != '`' && first != '_' && 
        first != '^' && first != '{' && first != '|' && first != '}') {
        return false;
    }
    
    // rest can be letters, digits, or special chars
    for (size_t i = 1; i < nick.length(); i++) {
        char c = nick[i];
        if (!std::isalnum(c) && c != '[' && c != ']' && c != '\\' && 
            c != '`' && c != '_' && c != '^' && c != '{' && 
            c != '|' && c != '}' && c != '-') {
            return false;
        }
    }
    
    return true;
}

bool Server::isValidChannelName(const std::string& name) const {
    if (name.empty() || name.length() > 50) {
        return false;
    }
    
    if (name[0] != '#' && name[0] != '&') {
        return false;
    }
    
    for (size_t i = 1; i < name.length(); i++) {
        char c = name[i];
        if (c == ' ' || c == ',' || c == ':') {
            return false;
        }
    }
    
    return true;
}

// handle clients
void Server::removeClient(int clientFd) {
    std::map<int, Client*>::iterator it = clients.find(clientFd);
    if (it == clients.end()) return;
    
    Client* client = it->second;
    
    // Remove from all the joind channels
    const std::set<Channel*>& joinedChannels = client->getJoinedChannels();
    std::set<Channel*> channelsCopy = joinedChannels;  // Copy so we can modify it
    
    for (std::set<Channel*>::iterator chanIt = channelsCopy.begin(); 
         chanIt != channelsCopy.end(); ++chanIt) {
        Channel* channel = *chanIt;
        std::string quitMsg = ":" + client->getPrefix() + " QUIT :Client disconnected";
        channel->broadcast(quitMsg, client);
        channel->removeMember(client);
    }
    
    // Remove from poll array
    for (std::vector<struct pollfd>::iterator pollIt = pollFds.begin(); 
         pollIt != pollFds.end(); ++pollIt) {
        if (pollIt->fd == clientFd) {
            pollFds.erase(pollIt);
            break;
        }
    }
    
    delete client;
    clients.erase(it);
    
    cleanupEmptyChannels();
    
    std::cout << "Client " << clientFd << " removed" << std::endl;
}

void Server::cleanupEmptyChannels() {
    std::vector<std::string> emptyChannels;
    
    for (std::map<std::string, Channel*>::iterator it = channels.begin();
         it != channels.end(); ++it) {
        if (it->second->isEmpty()) {
            emptyChannels.push_back(it->first);
        }
    }
    
    for (size_t i = 0; i < emptyChannels.size(); i++) {
        std::map<std::string, Channel*>::iterator it = channels.find(emptyChannels[i]);
        if (it != channels.end()) {
            delete it->second;
            channels.erase(it);
            std::cout << "Channel " << emptyChannels[i] << " removed" << std::endl;
        }
    }
}

Client* Server::getClientByNickname(const std::string& nickname) {
    for (std::map<int, Client*>::iterator it = clients.begin(); it != clients.end(); ++it) {
        std::string clientNick = it->second->getNickname();
        std::string searchNick = nickname;
        
        for (size_t i = 0; i < clientNick.length(); i++) {
            clientNick[i] = std::tolower(clientNick[i]);
        }
        for (size_t i = 0; i < searchNick.length(); i++) {
            searchNick[i] = std::tolower(searchNick[i]);
        }
        
        if (clientNick == searchNick) {
            return it->second;
        }
    }
    return NULL;
}

Channel* Server::getChannel(const std::string& channelName) {
    std::string lowerName = channelName;
    for (size_t i = 0; i < lowerName.length(); i++) {
        lowerName[i] = std::tolower(lowerName[i]);
    }
    
    for (std::map<std::string, Channel*>::iterator it = channels.begin();
         it != channels.end(); ++it) {
        std::string existingName = it->first;
        for (size_t i = 0; i < existingName.length(); i++) {
            existingName[i] = std::tolower(existingName[i]);
        }
        if (existingName == lowerName) {
            return it->second;
        }
    }
    return NULL;
}

Channel* Server::createChannel(const std::string& channelName, Client* creator) {
    Channel* channel = new Channel(channelName);
    channels[channelName] = channel;
    channel->addMember(creator);
    channel->addOperator(creator);
    return channel;
}

// commands
void Server::cmdPass(Client* client, const std::vector<std::string>& tokens) {
    if (client->getRegistered()) {
        client->queueMessage("462 :You may not reregister");
        return;
    }
    
    if (tokens.size() < 2) {
        client->queueMessage("461 PASS :Not enough parameters");
        return;
    }
    
    if (tokens[1] == password) {
        client->setAuthenticated(true);
    } else {
        client->queueMessage("464 :Password incorrect");
    }
}

void Server::cmdNick(Client* client, const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) {
        client->queueMessage("431 :No nickname given");
        return;
    }
    
    std::string newNick = tokens[1];
    
    if (!isValidNickname(newNick)) {
        client->queueMessage("432 " + newNick + " :Erroneous nickname");
        return;
    }
    
    Client* existing = getClientByNickname(newNick);
    if (existing != NULL && existing != client) {
        client->queueMessage("433 * " + newNick + " :Nickname is already in use");
        return;
    }
    
    std::string oldNick = client->getNickname();
    client->setNickname(newNick);
    
    // if already registered, notify about nick change
    if (client->getRegistered()) {
        std::string msg = ":" + (oldNick.empty() ? newNick : oldNick) + " NICK :" + newNick;
        client->queueMessage(msg);
        
        // notify all channels this user is in
        const std::set<Channel*>& joinedChannels = client->getJoinedChannels();
        for (std::set<Channel*>::const_iterator it = joinedChannels.begin();
             it != joinedChannels.end(); ++it) {
            (*it)->broadcast(msg, client);
        }
    }
    
    tryCompleteRegistration(client);
}

void Server::cmdUser(Client* client, const std::vector<std::string>& tokens) {
    if (client->getRegistered()) {
        client->queueMessage("462 :You may not reregister");
        return;
    }
    
    if (tokens.size() < 5) {
        client->queueMessage("461 USER :Not enough parameters");
        return;
    }
    
    client->setUsername(tokens[1]);
    client->setRealname(tokens[4]);
    
    tryCompleteRegistration(client);
}

void Server::tryCompleteRegistration(Client* client) {
    if (!client->getAuthenticated()) {
        return;
    }
    
    if (client->getNickname().empty() || client->getUsername().empty()) {
        return;
    }
    
    if (client->getRegistered()) {
        return;
    }
    
    client->setRegistered(true);
    
    std::string nick = client->getNickname();
    client->queueMessage("001 " + nick + " :Welcome to the Internet Relay Network " + client->getPrefix());
    client->queueMessage("002 " + nick + " :Your host is " + serverName + ", running the latest version");
    client->queueMessage("003 " + nick + " :This server was created today");
    client->queueMessage("004 " + nick + " :" + serverName + " o itkol");
    client->queueMessage("375 " + nick + " :" + serverName + " Message of the Day -");
    client->queueMessage("372 " + nick + " :*Happy Christmas* and welcome to our little IRC server!");
    client->queueMessage("376 " + nick + " :End of /MOTD command");
}

void Server::cmdJoin(Client* client, const std::vector<std::string>& tokens) {
    if (!client->getRegistered()) {
        client->queueMessage("451 :You have not registered");
        return;
    }
    
    if (tokens.size() < 2) {
        client->queueMessage("461 JOIN :Not enough parameters");
        return;
    }
    
    // Handle multiple channels (comma-separated)
    std::string channelList = tokens[1];
    std::string keyList = (tokens.size() >= 3) ? tokens[2] : "";
    
    std::vector<std::string> channelNames;
    std::vector<std::string> keys;
    
    // Split channels
    std::istringstream chanStream(channelList);
    std::string chanToken;
    while (std::getline(chanStream, chanToken, ',')) {
        if (!chanToken.empty()) {
            channelNames.push_back(chanToken);
        }
    }
    
    // Split keys
    std::istringstream keyStream(keyList);
    std::string keyToken;
    while (std::getline(keyStream, keyToken, ',')) {
        keys.push_back(keyToken);
    }
    
    for (size_t i = 0; i < channelNames.size(); i++) {
        std::string channelName = channelNames[i];
        std::string key = (i < keys.size()) ? keys[i] : "";
        
        if (!isValidChannelName(channelName)) {
            client->queueMessage("403 " + channelName + " :No such channel");
            continue;
        }
        
        Channel* channel = getChannel(channelName);
        
        if (!channel) {
            channel = createChannel(channelName, client);
        } else {
            if (channel->isMember(client)) {
                continue;
            }
            if (channel->getInviteOnly() && !channel->isInvited(client)) {
                client->queueMessage("473 " + channelName + " :Cannot join channel (+i)");
                continue;
            }
            if (channel->getHasUserLimit() && 
                channel->getMemberCount() >= channel->getUserLimit()) {
                client->queueMessage("471 " + channelName + " :Cannot join channel (+l)");
                continue;
            }
            if (channel->getHasKey() && key != channel->getKey()) {
                client->queueMessage("475 " + channelName + " :Cannot join channel (+k)");
                continue;
            }
            channel->addMember(client);
        }
        
        // inform all members about joining
        std::string joinMsg = ":" + client->getPrefix() + " JOIN " + channel->getName();
        channel->broadcast(joinMsg, NULL);
        
        // send topic
        if (!channel->getTopic().empty()) {
            client->queueMessage("332 " + client->getNickname() + " " + channel->getName() + 
                              " :" + channel->getTopic());
        }
        
        // send names list
        client->queueMessage(channel->getNamesReply(client->getNickname()));
        client->queueMessage("366 " + client->getNickname() + " " + channel->getName() + 
                          " :End of /NAMES list");
    }
}

void Server::cmdPart(Client* client, const std::vector<std::string>& tokens) {
    if (!client->getRegistered()) {
        client->queueMessage("451 :You have not registered");
        return;
    }
    
    if (tokens.size() < 2) {
        client->queueMessage("461 PART :Not enough parameters");
        return;
    }
    
    std::string reason = (tokens.size() >= 3) ? tokens[2] : client->getNickname();
    
    std::istringstream chanStream(tokens[1]);
    std::string channelName;
    
    while (std::getline(chanStream, channelName, ',')) {
        if (channelName.empty()) continue;
        
        Channel* channel = getChannel(channelName);
        if (!channel) {
            client->queueMessage("403 " + channelName + " :No such channel");
            continue;
        }
        if (!channel->isMember(client)) {
            client->queueMessage("442 " + channelName + " :You're not on that channel");
            continue;
        }
        
        // inform all members about leaving
        std::string partMsg = ":" + client->getPrefix() + " PART " + channel->getName() + " :" + reason;
        channel->broadcast(partMsg, NULL);
        
        channel->removeMember(client);
    }
    
    cleanupEmptyChannels();
}

void Server::cmdPrivmsg(Client* client, const std::vector<std::string>& tokens) {
    if (!client->getRegistered()) {
        client->queueMessage("451 :You have not registered");
        return;
    }
    if (tokens.size() < 2) {
        client->queueMessage("411 :No recipient given (PRIVMSG)");
        return;
    }
    if (tokens.size() < 3) {
        client->queueMessage("412 :No text to send");
        return;
    }
    
    std::string target = tokens[1];
    std::string message = tokens[2];
    
    if (target[0] == '#' || target[0] == '&') {
        Channel* channel = getChannel(target);
        if (!channel) {
            client->queueMessage("403 " + target + " :No such channel");
            return;
        }
        if (!channel->isMember(client)) {
            client->queueMessage("404 " + target + " :Cannot send to channel");
            return;
        }
        
        std::string msg = ":" + client->getPrefix() + " PRIVMSG " + channel->getName() + " :" + message;
        channel->broadcast(msg, client);
    } else {
        Client* targetClient = getClientByNickname(target);
        if (!targetClient) {
            client->queueMessage("401 " + target + " :No such nick/channel");
            return;
        }
        
        std::string msg = ":" + client->getPrefix() + " PRIVMSG " + targetClient->getNickname() + " :" + message;
        targetClient->queueMessage(msg);
    }
}

void Server::cmdKick(Client* client, const std::vector<std::string>& tokens) {
    if (!client->getRegistered()) {
        client->queueMessage("451 :You have not registered");
        return;
    }
    if (tokens.size() < 3) {
        client->queueMessage("461 KICK :Not enough parameters");
        return;
    }
    
    std::string channelName = tokens[1];
    std::string targetNick = tokens[2];
    std::string reason = (tokens.size() >= 4) ? tokens[3] : client->getNickname();
    
    Channel* channel = getChannel(channelName);
    if (!channel) {
        client->queueMessage("403 " + channelName + " :No such channel");
        return;
    }
    if (!channel->isMember(client)) {
        client->queueMessage("442 " + channelName + " :You're not on that channel");
        return;
    }
    if (!channel->isOperator(client)) {
        client->queueMessage("482 " + channelName + " :You're not channel operator");
        return;
    }
    
    Client* targetClient = getClientByNickname(targetNick);
    if (!targetClient || !channel->isMember(targetClient)) {
        client->queueMessage("441 " + targetNick + " " + channelName + " :They aren't on that channel");
        return;
    }
    
    std::string kickMsg = ":" + client->getPrefix() + " KICK " + channel->getName() + " " + 
                         targetClient->getNickname() + " :" + reason;
    channel->broadcast(kickMsg, NULL);
    
    channel->removeMember(targetClient);
    cleanupEmptyChannels();
}

void Server::cmdInvite(Client* client, const std::vector<std::string>& tokens) {
    if (!client->getRegistered()) {
        client->queueMessage("451 :You have not registered");
        return;
    }
    if (tokens.size() < 3) {
        client->queueMessage("461 INVITE :Not enough parameters");
        return;
    }
    
    std::string targetNick = tokens[1];
    std::string channelName = tokens[2];
    
    Channel* channel = getChannel(channelName);
    if (!channel) {
        client->queueMessage("403 " + channelName + " :No such channel");
        return;
    }
    if (!channel->isMember(client)) {
        client->queueMessage("442 " + channelName + " :You're not on that channel");
        return;
    }
    if (channel->getInviteOnly() && !channel->isOperator(client)) {
        client->queueMessage("482 " + channelName + " :You're not channel operator");
        return;
    }
    
    Client* targetClient = getClientByNickname(targetNick);
    if (!targetClient) {
        client->queueMessage("401 " + targetNick + " :No such nick/channel");
        return;
    }
    
    if (channel->isMember(targetClient)) {
        client->queueMessage("443 " + targetNick + " " + channelName + " :is already on channel");
        return;
    }
    
    channel->addToInviteList(targetClient);
    client->queueMessage("341 " + client->getNickname() + " " + targetClient->getNickname() + 
                        " " + channel->getName());
    targetClient->queueMessage(":" + client->getPrefix() + " INVITE " + targetClient->getNickname() + 
                              " :" + channel->getName());
}

void Server::cmdTopic(Client* client, const std::vector<std::string>& tokens) {
    if (!client->getRegistered()) {
        client->queueMessage("451 :You have not registered");
        return;
    }
    if (tokens.size() < 2) {
        client->queueMessage("461 TOPIC :Not enough parameters");
        return;
    }
    
    std::string channelName = tokens[1];
    Channel* channel = getChannel(channelName);
    
    if (!channel) {
        client->queueMessage("403 " + channelName + " :No such channel");
        return;
    }
    if (!channel->isMember(client)) {
        client->queueMessage("442 " + channelName + " :You're not on that channel");
        return;
    }
    
    // view and set topic
    if (tokens.size() == 2) {
        if (channel->getTopic().empty()) {
            client->queueMessage("331 " + client->getNickname() + " " + channel->getName() + 
                              " :No topic is set");
        } else {
            client->queueMessage("332 " + client->getNickname() + " " + channel->getName() + 
                              " :" + channel->getTopic());
        }
    } else {
        if (channel->getTopicRestricted() && !channel->isOperator(client)) {
            client->queueMessage("482 " + channelName + " :You're not channel operator");
            return;
        }
        
        std::string newTopic = tokens[2];
        channel->setTopic(newTopic, client->getNickname());
        
        std::string topicMsg = ":" + client->getPrefix() + " TOPIC " + channel->getName() + " :" + newTopic;
        channel->broadcast(topicMsg, NULL);
    }
}

void Server::cmdMode(Client* client, const std::vector<std::string>& tokens) {
    if (!client->getRegistered()) {
        client->queueMessage("451 :You have not registered");
        return;
    }
    if (tokens.size() < 2) {
        client->queueMessage("461 MODE :Not enough parameters");
        return;
    }
    
    std::string target = tokens[1];
    
    // change mode for channel
    if (target[0] == '#' || target[0] == '&') {
        Channel* channel = getChannel(target);
        
        if (!channel) {
            client->queueMessage("403 " + target + " :No such channel");
            return;
        }
        
        if (tokens.size() == 2) {
            client->queueMessage("324 " + client->getNickname() + " " + channel->getName() + 
                              " " + channel->getModeString());
            return;
        }
        
        if (!channel->isMember(client)) {
            client->queueMessage("442 " + target + " :You're not on that channel");
            return;
        }
        
        if (!channel->isOperator(client)) {
            client->queueMessage("482 " + target + " :You're not channel operator");
            return;
        }
        
        std::string modeStr = tokens[2];
        bool adding = true;
        size_t paramIndex = 3;
        
        std::string appliedModes = "";
        std::string appliedParams = "";
        bool currentAdding = true;
        
        for (size_t i = 0; i < modeStr.length(); i++) {
            char mode = modeStr[i];
            
            if (mode == '+') {
                adding = true;
                if (currentAdding != adding) {
                    appliedModes += '+';
                    currentAdding = adding;
                }
            } else if (mode == '-') {
                adding = false;
                if (currentAdding != adding) {
                    appliedModes += '-';
                    currentAdding = adding;
                }
            } else if (mode == 'i') {
                channel->setInviteOnly(adding);
                if (appliedModes.empty() || (appliedModes[appliedModes.length()-1] != '+' && 
                    appliedModes[appliedModes.length()-1] != '-')) {
                    appliedModes += (adding ? "+" : "-");
                }
                appliedModes += 'i';
            } else if (mode == 't') {
                channel->setTopicRestricted(adding);
                if (appliedModes.empty() || (appliedModes[appliedModes.length()-1] != '+' && 
                    appliedModes[appliedModes.length()-1] != '-')) {
                    appliedModes += (adding ? "+" : "-");
                }
                appliedModes += 't';
            } else if (mode == 'k') {
                if (adding) {
                    if (paramIndex < tokens.size()) {
                        channel->setKey(tokens[paramIndex]);
                        if (appliedModes.empty() || (appliedModes[appliedModes.length()-1] != '+' && 
                            appliedModes[appliedModes.length()-1] != '-')) {
                            appliedModes += "+";
                        }
                        appliedModes += 'k';
                        appliedParams += " " + tokens[paramIndex];
                        paramIndex++;
                    }
                } else {
                    channel->setKey("");
                    if (appliedModes.empty() || (appliedModes[appliedModes.length()-1] != '+' && 
                        appliedModes[appliedModes.length()-1] != '-')) {
                        appliedModes += "-";
                    }
                    appliedModes += 'k';
                }
            } else if (mode == 'o') {
                if (paramIndex < tokens.size()) {
                    Client* targetClient = getClientByNickname(tokens[paramIndex]);
                    if (targetClient && channel->isMember(targetClient)) {
                        if (adding) {
                            channel->addOperator(targetClient);
                        } else {
                            channel->removeOperator(targetClient);
                        }
                        if (appliedModes.empty() || (appliedModes[appliedModes.length()-1] != '+' && 
                            appliedModes[appliedModes.length()-1] != '-')) {
                            appliedModes += (adding ? "+" : "-");
                        }
                        appliedModes += 'o';
                        appliedParams += " " + tokens[paramIndex];
                    }
                    paramIndex++;
                }
            } else if (mode == 'l') {
                if (adding) {
                    if (paramIndex < tokens.size()) {
                        int limit = std::atoi(tokens[paramIndex].c_str());
                        if (limit > 0) {
                            channel->setUserLimit(limit);
                            if (appliedModes.empty() || (appliedModes[appliedModes.length()-1] != '+' && 
                                appliedModes[appliedModes.length()-1] != '-')) {
                                appliedModes += "+";
                            }
                            appliedModes += 'l';
                            appliedParams += " " + tokens[paramIndex];
                        }
                        paramIndex++;
                    }
                } else {
                    channel->setUserLimit(0);
                    if (appliedModes.empty() || (appliedModes[appliedModes.length()-1] != '+' && 
                        appliedModes[appliedModes.length()-1] != '-')) {
                        appliedModes += "-";
                    }
                    appliedModes += 'l';
                }
            } else {
                client->queueMessage("472 " + std::string(1, mode) + " :is unknown mode char to me");
            }
        }
        
        if (!appliedModes.empty() && appliedModes != "+" && appliedModes != "-") {
            std::string modeMsg = ":" + client->getPrefix() + " MODE " + channel->getName() + 
                                 " " + appliedModes + appliedParams;
            channel->broadcast(modeMsg, NULL);
        }
    // no user modes needed according to subject
    } else {
        client->queueMessage("502 :Cannot change mode for other users");
    }
}

void Server::cmdQuit(Client* client, const std::vector<std::string>& tokens) {
    std::string reason = (tokens.size() >= 2) ? tokens[1] : "Client Quit";
    
    const std::set<Channel*>& joinedChannels = client->getJoinedChannels();
    std::set<Channel*> channelsCopy = joinedChannels;
    
    for (std::set<Channel*>::iterator it = channelsCopy.begin(); 
         it != channelsCopy.end(); ++it) {
        Channel* channel = *it;
        std::string quitMsg = ":" + client->getPrefix() + " QUIT :" + reason;
        channel->broadcast(quitMsg, client);
        channel->removeMember(client);
    }
    
    // Send ERROR to the quitting client
    client->queueMessage("Quitting session: " + client->getHostname() + " (" + reason + ")");
    
    // Mark client for removal - don't call removeClient here!
    // The client will be removed after we finish processing and send the ERROR message
    client->setMarkedForRemoval(true);
    
    cleanupEmptyChannels();
}

void Server::cmdPing(Client* client, const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) {
        client->queueMessage("409 :No origin specified");
        return;
    }
    
    client->queueMessage("PONG " + serverName + " :" + tokens[1]);
}
