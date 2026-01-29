#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include "../include/Client.hpp"
#include <map>
#include <string>
#include <vector>

class Channel {

public:
  static const int EXCLUDE_NONE = -1;

  Channel();
  explicit Channel(const std::string &name);
  ~Channel();

  const std::string &getName() const;
  const std::string &getTopic() const;
  const std::string &getKey() const;
  std::string getUserList() const;
  std::vector<int> getClientFds() const;
  size_t getClientCount() const;

  void setTopic(const std::string &topic);
  void setKey(const std::string &key);
  void setUserLimit(int limit);

  bool isInviteOnly() const;
  bool isTopicRestricted() const;
  bool hasKey() const;
  void setInviteOnly(bool state);
  void setTopicRestricted(bool state);

  void addClient(Client &client);
  void removeClient(int clientFd);
  bool hasClient(int clientFd) const;
  bool isEmpty() const;

  void addOperator(int clientFd);
  void removeOperator(int clientFd);
  bool isOperator(int clientFd) const;

  void broadcast(const std::string &message, int excludeFd = EXCLUDE_NONE);

  void addInvite(const std::string &nickname);
  bool isInvited(const std::string &nickname) const;

private:
  bool _hasKey;
  bool _inviteOnly;
  bool _topicRestricted;
  int _userLimit;
  std::map<int, std::string> _clients;
  std::vector<std::string> _invited;
  std::string _topic;
  std::string _name;
  std::string _key;
};

#endif