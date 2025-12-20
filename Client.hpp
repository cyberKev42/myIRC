/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kbrauer <kbrauer@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/10 15:19:01 by kbrauer           #+#    #+#             */
/*   Updated: 2025/12/20 15:01:01 by kbrauer          ###   ########.fr       */
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
    bool markedForRemoval;
    
    std::string inputBuffer; 
    std::string outputBuffer; 
    
    std::set<Channel*> joinedChannels;

public:
    Client(int fd);
    ~Client();
    
    // Getters
    int getFd() const;
    const std::string& getNickname() const;
    const std::string& getUsername() const;
    const std::string& getRealname() const;
    const std::string& getHostname() const;
    bool getAuthenticated() const;
    bool getRegistered() const;
    bool isMarkedForRemoval() const;
    std::string& getInputBuffer();
    std::string& getOutputBuffer();
    const std::set<Channel*>& getJoinedChannels() const;
    
    // setters
    void setNickname(const std::string& nick);
    void setUsername(const std::string& user);
    void setRealname(const std::string& real);
    void setHostname(const std::string& host);
    void setAuthenticated(bool auth);
    void setRegistered(bool reg);
    void setMarkedForRemoval(bool mark);
    
    void addChannel(Channel* channel);
    void removeChannel(Channel* channel);
    
    void queueMessage(const std::string& message);
    bool sendOutputBuffer();
    bool hasDataToSend() const;
    
    std::string getPrefix() const;
};

#endif
