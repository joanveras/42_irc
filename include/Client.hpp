#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <iostream>

class Client {

public:
  Client();
  explicit Client(const int FD);
  ~Client();
  Client operator=(Client const &other);

  void setPassword(bool state);
  void setNickname(const std::string &nickname);
  void setUsername(const std::string &username);
  void setRealname(const std::string &realname);

  bool isAuthenticated() const;
  bool hasPassword() const;
  bool hasNick() const;
  bool hasUser() const;
  int getFd() const;
  const std::string &getBuffer() const;
  const std::string &getNickname() const;
  const std::string &getUsername() const;
  const std::string &getRealname() const;

  bool hasCompleteMessage() const;
  void clearBuffer();
  void checkAuthentication();
  void appendToBuffer(const std::string &data);
  std::string extractCommand();

private:
  bool _is_authenticated;
  bool _has_password;
  bool _has_nick;
  bool _has_user;
  int _fd;
  std::string _buffer;
  std::string _nickname;
  std::string _username;
  std::string _realname;
};

#endif