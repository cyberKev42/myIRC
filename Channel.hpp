/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mvolgger <mvolgger@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/10 15:18:52 by kbrauer           #+#    #+#             */
/*   Updated: 2025/12/19 18:23:50 by mvolgger         ###   ########.fr       */
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
    std::string name;
    std::string topic;
    std::string topicSetBy;        // Who set the topic
    std::string key;
    
    std::vector<Client*> members;
    std::set<Client*> operators;
    std::set<Client*> inviteList;
    
    // Channel modes
    bool inviteOnly;      // +i
    bool topicRestricted; // +t
    bool hasKey;          // +k
    bool hasUserLimit;    // +l
    size_t userLimit;

public:
    Channel(const std::string& channelName);
    ~Channel();
    
    // Member management
    void addMember(Client* client);
    void removeMember(Client* client);
    bool isMember(Client* client) const;
    size_t getMemberCount() const { return members.size(); }
    bool isEmpty() const { return members.empty(); }
    
    // Operator management
    void addOperator(Client* client);
    void removeOperator(Client* client);
    bool isOperator(Client* client) const;
    
    // Invite management
    void addToInviteList(Client* client);
    void removeFromInviteList(Client* client);
    bool isInvited(Client* client) const;
    
    // Broadcasting
    void broadcast(const std::string& message, Client* exclude = NULL);
    std::vector<Client*> getMembersWithPendingData(Client* exclude = NULL) const;
    
    // Getters
    const std::string& getName() const { return name; }
    const std::string& getTopic() const { return topic; }
    const std::string& getTopicSetBy() const { return topicSetBy; }
    const std::string& getKey() const { return key; }
    const std::vector<Client*>& getMembers() const { return members; }
    bool getInviteOnly() const { return inviteOnly; }
    bool getTopicRestricted() const { return topicRestricted; }
    bool getHasKey() const { return hasKey; }
    bool getHasUserLimit() const { return hasUserLimit; }
    size_t getUserLimit() const { return userLimit; }
    
    // Setters
    void setTopic(const std::string& newTopic, const std::string& setBy) { 
        topic = newTopic; 
        topicSetBy = setBy;
    }
    void setKey(const std::string& newKey) { 
        key = newKey; 
        hasKey = !newKey.empty(); 
    }
    void setInviteOnly(bool mode) { inviteOnly = mode; }
    void setTopicRestricted(bool mode) { topicRestricted = mode; }
    void setUserLimit(size_t limit) { 
        userLimit = limit; 
        hasUserLimit = (limit > 0); 
    }
    
    // Get mode string for MODE query
    std::string getModeString() const;
    
    // Get names list for NAMES reply
    std::string getNamesReply(const std::string& requestingNick) const;
};

#endif
