#ifndef SERVER_HPP
#define SERVER_HPP

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "./Client.hpp"
#include "./IRCMessage.hpp"

class Server {

public:
  Server();
  explicit Server(const int PORT, const std::string &PASSWORD);
  ~Server();
  Server(Server const &other);

  Server operator=(Server const &other);

  void run();

private:
  int _port;
  int _server_socket;
  std::string _password;
  std::string _server_name;
  std::vector<Client> _clients;
  std::vector<pollfd> _poll_fds;

  void initSocket(const int PORT);
  void setNonBlocking(int fd);
  void acceptClient();
  void handleClientData(Client &client);
  void removeClient(size_t index);
  void processCommand(Client &client, const std::string &command);

  void handlePASS(Client &client, const std::vector<std::string> &args);
  void handleNICK(Client &client, const std::vector<std::string> &args);
  void handleUSER(Client &client, const std::vector<std::string> &args);
  void handleQUIT(Client &client, const std::vector<std::string> &args);

  void sendError(Client &client, const std::string &code, const std::string &message);
  void sendReply(Client &client, const std::string &message);
  const std::string &getServerName() const;
  std::vector<std::string> splitCommand(const std::string &command);

  void handleJOIN(Client &client, const IRCMessage &msg);
  void handlePART(Client &client, const IRCMessage &msg);
  void handlePRIVMSG(Client &client, const IRCMessage &msg);
  void handlePING(Client &client, const IRCMessage &msg);
  void handleWHOIS(Client &client, const IRCMessage &msg);
  void handleMODE(Client &client, const IRCMessage &msg);
  void handleLIST(Client &client, const IRCMessage &msg);
  void handleNAMES(Client &client, const IRCMessage &msg);

  void sendWelcome(Client &client);
  void sendISupport(Client &client);
  void sendMOTD(Client &client);

  void broadcastToChannel(const std::string &channel, const std::string &message, Client *exclude = NULL);
};

#endif