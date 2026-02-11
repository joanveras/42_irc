#include "../include/Channel.hpp"
#include <algorithm>
#include <cstddef>
#include <map>
#include <sys/socket.h>
#include <sys/types.h>
#include <utility>

Channel::Channel() {
  _name = "";
  _topic = "";
  _key = "";
  _limit = 0;
  _modeI = false;
  _modeT = false;
  _modeK = false;
  _modeL = false;
  _banned = false;
}

Channel::Channel(const std::string &name) {
  _name = name;
  _topic = "";
  _key = "";
  _limit = 0;
  _modeI = false;
  _modeT = false;
  _modeK = false;
  _modeL = false;
}

Channel::~Channel() {
  _members.clear();
  _operators.clear();
  _invitedFds.clear();
}

Channel::Channel(const Channel &other) {
  *this = other;
}

Channel &Channel::operator=(const Channel &other) {
  if (this != &other) {
    this->_name = other._name;
    this->_topic = other._topic;
    this->_key = other._key;
    this->_limit = other._limit;
    this->_modeI = other._modeI;
    this->_modeT = other._modeT;
    this->_modeK = other._modeK;
    this->_modeL = other._modeL;
    this->_banned = other._banned;
    this->_members = other._members;
    this->_operators = other._operators;
    this->_invitedFds = other._invitedFds;
  }
  return *this;
}

void Channel::setName(const std::string &name) {
  _name = name;
}

void Channel::setTopic(const std::string &topic) {
  _topic = topic;
}

void Channel::setKey(const std::string &key) {
  _key = key;
}

void Channel::setLimit(std::size_t limit) {
  _limit = limit;
}

std::size_t Channel::getLimit() const {
  return _limit;
}

std::size_t Channel::getMembersNumber() const {
  return _members.size();
}

const std::string &Channel::getKey() const {
  return _key;
}

const std::string &Channel::getTopic() const {
  return _topic;
}

const std::string &Channel::getName() const {
  return _name;
}

const std::string Channel::getUserList() const {
  std::string userList;

  for (std::map<int, Client *>::const_iterator it = _members.begin(); it != _members.end(); ++it) {
    if (it == _members.begin()) {
      userList += it->second->getUsername();
      continue;
    }

    if (it->second != NULL) {
      std::string prefix;
      if (isOperator(it->second->getFd()))
        prefix = "@";
      userList += " " + prefix + it->second->getUsername();
    }
  }

  return userList;
}

const std::map<int, Client *> &Channel::getMembers() const {
  return _members;
}

const std::vector<int> &Channel::getInvitedFds() const {
  return _invitedFds;
}


bool Channel::isMember(int clientFd) const {
  std::map<int, Client *>::const_iterator it = _members.find(clientFd);

  return it != _members.end() ? true : false;
}

bool Channel::isOperator(int clientFd) const {
  std::vector<int>::const_iterator it = std::find(_operators.begin(), _operators.end(), clientFd);

  return it != _operators.end() ? true : false;
}

bool Channel::hasKey() const {
  return _modeK;
}

bool Channel::isFull() const {
  return _members.size() == _limit ? true : false;
}

bool Channel::isInviteOnly() const {
  return _modeI;
}

bool Channel::isBanned() const {
  return _banned;
}

void Channel::addMember(Client *client) {
  // if (_modeL && _members.size() >= _limit) {
  //   // enviar mensagem em conformidade com RFC 1459
  //   return;
  // }
  _members.insert(std::pair<int, Client *>(client->getFd(), client));
}

void Channel::removeMember(int clientFd) {
  std::map<int, Client *>::iterator it = _members.find(clientFd);
  if (it != _members.end()) {
    _members.erase(it);
    this->removeOperator(clientFd);
  } else {
    // enviar mensagem em conformidade com RFC 1459
  }
}

void Channel::addOperator(int clientFd) {
  _operators.push_back(clientFd);
}

void Channel::removeOperator(int clientFd) {
  std::vector<int>::iterator it = std::find(_operators.begin(), _operators.end(), clientFd);
  if (it != _operators.end()) {
    _operators.erase(it);
  } else {
    // enviar mensagem em conformidade com RFC 1459
  }
}

void Channel::addBanned(int clientFd) {
  _membersBanned.push_back(clientFd);
}

void Channel::removeBanned(int clientFd) {
  std::vector<int>::iterator it = std::find(_membersBanned.begin(), _membersBanned.end(), clientFd);
  if (it != _membersBanned.end()) {
    _membersBanned.erase(it);
  } else {
    // enviar mensagem em conformidade com RFC 1459
  }
}

void Channel::setMode(char mode, bool setting) {
  switch (mode) {
  case 'i':
    _modeI = setting;
    break;
  case 't':
    _modeT = setting;
    break;
  case 'k':
    _modeK = setting;
    break;
  case 'l':
    _modeL = setting;
    break;
  default:
    break;
  }
}

bool Channel::getMode(char mode) const {
  if (mode == 'i') {
    return _modeI;
  } else if (mode == 't') {
    return _modeT;
  } else if (mode == 'k') {
    return _modeK;
  } else if (mode == 'l') {
    return _modeL;
  }
  return false;
}

void Channel::broadcast(const std::string &message, int excludeFd) {
  for (std::map<int, Client *>::iterator it = _members.begin(); it != _members.end(); it++) {
    if (it->first != excludeFd) {
      it->second->queueOutput(message);
    }
  }
}

bool Channel::canInvite(int clientFd) const {
  return isOperator(clientFd);
}

bool Channel::canKick(int clientFd) const {
  return isOperator(clientFd);
}

bool Channel::canSetMode(int clientFd) const {
  return isOperator(clientFd);
}

bool Channel::canSetTopic(int clientFd) const {
  return isOperator(clientFd);
}

void Channel::inviteMember(int clientFd) {
  if (std::find(_invitedFds.begin(), _invitedFds.end(), clientFd) == _invitedFds.end()) {
    _invitedFds.push_back(clientFd);
  }
}
