#ifndef CLIENT_HPP
# define CLIENT_HPP

# include <iostream>

class	Client {

	public:
							Client( void );
							Client( const int fd );
							~Client( void );

		Client				operator=( Client const& other );

		void				setPassword( bool state );
		void				setNickname( const std::string& nickname );
		void				setUsername( const std::string& username );
		void				setRealname( const std::string& realname );
		
		bool				isAuthenticated( void ) const;
		bool				hasPassword( void ) const;
		bool				hasNick() const;
		bool				hasUser() const;
		int					getFd( void ) const;
		const std::string&	getBuffer( void ) const;
		const std::string&	getNickname( void ) const;
		const std::string&	getUsername( void ) const;
		const std::string&	getRealname( void ) const;

		bool				hasCompleteMessage( void ) const;
		void				clearBuffer( void );
		void				checkAuthentication( void );
		void				appendToBuffer( const std::string& data );
		std::string			extractCommand( void );


	private:
		bool		_is_authenticated;
		bool		_has_password;
		bool		_has_nick;
		bool		_has_user;
		int			_fd;
		std::string	_buffer;
		std::string	_nickname;
		std::string	_username;
		std::string	_realname;

};

#endif