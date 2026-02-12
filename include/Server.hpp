#ifndef SERVER_HPP
#define SERVER_HPP

#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <set>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "./Channel.hpp"
#include "./Client.hpp"
#include "./IRCMessage.hpp"

/*
Cada resposta numérica deve seguir o formato: :servidor <número> <alvo> <parâmetros_específicos> :<mensagem>.

1. Erros de Acesso e Entrada (Comando JOIN)
Estes erros ocorrem quando um usuário tenta entrar em um canal, mas não cumpre os requisitos de modo ou estado.
403 ERR_NOSUCHCHANNEL: "<channel name> :No such channel"
  ◦ O nome do canal fornecido é inválido ou não existe.
405 ERR_TOOMANYCHANNELS: "<channel name> :You have joined too many channels"
  ◦ O usuário atingiu o limite máximo de canais (a RFC recomenda 10).
471 ERR_CHANNELISFULL: "<channel> :Cannot join channel (+l)"
  ◦ O canal atingiu o limite de usuários definido pelo modo +l.
473 ERR_INVITEONLYCHAN: "<channel> :Cannot join channel (+i)"
  ◦ O canal é apenas para convidados e o usuário não possui um convite ativo.
474 ERR_BANNEDFROMCHAN: "<channel> :Cannot join channel (+b)"
  ◦ O nickname/host do usuário coincide com uma máscara de banimento ativa no canal.
475 ERR_BADCHANNELKEY: "<channel> :Cannot join channel (+k)"
  ◦ A senha (key) fornecida está incorreta ou não foi enviada para um canal +k.
476 ERR_BADCHANMASK: "<channel> :Bad Channel Mask"
  ◦ Embora listado como reservado em algumas partes, é referenciado para máscaras de canal inválidas.


  2. Erros de Permissão e Moderação (Comandos KICK, MODE, TOPIC, INVITE)
Estes erros são disparados quando um usuário tenta realizar uma ação administrativa sem os privilégios necessários ou em alvos inválidos.
482 ERR_CHANOPRIVSNEEDED: "<channel> :You're not channel operator"
  ◦ A operação exige privilégios de operador de canal (chanop) e o usuário é um membro comum.
441 ERR_USERNOTINCHANNEL: "<nick> <channel> :They aren’t on that channel"
  ◦ O alvo de um comando (como KICK) não está presente no canal especificado.
442 ERR_NOTONCHANNEL: "<channel> :You're not on that channel"
  ◦ O próprio usuário que enviou o comando não é membro do canal que ele tenta afetar.
443 ERR_USERONCHANNEL: "<user> <channel> :is already on channel"
  ◦ Retornado pelo comando INVITE quando o alvo já é membro do canal.
467 ERR_KEYSET: "<channel> :Channel key already set"
  ◦ Tentativa de definir uma senha em um canal que já possui uma chave ativa.
472 ERR_UNKNOWNMODE: "<char> :is unknown mode char to me"
  ◦ O caractere de modo fornecido no comando MODE não é reconhecido pelo servidor.


  3. Erros de Comunicação (Comandos PRIVMSG e NOTICE)
Estes erros impedem a entrega de mensagens baseadas nas restrições do canal.
404 ERR_CANNOTSENDTOCHAN: "<channel name> :Cannot send to channel"
  ◦ Ocorre se: (a) o canal é +n e o usuário não é membro, ou (b) o canal é moderado (+m) e o usuário não é operador nem possui voz (+v).
411 ERR_NORECIPIENT: ":No recipient given (<command>)"
  ◦ O comando de mensagem foi enviado sem especificar o canal ou usuário alvo.
412 ERR_NOTEXTTOSEND: ":No text to send"
  ◦ A mensagem enviada para o canal está vazia.


  4. Erros Genéricos de Comando
Aplicáveis a quase todos os comandos de canal durante o parsing.
461 ERR_NEEDMOREPARAMS: "<command> :Not enough parameters"
  ◦ O comando foi enviado com menos argumentos do que o mínimo exigido (ex: JOIN sem o nome do canal).
401 ERR_NOSUCHNICK: "<nickname> :No such nick/channel"
  ◦ Usado quando um nickname ou nome de canal fornecido como parâmetro não existe no banco de dados do servidor.
*/

enum errorCode {

  //Erros de Acesso e Entrada (Comando JOIN)
  ERR_NOSUCHCHANNEL = 403, //"<channel name> :No such channel"
  ERR_TOOMANYCHANNELS = 405, //"<channel name> :You have joined too many channels"
  ERR_CHANNELISFULL = 471, //"<channel> :Cannot join channel (+l)"
  ERR_INVITEONLYCHAN = 473, //"<channel> :Cannot join channel (+i)"
  ERR_BANNEDFROMCHAN = 474, //"<channel> :Cannot join channel (+b)"
  ERR_BADCHANNELKEY = 475, //"<channel> :Cannot join channel (+k)"
  ERR_BADCHANMASK = 476, //"<channel> :Bad Channel Mask"
  
  //Erros de Permissão e Moderação (Comandos KICK, MODE, TOPIC, INVITE)
  ERR_CHANOPRIVSNEEDED = 482, //"<channel> :You're not channel operator"
  ERR_USERNOTINCHANNEL = 441, //"<nick> <channel> :They aren’t on that channel"
  ERR_NOTONCHANNEL = 442, //"<channel> :You're not on that channel"
  ERR_USERONCHANNEL = 443, //"<user> <channel> :is already on channel"
  ERR_KEYSET = 467, //"<channel> :Channel key already set"
  ERR_UNKNOWNMODE = 472, //"<char> :is unknown mode char to me"

  //Erros de Comunicação (Comandos PRIVMSG e NOTICE)
  ERR_CANNOTSENDTOCHAN = 404, //"<channel name> :Cannot send to channel"
  ERR_NORECIPIENT = 411, //":No recipient given (<command>)"
  ERR_NOTEXTTOSEND = 412, //":No text to send"

  //Erros Genéricos de Comando
  ERR_NEEDMOREPARAMS = 461, //"<command> :Not enough parameters"
  ERR_NOSUCHNICK = 401, //"<nickname> :No such nick/channel"

  //Erros de Registro e Comando Genérico
  ERR_NOORIGIN = 409, //":No origin specified"
  ERR_UNKNOWNCOMMAND = 421, //"<command> :Unknown command"
  ERR_NONICKNAMEGIVEN = 431, //":No nickname given"
  ERR_ERRONEUSNICKNAME = 432, //"<nick> :Erroneous nickname"
  ERR_NICKNAMEINUSE = 433, //"<nick> :Nickname is already in use"
  ERR_NOTREGISTERED = 451, //":You have not registered"
  ERR_ALREADYREGISTRED = 462, //":You may not reregister"
  ERR_PASSWDMISMATCH = 464, //":Password incorrect"
};

class Server {

public:
  Server();
  explicit Server(const int PORT, const std::string &PASSWORD);
  ~Server();
  Server(Server const &other);

  Server operator=(Server const &other);

  void run();

private:
  // Type alias for command handler function pointers
  typedef void (Server::*MessageHandler)(Client &, const IRCMessage &);

  int _port;
  int _server_socket;
  std::string _password;
  std::string _server_name;
  std::vector<Client*> _clients;
  std::map<std::string, Channel *> _channels;
  std::vector<pollfd> _poll_fds;
  std::set<int> _welcomed_clients;
  std::map<std::string, MessageHandler> _message_handlers;

  bool canJoin(const Client &client, const Channel &channel, const std::string &key, errorCode &error) const;

  void initSocket(const int PORT);
  void setNonBlocking(int fd);
  void acceptClient();
  void handleClientData(Client &client);
  void removeClient(size_t index);
  void processCommand(Client &client, const std::string &command);

  void sendError(Client &client, const std::string &code, const std::string &message);
  void sendError(Client &client, errorCode code, const std::string &context,
                 const std::string &channel = "", const std::string &command = "");
  void sendReply(Client &client, const std::string &message);
  void sendRaw(Client &client, const std::string &message);
  void flushClientOutput(Client &client);
  const std::string &getServerName() const;
  std::vector<std::string> splitCommand(const std::string &command);
  std::string getClientChannels(const Client &client) const;
  Client *findClientByNick(const std::string &nick);

  void handleTOPIC(Client &client, const IRCMessage &msg);
  void handleKICK(Client &client, const IRCMessage &msg);
  void handleJOIN(Client &client, const IRCMessage &msg);
  void handlePART(Client &client, const IRCMessage &msg);
  void handlePRIVMSG(Client &client, const IRCMessage &msg);
  void handlePING(Client &client, const IRCMessage &msg);
  void handleWHOIS(Client &client, const IRCMessage &msg);
  void handleMODE(Client &client, const IRCMessage &msg);
  void handleLIST(Client &client, const IRCMessage &msg);
  void handleNAMES(Client &client, const IRCMessage &msg);
  void handlePASS(Client &client, const IRCMessage &msg);
  void handleCAP(Client &client, const IRCMessage &msg);
  void handleNICK(Client &client, const IRCMessage &msg);
  void handleUSER(Client &client, const IRCMessage &msg);
  void handleQUIT(Client &client, const IRCMessage &msg);
  void handleINVITE(Client &client, const IRCMessage &msg);

  void sendWelcome(Client &client);
  void sendISupport(Client &client);
  void sendMOTD(Client &client);

  void broadcastToChannel(const std::string &channelName, const std::string &message, Client *exclude = NULL);

  Channel *getChannels(const std::string &name);

  void checkAndSendWelcome(Client &client);

  bool isValidChannelName(const std::string &name) const;
};

#endif
