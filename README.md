# ft_irc

Servidor IRC em C++98, não bloqueante, baseado em `poll()`, seguindo o escopo obrigatório do projeto `ft_irc` da 42.

## Objetivo

Implementar um servidor compatível com cliente IRC real, com:
- múltiplos clientes simultâneos;
- autenticação por senha;
- canais;
- comandos de operador;
- troca de mensagens privada e em canal.

## Stack e restrições do subject

- Linguagem: C++98
- I/O multiplexado: `poll()`
- Sockets em modo não bloqueante
- Sem bibliotecas externas
- Makefile com regras: `all`, `clean`, `fclean`, `re`

## Build

```bash
make
```

Binário gerado: `./ircserv`

## Execução

```bash
./ircserv <port> <password>
```

Exemplo:

```bash
./ircserv 6667 pass
```

## Comandos implementados

- Registro/autenticação:
`PASS`, `NICK`, `USER`, `QUIT`
- Básicos:
`PING`, `WHOIS`, `LIST`, `NAMES`
- Canais:
`JOIN`, `PART`, `TOPIC`, `MODE`, `INVITE`, `KICK`
- Mensagens:
`PRIVMSG` (usuário e canal)

## Modos de canal implementados

- `+i`: invite-only
- `+t`: tópico restrito a operador
- `+k`: senha do canal
- `+o`: operador
- `+l`: limite de usuários

## Arquitetura

- `Server`: socket TCP, loop principal com `poll()`, roteamento de comandos, replies/erros IRC.
- `Client`: estado de autenticação, dados de usuário, buffer de entrada e fila de saída.
- `Channel`: membros, operadores, convidados, modos de canal, broadcast.
- `IRCMessage`: parser de comandos IRC com suporte a parâmetros e trailing.

## Testes incluídos

- `verify_irc.sh`: smoke/integration test com `nc` (registro, JOIN, PRIVMSG, INVITE, MODE, TOPIC, KICK, PING e comando parcial).
- `irc_tester.py`: suíte de testes em Python para validar fluxo de comandos e respostas.

Execução:

```bash
./verify_irc.sh
python3 irc_tester.py
```

## Fluxo de demo (apresentação)

1. Subir servidor: `./ircserv 6667 pass`
2. Conectar 2 clientes (ex.: `nc 127.0.0.1 6667`)
3. Registrar cada cliente com:
`PASS`, `NICK`, `USER`
4. Criar/entrar em canal:
`JOIN #test`
5. Testar mensagem:
`PRIVMSG #test :hello`
6. Testar modos/operação:
`MODE #test +i`, `INVITE`, `TOPIC`, `KICK`, `MODE #test +k segredo`

## Estrutura do projeto

```text
include/
  Server.hpp
  Client.hpp
  Channel.hpp
  IRCMessage.hpp
src/
  Server.cpp
  Client.cpp
  Channel.cpp
  IRCMessage.cpp
main.cpp
Makefile
```
