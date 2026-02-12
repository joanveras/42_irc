#include "../include/Server.hpp"
#include "../include/Channel.hpp"
#include <algorithm>
#include <csignal>
#include <iostream>
#include <map>
#include <sstream>

namespace {
// Server constants for socket operations and polling
const int ERROR_CODE = -1;
const int POLL_TIMEOUT = -1;
const int SERVER_FD_INDEX = 0;
const int FIRST_CLIENT_INDEX = 1;
const size_t BUFFER_SIZE = 512;
const int SOCK_OPT = 1;
const int ONE_BYTE = 1;
// const int MIN_USER_ARGS = 5;
// const int REALNAME_ARG = 4;
const int MAX_CHANNELS_PER_USER = 10;

volatile sig_atomic_t g_shutdown_requested = 0;

void handleSignal(int signalNumber) {
  (void)signalNumber;
  g_shutdown_requested = 1;
}
} // namespace

Server::Server() {
}

Server::Server(const int PORT, const std::string &PASSWORD)
    : _port(PORT), _password(PASSWORD), _server_name("irc.server") {

  _message_handlers["PASS"] = &Server::handlePASS;
  _message_handlers["NICK"] = &Server::handleNICK;
  _message_handlers["USER"] = &Server::handleUSER;
  _message_handlers["QUIT"] = &Server::handleQUIT;

  _message_handlers["PING"] = &Server::handlePING;
  _message_handlers["JOIN"] = &Server::handleJOIN;
  _message_handlers["PART"] = &Server::handlePART;
  _message_handlers["PRIVMSG"] = &Server::handlePRIVMSG;
  _message_handlers["WHOIS"] = &Server::handleWHOIS;
  _message_handlers["LIST"] = &Server::handleLIST;
  _message_handlers["NAMES"] = &Server::handleNAMES;


  _message_handlers["MODE"] = &Server::handleMODE;
  _message_handlers["TOPIC"] = &Server::handleTOPIC;
  _message_handlers["INVITE"] = &Server::handleINVITE;
  _message_handlers["KICK"] = &Server::handleKICK;

}

Server::~Server() {
}

void Server::run() {
  std::signal(SIGINT, handleSignal);
  std::signal(SIGTERM, handleSignal);

  initSocket(_port);

  struct pollfd serverPollFd;
  serverPollFd.fd = _server_socket;
  serverPollFd.events = POLLIN;
  serverPollFd.revents = 0;

  _poll_fds.push_back(serverPollFd);

  std::cout << "Server running on port " << _port << std::endl;
  std::cout << "Waiting for connections..." << std::endl;

  while (!g_shutdown_requested) {
    for (size_t index = FIRST_CLIENT_INDEX; index < _poll_fds.size(); ++index) {
      Client &client = *_clients[index - FIRST_CLIENT_INDEX];
      short events = POLLIN;
      if (client.hasPendingOutput())
        events |= POLLOUT;
      _poll_fds[index].events = events;
    }

    int pollFd = poll(_poll_fds.data(), _poll_fds.size(), POLL_TIMEOUT);
    if (pollFd == ERROR_CODE) {
      if (errno == EINTR && g_shutdown_requested)
        break;
      std::cerr << "Poll error: " << strerror(errno) << std::endl;
      continue;
    }

    if ((_poll_fds[SERVER_FD_INDEX].revents & POLLIN) != 0)
      acceptClient();

    for (size_t index = FIRST_CLIENT_INDEX; index < _poll_fds.size(); ++index) {
      int clientFd = _poll_fds[index].fd;
      short revents = _poll_fds[index].revents;

      if ((revents & POLLIN) != 0) {
        Client &client = *_clients[index - FIRST_CLIENT_INDEX];
        handleClientData(client);
      }

      if (index >= _poll_fds.size())
        continue;
      if (_poll_fds[index].fd != clientFd)
        continue;

      if ((revents & POLLOUT) != 0) {
        Client &client = *_clients[index - FIRST_CLIENT_INDEX];
        flushClientOutput(client);
      }
    }
  }

  while (_poll_fds.size() > FIRST_CLIENT_INDEX) {
    removeClient(FIRST_CLIENT_INDEX);
  }
  if (_server_socket != ERROR_CODE) {
    close(_server_socket);
    _server_socket = ERROR_CODE;
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
    return;
  }

  setNonBlocking(CLIENT_SOCKET);

  _clients.push_back(new Client(CLIENT_SOCKET));

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
  const int clientFd = client.getFd();

  bytesRead = recv(client.getFd(), buffer, sizeof(buffer) - ONE_BYTE, 0);
  if (bytesRead > 0) {
    buffer[bytesRead] = '\0';
    std::string data(buffer, bytesRead);

    std::cout << "Received " << bytesRead << " bytes from client " << client.getFd() << std::endl;

    client.appendToBuffer(data);

    while (client.hasCompleteMessage()) {
      std::string command = client.extractCommand();
      //std::cout << "Complete command: [" << command << "]" << std::endl;

      processCommand(client, command);

      bool stillConnected = false;
      for (size_t index = FIRST_CLIENT_INDEX; index < _poll_fds.size(); ++index) {
        if (_poll_fds[index].fd == clientFd) {
          stillConnected = true;
          break;
        }
      }
      if (!stillConnected)
        return;
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

  for (std::map<std::string, Channel *>::iterator channelIt = _channels.begin(); channelIt != _channels.end();) {
    Channel *channel = channelIt->second;
    if (channel->isMember(clientFd)) {
      channel->removeMember(clientFd);
    }
    if (channel->getMembers().empty()) {
      delete channel;
      std::map<std::string, Channel *>::iterator toErase = channelIt++;
      _channels.erase(toErase);
      continue;
    }
    ++channelIt;
  }

  close(_poll_fds[index].fd);

  _poll_fds.erase(_poll_fds.begin() + index);
  delete _clients[index - FIRST_CLIENT_INDEX];
  _clients.erase(_clients.begin() + (index - FIRST_CLIENT_INDEX));
  _welcomed_clients.erase(clientFd);

  std::cout << "Client " << clientFd << " removed from poll set" << std::endl;
}

void	print(IRCMessage msg) {
	std::cout << "[ Prefix ] " << msg.getPrefix() << std::endl;
	std::cout << "[ CMD ] " << msg.getCommand() << std::endl;
	for (std::size_t i = 0; i < msg.getParams().size(); i++) {
		std::cout << "[ Params ]" << msg.getParams()[i] << std::endl;
	}
  std::cout << "[ TRAILING ] " << msg.getTrailing() << std::endl;
}

void Server::processCommand(Client &client, const std::string &raw)
{
	IRCMessage msg(raw);
  if (!msg.isValid()) {
    return;
  }
	std::string cmd = msg.getCommand();
	std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
  print(msg);
	if (!client.isAuthenticated() && cmd != "PASS" && cmd != "NICK" && cmd != "USER" && cmd != "QUIT")
	{
		sendError(client, "451", ":You have not registered");
		return;
	}

	std::map<std::string, MessageHandler>::iterator handler = _message_handlers.find(cmd);
	if (handler != _message_handlers.end())
	{
		(this->*(handler->second))(client, msg);
	}
	else
	{
		sendError(client, "421", cmd + " :Unknown command");
	}
}

void Server::sendError(Client &client, const std::string &code, const std::string &message) {
  std::string error = ":" + getServerName() + " " + code + " " +
                      (client.getNickname().empty() ? "*" : client.getNickname()) + " " + message + "\r\n";

  client.queueOutput(error);
}

void Server::sendReply(Client &client, const std::string &message) {
  std::string reply;

  if (message.find(":") == 0) {
    reply = message;
  } else {
    reply = ":" + _server_name + " " + message;
  }

  if (reply.find("\r\n") == std::string::npos) {
    reply += "\r\n";
  }

  client.queueOutput(reply);
}

void Server::sendRaw(Client &client, const std::string &message) {
  client.queueOutput(message);
}

void Server::flushClientOutput(Client &client) {
  std::string &outBuffer = client.getOutputBuffer();
  if (outBuffer.empty())
    return;

  ssize_t bytesSent = send(client.getFd(), outBuffer.c_str(), outBuffer.size(), 0);
  if (bytesSent > 0) {
    client.consumeOutput(static_cast<std::size_t>(bytesSent));
  } else if (bytesSent == ERROR_CODE && errno != EAGAIN && errno != EWOULDBLOCK) {
    for (size_t index = FIRST_CLIENT_INDEX; index < _poll_fds.size(); ++index) {
      if (_poll_fds[index].fd == client.getFd()) {
        removeClient(index);
        break;
      }
    }
  }
}

const std::string &Server::getServerName() const {
  return _server_name;
}

std::vector<std::string> Server::splitCommand(const std::string &command) {
  std::vector<std::string> args;
  std::istringstream iss(command);

  std::string arg;
  while (iss >> arg) {
    args.push_back(arg);
  }
  return args;
}

std::string Server::getClientChannels(const Client &client) const {
  std::string result;

  for (std::map<std::string, Channel *>::const_iterator it = _channels.begin(); it != _channels.end(); ++it) {
    if (it->second->isMember(client.getFd())) {
      if (!result.empty())
        result += " ";
      result += it->first;
    }
  }

  return result;
}

Client *Server::findClientByNick(const std::string &nick) {
  for (std::size_t i = 0; i < _clients.size(); ++i) {
    if (_clients[i]->getNickname() == nick) {
      return _clients[i];
    }
  }
  return NULL;
}

void Server::handlePASS(Client &client, const IRCMessage &msg) {
  if (msg.getParamCount() < 1) {
    sendError(client, "461", "PASS :Not enough parameters");
    return;
  }
  if (client.hasPassword()) {
    sendError(client, "462", "You may not reregister");
    return;
  }
  if (msg.getParams()[0] == _password) {
    client.setPassword(true);
    sendReply(client, "Password accepted");
    checkAndSendWelcome(client);
    return;
  }
  sendError(client, "464", "Password incorrect");
}

void Server::handleNICK(Client &client, const IRCMessage &msg) {
  if (msg.getParamCount() < 1) {
    sendError(client, "431", "No nickname given");
    return;
  }

  std::string nickname = msg.getParams()[0];

  if (nickname.empty() || nickname.find(' ') != std::string::npos) {
    sendError(client, "432", nickname + " :Erroneous nickname");
    return;
  }

  for (size_t i = 0; i < _clients.size(); ++i) {
    if (_clients[i]->getFd() != client.getFd() && _clients[i]->getNickname() == nickname) {
      sendError(client, "433", nickname + " :Nickname is already in use");
      return;
    }
  }

  client.setNickname(nickname);
  sendReply(client, "NICK set to: " + nickname);
  checkAndSendWelcome(client);
}

void Server::handleUSER(Client &client, const IRCMessage &msg) {
  // USER <username> <mode> <unused> :<realname>
  if (msg.getParamCount() < 3 || msg.getTrailing().empty()) {
    sendError(client, "461", "USER :Not enough parameters");
    return;
  }

  if (client.hasUser()) {
    sendError(client, "462", "You may not reregister");
    return;
  }

  client.setUsername(msg.getParams()[0]);
  client.setRealname(msg.getTrailing());
  sendReply(client, "USER registered");
  checkAndSendWelcome(client);
}

void Server::handleQUIT(Client &client, const IRCMessage &msg) {
  (void)msg;

  struct linger lingerOption;
  lingerOption.l_onoff = 1;
  lingerOption.l_linger = 0;
  setsockopt(client.getFd(), SOL_SOCKET, SO_LINGER, &lingerOption, sizeof(lingerOption));

  // shutdown(client.getFd(), SHUT_RDWR); uso somente no MAC para o teste do Quit
  for (std::size_t i = FIRST_CLIENT_INDEX; i < _poll_fds.size(); ++i) {
    if (_poll_fds[i].fd == client.getFd()) {
      removeClient(i);
      break;
    }
  }
}

void Server::handlePING(Client &client, const IRCMessage &msg) {
  if (msg.getParamCount() < IRC_PARAM_OFFSET && msg.getTrailing().empty()) {
    sendError(client, "409", "No origin specified");
    return;
  }

  std::string pingToken = msg.getParamCount() >= IRC_PARAM_OFFSET ? msg.getParams()[0] : msg.getTrailing();
  std::string pong = "PONG " + _server_name + " :" + pingToken;
  sendReply(client, pong);
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
  if (!isValidChannelName(channelName)) {
    sendError(client, "403", channelName + " :No such channel");
    return;
  }

  bool channelCreated = false;
  std::map<std::string, Channel *>::iterator channelIt = _channels.find(channelName);
  if (channelIt == _channels.end()) {
    channelCreated = true;
  }

  Channel *channel = getChannels(channelName);
  if (channel->isMember(client.getFd())) {
    return;
  }
  std::string channelKey = msg.getParamCount() > 1 ? msg.getParams()[1] : "";
  if (!canJoin(client, *channel, channelKey)) {
      return;
  }
  channel->addMember(&client);
  if (channelCreated) {
    channel->addOperator(client.getFd());
  }

  std::string nick = client.getNickname();
  std::string user = client.getUsername();
  std::string host = "localhost";

  std::string joinMsg = ":" + nick + "!" + user + "@" + host + " JOIN :" + channelName + "\r\n";

  sendRaw(client, joinMsg);
  channel->broadcast(joinMsg, client.getFd());
}

void Server::handlePART(Client &client, const IRCMessage &msg) {
  if (!client.isAuthenticated()) {
    sendError(client, "451", ":You have not registered");
    return;
  }

  if (msg.getParamCount() < 1) {
    sendError(client, "461", "PART :Not enough parameters");
    return;
  }

  std::string channelName = msg.getParams()[0];
  std::string reason = msg.getParamCount() > 1 ? msg.getTrailing() : client.getNickname();

  std::map<std::string, Channel *>::iterator it = _channels.find(channelName);
  if (it == _channels.end()) {
    sendError(client, "403", channelName + " :No such channel");
    return;
  }

  Channel &channel = *it->second;

  if (!channel.isMember(client.getFd())) {
    sendError(client, "442", channelName + " :You're not on that channel");
    return;
  }

  std::string partMsg = ":" + client.getNickname() + "!" + client.getUsername() + "@localhost PART " + channelName;
  if (!reason.empty()) {
    partMsg += " :" + reason;
  }
  partMsg += "\r\n";

  channel.broadcast(partMsg, client.getFd());
  channel.removeMember(client.getFd());

  if (channel.getMembersNumber() == 0) {
    _channels.erase(it);
  }

  sendRaw(client, partMsg);
}

void Server::handlePRIVMSG(Client &client, const IRCMessage &msg) {
  if (!client.isAuthenticated()) {
    sendError(client, "451", ":You have not registered");
    return;
  }

  if (msg.getParamCount() < 1) {
    sendError(client, "411", ":No recipient given (PRIVMSG)");
    return;
  }

  if (msg.getTrailing().empty()) {
    sendError(client, "412", ":No text to send");
    return;
  }

  std::string target = msg.getParams()[0];
  const std::string &message = msg.getTrailing();

  std::string prefix = ":" + client.getNickname() + "!" + client.getUsername() + "@localhost";

  if (target[0] == '#' || target[0] == '&') {
    std::map<std::string, Channel *>::iterator it = _channels.find(target);
    if (it == _channels.end()) {
      sendError(client, "403", target + " :No such channel");
      return;
    }

    Channel &channel = *it->second;

    if (!channel.isMember(client.getFd())) {
      sendError(client, "404", target + " :Cannot send to channel");
      return;
    }

    /*
    Verifica se nao esta mudo (+m mode) - simplificado por agora
    (implementar depois com modos)
    */

    std::string privmsg = prefix + " PRIVMSG " + target + " :" + message + "\r\n";
    channel.broadcast(privmsg, client.getFd());

  } else {
    Client *targetClient = findClientByNick(target);
    if (targetClient == NULL) {
      sendError(client, "401", target + " :No such nick/channel");
      return;
    }

    /* Verifica se nao esta +g (server notice) ou outros modos */

    std::string privmsg = prefix + " PRIVMSG " + target + " :" + message + "\r\n";
    sendRaw(*targetClient, privmsg);
  }
}

void Server::handleWHOIS(Client &client, const IRCMessage &msg) {
  if (!client.isAuthenticated()) {
    sendError(client, "451", ":You have not registered");
    return;
  }

  if (msg.getParamCount() < 1) {
    sendError(client, "431", ":No nickname given");
    return;
  }

  std::string targetNick = msg.getParams()[0];
  Client *targetClient = findClientByNick(targetNick);

  std::string senderNick = client.getNickname();

  // RPL_ENDOFWHOIS
  if (targetClient == NULL) {
    sendReply(client, " 401 " + senderNick + " " + targetNick + " :No such nick/channel\r\n");
    sendReply(client, " 318 " + senderNick + " " + targetNick + " :End of /WHOIS list\r\n");
    return;
  }

  // RPL_WHOISUSER
  sendReply(client, " 311 " + senderNick + " " + targetNick + " " + targetClient->getUsername() +
                        " localhost * :" + targetClient->getRealname() + "\r\n");

  // RPL_WHOISSERVER
  sendReply(client, " 312 " + senderNick + " " + targetNick + " " + " :ft_irc server\r\n");

  // RPL_WHOISCHANNELS (canais que o usuario esta)
  std::string channels = getClientChannels(*targetClient);
  if (!channels.empty()) {
    sendReply(client, " 319 " + senderNick + " " + targetNick + " :" + channels + "\r\n");
  }

  // RPL_WHOISIDLE (simplificado)
  sendReply(client, " 317 " + senderNick + " " + targetNick + " 0 0 :seconds idle, signon time\r\n");

  // RPL_ENDOFWHOIS
  sendReply(client, " 318 " + senderNick + " " + targetNick + " :End of /WHOIS list\r\n");
}

void Server::sendWelcome(Client &client) {
  const std::string &nick = client.getNickname();
  const std::string &user = client.getUsername();

  if (nick.empty() || user.empty())
    return;

  sendReply(client, "001 " + nick + " :Welcome to the Internet Relay Network " + nick + "!" + user + "@localhost");
  sendReply(client, "002 " + nick + " :Your host is " + _server_name + ", running version 1.0");
  sendReply(client, "003 " + nick + " :This server was created " + _server_name);
  sendReply(client, "004 " + nick + " " + _server_name + " 1.0 o o");

  sendISupport(client);
  sendMOTD(client);
}

void Server::sendMOTD(Client &client) {
  std::string nick = client.getNickname();

  // 375 RPL_MOTDSTART
  sendReply(client, "375 " + nick + " :- " + _server_name + " Message of the day -");

  // 372 RPL_MOTD (pode ter várias linhas)
  sendReply(client, "372 " + nick + " :- ========================================");
  sendReply(client, "372 " + nick + " :- Welcome to ft_irc - 42 School Project");
  sendReply(client, "372 " + nick + " :- Server implemented in C++98");
  sendReply(client, "372 " + nick + " :- Enjoy your stay!");
  sendReply(client, "372 " + nick + " :- ========================================");

  // 376 RPL_ENDOFMOTD
  sendReply(client, "376 " + nick + " :End of /MOTD command.");
}

void Server::sendISupport(Client &client) {
  const std::string &nick = client.getNickname();

  // Lista de features suportadas pelo servidor
  std::string features = "CHANNELLEN=32 "       // Maximo 32 caracteres no nome do canal
                         "NICKLEN=9 "           // Maximo 9 caracteres no nickname
                         "TOPICLEN=307 "        // Maximo 307 caracteres no topico
                         "CHANTYPES=#& "        // Tipos de canais suportados (# e &)
                         "PREFIX=(ov)@+ "       // Prefixos: @ para operador, + para voice
                         "CHANMODES=i,t,k,o,l " // Modos de canal suportados
                         "MODES=4 "             // Numero maximo de modos por comando
                         "MAXTARGETS=1 "        // Maximo de alvos por comando
                         "NETWORK=ft_irc "      // Nome da rede
                         "CASEMAPPING=ascii "   // Mapeamento de case (simplificado)
                         "CHARSET=ascii "       // Conjunto de caracteres
                         "NICKCHARS=abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789[]\\`_^{|}- "
                         "USERCHARS=abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.- "
                         "HOSTCHARS=abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.- "
                         "EXCEPTS "     // Suporte a ban masks (+e)
                         "INVEX "       // Suporte a invite exceptions (+I)
                         "SAFELIST "    // LIST nao causa flood
                         "WALLCHOPS "   // Envio de mensagens para ops
                         "WALLVOICES "; // Envio de mensagens para voiced users

  std::ostringstream oss;
  oss << "MAXCHANNELS=" << MAX_CHANNELS_PER_USER << " ";
  oss << "MAXBANS=30 "; // Maximo de bans por canal
  oss << "MAXPARA=32 "; // Maximo de parametros por comando

  features += oss.str();

  sendReply(client, _server_name + " 005 " + nick + " " + features + ":are supported by this server\r\n");

  // Linha adicional para mais features se necessario
  std::string features2 = "STATUSMSG=@+ " // Mensagens para grupos (@ ou +)
                          "ELIST=CMNTU "  // Extensoes para LIST
                          "EXTBAN=$,& "   // Tipos de extended bans
                          "MONITOR=30 ";  // Maximo de usuarios no MONITOR

  sendReply(client, _server_name + " 005 " + nick + " " + features2 + ":are also supported\r\n");
}

Channel *Server::getChannels(const std::string &name) {
  std::map<std::string, Channel *>::iterator it = _channels.find(name);

  if (it != _channels.end()) {
    return it->second;
  }

  std::cout << "\033[41m" << "Novo canal criado:" << "\033[0m " << name << std::endl;
  Channel *newChannel = new Channel(name);
  _channels[name] = newChannel;
  return newChannel;
}

bool Server::isValidChannelName(const std::string &name) const {
  if (name.length() > 200) {
    return false;
  }
  if (name[0] != '&' && name[0] != '#') {
    return false;
  }
  for (std::string::const_iterator it = name.begin(); it != name.end(); it++) {
    if (*it == ' ' || *it == ',' || *it == '\a') {
      return false;
    }
  }
  return true;
}

void Server::handleMODE(Client &client, const IRCMessage &msg) {
  if (!client.isAuthenticated()) {
    sendError(client, "451", ":You have not registered");
    return;
  }

  if (msg.getParamCount() < 1) {
    sendError(client, "461", "MODE :Not enough parameters");
    return;
  }

  std::string target = msg.getParams()[0];

  // if (target[0] != '#' && target[0] != '&') {
  //   sendError(client, "502", ":Cannot change mode for other users");
  //   return;
  // }

  std::map<std::string, Channel *>::iterator it = _channels.find(target);
  if (it == _channels.end()) {
    sendError(client, "403", target + " :No such channel");
    return;
  }

  Channel *channel = it->second;

  if (msg.getParamCount() == 1 && msg.getTrailing().empty()) {
    std::string modes = "+";
    std::string modeParams;

    if (channel->isInviteOnly())
      modes += "i";
    if (channel->getMode('t'))
      modes += "t";
    if (channel->hasKey()) {
      modes += "k";
      modeParams = " " + channel->getKey();
    }

    sendReply(client, "324 " + client.getNickname() + " " + target + " " + modes + modeParams);
    return;
  }

  if (!channel->isOperator(client.getFd())) {
    sendError(client, "482", target + " :You're not channel operator");
    return;
  }

  std::string modeStr = msg.getParams()[1];
  bool adding = true;
  size_t paramIndex = 2;

  std::string modeChanges;

  for (size_t i = 0; i < modeStr.length(); ++i) {
    char mode = modeStr[i];

    if (mode == '+') {
      adding = true;
      modeChanges += '+';
      continue;
    } else if (mode == '-') {
      adding = false;
      modeChanges += '-';
      continue;
    }

    switch (mode) {
    case 'i': // invite-only
      channel->setMode('i', adding);
      modeChanges += 'i';
      break;

    case 't': // topic restriction
      channel->setMode('t', adding);
      modeChanges += 't';
      break;

    case 'k': // channel key (password)
      if (adding) {
        if (paramIndex < msg.getParamCount()) {
          channel->setKey(msg.getParams()[paramIndex]);
          channel->setMode('k', true);
          modeChanges += 'k';
          modeChanges += " " + msg.getParams()[paramIndex++];
        } else {
          sendError(client, "461", "MODE k :Not enough parameters");
        }
      } else {
        channel->setKey("");
        channel->setMode('k', false);
        modeChanges += 'k';
      }
      break;

    case 'o': // operator privilege
      if (paramIndex < msg.getParamCount()) {
        std::string targetNick = msg.getParams()[paramIndex++];
        Client *targetClient = findClientByNick(targetNick);
        if (targetClient && channel->isMember(targetClient->getFd())) {
          if (adding) {
            channel->addOperator(targetClient->getFd());
          } else {
            channel->removeOperator(targetClient->getFd());
          }
          modeChanges += 'o';
          modeChanges += " " + targetNick;
        }
      }
      break;

    case 'l': // user limit
      if (adding) {
        if (paramIndex < msg.getParamCount()) {
          int limitValue = atoi(msg.getParams()[paramIndex++].c_str());
          if (limitValue > 0) {
            channel->setLimit(static_cast<std::size_t>(limitValue));
            channel->setMode('l', true);
            modeChanges += 'l';
            modeChanges += " " + msg.getParams()[paramIndex - 1];
          }
        }
      } else {
        channel->setLimit(0); // Remove limit
        channel->setMode('l', false);
        modeChanges += 'l';
      }
      break;

    default:
      sendError(client, "472", std::string(1, mode) + " :is unknown mode char to me");
      continue;
    }
  }

  if (!modeChanges.empty()) {
    std::string modeMsg = ":" + client.getNickname() + "!" + client.getUsername() + "@localhost MODE " + target + " " +
                          modeChanges + "\r\n";
    broadcastToChannel(target, modeMsg);
  }
}

void Server::handleLIST(Client &client, const IRCMessage &msg) {
  if (!client.isAuthenticated()) {
    sendError(client, "451", ":You have not registered");
    return;
  }

  std::string senderNick = client.getNickname();

  // RPL_LISTSTART (321)
  sendReply(client, "321 " + senderNick + " Channel :Users Name");

  std::string filter;
  if (msg.getParamCount() > 0) {
    filter = msg.getParams()[0];
  }

  for (std::map<std::string, Channel *>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
    // if (!filter.empty() && it->first != filter) {
    //   continue;
    // }

    Channel *channel = it->second;

    std::string topic = channel->getTopic();
    if (topic.empty()) {
      topic = "No topic";
    }

    // RPL_LIST (322)
    std::ostringstream userCount;
    userCount << channel->getMembersNumber();

    sendReply(client, "322 " + senderNick + " " + it->first + " " + userCount.str() + " :" + topic);
  }

  // RPL_LISTEND (323)
  sendReply(client, "323 " + senderNick + " :End of /LIST");
}

void Server::handleNAMES(Client &client, const IRCMessage &msg) {
  if (!client.isAuthenticated()) {
    sendError(client, "451", ":You have not registered");
    return;
  }

  if (msg.getParamCount() < 1) {
    sendError(client, "461", "NAMES :Not enough parameters");
    return;
  }

  std::string channelName = msg.getParams()[0];
  std::string senderNick = client.getNickname();

  std::map<std::string, Channel *>::iterator it = _channels.find(channelName);
  if (it == _channels.end()) {
    sendError(client, "403", channelName + " :No such channel");
    return;
  }

  Channel *channel = it->second;

  // RPL_NAMREPLY (353)
  // Formato: :server 353 nick = #channel :@op1 +voice1 normal1
  std::string userList = channel->getUserList();

  sendReply(client, "353 " + senderNick + " = " + channelName + " :" + userList);

  // RPL_ENDOFNAMES (366)
  sendReply(client, "366 " + senderNick + " " + channelName + " :End of /NAMES list");
}

void Server::handleTOPIC(Client &client, const IRCMessage &msg) {
  if (!client.isAuthenticated()) {
    sendError(client, "451", ":You have not registered");
    return;
  }

  if (msg.getParamCount() < 1) {
    sendError(client, "461", "TOPIC :Not enough parameters");
    return;
  }

  std::string channelName = msg.getParams()[0];
  std::map<std::string, Channel *>::iterator it = _channels.find(channelName);

  if (it == _channels.end()) {
    sendError(client, "403", channelName + " :No such channel");
    return;
  }

  Channel *channel = it->second;
  if (it->second == NULL)
    std::cerr << "Channel -> NULL: Server.cpp:891" << std::endl;

  if (!channel->isMember(client.getFd())) {
    sendError(client, "442", channelName + " :You're not on that channel");
    return;
  }

  if (msg.getParamCount() == 1 && msg.getTrailing().empty()) {
    std::string topic = channel->getTopic();
    if (topic.empty()) {
      sendReply(client, "331 " + client.getNickname() + " " + channelName + " :No topic is set");
    } else {
      sendReply(client, "332 " + client.getNickname() + " " + channelName + " :" + topic);
    }
    return;
  }

  if (!msg.getTrailing().empty() && !channel->isOperator(client.getFd())) {
    sendError(client, "482", channelName + " :You're not channel operator");
    return;
  }

  std::string newTopic = msg.getTrailing();
  channel->setTopic(newTopic);

  std::cout << "Topico do canal " << channel->getName() << ": " << channel->getTopic() << std::endl;

  std::string topicMsg = ":" + client.getNickname() + "!" + client.getUsername() + "@localhost TOPIC " + channelName +
                         " :" + newTopic + "\r\n";
  broadcastToChannel(channelName, topicMsg, &client);

  sendRaw(client, topicMsg);
}

void Server::handleINVITE(Client &client, const IRCMessage &msg) {
  if (!client.isAuthenticated()) {
    sendError(client, "451", ":You have not registered");
    return;
  }

  if (msg.getParamCount() < 2) {
    sendError(client, "461", "INVITE :Not enough parameters");
    return;
  }

  std::string targetNick = msg.getParams()[0];
  std::string channelName = msg.getParams()[1];

  std::map<std::string, Channel *>::iterator channelIt = _channels.find(channelName);
  if (channelIt == _channels.end()) {
    sendError(client, "403", channelName + " :No such channel");
    return;
  }

  Channel *channel = channelIt->second;
  if (!channel->isMember(client.getFd())) {
    sendError(client, "442", channelName + " :You're not on that channel");
    return;
  }

  if (!channel->isOperator(client.getFd())) {
    sendError(client, "482", channelName + " :You're not channel operator");
    return;
  }

  Client *targetClient = findClientByNick(targetNick);
  if (targetClient == NULL) {
    sendError(client, "401", targetNick + " :No such nick/channel");
    return;
  }

  channel->inviteMember(targetClient->getFd());

  std::string prefix = ":" + client.getNickname() + "!" + client.getUsername() + "@localhost";
  std::string inviteMsg = prefix + " INVITE " + targetNick + " :" + channelName + "\r\n";
  sendRaw(*targetClient, inviteMsg);
  sendReply(client, "341 " + client.getNickname() + " " + targetNick + " " + channelName);
}

void Server::handleKICK(Client &client, const IRCMessage &msg) {
  if (!client.isAuthenticated()) {
    sendError(client, "451", ":You have not registered");
    return;
  }

  if (msg.getParamCount() < 2) {
    sendError(client, "461", "KICK :Not enough parameters");
    return;
  }

  std::string channelName = msg.getParams()[0];
  std::string targetNick = msg.getParams()[1];
  std::string reason = msg.getParamCount() > 2 ? msg.getTrailing() : client.getNickname();

  std::map<std::string, Channel *>::iterator it = _channels.find(channelName);
  if (it == _channels.end()) {
    sendError(client, "403", channelName + " :No such channel");
    return;
  }

  Channel *channel = it->second;

  if (!channel->isMember(client.getFd())) {
    sendError(client, "442", channelName + " :You're not on that channel");
    return;
  }

  if (!channel->isOperator(client.getFd())) {
    sendError(client, "482", channelName + " :You're not channel operator");
    return;
  }

  Client *targetClient = findClientByNick(targetNick);
  if (!targetClient) {
    sendError(client, "401", targetNick + " :No such nick/channel");
    return;
  }

  if (!channel->isMember(targetClient->getFd())) {
    sendError(client, "441", targetNick + " " + channelName + " :They aren't on that channel");
    return;
  }

  std::string kickMsg = ":" + client.getNickname() + "!" + client.getUsername() + "@localhost KICK " + channelName +
                        " " + targetNick + " :" + reason + "\r\n";

  broadcastToChannel(channelName, kickMsg);

  channel->removeMember(targetClient->getFd());

  if (channel->getMembersNumber() == 0) {
    _channels.erase(it);
  }
}

void Server::broadcastToChannel(const std::string &channelName, const std::string &message, Client *exclude) {
  std::map<std::string, Channel *>::const_iterator it = _channels.find(channelName);

  if (it != _channels.end() && it->second != NULL) {
    Channel *channel = it->second;

    int FD = -1;
    if (exclude)
      FD = exclude->getFd();

    channel->broadcast(message, FD);
  }
}

void Server::checkAndSendWelcome(Client &client) {
  if (client.isAuthenticated() && _welcomed_clients.find(client.getFd()) == _welcomed_clients.end()) {

    _welcomed_clients.insert(client.getFd());
    sendWelcome(client);
  }
}

bool Server::canJoin(const Client &client, const Channel &channel, const std::string &key) const {
  if (channel.getMode('i')) {
    if (!channel.isInvitedFd(client.getFd())) {
      return false;
    }
  }
  if (channel.getMode('l') && channel.getMembersNumber() >= channel.getLimit()) {
    return false;
  }
  if (channel.getMode('k') &&  key != channel.getKey()) {
    return false;
  }
  return true;
}
