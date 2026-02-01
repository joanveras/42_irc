#include "../include/Channel.hpp"
#include <algorithm>

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

bool Channel::isMember(int clientFd) const {
    std::map<int, Client*>::const_iterator it = _members.find(clientFd);

    return it != _members.end() ? true : false;
}

bool Channel::isOperator(int clientFd) const {
    std::vector<int>::const_iterator it = std::find(_operators.begin(), _operators.end(), clientFd);
    
    return it != _operators.end() ? true : false;
}

bool Channel::hasKey() const {
    return _key.empty() ? false : true;
}

bool Channel::isInviteOnly() const {
}

bool Channel::isValidName(const std::string &name) const {
    if (name.length() > 200) {
        return false;
    }
    if (name[0] != '&' || name[0] != '#') {
        return false;
    }
    for (std::string::const_iterator it = name.begin(); it != name.end(); it++) {
        if (*it == ' ' || *it == ',' || *it == '\a') {
            return false;
        }
    }
    return true;
}

void Channel::addMember(Client *client) {
    if (_size < _limit) {
        _members.insert(std::pair<int, Client*>(client->getFd(),client));
        _size++;
    }
    else {
        //jogar uma exceção aqui (com uma mensagem de que a capacidade total ja foi atingida)
    }
}

void Channel::removeMember(int clientFd) {
    std::map<int, Client*>::iterator it = _members.find(clientFd);
    if (it == _members.end()) {
        //jogar uma exceção aqui (não encontrou o membro no canal)
    }
    else {
        _members.erase(it);
        _size--;
    }
}

void Channel::addOperator(int clientFd) {
    if (_size < _limit) {
        _operators.push_back(clientFd);
        _size++;
    }
    else {
        //jogar uma exceção aqui (capacidade maxima ja atingida)
    }
}

void Channel::removeOperator(int clientFd) {
    std::vector<int>::iterator it = std::find(_operators.begin(), _operators.end(), clientFd);
    if (it == _operators.end()) {
        //jogar uma exceção não encontrou o operador a ser removido
    }
    else {
        _operators.erase(it);
        _size--;
    }
}
