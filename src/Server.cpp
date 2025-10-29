#include "../include/Server.hpp"

Server::Server( void ) {
}

Server::Server( const int port, const std::string password ) :
	_port( port ), _password( password ), _server_name( "irc.server" ) {
}

Server::~Server( void ) {
}

void	Server::run( void ) {
	initSocket( _port );

	struct pollfd	server_poll_fd;
	server_poll_fd.fd = _server_socket;
	server_poll_fd.events = POLLIN;
	server_poll_fd.revents = 0;

	_poll_fds.push_back( server_poll_fd );

	std::cout << "Server running on port " << _port << std::endl;
	std::cout << "Waiting for connections..." << std::endl;

	while ( true ) {
		int	poll_fd = poll( _poll_fds.data(), _poll_fds.size(), -1 );
		if ( poll_fd == -1 ) {
			std::cerr << "Poll error: " << strerror( errno ) << std::endl;
			continue;
		}

		if ( _poll_fds[0].revents & POLLIN )
			acceptClient();

		for ( size_t i = 1; i < _poll_fds.size(); ++i ) {
			if ( _poll_fds[i].revents & POLLIN )
				handleClientData( _clients[i - 1] );
		}
	}
}



void	Server::initSocket( const int port ) {
	// OS Action: Creates an endpoint for communication
	// AF_INET: Requests IPv4 protocol family
	// SOCK_STREAM: Requests reliable, connection-oriented TCP semantics
	const int	sock_fd = socket( AF_INET, SOCK_STREAM, 0 );
	if ( sock_fd == -1 )
		throw std::runtime_error("Unable to initiate socket: Server::initSocket()");

	// OS Action: Modifies socket behavior in the kernel's socket structures
	// SO_REUSEADDR: Allows binding to a port that was recently used (avoids "Address already in use" errors)
	// Why needed: Without this, the OS would enforce a TIME_WAIT period before the port can be reused
	int	optval = 1;
	if ( setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1 ) {
		close( sock_fd );
		throw std::runtime_error("Unable to set SO_REUSEADDR: Server::initSocket()");
	}

	// OS Interpretation: Defines how the socket will be addressed
	// INADDR_ANY: Binds to all available network interfaces (0.0.0.0)
	// htons(): Converts port number from host byte order to network byte order (big-endian)
	struct sockaddr_in	server_addr;
	std::memset( &server_addr, 0, sizeof(server_addr) );
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons( port );
	server_addr.sin_addr.s_addr = INADDR_ANY;

	// OS Action: Associates the socket with the specified IP address and port
	// Kernel Work: Creates an entry in the routing table and marks the port as in use
	// Privilege Check: On Unix systems, ports < 1024 require root privileges
	struct sockaddr*	addr = reinterpret_cast<struct sockaddr*>( &server_addr );
	if ( bind(sock_fd, addr, sizeof(server_addr)) == -1 ) {
		close( sock_fd );
		throw std::runtime_error("Unable to bind Socket: Server::initSocket()");
	}

	// OS Action: Transitions socket from "closed" to "listening" state
	// SOMAXCONN: Maximum backlog queue size for pending connections (system-defined, typically 128+)
	// Kernel Creates:
		// Accept queue for established connections
		// SYN queue for half-open connections during TCP handshake
	if ( listen(sock_fd, SOMAXCONN) == -1 ) {
		close( sock_fd );
		throw std::runtime_error("Unable to listen Socket: Server::initSocket()");
	}

	setNonBlocking( sock_fd );

	_server_socket = sock_fd;
}

void	Server::setNonBlocking( const int fd ) {
	// OS Action: Modifies file descriptor flags via fcntl(fd, F_SETFL, O_NONBLOCK)
	// Effect: accept(), read(), write() operations return immediately rather than blocking
	// Use Case: Essential for event-driven servers using select(), poll(), or epoll
	if ( fcntl(fd, F_SETFL, O_NONBLOCK) == -1 )
		throw std::runtime_error("Unable to set non-blocking mode: Server::setNonBlocking()");
}

void	Server::acceptClient( void ) {
	struct sockaddr_in	client_addr;
	memset( &client_addr, 0, sizeof (client_addr) );


	socklen_t	client_len = sizeof( client_addr );
	struct sockaddr*	addr = reinterpret_cast<struct sockaddr*>( &client_addr );
	// accept() is a blocking call by default - it will pause the program until a client connects.
	// But the poll() in Server::run() handles it
	const int	client_socket = accept( _server_socket, addr, &client_len );
	if ( client_socket == -1 ) {
		close( client_socket );
		std::cerr << "accept() failed after poll: " << strerror( errno ) << std::endl;
	}

	setNonBlocking( client_socket );

	_clients.push_back( Client(client_socket) );

	struct pollfd	client_poll_fd;
	client_poll_fd.fd = client_socket;
	client_poll_fd.events = POLLIN;
	client_poll_fd.revents = 0;

	_poll_fds.push_back( client_poll_fd );

	std::cout << "Client connected! Socket: " << client_socket << std::endl;
}

void Server::handleClientData( Client& client ) {
	char	buffer[512];
	ssize_t	bytes_read;

	bytes_read = recv(client.getFd(), buffer, sizeof(buffer) - 1, 0);
	if ( bytes_read > 0) {
		buffer[bytes_read] = '\0';
		std::string	data( buffer, bytes_read );

		std::cout << "Received from client " << client.getFd() << ": [" << data.substr( 0, data.find('\n') ) << "]" << std::endl;

		client.appendToBuffer( data );

		while ( client.hasCompleteMessage() ) {
			std::string	command = client.extractCommand();
			std::cout << "Complete command: [" << command << "]" << std::endl;
			
			processCommand( client, command );
		}

		// bytes_read = recv( client.getFd(), buffer, sizeof(buffer) - 1, 0 );
	} else if ( bytes_read == 0 ) {
		std::cout << "Client disconnected: " << client.getFd() << std::endl;
		for ( size_t i = 1; i < _poll_fds.size(); ++i ) {
			if ( _poll_fds[i].fd == client.getFd() ) {
				removeClient( i );
				break;
			}
		}
		return;
	} else if ( bytes_read == -1 && errno != EAGAIN )
		std::cerr << "Read error for client " << client.getFd() << ": " << strerror( errno ) << std::endl;
}

void	Server::removeClient( size_t index ) {
	int	client_fd = _poll_fds[index].fd;

	if ( index < 1 || index >= _poll_fds.size() )
		return;
	
	close( _poll_fds[index].fd );

	_poll_fds.erase( _poll_fds.begin() + index );
	_clients.erase( _clients.begin() + (index - 1) );

	std::cout << "Client " << client_fd << " removed from poll set" << std::endl;
}

void	Server::processCommand( Client& client, const std::string& command ) {
	std::vector<std::string>	args = splitCommand( command );
	if ( args.empty()) return;

	std::string cmd = args[0];
	std::transform( cmd.begin(), cmd.end(), cmd.begin(), ::toupper );

	if ( !client.isAuthenticated() && cmd != "PASS" && cmd != "NICK" && cmd != "USER" && cmd != "QUIT" ) {
		sendError(client, "464", "Password incorrect");
		return;
	}

	enum	CommandCode { CMD_PASS, CMD_NICK, CMD_USER, CMD_QUIT, CMD_UNKNOWN };

	CommandCode	getCmdCode = CMD_UNKNOWN;
	if ( cmd == "PASS" ) getCmdCode = CMD_PASS;
	else if ( cmd == "NICK" ) getCmdCode = CMD_NICK;
	else if ( cmd == "USER" ) getCmdCode = CMD_USER;
	else if ( cmd == "QUIT" ) getCmdCode = CMD_QUIT;

	switch ( getCmdCode ) {
		case CMD_PASS:
			handlePASS( client, args );
			break;
		case CMD_NICK:
			handleNICK( client, args );
			break;
		case CMD_USER:
			handleUSER( client, args );
			break;
		case CMD_QUIT:
			handleQUIT( client, args );
			break;
		default:
			sendReply( client, "ECHO: " + command );
			break;
	}
}

void	Server::handlePASS( Client& client, const std::vector<std::string>& args ) {
	if ( args.size() < 2 ) {
		sendError( client, "461", "PASS :Not enough parameters" );
		return;
	}

	if ( client.hasPassword() ) {
		sendError( client, "462", "You may not reregister" );
		return;
	}

	if ( args[1] == _password ) {
		client.setPassword( true );
		sendReply( client, "Password accepted" );
		return;
	}

	sendError( client, "464", "Password incorrect" );
}

void	Server::handleNICK( Client& client, const std::vector<std::string>& args ) {
	if ( args.size() < 2 ) {
		sendError( client, "431", "No nickname given" );
		return;
	}

	std::string	nickname = args[1];

	if ( nickname.empty() || nickname.find(' ') != std::string::npos ) {
		sendError(client, "432", nickname + " :Erroneous nickname");
		return;
	}

	for ( size_t i = 0; i < _clients.size(); ++i ) {
		if ( _clients[i].getFd() != client.getFd() && _clients[i].getNickname() == nickname ) {
			sendError( client, "433", nickname + " :Nickname is already in use" );
			return;
		}
	}

	client.setNickname( nickname );
	sendReply( client, "NICK set to: " + nickname );
}

// Add more validations | args validations
void	Server::handleUSER( Client& client, const std::vector<std::string>& args ) {
	// If size is greater than 5?
	// I think, is going to pass, but, the rest of the args will not be used...
	if ( args.size() < 5 ) {
		sendError( client, "461", "USER :Not enough parameters" );
		return;
	}

	if ( client.hasUser() ) {
		sendError( client, "462", "You may not reregister" );
		return;
	}

	// here...
	std::string	username = args[1];
	std::string	realname = args[4];

	client.setUsername( username );
	client.setRealname( realname );
	sendReply( client, "USER registered" );
}

void Server::handleQUIT(Client& client, const std::vector<std::string>& args) {
	std::string	message = "Client quit";

	if ( args.size() > 1 ) message = args[1];

	std::cout << "Client " << client.getFd() << " quit: " << message << std::endl;

	for ( size_t i = 1; i < _poll_fds.size(); ++i ) {
		if ( _poll_fds[i].fd == client.getFd() ) {
			removeClient( i );
			break;
		}
	}
}

void	Server::sendError( Client& client, const std::string& code, const std::string& message ) {
	std::string error = ":" + getServerName() + " " + code + " " +
		(client.getNickname().empty() ? "*" : client.getNickname()) + " " + message + "\r\n";

	send( client.getFd(), error.c_str(), error.length(), 0 );
}

void	Server::sendReply( Client& client, const std::string& message ) {
	std::string reply = ":" + getServerName() + " " + message + "\r\n";

	send( client.getFd(), reply.c_str(), reply.length(), 0 );
}

const std::string&	Server::getServerName( void ) const {
	return _server_name;
}

std::vector<std::string>	Server::splitCommand( const std::string& command ) {
	std::vector<std::string>	args;
	std::istringstream			iss(command);

	std::string	arg;
	while ( iss >> arg ) {
		args.push_back( arg );
	}
	return args;
}
