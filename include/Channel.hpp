#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include "Client.hpp"
#include <map>
#include <string>
#include <vector>

class Channel {
private:
  std::string _name;
  std::string _topic;
  std::string _key;
  std::size_t _limit;

  std::map<int, Client *> _members;

  std::vector<int> _operators;
  std::vector<int> _invitedFds;
  std::vector<int> _membersBanned;

  bool _modeI;
  bool _modeT;
  bool _modeK;
  bool _modeL;

public:
  Channel();
  Channel(const std::string &name);
  ~Channel();
  Channel(const Channel &other);
  Channel &operator=(const Channel &other);

  bool isMember(int clientFd) const;
  bool isOperator(int clientFd) const;
  bool isInviteOnly() const;
  bool isTopicRestricted() const;
  bool hasKey() const;
  bool isFull() const;
  bool isInvitedFd(int clientFd) const;

  void setName(const std::string &name);
  void setTopic(const std::string &topic);
  void setKey(const std::string &key);
  void setLimit(std::size_t limit);

  std::size_t getLimit() const;
  std::size_t getMembersNumber() const;
  const std::string &getKey() const;
  const std::string &getTopic() const;
  const std::string &getName() const;
  const std::string getUserList() const;
  const std::map<int, Client *> &getMembers() const;
  const std::vector<int> &getInvitedFds() const ;

  void addMember(Client *client);
  void removeMember(int clientFd);
  void addOperator(int clientFd);
  void removeOperator(int clientFd);
  void addBanned(int clientFd);
  void removeBanned(int clientFd);

  void setMode(char mode, bool setting);
  bool getMode(char mode) const;

  void broadcast(const std::string &message, int excludeFd);

  void inviteMember(int clientFd);
  bool canSetMode(int clientFd) const;
  bool canKick(int clientFd) const;
  bool canInvite(int clientFd) const;
  bool canSetTopic(int clientFd) const;
};

#endif