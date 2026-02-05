#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include "Client.hpp"
#include <string>
#include <vector>
#include <map>

class Channel {
    private:
        std::string _name;
        std::string _topic;
        std::string _key;
        std::size_t _limit;

        std::map<int, Client*> _members;
        std::vector<int> _operators;
        std::vector<int> _invitedFds;

        bool _modeI;
        bool _modeT;
        bool _modeK;
        bool _modeL;

    public:
        //canonical orthodox form
        Channel();
        Channel(const std::string &name);
        ~Channel();
        Channel(const Channel &other);
        Channel &operator=(const Channel &other);

        bool isMember(int clientFd) const ;
        bool isOperator(int clientFd) const ;
        bool isInviteOnly() const ;
        bool isTopicRestricted() const ;
        bool hasKey() const ;
        bool isFull() const ;

        //setters
        void setName(const std::string &name);
        void setTopic(const std::string &topic);
        void setKey(const std::string &key);
        void setLimit(std::size_t limit);

        //getters
        std::size_t getLimit() const ;
        const std::string &getKey() const ;
        const std::string &getTopic() const ;
        const std::string &getName() const ;

        //member management
        void addMember(Client *client, std::string &givenKey);
        void removeMember(int clientFd);
        void addOperator(int clientFd);
        void removeOperator(int clientFd);

        // mode management
        void setMode(char mode, bool setting);
        bool getMode(char mode) const ;

        //communication
        void broadcast(const std::string &message, int excludeFd);// enviar mensagem para todos no canal

        bool canJoin(int clientFd, const std::string &giveKey, std::string &errorOut) const ;
        bool canChangeTopic(int clientFd) const ;
        void inviteMember(int clientFd);
};

#endif