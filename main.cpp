/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kbrauer <kbrauer@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/10 15:17:59 by kbrauer           #+#    #+#             */
/*   Updated: 2025/12/17 17:39:00 by kbrauer          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include <iostream>
#include <cstdlib>
#include <csignal>
#include <cerrno>
#include <climits>

// Global server pointer for signal handler
Server* g_server = NULL;

// Signal handler for clean shutdown (Ctrl+C)
void signalHandler(int signal) {
    (void)signal;
    std::cout << "\nReceived signal, shutting down server..." << std::endl;
    if (g_server) {
        g_server->stop();
    }
}

// Validate port number
bool isValidPort(const char* portStr, int& port) {
    char* endptr;
    errno = 0;
    long val = std::strtol(portStr, &endptr, 10);
    
    // Check for conversion errors
    if (errno != 0 || *endptr != '\0' || endptr == portStr) {
        return false;
    }
    
    // Check range (1-65535, but typically > 1024 for non-root)
    if (val <= 0 || val > 65535) {
        return false;
    }
    
    port = static_cast<int>(val);
    return true;
}

int main(int argc, char* argv[]) {
    // Check arguments
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <port> <password>" << std::endl;
        return 1;
    }
    
    // Parse and validate port
    int port;
    if (!isValidPort(argv[1], port)) {
        std::cerr << "Error: Invalid port number '" << argv[1] << "'" << std::endl;
        std::cerr << "Port must be a number between 1 and 65535" << std::endl;
        return 1;
    }
    
    // Get password
    std::string password = argv[2];
    if (password.empty()) {
        std::cerr << "Error: Password cannot be empty" << std::endl;
        return 1;
    }
    
    // Setup signal handlers using sigaction (more robust than signal())
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
    
    // Ignore SIGPIPE (happens when writing to closed socket)
    struct sigaction sa_pipe;
    sa_pipe.sa_handler = SIG_IGN;
    sigemptyset(&sa_pipe.sa_mask);
    sa_pipe.sa_flags = 0;
    if (sigaction(SIGPIPE, &sa_pipe, NULL) == -1) {
        std::cerr << "Error: Failed to ignore SIGPIPE" << std::endl;
        return 1;
    }
    
    try {
        Server server(port, password);
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
