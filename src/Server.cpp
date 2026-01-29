#include "../include/Server.hpp"

namespace {
// Server constants for socket operations and polling
const int ERROR_CODE = -1;
const int POLL_TIMEOUT = -1;
const int SERVER_FD_INDEX = 0;
const int FIRST_CLIENT_INDEX = 1;
const size_t BUFFER_SIZE = 512;
const int SOCK_OPT = 1;
const int ONE_BYTE = 1;
const int MIN_USER_ARGS = 5;
const int REALNAME_ARG = 4;
} // namespace ServerInternal

Server::Server() {
}

Server::Server(const int PORT, const std::string &PASSWORD)
    : _port(PORT), _password(PASSWORD), _server_name("irc.server") {
}

Server::~Server() {
}

void Server::run() {
  initSocket(_port);

  struct pollfd serverPollFd;
  serverPollFd.fd = _server_socket;
  serverPollFd.events = POLLIN;
  serverPollFd.revents = 0;

  _poll_fds.push_back(serverPollFd);

  std::cout << "Server running on port " << _port << std::endl;
  std::cout << "Waiting for connections..." << std::endl;

  while (true) {
    int pollFd = poll(_poll_fds.data(), _poll_fds.size(), POLL_TIMEOUT);
    if (pollFd == ERROR_CODE) {
      std::cerr << "Poll error: " << strerror(errno) << std::endl;
      continue;
    }

    if ((_poll_fds[SERVER_FD_INDEX].revents & POLLIN) != 0)
      acceptClient();

    for (size_t i = FIRST_CLIENT_INDEX; i < _poll_fds.size(); ++i) {
      if ((_poll_fds[i].revents & POLLIN) != 0)
        handleClientData(_clients[i - FIRST_CLIENT_INDEX]);
    }
  }
}

void Server::initSocket(const int PORT) {
  // OS Action: Creates an endpoint for communication
  // AF_INET: Requests IPv4 protocol family
  // SOCK_STREAM: Requests reliable, connection-oriented TCP semantics
  const int SOCK_FD = socket(AF_INET, SOCK_STREAM, 0);
  if (SOCK_FD == ERROR_CODE)
    throw std::runtime_error("Unable to initiate socket: Server::initSocket()");

  // OS Action: Modifies socket behavior in the kernel's socket structures
  // SO_REUSEADDR: Allows binding to a port that was recently used (avoids
  // "Address already in use" errors) Why needed: Without this, the OS would
  // enforce a TIME_WAIT period before the port can be reused
  int optval = SOCK_OPT;
  if (setsockopt(SOCK_FD, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == ERROR_CODE) {
    close(SOCK_FD);
    throw std::runtime_error("Unable to set SO_REUSEADDR: Server::initSocket()");
  }

  // OS Interpretation: Defines how the socket will be addressed
  // INADDR_ANY: Binds to all available network interfaces (0.0.0.0)
  // htons(): Converts port number from host byte order to network byte order
  // (big-endian)
  struct sockaddr_in serverAddr;
  std::memset(&serverAddr, 0, sizeof(serverAddr));
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(PORT);
  serverAddr.sin_addr.s_addr = INADDR_ANY;

  // OS Action: Associates the socket with the specified IP address and port
  // Kernel Work: Creates an entry in the routing table and marks the port as in
  // use Privilege Check: On Unix systems, ports < 1024 require root privileges
  struct sockaddr *addr = reinterpret_cast<struct sockaddr *>(&serverAddr);
  if (bind(SOCK_FD, addr, sizeof(serverAddr)) == ERROR_CODE) {
    close(SOCK_FD);
    throw std::runtime_error("Unable to bind Socket: Server::initSocket()");
  }

  // OS Action: Transitions socket from "closed" to "listening" state
  // SOMAXCONN: Maximum backlog queue size for pending connections
  // (system-defined, typically 128+) Kernel Creates: Accept queue for
  // established connections SYN queue for half-open connections during TCP
  // handshake
  if (listen(SOCK_FD, SOMAXCONN) == ERROR_CODE) {
    close(SOCK_FD);
    throw std::runtime_error("Unable to listen Socket: Server::initSocket()");
  }

  setNonBlocking(SOCK_FD);

  _server_socket = SOCK_FD;
}

void Server::setNonBlocking(int fd) {
  // OS Action: Modifies file descriptor flags via fcntl(fd, F_SETFL,
  // O_NONBLOCK) Effect: accept(), read(), write() operations return immediately
  // rather than blocking Use Case: Essential for event-driven servers using
  // select(), poll(), or epoll
  if (fcntl(fd, F_SETFL, O_NONBLOCK) == ERROR_CODE)
    throw std::runtime_error("Unable to set non-blocking mode: Server::setNonBlocking()");
}

void Server::acceptClient() {
  struct sockaddr_in clientAddr;
  memset(&clientAddr, 0, sizeof(clientAddr));

  socklen_t clientLen = sizeof(clientAddr);
  struct sockaddr *addr = reinterpret_cast<struct sockaddr *>(&clientAddr);
  // accept() is a blocking call by default - it will pause the program until a
  // client connects. But the poll() in Server::run() handles it
  const int CLIENT_SOCKET = accept(_server_socket, addr, &clientLen);
  if (CLIENT_SOCKET == ERROR_CODE) {
    close(CLIENT_SOCKET);
    std::cerr << "accept() failed after poll: " << strerror(errno) << std::endl;
  }

  setNonBlocking(CLIENT_SOCKET);

  _clients.push_back(Client(CLIENT_SOCKET));

  struct pollfd clientPollFd;
  clientPollFd.fd = CLIENT_SOCKET;
  clientPollFd.events = POLLIN;
  clientPollFd.revents = 0;

  _poll_fds.push_back(clientPollFd);

  std::cout << "Client connected! Socket: " << CLIENT_SOCKET << std::endl;
}

void Server::handleClientData(Client &client) {
  char buffer[BUFFER_SIZE];
  ssize_t bytesRead = 0;

  bytesRead = recv(client.getFd(), buffer, sizeof(buffer) - ONE_BYTE, 0);
  if (bytesRead > 0) {
    buffer[bytesRead] = '\0';
    std::string data(buffer, bytesRead);

    std::cout << "Received " << bytesRead << " bytes from client " << client.getFd() << std::endl;

    client.appendToBuffer(data);

    while (client.hasCompleteMessage()) {
      std::string command = client.extractCommand();
      std::cout << "Complete command: [" << command << "]" << std::endl;

      processCommand(client, command);
    }
  } else if (bytesRead == 0) {
    std::cout << "Client disconnected: " << client.getFd() << std::endl;
    for (size_t i = FIRST_CLIENT_INDEX; i < _poll_fds.size(); ++i) {
      if (_poll_fds[i].fd == client.getFd()) {
        removeClient(i);
        break;
      }
    }
    return;
  } else if (bytesRead == ERROR_CODE && errno != EAGAIN)
    std::cerr << "Read error for client " << client.getFd() << ": " << strerror(errno) << std::endl;
}

void Server::removeClient(size_t index) {
  int clientFd = _poll_fds[index].fd;

  if (index < FIRST_CLIENT_INDEX || index >= _poll_fds.size())
    return;

  close(_poll_fds[index].fd);

  _poll_fds.erase(_poll_fds.begin() + index);
  _clients.erase(_clients.begin() + (index - FIRST_CLIENT_INDEX));

  std::cout << "Client " << clientFd << " removed from poll set" << std::endl;
}

void Server::processCommand(Client &client, const std::string &command) {
  std::vector<std::string> args = splitCommand(command);
  if (args.empty())
    return;

  std::string cmd = args[0];
  std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

  if (!client.isAuthenticated() && cmd != "PASS" && cmd != "NICK" && cmd != "USER" && cmd != "QUIT") {
    sendError(client, "464", "Password incorrect");
    return;
  }

  enum CommandCode { CMD_PASS, CMD_NICK, CMD_USER, CMD_QUIT, CMD_UNKNOWN };

  CommandCode getCmdCode = CMD_UNKNOWN;
  if (cmd == "PASS")
    getCmdCode = CMD_PASS;
  else if (cmd == "NICK")
    getCmdCode = CMD_NICK;
  else if (cmd == "USER")
    getCmdCode = CMD_USER;
  else if (cmd == "QUIT")
    getCmdCode = CMD_QUIT;

  switch (getCmdCode) {
  case CMD_PASS:
    handlePASS(client, args);
    break;
  case CMD_NICK:
    handleNICK(client, args);
    break;
  case CMD_USER:
    handleUSER(client, args);
    break;
  case CMD_QUIT:
    handleQUIT(client, args);
    break;
  default:
    sendReply(client, "ECHO: " + command);
    break;
  }
}

void Server::handlePASS(Client &client, const std::vector<std::string> &args) {
  if (args.size() < FIRST_CLIENT_INDEX + ONE_BYTE) {
    sendError(client, "461", "PASS :Not enough parameters");
    return;
  }

  if (client.hasPassword()) {
    sendError(client, "462", "You may not reregister");
    return;
  }

  if (args[FIRST_CLIENT_INDEX] == _password) {
    client.setPassword(true);
    sendReply(client, "Password accepted");
    return;
  }

  sendError(client, "464", "Password incorrect");
}

void Server::handleNICK(Client &client, const std::vector<std::string> &args) {
  if (args.size() < FIRST_CLIENT_INDEX + ONE_BYTE) {
    sendError(client, "431", "No nickname given");
    return;
  }

  std::string nickname = args[FIRST_CLIENT_INDEX];

  if (nickname.empty() || nickname.find(' ') != std::string::npos) {
    sendError(client, "432", nickname + " :Erroneous nickname");
    return;
  }

  for (size_t i = 0; i < _clients.size(); ++i) {
    if (_clients[i].getFd() != client.getFd() && _clients[i].getNickname() == nickname) {
      sendError(client, "433", nickname + " :Nickname is already in use");
      return;
    }
  }

  client.setNickname(nickname);
  sendReply(client, "NICK set to: " + nickname);
}

// Add more validations | args validations
void Server::handleUSER(Client &client, const std::vector<std::string> &args) {
  // If size is greater than 5?
  // I think, is going to pass, but, the rest of the args will not be used...
  if (args.size() < static_cast<size_t>(MIN_USER_ARGS)) {
    sendError(client, "461", "USER :Not enough parameters");
    return;
  }

  if (client.hasUser()) {
    sendError(client, "462", "You may not reregister");
    return;
  }

  // here...
  std::string username = args[FIRST_CLIENT_INDEX];
  std::string realname = args[REALNAME_ARG];

  client.setUsername(username);
  client.setRealname(realname);
  sendReply(client, "USER registered");
}

void Server::handleQUIT(Client &client, const std::vector<std::string> &args) {
  std::string message = "Client quit";

  if (args.size() > FIRST_CLIENT_INDEX)
    message = args[FIRST_CLIENT_INDEX];

  std::cout << "Client " << client.getFd() << " quit: " << message << std::endl;

  for (size_t i = FIRST_CLIENT_INDEX; i < _poll_fds.size(); ++i) {
    if (_poll_fds[i].fd == client.getFd()) {
      removeClient(i);
      break;
    }
  }
}

void Server::sendError(Client &client, const std::string &code, const std::string &message) {
  std::string error = ":" + getServerName() + " " + code + " " +
                      (client.getNickname().empty() ? "*" : client.getNickname()) + " " + message + "\r\n";

  send(client.getFd(), error.c_str(), error.length(), 0);
}

void Server::sendReply(Client &client, const std::string &message) {
  std::string reply = ":" + getServerName() + " " + message + "\r\n";

  send(client.getFd(), reply.c_str(), reply.length(), 0);
}

const std::string &Server::getServerName() const {
  return _server_name;
}

std::vector<std::string> Server::splitCommand(const std::string &command) {
  std::vector<std::string> args;
  std::istringstream iss(command);

  std::string arg;
  while ((iss >> arg) != 0) {
    args.push_back(arg);
  }
  return args;
}

void Server::handlePING(Client &client, const IRCMessage &msg) {
  if (msg.getParamCount() < IRC_PARAM_OFFSET) {
    sendError(client, "409", "No origin specified");
    return;
  }

  std::string pong = "PONG " + _server_name + " :" + msg.getParams()[0];
  sendReply(client, ":" + _server_name + " " + pong + "\r\n");
}

void Server::handleJOIN(Client &client, const IRCMessage &msg) {
  if (!client.isAuthenticated()) {
    sendError(client, "451", ":You have not registered");
    return;
  }

  if (msg.getParamCount() < IRC_PARAM_OFFSET) {
    sendError(client, "461", "JOIN :Not enough parameters");
    return;
  }

  std::string channelName = msg.getParams()[0];
  if (channelName[0] != '#' && channelName[0] != '&') {
    sendError(client, "403", channelName + " :No such channel");
    return;
  }

  /*
  Channel &channel = getOrCreateChannel(channelName);
  channel.addClient(client);
  */

  std::string nick = client.getNickname();
  std::string user = client.getUsername();
  std::string host = "localhost";

  std::string joinMsg = ":" + nick + "!" + user + "@" + host + " JOIN :" + channelName + "\r\n";

  /*
  channel.broadcast(joinMsg, NULL);

  To implement: Mostrar lista de usuários

  To implement: Mostrar tópico do canal
  */
}

void Server::sendWelcome(Client &client) {
  std::string nick = client.getNickname();

  std::string replies[] = {"001 " + nick + " :Welcome to the Internet Relay Network " + nick + "!" +
                               client.getUsername() + "@localhost",
                           "002 " + nick + " :Your host is " + _server_name + ", running version 1.0",
                           "003 " + nick + " " + _server_name + " 1.0 o o",
                           "004 " + nick + " CHANNELLEN=32 NICKLEN=9 TOPICLEN=307 :are " + "supported by this server"};

  for (size_t i = 0; i < IRC_WELCOME_COUNT; i++) {
    sendReply(client, ":" + _server_name + " " + replies[i] + "\r\n");
  }

  sendMOTD(client);
}

void Server::sendMOTD(Client &client) {
  std::string nick = client.getNickname();

  sendReply(client, ":" + _server_name + " 375 " + nick + " :- " + _server_name + " Message of the day -\r\n");
  sendReply(client, ":" + _server_name + " 372 " + nick + " :Welcome to ft_irc server!\r\n");
  sendReply(client, ":" + _server_name + " 376 " + nick + " :End of /MOTD command\r\n");
}
