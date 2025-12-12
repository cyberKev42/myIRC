/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kbrauer <kbrauer@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/10 15:18:52 by kbrauer           #+#    #+#             */
/*   Updated: 2025/12/10 15:18:53 by kbrauer          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <vector>
#include <set>

class Client;

class Channel {
private:
    std::string name;                      // Channel name (e.g., #general)
    std::string topic;                     // Channel topic
    std::string key;                       // Channel password (if any)
    
    std::vector<Client*> members;          // All clients in channel
    std::set<Client*> operators;           // Clients with operator privileges
    std::set<Client*> inviteList;          // Invited clients (for invite-only)
    
    // Channel modes
    bool inviteOnly;                       // +i mode
    bool topicRestricted;                  // +t mode (only ops can change topic)
    bool hasKey;                           // +k mode (password protected)
    bool hasUserLimit;                     // +l mode
    int userLimit;                         // Max number of users

public:
    Channel(const std::string& channelName);
    ~Channel();
    
    // Member management
    void addMember(Client* client);
    void removeMember(Client* client);
    bool isMember(Client* client) const;
    
    // Operator management
    void addOperator(Client* client);
    void removeOperator(Client* client);
    bool isOperator(Client* client) const;
    
    // Invite management
    void addToInviteList(Client* client);
    bool isInvited(Client* client) const;
    
    // Broadcasting
    void broadcast(const std::string& message, Client* exclude = NULL);
    
    // Getters
    const std::string& getName() const { return name; }
    const std::string& getTopic() const { return topic; }
    const std::string& getKey() const { return key; }
    const std::vector<Client*>& getMembers() const { return members; }
    bool getInviteOnly() const { return inviteOnly; }
    bool getTopicRestricted() const { return topicRestricted; }
    bool getHasKey() const { return hasKey; }
    bool getHasUserLimit() const { return hasUserLimit; }
    int getUserLimit() const { return userLimit; }
    
    // Setters
    void setTopic(const std::string& newTopic) { topic = newTopic; }
    void setKey(const std::string& newKey) { key = newKey; hasKey = !newKey.empty(); }
    void setInviteOnly(bool mode) { inviteOnly = mode; }
    void setTopicRestricted(bool mode) { topicRestricted = mode; }
    void setUserLimit(int limit) { userLimit = limit; hasUserLimit = (limit > 0); }
};

#endif