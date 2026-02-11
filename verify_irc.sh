#!/usr/bin/env bash
set -euo pipefail

PORT="${PORT:-16667}"
PASS="${PASS:-pass}"
SERVER_BIN="./ircserv"
LOG_FILE="/tmp/ircserv_verify.log"

NC1_PID=""
NC2_PID=""
SERVER_PID=""
FIFO1=""
FIFO2=""
OUT1=""
OUT2=""
FD1=""
FD2=""
FD3=""

cleanup() {
  set +e
  if [[ -n "${NC1_PID}" ]]; then kill "${NC1_PID}" >/dev/null 2>&1; fi
  if [[ -n "${NC2_PID}" ]]; then kill "${NC2_PID}" >/dev/null 2>&1; fi
  if [[ -n "${SERVER_PID}" ]]; then kill "${SERVER_PID}" >/dev/null 2>&1; fi
  if [[ -n "${FD1}" ]]; then eval "exec ${FD1}>&-"; fi
  if [[ -n "${FD2}" ]]; then eval "exec ${FD2}>&-"; fi
  if [[ -n "${FD3}" ]]; then eval "exec ${FD3}>&-"; fi
  [[ -n "${FIFO1}" && -p "${FIFO1}" ]] && rm -f "${FIFO1}"
  [[ -n "${FIFO2}" && -p "${FIFO2}" ]] && rm -f "${FIFO2}"
  [[ -n "${OUT1}" ]] && rm -f "${OUT1}"
  [[ -n "${OUT2}" ]] && rm -f "${OUT2}"
}
trap cleanup EXIT

make re

"${SERVER_BIN}" "${PORT}" "${PASS}" > "${LOG_FILE}" 2>&1 &
SERVER_PID=$!

sleep 0.3
if grep -q "Unable to bind Socket" "${LOG_FILE}"; then
  echo "FAIL: server failed to bind on port ${PORT}"
  exit 1
fi

FIFO1="/tmp/irc_fifo1.$$"
FIFO2="/tmp/irc_fifo2.$$"
FIFO3="/tmp/irc_fifo3.$$"
OUT1="/tmp/irc_out1.$$"
OUT2="/tmp/irc_out2.$$"
OUT3="/tmp/irc_out3.$$"

mkfifo "${FIFO1}" "${FIFO2}" "${FIFO3}"

nc 127.0.0.1 "${PORT}" < "${FIFO1}" > "${OUT1}" &
NC1_PID=$!

nc 127.0.0.1 "${PORT}" < "${FIFO2}" > "${OUT2}" &
NC2_PID=$!

nc 127.0.0.1 "${PORT}" < "${FIFO3}" > "${OUT3}" &
NC3_PID=$!

sleep 0.2

exec 3> "${FIFO1}"
exec 4> "${FIFO2}"
exec 5> "${FIFO3}"
FD1=3
FD2=4
FD3=5

printf 'PASS %s\r\nNICK alpha\r\nUSER u 0 * :Real Name A\r\nJOIN #test\r\n' "${PASS}" >&3
printf 'PASS %s\r\nNICK beta\r\nUSER u 0 * :Real Name B\r\nJOIN #test\r\n' "${PASS}" >&4
printf 'PASS %s\r\nNICK gamma\r\nUSER u 0 * :Real Name C\r\n' "${PASS}" >&5

sleep 0.4

printf 'PRIVMSG #test :hello from alpha\r\n' >&3

sleep 0.4

printf 'JOIN #optest\r\n' >&3
sleep 0.2
printf 'MODE #optest +i\r\n' >&3
sleep 0.2
printf 'INVITE beta #optest\r\n' >&3
sleep 0.2
printf 'JOIN #optest\r\n' >&4
sleep 0.2
printf 'MODE #optest +t\r\n' >&3
sleep 0.2
printf 'TOPIC #optest :new topic by beta\r\n' >&4
sleep 0.2
printf 'KICK #optest beta :bye\r\n' >&3
sleep 0.2
printf 'MODE #optest +k secret\r\n' >&3
sleep 0.2
printf 'JOIN #optest\r\n' >&4
sleep 0.4

printf 'com' >&3
sleep 0.2
printf 'mand\r\n' >&3
sleep 0.2
printf 'PING :stillalive\r\n' >&5
sleep 0.4

WELCOME_OK=0
if grep -qE "(^| )001( |$)|Welcome" "${OUT1}"; then
  WELCOME_OK=1
fi

PRIVMSG_OK=0
if grep -q "PRIVMSG #test" "${OUT2}"; then
  PRIVMSG_OK=1
fi

INVITE_OK=0
if grep -q "INVITE beta :#optest" "${OUT2}"; then
  INVITE_OK=1
fi

TOPIC_DENY_OK=0
if grep -q " 482 " "${OUT2}"; then
  TOPIC_DENY_OK=1
fi

KICK_OK=0
if grep -q "KICK #optest beta" "${OUT2}"; then
  KICK_OK=1
fi

KEY_DENY_OK=0
if grep -q " 475 " "${OUT2}"; then
  KEY_DENY_OK=1
fi

PING_OK=0
if grep -q "PONG" "${OUT3}"; then
  PING_OK=1
fi

if [[ "${WELCOME_OK}" -eq 1 ]]; then
  echo "PASS: welcome reply detected"
else
  echo "FAIL: no welcome reply (001/Welcome) detected"
fi

if [[ "${PRIVMSG_OK}" -eq 1 ]]; then
  echo "PASS: PRIVMSG broadcast detected"
else
  echo "FAIL: PRIVMSG broadcast not detected"
fi

if [[ "${INVITE_OK}" -eq 1 ]]; then
  echo "PASS: INVITE delivered"
else
  echo "FAIL: INVITE not delivered"
fi

if [[ "${TOPIC_DENY_OK}" -eq 1 ]]; then
  echo "PASS: TOPIC denied for non-operator"
else
  echo "FAIL: TOPIC not denied for non-operator"
fi

if [[ "${KICK_OK}" -eq 1 ]]; then
  echo "PASS: KICK broadcast detected"
else
  echo "FAIL: KICK broadcast not detected"
fi

if [[ "${KEY_DENY_OK}" -eq 1 ]]; then
  echo "PASS: JOIN denied by +k"
else
  echo "FAIL: JOIN not denied by +k"
fi

if [[ "${PING_OK}" -eq 1 ]]; then
  echo "PASS: server stayed responsive after partial command"
else
  echo "FAIL: server not responsive after partial command"
fi

if [[ "${WELCOME_OK}" -eq 1 && "${PRIVMSG_OK}" -eq 1 && "${INVITE_OK}" -eq 1 && "${TOPIC_DENY_OK}" -eq 1 && "${KICK_OK}" -eq 1 && "${KEY_DENY_OK}" -eq 1 && "${PING_OK}" -eq 1 ]]; then
  exit 0
fi

exit 1
