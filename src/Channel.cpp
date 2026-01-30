#include "../include/Channel.hpp"

Channel::Channel(const std::string &name) {
    _name = name;
}

Channel::~Channel() {

}

Channel::Channel(const Channel &other) {
    *this = other;
}

Channel &Channel::operator=(const Channel &other) {
    if (this != &other) {

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

const std::string &Channel::getKey() const {
    return _key;
}

const std::string &Channel::getTopic() const {
    return _topic;
}

const std::string &Channel::getName() const {
    return _name;
}

bool Channel::isMember(int fd) const {
    std::map<int, Client*>::const_iterator it = _members.find(fd);

    return (it != _members.end()) ? true : false;
}

bool Channel::isOperator(int fd) const {
    for (std::vector<int>::const_iterator it = _operators.begin(); it != _operators.end(); it++) {
        if (*it == fd) {
            return true;
        }
    }
    return false;
}

bool Channel::hasKey() const {
    return _key.empty() ? false : true;
}

bool Channel::isInviteOnly() const {

}
