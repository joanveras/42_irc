#include "../include/Client.hpp"

namespace {
// Client buffer processing constants
const size_t CARRIAGE_RETURN_OFFSET = 1;
} // namespace ClientInternal

Client::Client() {
}

Client::Client(const int FD)
    : _is_authenticated(false), _has_password(false), _has_nick(false), _has_user(false), _fd(FD) {
}

Client::~Client() {
}

Client Client::operator=(Client const &other) {
  if (this != &other) {
    _fd = other._fd;
    _buffer = other._buffer;
  }
  return *this;
}

bool Client::isAuthenticated() const {
  return _is_authenticated;
}

bool Client::hasPassword() const {
  return _has_password;
}

bool Client::hasNick() const {
  return _has_nick;
}

bool Client::hasUser() const {
  return _has_user;
}

bool Client::hasCompleteMessage() const {
  return _buffer.find('\n') != std::string::npos;
}

int Client::getFd() const {
  return _fd;
}

void Client::appendToBuffer(const std::string &data) {
  _buffer.append(data);
}

void Client::clearBuffer() {
  _buffer.clear();
}

void Client::setNickname(const std::string &nickname) {
  _nickname = nickname;
  _has_nick = true;
  checkAuthentication();
}

void Client::setUsername(const std::string &username) {
  _username = username;
  _has_user = true;
  checkAuthentication();
}

void Client::setRealname(const std::string &realname) {
  _realname = realname;
}

void Client::setPassword(bool state) {
  _has_password = state;
  checkAuthentication();
}

void Client::checkAuthentication() {
  _is_authenticated = (_has_password && _has_nick && _has_user);
}

const std::string &Client::getBuffer() const {
  return _buffer;
}

void Client::queueOutput(const std::string &data) {
  _out_buffer.append(data);
}

bool Client::hasPendingOutput() const {
  return !_out_buffer.empty();
}

std::string &Client::getOutputBuffer() {
  return _out_buffer;
}

void Client::consumeOutput(std::size_t count) {
  _out_buffer.erase(0, count);
}

const std::string &Client::getNickname() const {
  return _nickname;
}

const std::string &Client::getUsername() const {
  return _username;
}

const std::string &Client::getRealname() const {
  return _realname;
}

std::string Client::extractCommand() {
  size_t pos = _buffer.find('\n');
  if (pos == std::string::npos)
    return "";

  std::string command = _buffer.substr(0, pos);
  _buffer = _buffer.substr(pos + CARRIAGE_RETURN_OFFSET);

  if (!command.empty() && command[command.size() - CARRIAGE_RETURN_OFFSET] == '\r')
    command.resize(command.size() - CARRIAGE_RETURN_OFFSET);

  return command;
}
