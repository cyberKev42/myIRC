/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mvolgger <mvolgger@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/19 18:04:24 by mvolgger          #+#    #+#             */
/*   Updated: 2025/12/19 18:04:27 by mvolgger         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */



#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <map>
#include <poll.h>
#include <netinet/in.h>

/*
001 RPL_WELCOME
002 RPL_YOURHOST
003 RPL_CREATED
004 RPL_MYINFO
324 RPL_CHANNELMODEIS
331 RPL_NOTOPIC
332 RPL_TOPIC
341 RPL_INVITING
353 RPL_NAMREPLY
366 RPL_ENDOFNAMES
372 RPL_MOTD
375 RPL_MOTDSTART
376 RPL_ENDOFMOTD
401 ERR_NOSUCHNICK
403 ERR_NOSUCHCHANNEL
404 ERR_CANNOTSENDTOCHAN
411 ERR_NORECIPIENT
412 ERR_NOTEXTTOSEND
421 ERR_UNKNOWNCOMMAND
431 ERR_NONICKNAMEGIVEN
432 ERR_ERRONEUSNICKNAME
433 ERR_NICKNAMEINUSE
441 ERR_USERNOTINCHANNEL
442 ERR_NOTONCHANNEL
443 ERR_USERONCHANNEL
451 ERR_NOTREGISTERED
461 ERR_NEEDMOREPARAMS
462 ERR_ALREADYREGISTRED
464 ERR_PASSWDMISMATCH
471 ERR_CHANNELISFULL
472 ERR_UNKNOWNMODE
473 ERR_INVITEONLYCHAN
475 ERR_BADCHANNELKEY
482 ERR_CHANOPRIVSNEEDED
*/


class Client;
class Channel;

class Server {
private:
    int serverSocket;
    int port;
    std::string password;
    std::string serverName;
    
    std::vector<struct pollfd> pollFds;
    std::map<int, Client*> clients;
    std::map<std::string, Channel*> channels;
    
    bool isRunning;

    void setupServerSocket();
    void acceptNewClient();
    void handleClientData(int clientFd);
    void handleClientWrite(int clientFd);
    void removeClient(int clientFd);
    void parseCommand(Client* client, const std::string& message);
    
    bool isValidNickname(const std::string& nick) const;
    bool isValidChannelName(const std::string& name) const;
    
    // commands
    void cmdPass(Client* client, const std::vector<std::string>& tokens);
    void cmdNick(Client* client, const std::vector<std::string>& tokens);
    void cmdUser(Client* client, const std::vector<std::string>& tokens);
    void cmdJoin(Client* client, const std::vector<std::string>& tokens);
    void cmdPart(Client* client, const std::vector<std::string>& tokens);
    void cmdPrivmsg(Client* client, const std::vector<std::string>& tokens);
    void cmdKick(Client* client, const std::vector<std::string>& tokens);
    void cmdInvite(Client* client, const std::vector<std::string>& tokens);
    void cmdTopic(Client* client, const std::vector<std::string>& tokens);
    void cmdMode(Client* client, const std::vector<std::string>& tokens);
    void cmdQuit(Client* client, const std::vector<std::string>& tokens);
    void cmdPing(Client* client, const std::vector<std::string>& tokens);
    
    void tryCompleteRegistration(Client* client);
    void updatePollEvents(int fd, short events);
    void sendAllData();
    void removeMarkedClients();
    void cleanupEmptyChannels();
    
public:
    Server(int port, const std::string& password);
    ~Server();
    
    void start();
    void stop();
    
    const std::string& getPassword() const;
    const std::string& getServerName() const;
    Client* getClientByNickname(const std::string& nickname);
    Channel* getChannel(const std::string& channelName);
    Channel* createChannel(const std::string& channelName, Client* creator);
};

#endif
