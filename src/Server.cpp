#include "../include/Server.hpp"
#include "../include/Channel.hpp"

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
const int MAX_CHANNELS_PER_USER = 10;
} // namespace

Server::Server() {
}

Server::Server(const int PORT, const std::string &PASSWORD)
    : _port(PORT), _password(PASSWORD), _server_name("irc.server") {
  _command_handlers["PASS"] = &Server::handlePASS;
  _command_handlers["NICK"] = &Server::handleNICK;
  _command_handlers["USER"] = &Server::handleUSER;
  _command_handlers["QUIT"] = &Server::handleQUIT;

  _message_handlers["PASS"] = &Server::handlePASS;
  _message_handlers["NICK"] = &Server::handleNICK;
  _message_handlers["USER"] = &Server::handleUSER;
  _message_handlers["QUIT"] = &Server::handleQUIT;

  _message_handlers["PING"] = &Server::handlePING;
  _message_handlers["JOIN"] = &Server::handleJOIN;
  _message_handlers["PART"] = &Server::handlePART;
  _message_handlers["PRIVMSG"] = &Server::handlePRIVMSG;
  _message_handlers["WHOIS"] = &Server::handleWHOIS;


  _message_handlers["MODE"] = &Server::handleMODE;
  _message_handlers["TOPIC"] = &Server::handleTOPIC;
  _message_handlers["INVITE"] = &Server::handleINVITE;
  _message_handlers["KICK"] = &Server::handleKICK;

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
    return;
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


void Server::processCommand(Client &client, const std::string &raw)
{
	IRCMessage msg(raw);
	std::string cmd = msg.getCommand();
	std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

	if (!client.isAuthenticated() && cmd != "PASS" && cmd != "NICK" && cmd != "USER" && cmd != "QUIT")
	{
		sendError(client, "451", ":You have not registered");
		return;
	}

	std::map<std::string, MessageHandler>::iterator it = _message_handlers.find(cmd);
  if (it != _message_handlers.end()) {
    (this->*(it->second))(client, msg);
  } else {
    sendError(client, "421", cmd + " :Unknown command");
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
  while (iss >> arg) {
    args.push_back(arg);
  }
  return args;
}

std::string Server::getClientChannels(const Client &client) const {
  std::string result;

  for (std::map<std::string, Channel*>::const_iterator it = _channels.begin(); it != _channels.end(); ++it) {
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
    if (_clients[i].getNickname() == nick) {
      return &_clients[i];
    }
  }
  return NULL;
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

  for (std::size_t i = FIRST_CLIENT_INDEX; i < _poll_fds.size(); ++i) {
    if (_poll_fds[i].fd == client.getFd()) {
      removeClient(i);
      break;
    }
  }
}

// ===== Registration handlers (IRCMessage overloads) =====

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
    if (_clients[i].getFd() != client.getFd() && _clients[i].getNickname() == nickname) {
      sendError(client, "433", nickname + " :Nickname is already in use");
      return;
    }
  }

  client.setNickname(nickname);
  sendReply(client, "NICK set to: " + nickname);
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
}

void Server::handleQUIT(Client &client, const IRCMessage &msg) {
  (void)msg;

  for (std::size_t i = FIRST_CLIENT_INDEX; i < _poll_fds.size(); ++i) {
    if (_poll_fds[i].fd == client.getFd()) {
      removeClient(i);
      break;
    }
  }
}

void Server::handleMODE(Client &client, const IRCMessage &msg) { (void)client; (void)msg; }
void Server::handleTOPIC(Client &client, const IRCMessage &msg) { (void)client; (void)msg; }
void Server::handleINVITE(Client &client, const IRCMessage &msg) { (void)client; (void)msg; }
void Server::handleKICK(Client &client, const IRCMessage &msg) { (void)client; (void)msg; }

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
  // validação de nome aqui
  // if (channelName[0] != '#' && channelName[0] != '&') {
  //   sendError(client, "403", channelName + " :No such channel");
  //   return;
  // }

  Channel *channel = getChannels(channelName);
  channel->addMember(&client);

  std::cout << "Nome do canal: " << channel->getName() << std::endl;

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

  std::map<std::string, Channel*>::iterator it = _channels.find(channelName);
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

  this->checkEmptyChannel(it->first);
  // if (channel.isEmpty()) {
  //   _channels.erase(it);
  // }

  sendReply(client, partMsg);
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
    std::map<std::string, Channel*>::iterator it = _channels.find(target);
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
    Verifica se não está mudo (+m mode) - simplificado por agora
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

    /* Verifica se não está +g (server notice) ou outros modos */

    std::string privmsg = prefix + " PRIVMSG " + target + " :" + message + "\r\n";
    sendReply(*targetClient, privmsg);
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
    sendReply(client, ":" + _server_name + " 401 " + senderNick + " " + targetNick + " :No such nick/channel\r\n");
    sendReply(client, ":" + _server_name + " 318 " + senderNick + " " + targetNick + " :End of /WHOIS list\r\n");
    return;
  }

  // RPL_WHOISUSER
  sendReply(client, ":" + _server_name + " 311 " + senderNick + " " + targetNick + " " + targetClient->getUsername() +
                        " localhost * :" + targetClient->getRealname() + "\r\n");

  // RPL_WHOISSERVER
  sendReply(client,
            ":" + _server_name + " 312 " + senderNick + " " + targetNick + " " + _server_name + " :ft_irc server\r\n");

  // RPL_WHOISCHANNELS (canais que o usuário está)
  std::string channels = getClientChannels(*targetClient);
  if (!channels.empty()) {
    sendReply(client, ":" + _server_name + " 319 " + senderNick + " " + targetNick + " :" + channels + "\r\n");
  }

  // RPL_WHOISIDLE (simplificado)
  sendReply(client,
            ":" + _server_name + " 317 " + senderNick + " " + targetNick + " 0 0 :seconds idle, signon time\r\n");

  // RPL_ENDOFWHOIS
  sendReply(client, ":" + _server_name + " 318 " + senderNick + " " + targetNick + " :End of /WHOIS list\r\n");
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

void Server::sendISupport(Client &client) {
  const std::string &nick = client.getNickname();

  // Lista de features suportadas pelo servidor
  std::string features = "CHANNELLEN=32 "       // Máximo 32 caracteres no nome do canal
                         "NICKLEN=9 "           // Máximo 9 caracteres no nickname
                         "TOPICLEN=307 "        // Máximo 307 caracteres no tópico
                         "CHANTYPES=#& "        // Tipos de canais suportados (# e &)
                         "PREFIX=(ov)@+ "       // Prefixos: @ para operador, + para voice
                         "CHANMODES=i,t,k,o,l " // Modos de canal suportados
                         "MODES=4 "             // Número máximo de modos por comando
                         "MAXTARGETS=1 "        // Máximo de alvos por comando
                         "NETWORK=ft_irc "      // Nome da rede
                         "CASEMAPPING=ascii "   // Mapeamento de case (simplificado)
                         "CHARSET=ascii "       // Conjunto de caracteres
                         "NICKCHARS=abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789[]\\`_^{|}- "
                         "USERCHARS=abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.- "
                         "HOSTCHARS=abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.- "
                         "EXCEPTS "     // Suporte a ban masks (+e)
                         "INVEX "       // Suporte a invite exceptions (+I)
                         "SAFELIST "    // LIST não causa flood
                         "WALLCHOPS "   // Envio de mensagens para ops
                         "WALLVOICES "; // Envio de mensagens para voiced users

  std::ostringstream oss;
  oss << "MAXCHANNELS=" << MAX_CHANNELS_PER_USER << " ";
  oss << "MAXBANS=30 "; // Máximo de bans por canal
  oss << "MAXPARA=32 "; // Máximo de parâmetros por comando

  features += oss.str();

  sendReply(client, ":" + _server_name + " 005 " + nick + " " + features + ":are supported by this server\r\n");

  // Linha adicional para mais features se necessário
  std::string features2 = "STATUSMSG=@+ " // Mensagens para grupos (@ ou +)
                          "ELIST=CMNTU "  // Extensões para LIST
                          "EXTBAN=$,& "   // Tipos de extended bans
                          "MONITOR=30 ";  // Máximo de usuários no MONITOR

  sendReply(client, ":" + _server_name + " 005 " + nick + " " + features2 + ":are also supported\r\n");
}

// void Server::broadcastToChannel(const std::string &channelName, const std::string &rawMessage, Client *exclude) {
//   std::map<std::string, Channel*>::iterator it = _channels.find(channelName);
//   if (it == _channels.end())
//     return;

//   Channel &channel = *it->second;

//   std::string message = rawMessage;
//   if (message.find("\r\n") == std::string::npos) {
//     message += "\r\n";
//   }

//   const std::vector<int> &fds = channel.getClientFds();
//   int excludeFd = exclude != NULL ? exclude->getFd() : -1;

//   for (size_t i = 0; i < fds.size(); ++i) {
//     if (fds[i] != excludeFd) {
//       send(fds[i], message.c_str(), message.length(), 0);

// // Log para debug (opcional)
// #ifdef DEBUG
//       std::cout << "Broadcast to fd " << fds[i] << ": " << message.substr(0, message.find("\r\n")) << std::endl;
// #endif
//     }
//   }
// }

Channel *Server::getChannels(const std::string &name) {
	std::map<std::string, Channel*>::iterator it = _channels.find(name);
	
	if (it != _channels.end()) {
		return it->second;
	}
	
	Channel *newChannel = new Channel(name);
	_channels[name] = newChannel;
	return newChannel;
}

void Server::checkEmptyChannel(const std::string &name) {
	std::map<std::string, Channel*>::iterator it = _channels.find(name);

	if (it != _channels.end()) {
		delete it->second;
		_channels.erase(it);
		std::cout << "Canal " << name << " deletado ou vázio." << std::endl;
	}
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
