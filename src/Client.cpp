#include "../include/Client.hpp"

Client::Client( void ) {
}

Client::Client( int fd ) : _is_authenticated( false ), _has_password( false ), _has_nick( false ), _has_user( false ), _fd( fd ) {
}

Client::~Client( void ) {
}

Client	Client::operator=( Client const& other ) {
	if ( this != &other ) {
		_fd = other._fd;
		_buffer = other._buffer;
	}
	return *this;
}



bool	Client::isAuthenticated( void ) const {
	return _is_authenticated;
}

bool	Client::hasPassword( void ) const {
	return _has_password;
}

bool	Client::hasNick( void ) const {
	return _has_nick;
}

bool	Client::hasUser( void ) const {
	return _has_user;
}

bool	Client::hasCompleteMessage( void ) const {
	return _buffer.find('\n') != std::string::npos;
}

int	Client::getFd( void ) const {
	return _fd;
}

void	Client::appendToBuffer( const std::string& data ) {
	_buffer.append( data );
}

void	Client::clearBuffer( void ) {
	_buffer.clear();
}

void Client::setNickname( const std::string& nickname ) {
	_nickname = nickname;
	_has_nick = true;
	checkAuthentication();
}

void Client::setUsername( const std::string& username ) {
	_username = username;
	_has_user = true;
	checkAuthentication();
}

void Client::setRealname( const std::string& realname ) {
	_realname = realname;
}

void Client::setPassword( bool state ) {
	_has_password = state;
	checkAuthentication();
}

void Client::checkAuthentication( void ) {
	_is_authenticated = ( _has_password && _has_nick && _has_user );
}

const std::string&	Client::getBuffer( void ) const {
	 return _buffer;
}

const std::string&	Client::getNickname( void ) const {
	return _nickname;
}

const std::string&	Client::getUsername( void ) const {
	return _username;
}

const std::string&	Client::getRealname( void ) const {
	return _realname;
}

std::string Client::extractCommand( void ) {
	size_t	pos = _buffer.find( '\n' );
	if ( pos == std::string::npos )
		return "";
		
	std::string command = _buffer.substr( 0, pos );
	_buffer = _buffer.substr( pos + 1 );

	if ( !command.empty() && command[command.size() - 1] == '\r' )
		command.resize( command.size() - 1 );

	return command;
}
