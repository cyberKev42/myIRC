# IRC Server Test Plan

This document contains comprehensive tests for your ft_irc server.
Run the server with: `./ircserv 6667 testpass`

-----------------------------------------------

## Prerequisites
- Have 2-3 terminal windows ready for irssi clients
- Have 1 terminal for netcat (nc) testing
- Keep the server terminal visible to see logs

# Terminal 1: Start server
## Without valgrind
./ircserv 6667 testpass

## With valgrind
valgrind --leak-check=full ./ircserv 6667 testpass


# Terminal 2: netcat tests
nc -C 127.0.0.1 6667

# Terminal 3+: irssi clients
irssi
/connect 127.0.0.1 6667 testpass


-----------------------------------------------


## 1. CONNECTION & AUTHENTICATION TESTS

### 1.1 Basic Connection
```bash
# Terminal: netcat
nc -C 127.0.0.1 6667
```
Expected: Connection established, server waits for commands

### 1.2 Correct Authentication Order
```bash
nc -C 127.0.0.1 6667
PASS testpass
NICK testuser
USER testuser 0 * :Test User
```
Expected: Welcome messages (001, 002, 003, 004, MOTD)

### 1.3 Wrong Password
```bash
nc -C 127.0.0.1 6667
PASS wrongpassword
NICK testuser
USER testuser 0 * :Test User
```
Expected: "464 :Password incorrect", no welcome messages

### 1.4 Missing Password
```bash
nc -C 127.0.0.1 6667
NICK testuser
USER testuser 0 * :Test User
```
Expected: No welcome messages (not registered without PASS)

### 1.5 NICK Before PASS
```bash
nc -C 127.0.0.1 6667
NICK testuser
PASS testpass
USER testuser 0 * :Test User
```
Expected: Should still work (order of NICK/PASS doesn't matter, just need both before USER completes registration)

### 1.6 Duplicate Nickname
```bash
# Client 1:
nc -C 127.0.0.1 6667
PASS testpass
NICK john
USER john 0 * :John

# Client 2 (new terminal):
nc -C 127.0.0.1 6667
PASS testpass
NICK john
```
Expected: Client 2 gets "433 * john :Nickname is already in use"

### 1.7 Invalid Nicknames
```bash
nc -C 127.0.0.1 6667
PASS testpass
NICK 123invalid
```
Expected: "432 123invalid :Erroneous nickname" (can't start with number)

```bash
NICK this_is_a_very_long_nickname
```
Expected: "432 ... :Erroneous nickname" (too long, max 9 chars)

```bash
NICK test user
```
Expected: Should only take "test" (space separates parameters)

### 1.8 Change Nickname After Registration
```bash
nc -C 127.0.0.1 6667
PASS testpass
NICK oldnick
USER oldnick 0 * :Old Nick
NICK newnick
```
Expected: ":oldnick NICK :newnick" confirmation

---

## 2. CHANNEL TESTS

### 2.1 Create and Join Channel
```bash
# After registration:
JOIN #test
```
Expected:
- JOIN confirmation
- NAMES list showing you with @ (operator)
- "End of /NAMES list"

### 2.2 Join Non-Existent Channel (Creates It)
```bash
JOIN #newchannel
```
Expected: Channel created, you become operator

### 2.3 Invalid Channel Names
```bash
JOIN test
```
Expected: "403 test :No such channel" (missing #)

```bash
JOIN #
```
Expected: Should fail or create channel named "#"

### 2.4 Join Multiple Channels
```bash
JOIN #chan1,#chan2,#chan3
```
Expected: Joined all three channels, operator in all

### 2.5 Part Channel
```bash
JOIN #test
PART #test
```
Expected: PART message sent to channel

### 2.6 Part with Reason
```bash
JOIN #test
PART #test :Goodbye everyone!
```
Expected: PART message includes reason

### 2.7 Part Channel Not In
```bash
PART #nonexistent
```
Expected: "403 #nonexistent :No such channel"

```bash
JOIN #test
PART #test
PART #test
```
Expected: "442 #test :You're not on that channel"

---

## 3. MESSAGING TESTS

### 3.1 Channel Message
```bash
# Client 1: Join and send
JOIN #test
PRIVMSG #test :Hello everyone!

# Client 2: Join same channel
JOIN #test
```
Expected: Client 2 sees messages from Client 1

### 3.2 Private Message
```bash
# Client 1: Register as "alice"
# Client 2: Register as "bob"

# From alice:
PRIVMSG bob :Hey Bob, private message!
```
Expected: Bob receives the message

### 3.3 Message to Non-Existent User
```bash
PRIVMSG nobody :Hello?
```
Expected: "401 nobody :No such nick/channel"

### 3.4 Message to Channel Not In
```bash
# Don't join #secret
PRIVMSG #secret :Can you hear me?
```
Expected: "404 #secret :Cannot send to channel"

### 3.5 Empty Message
```bash
PRIVMSG #test
```
Expected: "412 :No text to send"

### 3.6 Message Without Target
```bash
PRIVMSG
```
Expected: "411 :No recipient given (PRIVMSG)"

---

## 4. CHANNEL MODE TESTS

### 4.1 View Channel Modes
```bash
JOIN #test
MODE #test
```
Expected: "324 yournick #test +t" (or similar, showing current modes)

### 4.2 Invite-Only Mode (+i)
```bash
# Client 1 (operator):
JOIN #test
MODE #test +i

# Client 2:
JOIN #test
```
Expected: Client 2 gets "473 #test :Cannot join channel (+i)"

### 4.3 Remove Invite-Only (-i)
```bash
# Client 1:
MODE #test -i

# Client 2:
/wc  (close window in irssi)
JOIN #test
```
Expected: Client 2 can now join

### 4.4 Topic Restricted Mode (+t)
```bash
# Client 1 creates channel (has +t by default)
JOIN #test

# Client 2 joins
JOIN #test

# Client 2 tries to set topic
TOPIC #test :New topic
```
Expected: "482 #test :You're not channel operator"

### 4.5 Remove Topic Restriction (-t)
```bash
# Client 1 (operator):
MODE #test -t

# Client 2:
TOPIC #test :Now I can set topic!
```
Expected: Topic change succeeds, broadcast to channel

### 4.6 Channel Key Mode (+k)
```bash
# Client 1:
JOIN #test
MODE #test +k secretkey

# Client 2:
JOIN #test
```
Expected: "475 #test :Cannot join channel (+k)"

```bash
# Client 2 with correct key:
JOIN #test secretkey
```
Expected: Successfully joins

### 4.7 Remove Channel Key (-k)
```bash
# Client 1:
MODE #test -k
```
Expected: Key removed, anyone can join

### 4.8 User Limit Mode (+l)
```bash
# Client 1:
JOIN #test
MODE #test +l 2

# Client 2:
JOIN #test

# Client 3:
JOIN #test
```
Expected: Client 3 gets "471 #test :Cannot join channel (+l)"

### 4.9 Remove User Limit (-l)
```bash
# Client 1:
MODE #test -l

# Client 3:
JOIN #test
```
Expected: Client 3 can now join

### 4.10 Operator Mode (+o / -o)
```bash
# Client 1 (operator):
JOIN #test

# Client 2:
JOIN #test

# Client 1 gives operator to Client 2:
MODE #test +o client2nick

# Verify Client 2 can now use operator commands:
# Client 2:
MODE #test +i
```
Expected: Client 2 can now change modes

```bash
# Client 1 removes operator from Client 2:
MODE #test -o client2nick
```
Expected: Client 2 loses operator privileges

### 4.11 Multiple Modes at Once
```bash
MODE #test +itl 5
```
Expected: Sets invite-only, topic-restricted, and limit of 5

```bash
MODE #test -it
```
Expected: Removes invite-only and topic-restricted

### 4.12 Non-Operator Tries to Change Mode
```bash
# Client 2 (not operator):
MODE #test +i
```
Expected: "482 #test :You're not channel operator"

---

## 5. OPERATOR COMMAND TESTS

### 5.1 KICK User
```bash
# Client 1 (operator):
JOIN #test

# Client 2:
JOIN #test

# Client 1:
KICK #test client2nick :Goodbye!
```
Expected: Client 2 is removed from channel, everyone sees KICK message

### 5.2 KICK Without Being Operator
```bash
# Client 2 (not operator) tries to kick Client 1:
KICK #test client1nick :Trying to kick
```
Expected: "482 #test :You're not channel operator"

### 5.3 KICK Non-Existent User
```bash
KICK #test nobody :Bye
```
Expected: "441 nobody #test :They aren't on that channel"

### 5.4 INVITE User
```bash
# Client 1:
JOIN #test
MODE #test +i

# Client 1 invites Client 2:
INVITE client2nick #test
```
Expected: 
- Client 1 gets "341 client1nick client2nick #test"
- Client 2 gets invitation message

```bash
# Client 2 can now join:
JOIN #test
```
Expected: Successfully joins despite +i

### 5.5 INVITE to Non-Invite-Only Channel
```bash
# Without +i, any member can invite
MODE #test -i
# Client 2 (regular member):
INVITE client3nick #test
```
Expected: Should work

### 5.6 INVITE User Already in Channel
```bash
INVITE client2nick #test
# (when client2 is already in #test)
```
Expected: "443 client2nick #test :is already on channel"

### 5.7 TOPIC - View Topic
```bash
JOIN #test
TOPIC #test
```
Expected: "331 yournick #test :No topic is set" (if no topic)

### 5.8 TOPIC - Set Topic
```bash
TOPIC #test :Welcome to the test channel!
```
Expected: Topic broadcast to all members

### 5.9 TOPIC - View After Setting
```bash
TOPIC #test
```
Expected: "332 yournick #test :Welcome to the test channel!"

---

## 6. PARTIAL DATA / BUFFERING TESTS

### 6.1 Partial Command with Ctrl+D
```bash
nc -C 127.0.0.1 6667
PAS^DS tes^Dtpass^D
NIC^DK tes^Dtuser^D
USER testuser 0 * :Test^D User^D
```
(Press Ctrl+D where ^D appears to send partial data)

Expected: Server buffers and processes complete commands correctly

### 6.2 Multiple Commands in One Packet
```bash
# Send multiple commands at once:
printf "PASS testpass\r\nNICK testuser\r\nUSER testuser 0 * :Test\r\n" | nc 127.0.0.1 6667
```
Expected: All commands processed, welcome messages received

### 6.3 Very Long Message
```bash
# After registration, send a message longer than 512 bytes
PRIVMSG #test :AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA

... (repeat to make very long)
```
Expected: Should handle gracefully (truncate or process)

---

## 7. DISCONNECTION TESTS

### 7.1 QUIT Command
```bash
JOIN #test
QUIT :Leaving now
```
Expected: 
- Client receives ERROR message
- Other channel members see QUIT message
- Connection closes

### 7.2 QUIT Without Reason
```bash
QUIT
```
Expected: Uses default reason "Client Quit"

### 7.3 Abrupt Disconnect
```bash
# Join a channel, then close terminal without QUIT
# Or: kill the nc process
```
Expected: Server detects disconnect, notifies channel members

### 7.4 Disconnect Cleans Up Channels
```bash
# Client 1 creates #test, is alone
JOIN #test

# Client 1 disconnects (close terminal)
```
Expected: Channel #test should be deleted (check server logs)

---

## 8. EDGE CASES & ERROR HANDLING

### 8.1 Commands Before Registration
```bash
nc -C 127.0.0.1 6667
JOIN #test
```
Expected: "451 :You have not registered"

### 8.2 Unknown Command
```bash
# After registration:
UNKNOWNCMD test
```
Expected: "421 yournick UNKNOWNCMD :Unknown command"

### 8.3 PING/PONG
```bash
PING server
```
Expected: "PONG ircserv :server"

### 8.4 Case Insensitivity
```bash
# Commands should be case-insensitive:
join #TEST
Join #test
JOIN #Test
```
Expected: All should work

```bash
# Nicknames should be case-insensitive:
PRIVMSG JOHN :Hello
PRIVMSG john :Hello
PRIVMSG John :Hello
```
Expected: All reach the same user

### 8.5 Channel Case Insensitivity
```bash
JOIN #Test
# Another client:
JOIN #test
```
Expected: Both in same channel

### 8.6 Empty Parameters
```bash
NICK
USER
JOIN
PART
PRIVMSG
KICK
INVITE
TOPIC
MODE
```
Expected: Appropriate "Not enough parameters" errors

### 8.7 Server Doesn't Crash
Try various malformed inputs:
```bash
:
:::
PASS
   
\r\n\r\n
```
Expected: Server handles gracefully, doesn't crash

---

## 9. STRESS TESTS

### 9.1 Many Clients
Open 10+ clients connecting simultaneously.
Expected: Server handles all connections

### 9.2 Rapid Messages
```bash
# Send many messages quickly:
for i in {1..100}; do echo "PRIVMSG #test :Message $i"; done
```
Expected: All messages delivered

### 9.3 Join/Part Rapidly
```bash
for i in {1..20}; do 
  echo "JOIN #test"
  echo "PART #test"
done
```
Expected: Server handles without issues

---

## 10. IRSSI-SPECIFIC TESTS

Use irssi for these tests:

### 10.1 Standard Connection
```
/connect 127.0.0.1 6667 testpass
/nick yournick
/join #test
```

### 10.2 Private Message
```
/msg othernick Hello there!
```

### 10.3 Channel Operations
```
/topic #test New Topic Here
/kick #test baduser Get out!
/invite friend #test
/mode #test +i
```

### 10.4 Window Management
- After failed join, use `/wc` to close window
- Then `/join` again

---

## Test Results Checklist

| Test | Status | Notes |
|------|--------|-------|
| 1.1 Basic Connection | ⬜ | |
| 1.2 Correct Auth | ⬜ | |
| 1.3 Wrong Password | ⬜ | |
| ... | ... | ... |

Fill in ✅ for pass, ❌ for fail, with notes on any issues found.

---

## Common Issues to Watch For

1. **Messages not appearing instantly** - POLLOUT issue
2. **Channel windows opening on failed JOIN** - Client behavior, use /wc
3. **Server crash** - Check for any input that crashes server
4. **Memory leaks** - Run with valgrind: `valgrind ./ircserv 6667 pass`
5. **Zombie channels** - Channels not deleted when empty
6. **Duplicate nicknames allowed** - Case sensitivity issue

Good luck testing! 🧪
