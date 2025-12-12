/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kbrauer <kbrauer@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/10 15:17:50 by kbrauer           #+#    #+#             */
/*   Updated: 2025/12/10 15:23:57 by kbrauer          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <cerrno>

// Constructor: Initialize server with port and password
Server::Server(int port, const std::string& password) 
    : serverSocket(-1), port(port), password(password), isRunning(false) {
}

// Destructor: Clean up resources
Server::~Server() {
    // Close all client connections
    for (std::map<int, Client*>::iterator it = clients.begin(); it != clients.end(); ++it) {
        delete it->second;
    }
    
    // Delete all channels
    for (std::map<std::string, Channel*>::iterator it = channels.begin(); it != channels.end(); ++it) {
        delete it->second;
    }
    
    // Close server socket
    if (serverSocket != -1) {
        close(serverSocket);
    }
}

// Setup the server socket - THE HEART OF THE SERVER
void Server::setupServerSocket() {
    // STEP 1: Create a socket
    // AF_INET = IPv4, SOCK_STREAM = TCP, 0 = default protocol
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        throw std::runtime_error("Failed to create socket");
    }
    
    // STEP 2: Set socket options
    // SO_REUSEADDR allows reusing the address immediately after server restarts
    // Without this, you'd have to wait ~2 minutes after closing the server
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(serverSocket);
        throw std::runtime_error("Failed to set socket options");
    }
    
    // STEP 3: Set socket to non-blocking mode
    // fcntl = file control, F_SETFL = set flags, O_NONBLOCK = non-blocking
    // This means accept(), recv(), send() won't block execution
    if (fcntl(serverSocket, F_SETFL, O_NONBLOCK) < 0) {
        close(serverSocket);
        throw std::runtime_error("Failed to set non-blocking mode");
    }
    
    // STEP 4: Create address structure
    struct sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));  // Zero out the structure
    serverAddr.sin_family = AF_INET;                  // IPv4
    serverAddr.sin_addr.s_addr = INADDR_ANY;          // Listen on all interfaces (0.0.0.0)
    serverAddr.sin_port = htons(port);                // Convert port to network byte order
    
    // STEP 5: Bind socket to address and port
    // This associates our socket with a specific port
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        close(serverSocket);
        throw std::runtime_error("Failed to bind socket");
    }
    
    // STEP 6: Start listening for connections
    // Backlog of 10 = up to 10 pending connections can wait in queue
    if (listen(serverSocket, 10) < 0) {
        close(serverSocket);
        throw std::runtime_error("Failed to listen on socket");
    }
    
    // STEP 7: Add server socket to poll array
    // pollfd is a structure that tells poll() what to monitor
    struct pollfd serverPollFd;
    serverPollFd.fd = serverSocket;       // File descriptor to monitor
    serverPollFd.events = POLLIN;         // We want to know about incoming data/connections
    serverPollFd.revents = 0;             // Returned events (poll fills this in)
    pollFds.push_back(serverPollFd);
    
    std::cout << "Server listening on port " << port << std::endl;
}

// Accept a new client connection
void Server::acceptNewClient() {
    struct sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);
    
    // accept() creates a NEW socket for the client
    // The server socket keeps listening, the new socket handles this specific client
    int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
    
    if (clientSocket < 0) {
        if (errno != EWOULDBLOCK && errno != EAGAIN) {
            std::cerr << "Error accepting client" << std::endl;
        }
        return;
    }
    
    // Set client socket to non-blocking
    if (fcntl(clientSocket, F_SETFL, O_NONBLOCK) < 0) {
        std::cerr << "Failed to set client socket to non-blocking" << std::endl;
        close(clientSocket);
        return;
    }
    
    // Create Client object
    Client* newClient = new Client(clientSocket);
    clients[clientSocket] = newClient;
    
    // Add client socket to poll array
    struct pollfd clientPollFd;
    clientPollFd.fd = clientSocket;
    clientPollFd.events = POLLIN;  // Monitor for incoming data
    clientPollFd.revents = 0;
    pollFds.push_back(clientPollFd);
    
    std::cout << "New client connected: fd " << clientSocket << std::endl;
}

// Main server loop
void Server::start() {
    setupServerSocket();
    isRunning = true;
    
    std::cout << "Server started. Waiting for connections..." << std::endl;
    
    // THE MAIN EVENT LOOP
    while (isRunning) {
        // poll() waits for events on our file descriptors
        // Parameters:
        //   - array of pollfd structures
        //   - number of fds
        //   - timeout in milliseconds (-1 = wait forever, 0 = return immediately)
        int pollCount = poll(&pollFds[0], pollFds.size(), -1);
        
        if (pollCount < 0) {
            std::cerr << "Poll error" << std::endl;
            break;
        }
        
        // Check each file descriptor for events
        for (size_t i = 0; i < pollFds.size(); i++) {
            // revents tells us what happened
            if (pollFds[i].revents & POLLIN) {
                // POLLIN = data available to read
                
                if (pollFds[i].fd == serverSocket) {
                    // New connection on server socket
                    acceptNewClient();
                } else {
                    // Data from existing client
                    handleClientData(pollFds[i].fd);
                }
            }
        }
    }
}

void Server::stop() {
    isRunning = false;
}

// Handle incoming data from a client
void Server::handleClientData(int clientFd) {
    Client* client = clients[clientFd];
    if (!client) return;
    
    // Buffer to receive data
    char buffer[512];  // IRC messages are max 512 bytes
    std::memset(buffer, 0, sizeof(buffer));
    
    // recv() reads data from the socket
    // Parameters:
    //   - socket fd
    //   - buffer to store data
    //   - max bytes to read
    //   - flags (0 = default)
    // Returns: number of bytes read, 0 if connection closed, -1 on error
    ssize_t bytesRead = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytesRead <= 0) {
        // bytesRead == 0: client disconnected gracefully
        // bytesRead < 0: error (in non-blocking mode, EAGAIN/EWOULDBLOCK are normal)
        if (bytesRead == 0) {
            std::cout << "Client " << clientFd << " disconnected" << std::endl;
        } else if (errno != EWOULDBLOCK && errno != EAGAIN) {
            std::cerr << "Error reading from client " << clientFd << std::endl;
        }
        removeClient(clientFd);
        return;
    }
    
    // Add received data to client's buffer
    client->getBuffer() += std::string(buffer, bytesRead);
    
    // Process complete messages (lines ending with \n)
    std::string& inputBuffer = client->getBuffer();
    size_t pos;
    
    // Keep extracting complete lines
    while ((pos = inputBuffer.find('\n')) != std::string::npos) {
        // Extract one complete line
        std::string line = inputBuffer.substr(0, pos);
        
        // Remove the processed line from buffer
        inputBuffer.erase(0, pos + 1);
        
        // Remove trailing \r if present (IRC uses \r\n)
        if (!line.empty() && line[line.size() - 1] == '\r') {
            line.erase(line.size() - 1);
        }
        
        // Skip empty lines
        if (line.empty()) continue;
        
        std::cout << "Received from client " << clientFd << ": " << line << std::endl;
        
        // Parse and execute the command
        parseCommand(client, line);
    }
}

// Parse and execute an IRC command
void Server::parseCommand(Client* client, const std::string& message) {
    // Split message into tokens
    std::vector<std::string> tokens;
    std::istringstream iss(message);
    std::string token;
    
    // Parse until we hit a ':' (which indicates "rest of line" parameter)
    while (iss >> token) {
        if (token[0] == ':') {
            // Everything after ':' is one parameter (with spaces)
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
    
    // Get command (first token, uppercase)
    std::string command = tokens[0];
    for (size_t i = 0; i < command.length(); i++) {
        command[i] = std::toupper(command[i]);
    }
    
    // Route to appropriate handler
    if (command == "PASS") {
        handlePass(client, tokens);
    } else if (command == "NICK") {
        handleNick(client, tokens);
    } else if (command == "USER") {
        handleUser(client, tokens);
    } else if (command == "JOIN") {
        handleJoin(client, tokens);
    } else if (command == "PRIVMSG") {
        handlePrivmsg(client, tokens);
    } else if (command == "KICK") {
        handleKick(client, tokens);
    } else if (command == "INVITE") {
        handleInvite(client, tokens);
    } else if (command == "TOPIC") {
        handleTopic(client, tokens);
    } else if (command == "MODE") {
        handleMode(client, tokens);
    } else {
        // Unknown command
        client->sendMessage("421 " + client->getNickname() + " " + command + " :Unknown command");
    }
}

// Remove a client and clean up
void Server::removeClient(int clientFd) {
    // Find client
    std::map<int, Client*>::iterator it = clients.find(clientFd);
    if (it == clients.end()) return;
    
    Client* client = it->second;
    
    // Remove from all channels
    for (std::map<std::string, Channel*>::iterator chanIt = channels.begin(); 
         chanIt != channels.end(); ++chanIt) {
        Channel* channel = chanIt->second;
        if (channel->isMember(client)) {
            // Notify channel members
            std::string quitMsg = ":" + client->getPrefix() + " QUIT :Client disconnected";
            channel->broadcast(quitMsg, client);
            channel->removeMember(client);
        }
    }
    
    // Remove from poll array
    for (std::vector<struct pollfd>::iterator pollIt = pollFds.begin(); 
         pollIt != pollFds.end(); ++pollIt) {
        if (pollIt->fd == clientFd) {
            pollFds.erase(pollIt);
            break;
        }
    }
    
    // Delete client object
    delete client;
    clients.erase(it);
    
    std::cout << "Client " << clientFd << " removed" << std::endl;
}

// Helper functions for Server
Client* Server::getClientByNickname(const std::string& nickname) {
    for (std::map<int, Client*>::iterator it = clients.begin(); it != clients.end(); ++it) {
        if (it->second->getNickname() == nickname) {
            return it->second;
        }
    }
    return NULL;
}

Channel* Server::getChannel(const std::string& channelName) {
    std::map<std::string, Channel*>::iterator it = channels.find(channelName);
    if (it != channels.end()) {
        return it->second;
    }
    return NULL;
}

Channel* Server::createChannel(const std::string& channelName, Client* creator) {
    Channel* channel = new Channel(channelName);
    channels[channelName] = channel;
    channel->addMember(creator);
    channel->addOperator(creator);  // Channel creator is operator
    return channel;
}

// ============================================================================
// IRC COMMAND HANDLERS
// ============================================================================

// PASS command: Authenticate with server password
// Usage: PASS <password>
void Server::handlePass(Client* client, const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) {
        client->sendMessage("461 PASS :Not enough parameters");
        return;
    }
    
    if (client->getRegistered()) {
        client->sendMessage("462 :You may not reregister");
        return;
    }
    
    // Check password
    if (tokens[1] == password) {
        client->setAuthenticated(true);
    } else {
        client->sendMessage("464 :Password incorrect");
    }
}

// NICK command: Set nickname
// Usage: NICK <nickname>
void Server::handleNick(Client* client, const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) {
        client->sendMessage("431 :No nickname given");
        return;
    }
    
    std::string newNick = tokens[1];
    
    // Check if nickname is already in use
    if (getClientByNickname(newNick) != NULL) {
        client->sendMessage("433 " + newNick + " :Nickname is already in use");
        return;
    }
    
    // Set nickname
    std::string oldNick = client->getNickname();
    client->setNickname(newNick);
    
    // If already registered, notify about nick change
    if (client->getRegistered()) {
        std::string msg = ":" + oldNick + " NICK :" + newNick;
        client->sendMessage(msg);
    }
    
    // Check if registration is complete
    tryCompleteRegistration(client);
}

// USER command: Set username and realname
// Usage: USER <username> <mode> <unused> :<realname>
void Server::handleUser(Client* client, const std::vector<std::string>& tokens) {
    if (tokens.size() < 5) {
        client->sendMessage("461 USER :Not enough parameters");
        return;
    }
    
    if (client->getRegistered()) {
        client->sendMessage("462 :You may not reregister");
        return;
    }
    
    client->setUsername(tokens[1]);
    client->setRealname(tokens[4]);  // tokens[4] is the :realname part
    
    // Check if registration is complete
    tryCompleteRegistration(client);
}

// Check if client can complete registration (has PASS, NICK, and USER)
void Server::tryCompleteRegistration(Client* client) {
    if (!client->getAuthenticated()) {
        return;  // Need password first
    }
    
    if (client->getNickname().empty() || client->getUsername().empty()) {
        return;  // Need both NICK and USER
    }
    
    if (client->getRegistered()) {
        return;  // Already registered
    }
    
    // Complete registration!
    client->setRegistered(true);
    
    // Send welcome messages (IRC numerics)
    std::string nick = client->getNickname();
    client->sendMessage("001 " + nick + " :Welcome to the IRC Network " + client->getPrefix());
    client->sendMessage("002 " + nick + " :Your host is ircserv, running version 1.0");
    client->sendMessage("003 " + nick + " :This server was created just now");
    client->sendMessage("004 " + nick + " :ircserv 1.0 io itklmo");
}

// JOIN command: Join a channel
// Usage: JOIN <channel>
void Server::handleJoin(Client* client, const std::vector<std::string>& tokens) {
    if (!client->getRegistered()) {
        client->sendMessage("451 :You have not registered");
        return;
    }
    
    if (tokens.size() < 2) {
        client->sendMessage("461 JOIN :Not enough parameters");
        return;
    }
    
    std::string channelName = tokens[1];
    
    // Channel names must start with #
    if (channelName[0] != '#') {
        client->sendMessage("403 " + channelName + " :No such channel");
        return;
    }
    
    // Get or create channel
    Channel* channel = getChannel(channelName);
    if (!channel) {
        channel = createChannel(channelName, client);
    } else {
        // Channel exists, check if we can join
        
        // Check if invite-only
        if (channel->getInviteOnly() && !channel->isInvited(client)) {
            client->sendMessage("473 " + channelName + " :Cannot join channel (+i)");
            return;
        }
        
        // Check user limit
        if (channel->getHasUserLimit() && 
            channel->getMembers().size() >= (size_t)channel->getUserLimit()) {
            client->sendMessage("471 " + channelName + " :Cannot join channel (+l)");
            return;
        }
        
        // Check channel key (password)
        if (channel->getHasKey()) {
            if (tokens.size() < 3 || tokens[2] != channel->getKey()) {
                client->sendMessage("475 " + channelName + " :Cannot join channel (+k)");
                return;
            }
        }
        
        // Add to channel
        channel->addMember(client);
    }
    
    // Send JOIN message to all members
    std::string joinMsg = ":" + client->getPrefix() + " JOIN " + channelName;
    channel->broadcast(joinMsg, NULL);  // NULL = send to everyone including sender
    
    // Send topic if exists
    if (!channel->getTopic().empty()) {
        client->sendMessage("332 " + client->getNickname() + " " + channelName + 
                          " :" + channel->getTopic());
    }
    
    // Send names list (members)
    std::string namesList = "353 " + client->getNickname() + " = " + channelName + " :";
    const std::vector<Client*>& members = channel->getMembers();
    for (size_t i = 0; i < members.size(); i++) {
        if (channel->isOperator(members[i])) {
            namesList += "@";  // @ prefix for operators
        }
        namesList += members[i]->getNickname();
        if (i < members.size() - 1) {
            namesList += " ";
        }
    }
    client->sendMessage(namesList);
    client->sendMessage("366 " + client->getNickname() + " " + channelName + " :End of /NAMES list");
}

// PRIVMSG command: Send message to user or channel
// Usage: PRIVMSG <target> :<message>
void Server::handlePrivmsg(Client* client, const std::vector<std::string>& tokens) {
    if (!client->getRegistered()) {
        client->sendMessage("451 :You have not registered");
        return;
    }
    
    if (tokens.size() < 3) {
        client->sendMessage("411 :No recipient given (PRIVMSG)");
        return;
    }
    
    std::string target = tokens[1];
    std::string message = tokens[2];
    
    // Check if target is a channel
    if (target[0] == '#') {
        Channel* channel = getChannel(target);
        if (!channel) {
            client->sendMessage("403 " + target + " :No such channel");
            return;
        }
        
        if (!channel->isMember(client)) {
            client->sendMessage("442 " + target + " :You're not on that channel");
            return;
        }
        
        // Broadcast to channel (excluding sender)
        std::string msg = ":" + client->getPrefix() + " PRIVMSG " + target + " :" + message;
        channel->broadcast(msg, client);
    } else {
        // Private message to user
        Client* targetClient = getClientByNickname(target);
        if (!targetClient) {
            client->sendMessage("401 " + target + " :No such nick/channel");
            return;
        }
        
        std::string msg = ":" + client->getPrefix() + " PRIVMSG " + target + " :" + message;
        targetClient->sendMessage(msg);
    }
}

// KICK command: Kick user from channel (operator only)
// Usage: KICK <channel> <user> :<reason>
void Server::handleKick(Client* client, const std::vector<std::string>& tokens) {
    if (!client->getRegistered()) {
        client->sendMessage("451 :You have not registered");
        return;
    }
    
    if (tokens.size() < 3) {
        client->sendMessage("461 KICK :Not enough parameters");
        return;
    }
    
    std::string channelName = tokens[1];
    std::string targetNick = tokens[2];
    std::string reason = (tokens.size() >= 4) ? tokens[3] : "No reason given";
    
    Channel* channel = getChannel(channelName);
    if (!channel) {
        client->sendMessage("403 " + channelName + " :No such channel");
        return;
    }
    
    if (!channel->isMember(client)) {
        client->sendMessage("442 " + channelName + " :You're not on that channel");
        return;
    }
    
    if (!channel->isOperator(client)) {
        client->sendMessage("482 " + channelName + " :You're not channel operator");
        return;
    }
    
    Client* targetClient = getClientByNickname(targetNick);
    if (!targetClient || !channel->isMember(targetClient)) {
        client->sendMessage("441 " + targetNick + " " + channelName + " :They aren't on that channel");
        return;
    }
    
    // Send KICK message to channel
    std::string kickMsg = ":" + client->getPrefix() + " KICK " + channelName + " " + 
                         targetNick + " :" + reason;
    channel->broadcast(kickMsg, NULL);
    
    // Remove from channel
    channel->removeMember(targetClient);
}

// INVITE command: Invite user to channel (operator only for +i channels)
// Usage: INVITE <nickname> <channel>
void Server::handleInvite(Client* client, const std::vector<std::string>& tokens) {
    if (!client->getRegistered()) {
        client->sendMessage("451 :You have not registered");
        return;
    }
    
    if (tokens.size() < 3) {
        client->sendMessage("461 INVITE :Not enough parameters");
        return;
    }
    
    std::string targetNick = tokens[1];
    std::string channelName = tokens[2];
    
    Channel* channel = getChannel(channelName);
    if (!channel) {
        client->sendMessage("403 " + channelName + " :No such channel");
        return;
    }
    
    if (!channel->isMember(client)) {
        client->sendMessage("442 " + channelName + " :You're not on that channel");
        return;
    }
    
    // For invite-only channels, must be operator
    if (channel->getInviteOnly() && !channel->isOperator(client)) {
        client->sendMessage("482 " + channelName + " :You're not channel operator");
        return;
    }
    
    Client* targetClient = getClientByNickname(targetNick);
    if (!targetClient) {
        client->sendMessage("401 " + targetNick + " :No such nick/channel");
        return;
    }
    
    if (channel->isMember(targetClient)) {
        client->sendMessage("443 " + targetNick + " " + channelName + " :is already on channel");
        return;
    }
    
    // Add to invite list
    channel->addToInviteList(targetClient);
    
    // Notify both users
    client->sendMessage("341 " + client->getNickname() + " " + targetNick + " " + channelName);
    targetClient->sendMessage(":" + client->getPrefix() + " INVITE " + targetNick + " :" + channelName);
}

// TOPIC command: View or change channel topic
// Usage: TOPIC <channel> [:<new topic>]
void Server::handleTopic(Client* client, const std::vector<std::string>& tokens) {
    if (!client->getRegistered()) {
        client->sendMessage("451 :You have not registered");
        return;
    }
    
    if (tokens.size() < 2) {
        client->sendMessage("461 TOPIC :Not enough parameters");
        return;
    }
    
    std::string channelName = tokens[1];
    Channel* channel = getChannel(channelName);
    
    if (!channel) {
        client->sendMessage("403 " + channelName + " :No such channel");
        return;
    }
    
    if (!channel->isMember(client)) {
        client->sendMessage("442 " + channelName + " :You're not on that channel");
        return;
    }
    
    if (tokens.size() == 2) {
        // View topic
        if (channel->getTopic().empty()) {
            client->sendMessage("331 " + client->getNickname() + " " + channelName + " :No topic is set");
        } else {
            client->sendMessage("332 " + client->getNickname() + " " + channelName + 
                              " :" + channel->getTopic());
        }
    } else {
        // Set topic
        if (channel->getTopicRestricted() && !channel->isOperator(client)) {
            client->sendMessage("482 " + channelName + " :You're not channel operator");
            return;
        }
        
        std::string newTopic = tokens[2];
        channel->setTopic(newTopic);
        
        // Broadcast topic change
        std::string topicMsg = ":" + client->getPrefix() + " TOPIC " + channelName + " :" + newTopic;
        channel->broadcast(topicMsg, NULL);
    }
}

// MODE command: Change channel modes
// Usage: MODE <channel> <+/-><modes> [parameters]
void Server::handleMode(Client* client, const std::vector<std::string>& tokens) {
    if (!client->getRegistered()) {
        client->sendMessage("451 :You have not registered");
        return;
    }
    
    if (tokens.size() < 2) {
        client->sendMessage("461 MODE :Not enough parameters");
        return;
    }
    
    std::string channelName = tokens[1];
    Channel* channel = getChannel(channelName);
    
    if (!channel) {
        client->sendMessage("403 " + channelName + " :No such channel");
        return;
    }
    
    if (!channel->isMember(client)) {
        client->sendMessage("442 " + channelName + " :You're not on that channel");
        return;
    }
    
    if (tokens.size() == 2) {
        // View modes
        std::string modes = "+";
        if (channel->getInviteOnly()) modes += "i";
        if (channel->getTopicRestricted()) modes += "t";
        if (channel->getHasKey()) modes += "k";
        if (channel->getHasUserLimit()) modes += "l";
        
        client->sendMessage("324 " + client->getNickname() + " " + channelName + " " + modes);
        return;
    }
    
    // Must be operator to change modes
    if (!channel->isOperator(client)) {
        client->sendMessage("482 " + channelName + " :You're not channel operator");
        return;
    }
    
    std::string modeStr = tokens[2];
    bool adding = true;
    size_t paramIndex = 3;
    
    for (size_t i = 0; i < modeStr.length(); i++) {
        char mode = modeStr[i];
        
        if (mode == '+') {
            adding = true;
        } else if (mode == '-') {
            adding = false;
        } else if (mode == 'i') {
            channel->setInviteOnly(adding);
        } else if (mode == 't') {
            channel->setTopicRestricted(adding);
        } else if (mode == 'k') {
            if (adding && paramIndex < tokens.size()) {
                channel->setKey(tokens[paramIndex++]);
            } else if (!adding) {
                channel->setKey("");
            }
        } else if (mode == 'o') {
            if (paramIndex < tokens.size()) {
                Client* targetClient = getClientByNickname(tokens[paramIndex++]);
                if (targetClient && channel->isMember(targetClient)) {
                    if (adding) {
                        channel->addOperator(targetClient);
                    } else {
                        channel->removeOperator(targetClient);
                    }
                }
            }
        } else if (mode == 'l') {
            if (adding && paramIndex < tokens.size()) {
                int limit = std::atoi(tokens[paramIndex++].c_str());
                channel->setUserLimit(limit);
            } else if (!adding) {
                channel->setUserLimit(0);
            }
        }
    }
    
    // Broadcast mode change
    std::string modeMsg = ":" + client->getPrefix() + " MODE " + channelName + " " + modeStr;
    for (size_t i = 3; i < tokens.size(); i++) {
        modeMsg += " " + tokens[i];
    }
    channel->broadcast(modeMsg, NULL);
}