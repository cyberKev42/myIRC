/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kbrauer <kbrauer@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/10 15:17:59 by kbrauer           #+#    #+#             */
/*   Updated: 2025/12/19 18:01:58 by kbrauer          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include <iostream>
#include <cstdlib>
#include <csignal>
#include <cerrno>
#include <climits>

Server* g_server = NULL;

void signalHandler(int signal) {
    (void)signal;
    std::cout << "\nReceived signal, shutting down server..." << std::endl;
    if (g_server) {
        g_server->stop();
    }
}

bool isValidPort(const char* portStr, int& port) {
    char* endptr;
    long val = std::strtol(portStr, &endptr, 10);
    
    if (*endptr != '\0')
        return false;
    
    port = static_cast<int>(val);
    return (port > 0 && port <= 65535);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <port> <password>" << std::endl;
        return 1;
    }
    
    int port;
    if (!isValidPort(argv[1], port)) {
        std::cerr << "Error: Invalid port number '" << argv[1] << "'" << std::endl;
        std::cerr << "Port must be a number between 1 and 65535" << std::endl;
        return 1;
    }
    
    std::string password = argv[2];
    if (password.empty()) {
        std::cerr << "Error: Password cannot be empty" << std::endl;
        return 1;
    }
    
    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        std::cerr << "Error: Failed to set SIGINT handler" << std::endl;
        return 1;
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        std::cerr << "Error: Failed to set SIGTERM handler" << std::endl;
        return 1;
    }
    
    // ignore problems that happen when writing to closed socketFD
    struct sigaction sa_pipe;
    sa_pipe.sa_handler = SIG_IGN;
    sigemptyset(&sa_pipe.sa_mask);
    sa_pipe.sa_flags = 0;
    if (sigaction(SIGPIPE, &sa_pipe, NULL) == -1) {
        std::cerr << "Error: Failed to ignore SIGPIPE" << std::endl;
        return 1;
    }
    
    try {
        static Server server(port, password);
        g_server = &server;
        
        std::cout << "Starting IRC server on port " << port << std::endl;
        server.start();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "Server stopped." << std::endl;
    return 0;
}
