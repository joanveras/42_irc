#ifndef IRCMESSAGE_HPP
#define IRCMESSAGE_HPP

#include <cstring>
#include <iostream>
#include <vector>
#include <string>

const std::size_t IRC_MAX_MESSAGE_LENGTH = 512;
const int IRC_PARAM_OFFSET = 1;
const int IRC_WELCOME_COUNT = 5;

class IRCMessage {

public:
  IRCMessage();
  explicit IRCMessage(const std::string &raw);
  IRCMessage(const IRCMessage &other);
  ~IRCMessage();

  IRCMessage &operator=(const IRCMessage &other);

  bool parse(const std::string &raw);
  bool isValid() const;

  const std::string &getPrefix() const;
  const std::string &getCommand() const;
  const std::vector<std::string> &getParams() const;
  const std::string &getTrailing() const;
  std::size_t getParamCount() const;

  bool hasTrailing() const;
  std::string getSourceNick() const;

  static std::string formatReply(const std::string &prefix, const std::string &code, const std::string &target,
                                 const std::string &message);

private:
  bool _valid;
  std::string _prefix;
  std::string _command;
  std::string _trailing;
  std::vector<std::string> _params;
};

#endif
