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

  void sendError(Client &client, const std::string &code,
                 const std::string &message);
  void sendReply(Client &client, const std::string &message);
  const std::string &getServerName() const;
  std::vector<std::string> splitCommand(const std::string &command);
};

#endif