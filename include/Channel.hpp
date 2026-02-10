#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include "Client.hpp"
#include <cstddef>
#include <map>
#include <string>
#include <vector>

/*

403 ERR_NOSUCHCHANNEL: "<channel name> :No such channel"
405 ERR_TOOMANYCHANNELS: "<channel name> :You have joined too many channels"
471 ERR_CHANNELISFULL: "<channel> :Cannot join channel (+l)"
473 ERR_INVITEONLYCHAN: "<channel> :Cannot join channel (+i)"
474 ERR_BANNEDFROMCHAN: "<channel> :Cannot join channel (+b)"
475 ERR_BADCHANNELKEY: "<channel> :Cannot join channel (+k)"
476 ERR_BADCHANMASK: "<channel> :Bad Channel Mask"

482 ERR_CHANOPRIVSNEEDED: "<channel> :You're not channel operator"
441 ERR_USERNOTINCHANNEL: "<nick> <channel> :They aren’t on that channel"
442 ERR_NOTONCHANNEL: "<channel> :You're not on that channel"
443 ERR_USERONCHANNEL: "<user> <channel> :is already on channel"
467 ERR_KEYSET: "<channel> :Channel key already set"
472 ERR_UNKNOWNMODE: "<char> :is unknown mode char to me"

404 ERR_CANNOTSENDTOCHAN: "<channel name> :Cannot send to channel"
411 ERR_NORECIPIENT: ":No recipient given (<command>)"
412 ERR_NOTEXTTOSEND: ":No text to send"

461 ERR_NEEDMOREPARAMS: "<command> :Not enough parameters"
401 ERR_NOSUCHNICK: "<nickname> :No such nick/channel"
*/
class Channel {
private:
  std::string _name;
  std::string _topic;
  std::string _key;
  std::size_t _limit;

  std::map<int, Client *> _members;
  std::size_t _numberOfClients;

  std::vector<int> _operators;
  std::vector<int> _invitedFds;

  bool _banned;
  bool _modeI;
  bool _modeT;
  bool _modeK;
  bool _modeL;

public:
  // canonical orthodox form
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
  bool isBanned() const;

  // setters
  void setName(const std::string &name);
  void setTopic(const std::string &topic);
  void setKey(const std::string &key);
  void setLimit(std::size_t limit);

  // getters
  std::size_t getLimit() const;
  std::size_t getMembersNumber() const;
  const std::string &getKey() const;
  const std::string &getTopic() const;
  const std::string &getName() const;
  const std::string getUserList() const;
  std::map<int, Client *> getMembers() const;

  // member management
  void addMember(Client *client, std::string &givenKey);
  void addMember(Client *client);
  void removeMember(int clientFd);
  void addOperator(int clientFd);
  void removeOperator(int clientFd);

  // mode management
  void setMode(char mode, bool setting);
  bool getMode(char mode) const;

  // communication
  void broadcast(const std::string &message, int excludeFd); // enviar mensagem para todos no canal

  bool canJoin(int clientFd, const std::string &giveKey, std::string &errorOut) const;
  bool canChangeTopic(int clientFd) const;
  void inviteMember(int clientFd);
};

#endif