/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kbrauer <kbrauer@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/10 15:18:52 by kbrauer           #+#    #+#             */
/*   Updated: 2025/12/20 14:55:06 by kbrauer          ###   ########.fr       */
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
    std::string topicSetBy;
    std::string key;
    
    std::vector<Client*> members;
    std::set<Client*> operators;
    std::set<Client*> inviteList;
    
    // modes
    bool inviteOnly;      // +i
    bool topicRestricted; // +t
    bool hasKey;          // +k
    bool hasUserLimit;    // +l
    size_t userLimit;

public:
    Channel(const std::string& channelName);
    ~Channel();
    
    void addMember(Client* client);
    void removeMember(Client* client);
    bool isMember(Client* client) const;
    size_t getMemberCount() const { return members.size(); }
    bool isEmpty() const { return members.empty(); }
    
    void addOperator(Client* client);
    void removeOperator(Client* client);
    bool isOperator(Client* client) const;
    
    void addToInviteList(Client* client);
    void removeFromInviteList(Client* client);
    bool isInvited(Client* client) const;
    
    void broadcast(const std::string& message, Client* exclude = NULL);
    std::vector<Client*> getMembersWithPendingData(Client* exclude = NULL) const;
    
    // Getters
    const std::string& getName() const ;
    const std::string& getTopic() const;
    const std::string& getTopicSetBy() const;
    const std::string& getKey() const;
    const std::vector<Client*>& getMembers() const;
    bool getInviteOnly() const;
    bool getTopicRestricted() const;
    bool getHasKey() const;
    bool getHasUserLimit() const;
    size_t getUserLimit() const;
    
    // Settlers
    void setTopic(const std::string& newTopic, const std::string& setBy);
    
    void setKey(const std::string& newKey);
    void setInviteOnly(bool mode);
    void setTopicRestricted(bool mode);
    void setUserLimit(size_t limit);
    
    std::string getModeString() const;
    
    std::string getNamesReply(const std::string& requestingNick) const;
};

#endif
