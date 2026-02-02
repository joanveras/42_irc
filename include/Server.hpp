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

# include "./Client.hpp"
# include "Channel.hpp"
# include "IRCMessage.hpp"

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
  typedef void (Server::*CommandHandler)(Client &, const std::vector<std::string> &);

	private:
		int					_port;
		int					_server_socket;
		std::string			_password;
		std::string			_server_name;
		std::vector<Client>	_clients;
		std::vector<pollfd>	_poll_fds;
		std::map<std::string, Channel*> _channels;
		
		void						initSocket( const int port );
		void						setNonBlocking( int fd );
		void						acceptClient( void );
		void						handleClientData( Client& client );
		void						removeClient( size_t index );
		void						processCommand( Client& client, const std::string& command );

  bool canSetMode(const Client& client, const Channel& channel) const;
  bool canKick(const Client& client, const Channel& channel) const;
  bool canInvite(const Client& client, const Channel& channel) const;
  bool canSetTopic(const Client& client, const Channel& channel) const;

  void initSocket(const int PORT);
  void setNonBlocking(int fd);
  void acceptClient();
  void handleClientData(Client &client);
  void removeClient(size_t index);
  void processCommand(Client &client, const std::string &command);

		void						sendError( Client& client, const std::string& code, const std::string& message );
		void						sendReply( Client& client, const std::string& message );
		const std::string&			getServerName( void ) const;
		std::vector<std::string>	splitCommand( const std::string& command );

		Channel *getChannels(const std::string &name);
		void checkEmptyChannel(const std::string &name);
        bool isValidChannelName(const std::string &name) const ;


};

#endif