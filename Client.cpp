/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kbrauer <kbrauer@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/10 15:20:44 by kbrauer           #+#    #+#             */
/*   Updated: 2025/12/12 10:00:00 by kbrauer          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <cerrno>

Client::Client(int fd) 
    : socketFd(fd), 
      isAuthenticated(false), 
      isRegistered(false),
      markedForRemoval(false) {
}

Client::~Client() {
    close(socketFd);
}

// Queue a message for sending
// This adds to the output buffer instead of sending directly
// The server's poll loop will handle actually sending when the socket is ready
void Client::queueMessage(const std::string& message) {
    std::string fullMessage = message;
    
    // Ensure message ends with \r\n (IRC protocol requirement)
    if (fullMessage.length() < 2 || 
        fullMessage.substr(fullMessage.length() - 2) != "\r\n") {
        // Remove any existing line endings first
        while (!fullMessage.empty() && 
               (fullMessage[fullMessage.length() - 1] == '\r' || 
                fullMessage[fullMessage.length() - 1] == '\n')) {
            fullMessage.erase(fullMessage.length() - 1);
        }
        fullMessage += "\r\n";
    }
    
    outputBuffer += fullMessage;
}

// Try to send data from the output buffer
// Returns true if buffer is now empty (all data sent)
// Returns false if there's still data to send
bool Client::flushOutputBuffer() {
    if (outputBuffer.empty()) {
        return true;
    }
    
    // Try to send as much as possible
    ssize_t bytesSent = send(socketFd, outputBuffer.c_str(), outputBuffer.length(), 0);
    
    if (bytesSent < 0) {
        // EAGAIN/EWOULDBLOCK means socket buffer is full, try again later
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return false;
        }
        // Real error - mark as error but don't crash
        std::cerr << "Error sending to client " << socketFd << ": " << errno << std::endl;
        return false;
    }
    
    if (bytesSent == 0) {
        return false;
    }
    
    // Remove the sent data from buffer
    outputBuffer.erase(0, bytesSent);
    
    return outputBuffer.empty();
}

// Get the client's prefix for IRC messages
// Format: nickname!username@hostname
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
