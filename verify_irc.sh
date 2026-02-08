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

cleanup() {
  set +e
  if [[ -n "${NC1_PID}" ]]; then kill "${NC1_PID}" >/dev/null 2>&1; fi
  if [[ -n "${NC2_PID}" ]]; then kill "${NC2_PID}" >/dev/null 2>&1; fi
  if [[ -n "${SERVER_PID}" ]]; then kill "${SERVER_PID}" >/dev/null 2>&1; fi
  if [[ -n "${FD1}" ]]; then eval "exec ${FD1}>&-"; fi
  if [[ -n "${FD2}" ]]; then eval "exec ${FD2}>&-"; fi
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
OUT1="/tmp/irc_out1.$$"
OUT2="/tmp/irc_out2.$$"

mkfifo "${FIFO1}" "${FIFO2}"

nc 127.0.0.1 "${PORT}" < "${FIFO1}" > "${OUT1}" &
NC1_PID=$!

nc 127.0.0.1 "${PORT}" < "${FIFO2}" > "${OUT2}" &
NC2_PID=$!

sleep 0.2

exec 3> "${FIFO1}"
exec 4> "${FIFO2}"
FD1=3
FD2=4

printf 'PASS %s\r\nNICK alpha\r\nUSER u 0 * :Real Name A\r\nJOIN #test\r\n' "${PASS}" >&3
printf 'PASS %s\r\nNICK beta\r\nUSER u 0 * :Real Name B\r\nJOIN #test\r\n' "${PASS}" >&4

sleep 0.4

printf 'PRIVMSG #test :hello from alpha\r\n' >&3

sleep 0.4

WELCOME_OK=0
if grep -qE "(^| )001( |$)|Welcome" "${OUT1}"; then
  WELCOME_OK=1
fi

PRIVMSG_OK=0
if grep -q "PRIVMSG #test" "${OUT2}"; then
  PRIVMSG_OK=1
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

if [[ "${WELCOME_OK}" -eq 1 && "${PRIVMSG_OK}" -eq 1 ]]; then
  exit 0
fi

exit 1

