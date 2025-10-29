#ifndef SERVER_HPP
# define SERVER_HPP

# include <map>
# include <poll.h>
# include <string>
# include <vector>
# include <cerrno>
# include <fcntl.h>
# include <cstring>
# include <cstdlib>
# include <netdb.h>
# include <sstream>
# include <iostream>
# include <unistd.h>
# include <algorithm>
# include <stdexcept>
# include <sys/types.h>
# include <arpa/inet.h>
# include <sys/socket.h>
# include <netinet/in.h>

# include "./Client.hpp"

class	Server {

	public:
				Server( void );
				Server( const int port, const std::string password );
				~Server( void );
				Server( Server const& other );

		Server	operator=( Server const& other );

		void	run( void );


	private:
		int					_port;
		int					_server_socket;
		std::string			_password;
		std::string			_server_name;
		std::vector<Client>	_clients;
		std::vector<pollfd>	_poll_fds;
		
		void						initSocket( const int port );
		void						setNonBlocking( int fd );
		void						acceptClient( void );
		void						handleClientData( Client& client );
		void						removeClient( size_t index );
		void						processCommand( Client& client, const std::string& command );

		void						handlePASS( Client& client, const std::vector<std::string>& args );
		void						handleNICK( Client& client, const std::vector<std::string>& args );
		void						handleUSER( Client& client, const std::vector<std::string>& args );
		void						handleQUIT( Client& client, const std::vector<std::string>& args );

		void						sendError( Client& client, const std::string& code, const std::string& message );
		void						sendReply( Client& client, const std::string& message );
		const std::string&			getServerName( void ) const;
		std::vector<std::string>	splitCommand( const std::string& command );

};

#endif