# ft_irc Server - Technical Documentation

## Implementation Details

- **Programming Language**: C++98
- **Event Mechanism**: `poll()` for I/O multiplexing
- **Implemented IRC Commands**: PASS, NICK, USER, JOIN, PART, PRIVMSG, KICK, INVITE, TOPIC, MODE, QUIT, PING

---

# Table of Contents

1. [High-Level Overview](#1-high-level-overview)
2. [Project Structure & File Responsibilities](#2-project-structure--file-responsibilities)
3. [Server Lifecycle](#3-server-lifecycle)
4. [Networking & Event Handling](#4-networking--event-handling)
5. [Client Handling](#5-client-handling)
6. [IRC Protocol Parsing](#6-irc-protocol-parsing)
7. [Command Dispatching](#7-command-dispatching)
8. [Core IRC Commands](#8-core-irc-commands)
9. [Channels](#9-channels)
10. [Error Handling & Edge Cases](#10-error-handling--edge-cases)
11. [Design Decisions & Trade-offs](#11-design-decisions--trade-offs)
12. [How to Extend the Server](#12-how-to-extend-the-server)

---

# 1. High-Level Overview

## What This IRC Server Does

This is a fully functional Internet Relay Chat (IRC) server implementing the core subset of the IRC protocol as defined in RFC 1459 and RFC 2812. It enables multiple clients to connect simultaneously, authenticate, join channels, and exchange messages in real-time.

## Supported IRC Features

| Feature Category | Supported Capabilities |
|------------------|------------------------|
| **Authentication** | Password-based server authentication |
| **User Identity** | Nickname and username registration |
| **Channels** | Creation, joining, parting, topic management |
| **Channel Modes** | +i (invite-only), +t (topic lock), +k (key), +l (limit), +o (operator) |
| **Messaging** | Channel messages, private messages |
| **Operator Commands** | KICK, INVITE, TOPIC, MODE |
| **Connection** | PING/PONG keepalive, graceful QUIT |

## Supported Clients

The server has been designed and tested to work with:

- **netcat (nc)**: Raw TCP testing with `-C` flag for CRLF line endings
- **irssi**: Full-featured terminal IRC client
- **WeeChat**: Terminal IRC client
- **HexChat**: GUI IRC client

## Overall Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                        SINGLE-PROCESS SERVER                        │
│                                                                     │
│  ┌───────────────────────────────────────────────────────────────┐ │
│  │                      EVENT LOOP (poll)                        │ │
│  │                                                               │ │
│  │   ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐  │ │
│  │   │ Server  │    │ Client  │    │ Client  │    │ Client  │  │ │
│  │   │ Socket  │    │   #1    │    │   #2    │    │   #N    │  │ │
│  │   │ (listen)│    │  (r/w)  │    │  (r/w)  │    │  (r/w)  │  │ │
│  │   └────┬────┘    └────┬────┘    └────┬────┘    └────┬────┘  │ │
│  │        │              │              │              │        │ │
│  │        └──────────────┴──────────────┴──────────────┘        │ │
│  │                           │                                   │ │
│  │                     pollfd array                              │ │
│  └───────────────────────────┴───────────────────────────────────┘ │
│                              │                                     │
│                              ▼                                     │
│  ┌───────────────────────────────────────────────────────────────┐ │
│  │                    COMMAND DISPATCHER                         │ │
│  │                                                               │ │
│  │  PASS │ NICK │ USER │ JOIN │ PART │ PRIVMSG │ KICK │ ...    │ │
│  └───────────────────────────────────────────────────────────────┘ │
│                              │                                     │
│                              ▼                                     │
│  ┌─────────────────────┐         ┌─────────────────────────────┐  │
│  │   Client Objects    │◄───────►│     Channel Objects         │  │
│  │                     │         │                             │  │
│  │  • Socket FD        │         │  • Member list              │  │
│  │  • Buffers          │         │  • Operator set             │  │
│  │  • Auth state       │         │  • Mode flags               │  │
│  │  • Channel set      │         │  • Topic                    │  │
│  └─────────────────────┘         └─────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
```

**Architecture Pattern**: Reactor Pattern
- Single-threaded, event-driven design
- Non-blocking I/O on all sockets
- `poll()` as the event demultiplexer
- Synchronous command processing

**Why This Architecture?**
1. **Simplicity**: No threading complexity, no race conditions, no mutexes
2. **Portability**: `poll()` is POSIX-standard, works across Unix systems
3. **Efficiency**: No context switching overhead, minimal memory footprint
4. **Predictability**: Deterministic execution order simplifies debugging

---

# 2. Project Structure & File Responsibilities

## File Dependency Graph

```
                    ┌──────────┐
                    │ main.cpp │
                    └────┬─────┘
                         │
                         ▼
                  ┌────────────┐
                  │ Server.hpp │◄────────────────────┐
                  └──────┬─────┘                     │
                         │                           │
           ┌─────────────┼─────────────┐            │
           │             │             │            │
           ▼             ▼             ▼            │
    ┌────────────┐ ┌────────────┐ ┌────────────┐   │
    │ Client.hpp │ │Channel.hpp │ │ Server.cpp │───┘
    └──────┬─────┘ └──────┬─────┘ └────────────┘
           │             │
           ▼             ▼
    ┌────────────┐ ┌────────────┐
    │ Client.cpp │ │Channel.cpp │
    └────────────┘ └────────────┘
```

---

## main.cpp

### Purpose
Entry point for the IRC server. Handles program initialization, argument validation, signal handler setup, and server lifecycle management.

### Key Functions

#### `main(int argc, char* argv[])`
- Validates command-line arguments (port and password)
- Installs signal handlers for graceful shutdown
- Instantiates and starts the Server object
- Catches and reports exceptions

#### `signalHandler(int signal)`
- Global signal handler for SIGINT (Ctrl+C) and SIGTERM
- Sets `isRunning = false` via `g_server->stop()` to break the event loop
- Uses a global pointer `g_server` because signal handlers cannot capture context

#### `isValidPort(const char* portStr, int& port)`
- Converts string to integer safely using `strtol()`
- Validates range: 1-65535
- Returns false on conversion errors or out-of-range values

### Signal Handler Design

**Why `sigaction()` over `signal()`?**
- `signal()` behavior varies across platforms
- `sigaction()` provides portable, well-defined semantics
- Allows control over signal masking during handler execution

**Why ignore SIGPIPE?**
When writing to a socket whose peer has closed, the kernel sends SIGPIPE. Without handling, this terminates the process. Ignoring it allows `send()` to return -1 with `errno = EPIPE`, which the server handles gracefully.

---

## Server.hpp / Server.cpp

### Purpose
Central orchestrator of the IRC server. Owns all server resources, implements the event loop, and routes commands to handlers.

### Class Definition

```cpp
class Server {
private:
    int serverSocket;                          // Listening socket
    int port;                                  // TCP port number
    std::string password;                      // Server password
    std::string serverName;                    // "ircserv"
    
    std::vector<struct pollfd> pollFds;        // File descriptors for poll()
    std::map<int, Client*> clients;            // fd → Client mapping
    std::map<std::string, Channel*> channels;  // name → Channel mapping
    
    bool isRunning;                            // Event loop control flag
};
```

### Key Methods

#### `setupServerSocket()`
Creates and configures the listening socket:
1. Create TCP socket
2. Enable address reuse (`SO_REUSEADDR`)
3. Set non-blocking mode (`O_NONBLOCK`)
4. Bind to address
5. Start listening
6. Add to poll array

**Why `SO_REUSEADDR`?**
After closing a socket, the OS keeps it in TIME_WAIT state for ~60 seconds. Without `SO_REUSEADDR`, restarting the server immediately fails with "Address already in use."

**Why `O_NONBLOCK`?**
Non-blocking sockets return immediately from I/O operations instead of waiting. This is essential for single-threaded event-driven design.

#### `start()` - The Event Loop
The main loop calls `poll()` to wait for events, then dispatches to appropriate handlers based on which file descriptors have activity.

**Why backward iteration?**
When processing events, we may remove clients (erasing from `pollFds`). Forward iteration would cause index invalidation. Backward iteration ensures unprocessed elements are unaffected by removals.

#### `acceptNewClient()`
Accepts new TCP connections, creates Client objects, and adds them to the poll array.

#### `handleClientData(int clientFd)`
Receives data from clients, buffers incomplete messages, and processes complete lines.

#### `removeClient(int clientFd)`
Cleans up client resources, notifies channels, and removes from all data structures.

---

## Client.hpp / Client.cpp

### Purpose
Represents a single connected IRC client. Manages socket association, authentication state, message buffering, and channel membership tracking.

### Class Definition

```cpp
class Client {
private:
    int socketFd;              // Client's TCP socket
    std::string nickname;      // IRC nickname
    std::string username;      // Username from USER command
    std::string realname;      // Real name from USER command
    std::string hostname;      // IP address of client
    
    bool isAuthenticated;      // PASS command succeeded
    bool isRegistered;         // Full registration complete
    bool markedForRemoval;     // Deferred deletion flag
    
    std::string inputBuffer;   // Incoming data accumulator
    std::string outputBuffer;  // Outgoing data queue
    
    std::set<Channel*> joinedChannels;  // Channels this client is in
};
```

### State Machine

```
CONNECTED (not auth/reg)
    │
    PASS (correct)
    │
    ▼
AUTHENTICATED (not reg yet)
    │
    NICK + USER complete
    │
    ▼
REGISTERED ◄──── Can use all commands
    │
    QUIT or disconnect
    │
    ▼
MARKED_REMOVAL → DISCONNECTED
```

### Key Methods

#### `queueMessage(const std::string& message)`
Adds message to output buffer with proper IRC line ending (`\r\n`).

#### `flushOutputBuffer()`
Attempts to send buffered data. Returns true if buffer is now empty.

#### `getPrefix()`
Returns IRC message prefix format: `nickname!username@hostname`

---

## Channel.hpp / Channel.cpp

### Purpose
Represents an IRC channel (chat room). Manages membership, operators, modes, topic, and message broadcasting.

### Class Definition

```cpp
class Channel {
private:
    std::string name;           // Channel name (e.g., "#general")
    std::string topic;          // Channel topic
    std::string topicSetBy;     // Who set the topic
    std::string key;            // Channel password (+k mode)
    
    std::vector<Client*> members;    // All channel members
    std::set<Client*> operators;     // Operators (have @)
    std::set<Client*> inviteList;    // Invited users (+i mode)
    
    // Mode flags
    bool inviteOnly;       // +i
    bool topicRestricted;  // +t
    bool hasKey;           // +k
    bool hasUserLimit;     // +l
    size_t userLimit;      // Limit value for +l
};
```

### Key Methods

#### `addMember(Client* client)`
Adds client to channel and establishes bidirectional relationship.

#### `broadcast(const std::string& message, Client* exclude)`
Sends message to all members except the excluded client.

#### `getModeString()`
Returns current mode string for MODE queries.

---

# 3. Server Lifecycle

## Complete Startup Sequence

```
PROGRAM START
    │
    ▼
1. ARGUMENT VALIDATION
    • Check argc == 3
    • Parse port with strtol()
    • Validate port range (1-65535)
    • Check password non-empty
    │
    ▼
2. SIGNAL HANDLER SETUP
    • SIGINT → signalHandler
    • SIGTERM → signalHandler
    • SIGPIPE → SIG_IGN
    │
    ▼
3. SERVER OBJECT CONSTRUCTION
    • Store port and password
    • Initialize serverSocket = -1
    │
    ▼
4. SOCKET SETUP
    socket() → setsockopt() → fcntl() → bind() → listen()
    │
    ▼
5. EVENT LOOP ENTRY
    while (isRunning) { poll(); dispatch(); }
```

## Shutdown Sequence

```
SHUTDOWN TRIGGER (Ctrl+C or SIGTERM)
    │
    ▼
1. EVENT LOOP EXIT
    • isRunning = false
    │
    ▼
2. SERVER DESTRUCTOR
    • Delete all clients (closes sockets)
    • Delete all channels
    • Close server socket
    │
    ▼
3. PROGRAM EXIT
```

---

# 4. Networking & Event Handling

## Why `poll()` Was Chosen

| Mechanism | Pros | Cons |
|-----------|------|------|
| **Blocking I/O** | Simple | One client blocks all |
| **`select()`** | POSIX standard | FD limit (~1024) |
| **`poll()`** | No FD limit, cleaner API | O(n) scan |
| **`epoll()`** | O(1) event retrieval | Linux-only |

**Decision**: `poll()` for portability and simplicity.

## How `poll()` Works

```cpp
struct pollfd {
    int fd;        // File descriptor to monitor
    short events;  // Requested events (POLLIN, POLLOUT)
    short revents; // Returned events
};
```

The server maintains a vector of `pollfd` structures. When `poll()` returns, it checks `revents` to determine what happened on each socket.

## Non-Blocking I/O Semantics

With `O_NONBLOCK` set:
- `recv()` returns immediately (data, 0 for close, or -1 with EAGAIN)
- `send()` returns immediately (bytes sent, or -1 with EAGAIN)
- `accept()` returns immediately (new fd, or -1 with EAGAIN)

This allows the single thread to handle multiple clients without blocking.

---

# 5. Client Handling

## Input Buffer Management

### The Problem: TCP Stream Fragmentation

TCP doesn't preserve message boundaries. A single `recv()` might return:
- Partial message: `"JOI"` (rest comes later)
- Multiple messages: `"JOIN #a\r\nJOIN #b\r\n"`
- Mixed: `"JOIN #a\r\nJOI"`

### The Solution: Line-Based Buffering

```cpp
client->getInputBuffer() += std::string(buffer, bytesRead);

while ((pos = inputBuffer.find('\n')) != std::string::npos) {
    std::string line = inputBuffer.substr(0, pos);
    inputBuffer.erase(0, pos + 1);
    // Strip \r, process command
}
```

## Output Buffer Management

Messages are queued in `outputBuffer` and sent when `POLLOUT` fires. This prevents blocking on slow clients and ensures message delivery even when the socket buffer is full.

---

# 6. IRC Protocol Parsing

## IRC Message Format (RFC 2812)

```
[:prefix] COMMAND [params] [:trailing]
```

### Examples

```
PASS secretpass
NICK john
USER john 0 * :John Smith
:john!~john@host PRIVMSG #channel :Hello world!
```

## Parsing Implementation

The parser tokenizes by whitespace, with special handling for the trailing parameter (prefixed with `:`):

```cpp
while (iss >> token) {
    if (token[0] == ':' && tokens.size() > 0) {
        // Everything after ':' is one parameter
        std::string rest = token.substr(1);
        std::getline(iss, remainder);
        rest += remainder;
        tokens.push_back(rest);
        break;
    }
    tokens.push_back(token);
}
```

---

# 7. Command Dispatching

## Dispatch Mechanism

```cpp
if (command == "PASS") handlePass(client, tokens);
else if (command == "NICK") handleNick(client, tokens);
else if (command == "USER") handleUser(client, tokens);
// ... etc
else if (client->getRegistered())
    client->queueMessage("421 " + nick + " " + command + " :Unknown command");
```

## Handler Pattern

Each handler follows a consistent structure:
1. Check registration state
2. Check parameter count
3. Validate parameters
4. Execute action
5. Send responses

---

# 8. Core IRC Commands

## PASS - Server Password

**Purpose**: Authenticate with the server password.

**Responses**:
- `462` - Already registered
- `461` - No password given
- `464` - Wrong password

---

## NICK - Set Nickname

**Purpose**: Set or change nickname.

**Validation**:
- Maximum 9 characters
- Must start with letter or `[]\`_^{|}`
- Case-insensitive collision detection

**Responses**:
- `431` - No nickname given
- `432` - Invalid format
- `433` - Already in use

---

## USER - Set Username/Realname

**Purpose**: Complete registration with username and realname.

**Format**: `USER <username> <mode> <unused> :<realname>`

**Responses**:
- `462` - Already registered
- `461` - Not enough parameters

---

## JOIN - Join Channel

**Purpose**: Join one or more channels.

**Checks**:
- Valid channel name (starts with # or &)
- +i (invite-only) mode
- +k (key) mode
- +l (limit) mode

**Responses**:
- `473` - Invite-only
- `475` - Bad key
- `471` - Channel full
- JOIN message, topic (332), names list (353, 366)

---

## PART - Leave Channel

**Purpose**: Leave one or more channels.

**Responses**:
- `403` - No such channel
- `442` - Not on channel
- PART message to channel

---

## PRIVMSG - Send Message

**Purpose**: Send message to channel or user.

**Responses**:
- `411` - No recipient
- `412` - No text
- `403` - No such channel
- `404` - Cannot send
- `401` - No such nick

---

## KICK - Remove User from Channel

**Purpose**: Forcibly remove a user from a channel.

**Requirements**: Channel operator status

**Responses**:
- `482` - Not operator
- `441` - Target not on channel

---

## INVITE - Invite User to Channel

**Purpose**: Invite a user to join a channel.

**Requirements**: If +i mode, requires operator status

**Responses**:
- `341` - Invite sent
- `443` - Already on channel

---

## TOPIC - View/Set Channel Topic

**Purpose**: View or change channel topic.

**Requirements**: If +t mode, requires operator status

**Responses**:
- `331` - No topic set
- `332` - Current topic
- `482` - Not operator

---

## MODE - Channel Modes

**Supported Modes**:
| Mode | Parameter | Description |
|------|-----------|-------------|
| +i | - | Invite-only |
| +t | - | Topic restricted |
| +k | key | Channel key |
| +l | limit | User limit |
| +o | nick | Operator status |

---

## QUIT - Disconnect

**Purpose**: Gracefully disconnect from server.

**Process**:
1. Notify all channels
2. Send ERROR message
3. Mark for deferred removal
4. Close connection

---

## PING/PONG - Keepalive

**Purpose**: Verify connection is alive.

Server responds to PING with PONG.

---

# 9. Channels

## Channel Representation

Each channel tracks:
- Members (vector)
- Operators (set)
- Invite list (set)
- Mode flags (i, t, k, l)
- Topic and key

## Case-Insensitive Operations

Channel names are compared case-insensitively, so `#Test` and `#test` refer to the same channel.

## Empty Channel Cleanup

Channels are automatically deleted when all members leave.

---

# 10. Error Handling & Edge Cases

## Protocol Errors (Numeric Replies)

| Code | Name | When |
|------|------|------|
| 401 | ERR_NOSUCHNICK | Target user doesn't exist |
| 403 | ERR_NOSUCHCHANNEL | Invalid channel |
| 432 | ERR_ERRONEUSNICKNAME | Invalid nickname |
| 433 | ERR_NICKNAMEINUSE | Nickname collision |
| 451 | ERR_NOTREGISTERED | Command requires registration |
| 461 | ERR_NEEDMOREPARAMS | Missing parameters |
| 482 | ERR_CHANOPRIVSNEEDED | Not operator |

## Network Error Handling

- `recv() == 0`: Clean disconnect
- `recv() < 0` with EAGAIN: Try again later
- `POLLHUP/POLLERR`: Connection lost

## Partial Message Handling

Incomplete messages are buffered until `\n` is received.

---

# 11. Design Decisions & Trade-offs

## Single-Threaded Event Loop

**Advantages**:
- No race conditions
- No locks needed
- Predictable execution
- Low overhead

**Acceptable because**: IRC is I/O-bound, not CPU-bound.

## Data Structure Choices

- `std::vector<Client*>` for members: Cache-friendly iteration
- `std::set<Client*>` for operators: O(log n) lookup
- `std::map<int, Client*>` for clients: O(log n) by fd

## What Would Change for Multithreading

- Add mutexes for shared state
- Careful lock ordering
- Message queues between threads

## What Would Change for `epoll`

- Replace `pollFds` with `epoll_fd`
- Replace `poll()` with `epoll_wait()`
- O(1) event retrieval instead of O(n) scan

## What Would Change for TLS

- Add OpenSSL dependency
- Wrap sockets in SSL structures
- Replace recv/send with SSL_read/SSL_write

---

# 12. How to Extend the Server

## Adding a New IRC Command

### Step 1: Declare Handler in Server.hpp

```cpp
void handleWho(Client* client, const std::vector<std::string>& tokens);
```

### Step 2: Add Dispatch in parseCommand()

```cpp
} else if (command == "WHO") {
    handleWho(client, tokens);
}
```

### Step 3: Implement Handler in Server.cpp

```cpp
void Server::handleWho(Client* client, const std::vector<std::string>& tokens) {
    if (!client->getRegistered()) {
        client->queueMessage("451 :You have not registered");
        return;
    }
    
    // Implementation...
    
    client->queueMessage("315 " + client->getNickname() + " * :End of WHO list");
}
```

## Adding New Client State

1. Add member to Client class
2. Add getter/setter
3. Use in relevant handlers

## Adding Logging

```cpp
void Server::log(const std::string& message) {
    time_t now = time(NULL);
    logFile << "[" << ctime(&now) << "] " << message << std::endl;
}
```

## Performance Improvements

1. **Use `epoll`**: O(1) event retrieval
2. **Use hash maps** (C++11+): O(1) lookup
3. **Connection pooling**: Avoid allocation overhead

---

# Appendix: Quick Reference

## Numeric Reply Codes Used

```
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
```

## File Summary

| File | Purpose |
|------|---------|
| main.cpp | Entry point, signals |
| Server.hpp/cpp | Server implementation |
| Client.hpp/cpp | Client representation |
| Channel.hpp/cpp | Channel management |

## Build Commands

```bash
make           # Standard build
make re        # Clean rebuild
make debug     # Debug build with sanitizers
./ircserv 6667 mypassword  # Run server
```

## Test Commands

```bash
# netcat
nc -C 127.0.0.1 6667

# irssi
irssi
/connect 127.0.0.1 6667 mypassword
```

---

*End of Documentation*
