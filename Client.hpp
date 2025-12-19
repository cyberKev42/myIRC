/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kbrauer <kbrauer@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/10 15:19:01 by kbrauer           #+#    #+#             */
/*   Updated: 2025/12/19 15:07:20 by kbrauer          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <set>

class Channel;

class Client {
private:
    int socketFd;
    std::string nickname;
    std::string username;
    std::string realname;
    std::string hostname;
    
    bool isAuthenticated;
    bool isRegistered;
    bool markedForRemoval;        // Flag to mark client for removal after QUIT
    
    std::string inputBuffer;   // Buffer for incomplete incoming messages
    std::string outputBuffer;  // Buffer for pending outgoing messages
    
    std::set<Channel*> joinedChannels;  // Track channels this client is in

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
    bool isMarkedForRemoval() const { return markedForRemoval; }
    std::string& getInputBuffer() { return inputBuffer; }
    std::string& getOutputBuffer() { return outputBuffer; }
    const std::set<Channel*>& getJoinedChannels() const { return joinedChannels; }
    
    // Setters
    void setNickname(const std::string& nick) { nickname = nick; }
    void setUsername(const std::string& user) { username = user; }
    void setRealname(const std::string& real) { realname = real; }
    void setHostname(const std::string& host) { hostname = host; }
    void setAuthenticated(bool auth) { isAuthenticated = auth; }
    void setRegistered(bool reg) { isRegistered = reg; }
    void setMarkedForRemoval(bool mark) { markedForRemoval = mark; }
    
    // Channel tracking
    void addChannel(Channel* channel) { joinedChannels.insert(channel); }
    void removeChannel(Channel* channel) { joinedChannels.erase(channel); }
    
    // Queue message for sending (adds to output buffer)
    void queueMessage(const std::string& message);
    
    // Try to flush output buffer (returns true if buffer is now empty)
    bool sendOutputBuffer();
    
    // Check if there's data waiting to be sent
    bool hasDataToSend() const { return !outputBuffer.empty(); }
    
    // Get client prefix for messages (nickname!username@hostname)
    std::string getPrefix() const;
};

#endif
