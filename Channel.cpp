/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kbrauer <kbrauer@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/10 15:18:35 by kbrauer           #+#    #+#             */
/*   Updated: 2025/12/10 15:18:36 by kbrauer          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Channel.hpp"
#include "Client.hpp"
#include <algorithm>

// Constructor: Create a new channel
Channel::Channel(const std::string& channelName) 
    : name(channelName), 
      inviteOnly(false), 
      topicRestricted(true),  // By default, only operators can change topic
      hasKey(false), 
      hasUserLimit(false),
      userLimit(0) {
}

Channel::~Channel() {
    // Clean up (members are managed by Server, so we don't delete them)
}

// Add a client to the channel
void Channel::addMember(Client* client) {
    // Check if already a member
    if (!isMember(client)) {
        members.push_back(client);
    }
}

// Remove a client from the channel
void Channel::removeMember(Client* client) {
    // Find and remove from members vector
    std::vector<Client*>::iterator it = std::find(members.begin(), members.end(), client);
    if (it != members.end()) {
        members.erase(it);
    }
    
    // Also remove from operators if they were one
    operators.erase(client);
}

// Check if client is in the channel
bool Channel::isMember(Client* client) const {
    return std::find(members.begin(), members.end(), client) != members.end();
}

// Make a client an operator
void Channel::addOperator(Client* client) {
    if (isMember(client)) {
        operators.insert(client);
    }
}

// Remove operator privileges
void Channel::removeOperator(Client* client) {
    operators.erase(client);
}

// Check if client is an operator
bool Channel::isOperator(Client* client) const {
    return operators.find(client) != operators.end();
}

// Add client to invite list (for invite-only channels)
void Channel::addToInviteList(Client* client) {
    inviteList.insert(client);
}

// Check if client is invited
bool Channel::isInvited(Client* client) const {
    return inviteList.find(client) != inviteList.end();
}

// Send a message to all channel members
// exclude: optional client to skip (usually the sender)
void Channel::broadcast(const std::string& message, Client* exclude) {
    for (size_t i = 0; i < members.size(); i++) {
        // Skip the excluded client (don't echo back to sender)
        if (members[i] != exclude) {
            members[i]->sendMessage(message);
        }
    }
}