# ft_irc – IRC Server in C++98

## 📋 Current Implementation Summary

### ✅ **Already Implemented**

- **TCP server** with non-blocking sockets using `poll()` (or equivalent)
- **Multi-client support** – handles multiple simultaneous connections
- **Basic client authentication** via `PASS`, `NICK`, and `USER` commands
- **Command parsing** with buffer aggregation (supports partial messages)
- **Error handling** with proper IRC-style error responses
- **Clean client disconnection** handling

### 🏗️ **Core Structure**

```
Server Class
├── Socket initialization & binding
├── poll() event loop
├── Client connection management
├── Command routing (PASS, NICK, USER, QUIT)
└── IRC message formatting

Client Class
├── Connection state (fd, authentication flags)
├── User information (nickname, username, realname)
├── Message buffer management
└── Authentication status tracking
```

### ⚙️ **Key Features Working**

- ✅ Server listens on specified port with `SO_REUSEADDR`
- ✅ Non-blocking I/O operations
- ✅ Password-protected access
- ✅ Unique nickname validation
- ✅ Proper IRC message delimiters (`\r\n`)
- ✅ Graceful client removal on disconnect/QUIT

## 🚨 **Missing (Required by Subject)**

1. **Channel System**
   - `JOIN` / `PART` / `NAMES` commands
   - Channel message broadcasting (`PRIVMSG #channel`)
   - Channel user list management

2. **Operator Commands**
   - `KICK` – eject client from channel
   - `INVITE` – invite client to channel
   - `TOPIC` – view/change channel topic
   - `MODE` – channel modes:
     - `i` (invite-only)
     - `t` (topic restriction)
     - `k` (channel password)
     - `o` (operator privilege)
     - `l` (user limit)

3. **Message Routing**
   - Private messages between users
   - Channel message forwarding to all members

4. **Robustness Requirements**
   - Handling partial commands over slow connections
   - Proper resource cleanup
   - Full compliance with reference IRC client

## 🛠️ **Technical Constraints Met**

- ✅ C++98 standard compliance
- ✅ No external libraries (only standard C++98 and system calls)
- ✅ Single `poll()` for all I/O operations
- ✅ Non-blocking file descriptors
- ✅ MacOS compatibility with `fcntl(fd, F_SETFL, O_NONBLOCK)`
- ✅ Makefile with required rules (NAME, all, clean, fclean, re)

## 📁 **Project Structure**

```
src/
├── Server.cpp/hpp # Main server logic
├── Client.cpp/hpp # Client state management
├── Channel.cpp/hpp # (TO BE IMPLEMENTED)
├── main.cpp # Entry point
└── Makefile
```
