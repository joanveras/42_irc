#include "../include/IRCMessage.hpp"

IRCMessage::IRCMessage() : _valid(false), _hasTrailing(false) {
}

IRCMessage::IRCMessage(const std::string &raw) : _valid(false), _hasTrailing(false) {
  parse(raw);
}

bool IRCMessage::isValid() const {
    return _valid;
}

IRCMessage::IRCMessage(const IRCMessage &other)
    : _valid(other._valid), _hasTrailing(other._hasTrailing), _prefix(other._prefix), _command(other._command), _trailing(other._trailing),
      _params(other._params) {
}

IRCMessage::~IRCMessage() {
}

IRCMessage &IRCMessage::operator=(const IRCMessage &other) {
  if (this != &other) {
    _valid = other._valid;
    _hasTrailing = other._hasTrailing;
    _prefix = other._prefix;
    _command = other._command;
    _trailing = other._trailing;
    _params = other._params;
  }
  return *this;
}

bool IRCMessage::parse(const std::string &raw) {
  _prefix.clear();
  _command.clear();
  _params.clear();
  _trailing.clear();
  _valid = false;
  _hasTrailing = false;

  if (raw.empty() || raw.length() > IRC_MAX_MESSAGE_LENGTH)
    return false;

  // RFC: NULL bytes are not allowed TOPIC: 2.3.1 Message format
  if (raw.find('\0') != std::string::npos)
    return false;

  std::string line = raw;
  if (!line.empty() && line[line.size() - IRC_PARAM_OFFSET] == '\r')
    line.erase(line.size() - IRC_PARAM_OFFSET);

  size_t pos = 0;
  while (pos < line.length() && line[pos] == ' ')
    pos++;
  if (pos >= line.length())
    return false;

  // ==== PREFIX ====
  if (line[pos] == ':') {
    size_t end = line.find(' ', pos);
    if (end == std::string::npos)
      return false;

    _prefix = line.substr(pos + IRC_PARAM_OFFSET, end - pos - IRC_PARAM_OFFSET);
    if (_prefix.empty())
      return false;
    pos = end + IRC_PARAM_OFFSET;
    while (pos < line.length() && line[pos] == ' ')
      pos++;
  }

  // ==== COMMAND ====
  size_t cmdEnd = line.find(' ', pos);
  if (cmdEnd == std::string::npos) {
    _command = line.substr(pos);
  } else {
    _command = line.substr(pos, cmdEnd - pos);
  }
  if (_command.empty())
    return false;
  for (size_t i = 0; i < _command.length(); i++) {
    if (!std::isalpha(_command[i]) && !std::isdigit(_command[i]))
      return false;
  }
  if (cmdEnd == std::string::npos) {
    _valid = true;
    return true;
  }
  pos = cmdEnd + IRC_PARAM_OFFSET;
  while (pos < line.length() && line[pos] == ' ')
    pos++;
  // ==== PARAMS ====
  while (pos < line.length()) {
    if (line[pos] == ':') {
      _hasTrailing = true;
      _trailing = line.substr(pos + IRC_PARAM_OFFSET);
      break;
    }

    size_t end = line.find(' ', pos);
    if (end == std::string::npos) {
      _params.push_back(line.substr(pos));
      break;
    }

    _params.push_back(line.substr(pos, end - pos));
    pos = end + IRC_PARAM_OFFSET;
    while (pos < line.length() && line[pos] == ' ')
      pos++;
  }
  // ========================
  // RFC 1459 reference:
  // the command, and the command parameters (of which there may be up to 15).
  // ========================
  std::size_t totalParams = _params.size();
  if (!_trailing.empty()) {
    totalParams++;
  }
  if (totalParams > 15) {
    return false;
  }
  _valid = true;
  return true;
}

const std::vector<std::string> &IRCMessage::getParams() const {
  return _params;
}

const std::string &IRCMessage::getPrefix() const {
  return _prefix;
}

bool IRCMessage::hasTrailing() const {
  return _hasTrailing;
}

const std::string &IRCMessage::getCommand() const {
  return _command;
}

const std::string &IRCMessage::getTrailing() const {
  return _trailing;
}

std::size_t IRCMessage::getParamCount() const {
  return _params.size();
}

std::string IRCMessage::getSourceNick() const {
  if (_prefix.empty())
    return "";

  size_t end = _prefix.find('!');
  if (end == std::string::npos)
    return _prefix;

  return _prefix.substr(0, end);
}

std::string IRCMessage::formatReply(const std::string &prefix, const std::string &code, const std::string &target,
                                    const std::string &message) {
  return ":" + prefix + " " + code + " " + target + " " + message + "\r\n";
}
