/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kbrauer <kbrauer@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/10 15:20:44 by kbrauer           #+#    #+#             */
/*   Updated: 2025/12/10 15:20:45 by kbrauer          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>

// Constructor: Initialize a new client with their socket
Client::Client(int fd) : socketFd(fd), isAuthenticated(false), isRegistered(false) {
    // Client starts unauthenticated and unregistered
    // They need to provide password (PASS), nickname (NICK), and username (USER)
}

Client::~Client() {
    // Close the socket when client object is destroyed
    close(socketFd);
}

// Send a message to this client
void Client::sendMessage(const std::string& message) {
    // IRC protocol requires messages to end with \r\n (carriage return + newline)
    std::string fullMessage = message;
    if (fullMessage.empty() || fullMessage[fullMessage.size() - 1] != '\n') {
        fullMessage += "\r\n";
    }
    
    // send() transmits data through the socket
    // Parameters:
    //   - socketFd: the client's socket
    //   - data pointer: what to send
    //   - length: how many bytes
    //   - flags: 0 for default behavior
    ssize_t bytesSent = send(socketFd, fullMessage.c_str(), fullMessage.length(), 0);
    
    if (bytesSent < 0) {
        std::cerr << "Error sending message to client " << nickname << std::endl;
    }
}

// Get the client's prefix for IRC messages
// Format: nickname!username@hostname
// Example: john!~john@192.168.1.1
std::string Client::getPrefix() const {
    std::string prefix = nickname;
    if (!username.empty()) {
        prefix += "!" + username;
    }
    if (!hostname.empty()) {
        prefix += "@" + hostname;
    }
    return prefix;
}