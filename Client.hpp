/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kbrauer <kbrauer@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/10 15:19:01 by kbrauer           #+#    #+#             */
/*   Updated: 2025/12/10 15:19:03 by kbrauer          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>

class Client {
private:
    int socketFd;                  // Client's socket file descriptor
    std::string nickname;          // Client's nickname
    std::string username;          // Client's username
    std::string realname;          // Client's real name
    std::string hostname;          // Client's hostname
    
    bool isAuthenticated;          // Has provided correct password
    bool isRegistered;             // Has completed registration (NICK + USER)
    
    std::string inputBuffer;       // Buffer for incomplete messages

public:
    Client(int fd);
    ~Client();
    
    // Getters
    int getFd() const { return socketFd; }
    const std::string& getNickname() const { return nickname; }
    const std::string& getUsername() const { return username; }
    const std::string& getRealname() const { return realname; }
    const std::string& getHostname() const { return hostname; }
    bool getAuthenticated() const { return isAuthenticated; }
    bool getRegistered() const { return isRegistered; }
    std::string& getBuffer() { return inputBuffer; }
    
    // Setters
    void setNickname(const std::string& nick) { nickname = nick; }
    void setUsername(const std::string& user) { username = user; }
    void setRealname(const std::string& real) { realname = real; }
    void setHostname(const std::string& host) { hostname = host; }
    void setAuthenticated(bool auth) { isAuthenticated = auth; }
    void setRegistered(bool reg) { isRegistered = reg; }
    
    // Send data to client
    void sendMessage(const std::string& message);
    
    // Get client prefix for messages (nickname!username@hostname)
    std::string getPrefix() const;
};

#endif