#ifndef SERVER_HPP
#define SERVER_HPP

#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <set>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "./Channel.hpp"
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
  // Type alias for command handler function pointers
  typedef void (Server::*MessageHandler)(Client &, const IRCMessage &);

  int _port;
  int _server_socket;
  std::string _password;
  std::string _server_name;
  std::vector<Client*> _clients;
  std::map<std::string, Channel *> _channels;
  std::vector<pollfd> _poll_fds;
  std::set<int> _welcomed_clients;
  std::map<std::string, MessageHandler> _message_handlers;

  bool canSetMode(const Client &client, const Channel &channel) const;
  bool canKick(const Client &client, const Channel &channel) const;
  bool canInvite(const Client &client, const Channel &channel) const;
  bool canSetTopic(const Client &client, const Channel &channel) const;

  void initSocket(const int PORT);
  void setNonBlocking(int fd);
  void acceptClient();
  void handleClientData(Client &client);
  void removeClient(size_t index);
  void processCommand(Client &client, const std::string &command);

  void sendError(Client &client, const std::string &code, const std::string &message);
  void sendReply(Client &client, const std::string &message);
  void sendRaw(Client &client, const std::string &message);
  void flushClientOutput(Client &client);
  const std::string &getServerName() const;
  std::vector<std::string> splitCommand(const std::string &command);
  std::string getClientChannels(const Client &client) const;
  Client *findClientByNick(const std::string &nick);

  void handleTOPIC(Client &client, const IRCMessage &msg);
  void handleKICK(Client &client, const IRCMessage &msg);
  void handleJOIN(Client &client, const IRCMessage &msg);
  void handlePART(Client &client, const IRCMessage &msg);
  void handlePRIVMSG(Client &client, const IRCMessage &msg);
  void handlePING(Client &client, const IRCMessage &msg);
  void handleWHOIS(Client &client, const IRCMessage &msg);
  void handleMODE(Client &client, const IRCMessage &msg);
  void handleLIST(Client &client, const IRCMessage &msg);
  void handleNAMES(Client &client, const IRCMessage &msg);
  void handlePASS(Client &client, const IRCMessage &msg);
  void handleNICK(Client &client, const IRCMessage &msg);
  void handleUSER(Client &client, const IRCMessage &msg);
  void handleQUIT(Client &client, const IRCMessage &msg);
  void handleINVITE(Client &client, const IRCMessage &msg);

  void sendWelcome(Client &client);
  void sendISupport(Client &client);
  void sendMOTD(Client &client);

  void broadcastToChannel(const std::string &channelName, const std::string &message, Client *exclude = NULL);

  Channel *getChannels(const std::string &name);

  void checkAndSendWelcome(Client &client);

  bool isValidChannelName(const std::string &name) const;
};

#endif
