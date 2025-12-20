# IRC Server - Complete Guide

## What We Built

You now have a fully functional IRC server with:
- Non-blocking I/O using `poll()`
- Multiple client support
- Channel management
- User authentication
- All required IRC commands (PASS, NICK, USER, JOIN, PRIVMSG, KICK, INVITE, TOPIC, MODE)

## How to Build

```bash
make
```

This will compile all files and create the `ircserv` executable.

## How to Run

```bash
./ircserv <port> <password>
```

Example:
```bash
./ircserv 6667 mysecretpass
```

## Testing with netcat (nc)

### Test 1: Basic Connection
```bash
nc 127.0.0.1 6667
PASS mysecretpass
NICK testuser
USER testuser 0 * :Test User
```

You should see welcome messages!

### Test 2: Partial Messages (ctrl+D test)
```bash
nc -C 127.0.0.1 6667
PASS my^Dsecret^Dpass^D
NICK test^Duser^D
```

Press `ctrl+D` between parts. The server buffers and processes complete commands!

### Test 3: Multiple Clients

Terminal 1:
```bash
nc 127.0.0.1 6667
PASS mysecretpass
NICK alice
USER alice 0 * :Alice
JOIN #test
```

Terminal 2:
```bash
nc 127.0.0.1 6667
PASS mysecretpass
NICK bob
USER bob 0 * :Bob
JOIN #test
PRIVMSG #test :Hello from Bob!
```

Alice should see Bob's message!

## Testing with IRC Client

For a better experience, use a real IRC client:

### Using irssi:
```bash
irssi
/connect 127.0.0.1 6667 mysecretpass
/nick yournick
/join #general
/msg #general Hello everyone!
```

### Using WeeChat:
```bash
weechat
/server add myserver 127.0.0.1/6667 -password=mysecretpass
/connect myserver
/join #general
```

### Using HexChat (GUI):
1. Add new network
2. Server: 127.0.0.1:6667
3. Password: mysecretpass
4. Connect and join a channel

## IRC Commands Reference

### Authentication
- `PASS <password>` - Authenticate with server
- `NICK <nickname>` - Set your nickname
- `USER <username> 0 * :<realname>` - Set username

### Channel Operations
- `JOIN #channel [key]` - Join a channel
- `PRIVMSG #channel :message` - Send message to channel
- `PRIVMSG nickname :message` - Send private message

### Operator Commands (in channel)
- `KICK #channel nickname :reason` - Kick user
- `INVITE nickname #channel` - Invite user
- `TOPIC #channel :new topic` - Change topic
- `MODE #channel +i` - Make invite-only
- `MODE #channel +t` - Restrict topic changes to operators
- `MODE #channel +k password` - Set channel password
- `MODE #channel +o nickname` - Give operator status
- `MODE #channel +l 50` - Set user limit

## Architecture Overview

### Key Components

**Server Class:**
- Manages main server socket
- Uses `poll()` to monitor all connections
- Routes commands to handlers

**Client Class:**
- Represents one connected user
- Stores authentication state
- Buffers incomplete messages

**Channel Class:**
- Represents a chat room
- Tracks members and operators
- Handles channel modes

### The Event Loop

```
1. poll() waits for activity
2. New connection? → accept() and add to poll
3. Data from client? → recv() and buffer
4. Complete message? → parse and execute command
5. Go back to step 1
```

### Non-Blocking I/O

All sockets are set to non-blocking mode with:
```cpp
fcntl(fd, F_SETFL, O_NONBLOCK)
```

This means:
- `accept()` returns immediately (no waiting)
- `recv()` returns immediately (no waiting)
- `send()` returns immediately (no waiting)

If there's no data, they return `-1` with `errno` set to `EWOULDBLOCK`/`EAGAIN`.

### Message Buffering

IRC messages end with `\r\n`. TCP doesn't guarantee you receive complete messages:
- You might get partial: "HEL" then "LO\r\n"
- Or multiple at once: "NICK john\r\nUSER john 0 * :John\r\n"

We solve this with `Client::inputBuffer`:
1. Append all received data
2. Extract complete lines (up to `\n`)
3. Process each complete command
4. Keep incomplete data in buffer

## How poll() Works

```cpp
struct pollfd {
    int fd;         // File descriptor to watch
    short events;   // What to watch for (POLLIN = readable)
    short revents;  // What actually happened
};
```

Example:
```cpp
pollfd fds[3];
fds[0].fd = serverSocket;  // Watch for new connections
fds[1].fd = client1Socket; // Watch for data from client 1
fds[2].fd = client2Socket; // Watch for data from client 2

// All have events = POLLIN (watch for readable data)

poll(fds, 3, -1);  // Wait forever until something happens

// Check what happened:
if (fds[0].revents & POLLIN) {
    // New connection on server socket!
}
if (fds[1].revents & POLLIN) {
    // Client 1 sent data!
}
```

**Why poll() is better than blocking:**
- One thread handles all clients
- No busy waiting (CPU efficient)
- Scalable to hundreds of connections

## Common Issues & Solutions

### Issue: "Address already in use"
**Solution:** Wait 1-2 minutes or use different port. Or the `SO_REUSEADDR` option already handles this.

### Issue: Partial messages not working
**Solution:** Make sure you're using `nc -C` (adds \r) and the buffer logic is working.

### Issue: Client can't authenticate
**Solution:** Check password matches and that you're sending PASS before NICK/USER.

### Issue: Commands not recognized
**Solution:** Commands are case-insensitive in IRC. Make sure to uppercase in parsing.

## Next Steps for Learning

1. **Add PART command** - Leave a channel
2. **Add WHO command** - List users in channel  
3. **Add WHOIS command** - Get user info
4. **Error handling** - Better error messages
5. **Persistence** - Save channels to file
6. **Logging** - Log all messages to file

## Key System Functions Summary

- `socket()` - Create communication endpoint
- `bind()` - Bind socket to address/port
- `listen()` - Mark socket as passive (accepting)
- `accept()` - Accept incoming connection
- `recv()` - Receive data from socket
- `send()` - Send data through socket
- `poll()` - Wait for events on multiple file descriptors
- `fcntl()` - Control file descriptor properties
- `close()` - Close file descriptor
- `signal()` - Set signal handler

## Understanding IRC Protocol

IRC is text-based. Messages have this format:
```
[:prefix] COMMAND [params] [:trailing]
```

Examples:
```
PASS secretpass
NICK john
:john!~john@host PRIVMSG #channel :Hello!
```

The `:prefix` is who sent it (for server-to-client messages).

## Project Structure

```
ircserv/
├── Makefile          # Build system
├── main.cpp          # Entry point, signal handling
├── Server.hpp/cpp    # Server logic, poll loop
├── Client.hpp/cpp    # Client representation
└── Channel.hpp/cpp   # Channel management
```

## Congratulations!

You've built a real IRC server from scratch. You now understand:
- Non-blocking I/O
- Event-driven programming with poll()
- Network programming with sockets
- Protocol implementation
- Buffer management for TCP streams

This is the foundation for building any networked application!