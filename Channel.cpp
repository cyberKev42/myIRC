/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kbrauer <kbrauer@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/10 15:18:35 by kbrauer           #+#    #+#             */
/*   Updated: 2025/12/12 10:00:00 by kbrauer          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Channel.hpp"
#include "Client.hpp"
#include <algorithm>
#include <sstream>

Channel::Channel(const std::string& channelName) 
    : name(channelName), 
      inviteOnly(false), 
      topicRestricted(true),
      hasKey(false), 
      hasUserLimit(false),
      userLimit(0) {
}

Channel::~Channel() {
}

void Channel::addMember(Client* client) {
    if (!isMember(client)) {
        members.push_back(client);
        client->addChannel(this);
        // Remove from invite list once they've joined
        inviteList.erase(client);
    }
}

void Channel::removeMember(Client* client) {
    std::vector<Client*>::iterator it = std::find(members.begin(), members.end(), client);
    if (it != members.end()) {
        members.erase(it);
        client->removeChannel(this);
    }
    operators.erase(client);
    inviteList.erase(client);
}

bool Channel::isMember(Client* client) const {
    return std::find(members.begin(), members.end(), client) != members.end();
}

void Channel::addOperator(Client* client) {
    if (isMember(client)) {
        operators.insert(client);
    }
}

void Channel::removeOperator(Client* client) {
    operators.erase(client);
}

bool Channel::isOperator(Client* client) const {
    return operators.find(client) != operators.end();
}

void Channel::addToInviteList(Client* client) {
    inviteList.insert(client);
}

void Channel::removeFromInviteList(Client* client) {
    inviteList.erase(client);
}

bool Channel::isInvited(Client* client) const {
    return inviteList.find(client) != inviteList.end();
}

void Channel::broadcast(const std::string& message, Client* exclude) {
    for (size_t i = 0; i < members.size(); i++) {
        if (members[i] != exclude) {
            members[i]->queueMessage(message);
        }
    }
}

std::string Channel::getModeString() const {
    std::string modes = "+";
    std::string params = "";
    
    if (inviteOnly) modes += "i";
    if (topicRestricted) modes += "t";
    if (hasKey) {
        modes += "k";
        // Don't show key value in mode query for security
    }
    if (hasUserLimit) {
        modes += "l";
        std::ostringstream oss;
        oss << userLimit;
        params += " " + oss.str();
    }
    
    if (modes == "+") {
        return "+";  // No modes set
    }
    
    return modes + params;
}

std::string Channel::getNamesReply(const std::string& requestingNick) const {
    std::string result = "353 " + requestingNick + " = " + name + " :";
    
    for (size_t i = 0; i < members.size(); i++) {
        if (i > 0) result += " ";
        if (isOperator(members[i])) {
            result += "@";
        }
        result += members[i]->getNickname();
    }
    
    return result;
}
