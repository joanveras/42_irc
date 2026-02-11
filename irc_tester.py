#!/usr/bin/env python3
"""
IRC Server Tester - Específico para ircserv (porta 6667, senha passw)
"""

import socket
import time
import sys
import select
from enum import Enum

class Color:
    GREEN = '\033[92m'
    RED = '\033[91m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    END = '\033[0m'

class TestResult(Enum):
    PASS = "PASS"
    FAIL = "FAIL"
    WARNING = "WARNING"

class IRCServTester:
    def __init__(self, host='127.0.0.1', port=6667, password='passw'):
        self.host = host
        self.port = port
        self.password = password
        self.test_count = 0
        self.pass_count = 0
        self.fail_count = 0
        self.warning_count = 0
        
    def print_banner(self):
        """Imprime banner do tester"""
        print(f"{Color.CYAN}{'='*60}{Color.END}")
        print(f"{Color.CYAN}🚀 IRC SERVER TESTER - Específico para ircserv{Color.END}")
        print(f"{Color.CYAN}   Porta: {self.port}, Senha: '{self.password}'{Color.END}")
        print(f"{Color.CYAN}{'='*60}{Color.END}\n")
    
    def connect_client(self, nickname="testuser"):
        """Conecta um cliente ao servidor"""
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        try:
            sock.connect((self.host, self.port))
            return sock
        except ConnectionRefusedError:
            return None
        except Exception as e:
            print(f"{Color.RED}❌ Erro de conexão: {e}{Color.END}")
            return None
    
    def send_command(self, sock, command):
        """Envia um comando para o servidor"""
        if not command.endswith('\r\n'):
            command += '\r\n'
        try:
            sock.send(command.encode())
            return True
        except Exception as e:
            print(f"{Color.RED}❌ Erro ao enviar comando: {e}{Color.END}")
            return False
    
    def receive_response(self, sock, timeout=1):
        """Recebe resposta do servidor"""
        sock.settimeout(timeout)
        try:
            data = b""
            while True:
                chunk = sock.recv(1024)
                if not chunk:
                    break
                data += chunk
                # Verifica se há mais dados imediatamente disponíveis
                ready = select.select([sock], [], [], 0.1)
                if not ready[0]:
                    break
            return data.decode('utf-8', errors='ignore')
        except socket.timeout:
            return ""
        except Exception as e:
            return f"Error: {e}"
    
    def print_test(self, test_name, result, message=""):
        """Imprime resultado do teste formatado"""
        self.test_count += 1
        
        if result == TestResult.PASS:
            self.pass_count += 1
            symbol = "✅"
            color = Color.GREEN
        elif result == TestResult.FAIL:
            self.fail_count += 1
            symbol = "❌"
            color = Color.RED
        else:
            self.warning_count += 1
            symbol = "⚠️"
            color = Color.YELLOW
        
        status = f"{symbol} {color}{test_name:45}{Color.END}"
        if message:
            print(f"{status} {message}")
        else:
            print(status)
    
    def register_client(self, sock, nick="testuser", user="testuser"):
        """Registra um cliente (PASS + NICK + USER)"""
        self.send_command(sock, f"PASS {self.password}")
        self.send_command(sock, f"NICK {nick}")
        self.send_command(sock, f"USER {user} 0 * :{user} User")
        time.sleep(0.2)
        return self.receive_response(sock)
    
    def test_01_connection(self):
        """Testa conexão básica"""
        print(f"{Color.BLUE}[1] Testando conexão básica...{Color.END}")
        
        sock = self.connect_client()
        if sock:
            self.print_test("Conexão TCP estabelecida", TestResult.PASS, 
                          f"{self.host}:{self.port}")
            sock.close()
            return True
        else:
            self.print_test("Conexão TCP estabelecida", TestResult.FAIL, 
                          "Servidor não responde")
            return False
    
    def test_02_registration(self):
        """Testa registro completo"""
        print(f"\n{Color.BLUE}[2] Testando registro de usuário...{Color.END}")
        
        sock = self.connect_client()
        if not sock:
            return False
        
        try:
            # Teste PASS
            self.send_command(sock, "PASS wrongpass")
            response = self.receive_response(sock)
            if "464" in response or "incorrect" in response.lower():
                self.print_test("PASS com senha errada", TestResult.PASS)
            elif response:
                self.print_test("PASS com senha errada", TestResult.WARNING,
                              f"Resposta inesperada: {response[:50]}")
            
            # Registro correto
            self.send_command(sock, f"PASS {self.password}")
            self.send_command(sock, "NICK tester")
            self.send_command(sock, "USER tester 0 * :Test User")
            
            time.sleep(0.3)
            response = self.receive_response(sock, 2)
            
            # Verifica welcome messages
            has_welcome = "001" in response or "Welcome" in response
            has_nick_response = "NICK" in response or "433" not in response
            has_user_response = "USER" in response or "462" not in response
            
            if has_welcome:
                self.print_test("Registro completo (PASS+NICK+USER)", TestResult.PASS)
            else:
                self.print_test("Registro completo", TestResult.WARNING,
                              f"Sem mensagem welcome: {response[:100]}")
            
            if has_nick_response:
                self.print_test("Comando NICK aceito", TestResult.PASS)
            
            if has_user_response:
                self.print_test("Comando USER aceito", TestResult.PASS)
            
            sock.close()
            return has_welcome
            
        except Exception as e:
            self.print_test("Registro", TestResult.FAIL, f"Erro: {e}")
            return False
    
    def test_03_ping_pong(self):
        """Testa PING/PONG"""
        print(f"\n{Color.BLUE}[3] Testando PING/PONG...{Color.END}")
        
        sock = self.connect_client()
        if not sock:
            return False
        
        try:
            self.register_client(sock, "pinger")
            time.sleep(0.2)
            
            # Envia PING
            self.send_command(sock, "PING test123")
            time.sleep(0.2)
            response = self.receive_response(sock)
            
            if "PONG" in response:
                self.print_test("Resposta PONG recebida", TestResult.PASS)
                sock.close()
                return True
            else:
                self.print_test("Resposta PONG recebida", TestResult.FAIL,
                              f"Resposta: {response[:100]}")
                sock.close()
                return False
                
        except Exception as e:
            self.print_test("PING/PONG", TestResult.FAIL, f"Erro: {e}")
            return False
    
    def test_04_channel_join_part(self):
        """Testa JOIN e PART"""
        print(f"\n{Color.BLUE}[4] Testando JOIN e PART...{Color.END}")
        
        sock = self.connect_client()
        if not sock:
            return False
        
        try:
            self.register_client(sock, "chanuser")
            time.sleep(0.2)
            
            # JOIN
            self.send_command(sock, "JOIN #testchannel")
            time.sleep(0.2)
            join_response = self.receive_response(sock)
            
            has_join = "JOIN" in join_response
            if has_join:
                self.print_test("JOIN em canal", TestResult.PASS)
            else:
                self.print_test("JOIN em canal", TestResult.WARNING,
                              f"Resposta: {join_response[:100]}")
            
            # PART
            self.send_command(sock, "PART #testchannel")
            time.sleep(0.2)
            part_response = self.receive_response(sock)
            
            has_part = "PART" in part_response
            if has_part:
                self.print_test("PART do canal", TestResult.PASS)
            else:
                self.print_test("PART do canal", TestResult.WARNING,
                              f"Resposta: {part_response[:100]}")
            
            sock.close()
            return has_join and has_part
            
        except Exception as e:
            self.print_test("JOIN/PART", TestResult.FAIL, f"Erro: {e}")
            return False
    
    def test_05_privmsg_basic(self):
        """Testa PRIVMSG básico"""
        print(f"\n{Color.BLUE}[5] Testando PRIVMSG básico...{Color.END}")
        
        # Dois clientes
        client1 = self.connect_client()
        client2 = self.connect_client()
        
        if not client1 or not client2:
            return False
        
        try:
            # Registrar ambos
            self.register_client(client1, "user1")
            self.register_client(client2, "user2")
            time.sleep(0.3)
            
            # Limpar buffers
            self.receive_response(client1)
            self.receive_response(client2)
            
            # PRIVMSG de user1 para user2
            self.send_command(client1, "PRIVMSG user2 :Hello from user1!")
            time.sleep(0.3)
            
            # user2 deve receber a mensagem
            response = self.receive_response(client2)
            if "PRIVMSG" in response and "user1" in response and "Hello" in response:
                self.print_test("PRIVMSG entre usuários", TestResult.PASS)
                success = True
            else:
                self.print_test("PRIVMSG entre usuários", TestResult.FAIL,
                              f"Resposta: {response[:100]}")
                success = False
            
            client1.close()
            client2.close()
            return success
            
        except Exception as e:
            self.print_test("PRIVMSG", TestResult.FAIL, f"Erro: {e}")
            return False
    
    def test_06_channel_privmsg(self):
        """Testa PRIVMSG em canal"""
        print(f"\n{Color.BLUE}[6] Testando PRIVMSG em canal...{Color.END}")
        
        client1 = self.connect_client()
        client2 = self.connect_client()
        
        if not client1 or not client2:
            return False
        
        try:
            self.register_client(client1, "chanuser1")
            self.register_client(client2, "chanuser2")
            time.sleep(0.3)
            
            # Ambos entram no canal
            self.send_command(client1, "JOIN #msgchannel")
            self.send_command(client2, "JOIN #msgchannel")
            time.sleep(0.3)
            
            # Limpar buffers de JOIN
            self.receive_response(client1)
            self.receive_response(client2)
            
            # user1 manda mensagem no canal
            self.send_command(client1, "PRIVMSG #msgchannel :Hello channel!")
            time.sleep(0.3)
            
            # user2 deve receber a mensagem
            response = self.receive_response(client2)
            if "PRIVMSG" in response and "#msgchannel" in response and "Hello" in response:
                self.print_test("PRIVMSG em canal (broadcast)", TestResult.PASS)
                success = True
            else:
                self.print_test("PRIVMSG em canal", TestResult.WARNING,
                              f"Resposta: {response[:100]}")
                success = False
            
            client1.close()
            client2.close()
            return success
            
        except Exception as e:
            self.print_test("PRIVMSG canal", TestResult.FAIL, f"Erro: {e}")
            return False
    
    def test_07_whois_command(self):
        """Testa comando WHOIS"""
        print(f"\n{Color.BLUE}[7] Testando WHOIS...{Color.END}")
        
        sock = self.connect_client()
        if not sock:
            return False
        
        try:
            self.register_client(sock, "whoisuser")
            time.sleep(0.2)
            
            # WHOIS de si mesmo
            self.send_command(sock, "WHOIS whoisuser")
            time.sleep(0.3)
            response = self.receive_response(sock)
            
            # Verifica respostas WHOIS
            has_311 = "311" in response  # RPL_WHOISUSER
            has_318 = "318" in response  # RPL_ENDOFWHOIS
            
            if has_311 and has_318:
                self.print_test("Comando WHOIS", TestResult.PASS)
                success = True
            elif response:
                self.print_test("Comando WHOIS", TestResult.WARNING,
                              f"Resposta parcial: {response[:100]}")
                success = True
            else:
                self.print_test("Comando WHOIS", TestResult.FAIL,
                              "Nenhuma resposta")
                success = False
            
            sock.close()
            return success
            
        except Exception as e:
            self.print_test("WHOIS", TestResult.FAIL, f"Erro: {e}")
            return False
    
    def test_08_list_command(self):
        """Testa comando LIST"""
        print(f"\n{Color.BLUE}[8] Testando LIST...{Color.END}")
        
        sock = self.connect_client()
        if not sock:
            return False
        
        try:
            self.register_client(sock, "lister")
            time.sleep(0.2)
            
            # Criar um canal primeiro
            self.send_command(sock, "JOIN #listtest")
            time.sleep(0.2)
            self.receive_response(sock)  # Limpar JOIN message
            
            # Executar LIST
            self.send_command(sock, "LIST")
            time.sleep(0.3)
            response = self.receive_response(sock, 2)
            
            # Verifica respostas LIST
            has_322 = "322" in response  # RPL_LIST
            has_323 = "323" in response  # RPL_LISTEND
            
            if has_322 and has_323:
                self.print_test("Comando LIST", TestResult.PASS)
                success = True
            elif response:
                self.print_test("Comando LIST", TestResult.WARNING,
                              f"Resposta parcial: {response[:100]}")
                success = True
            else:
                self.print_test("Comando LIST", TestResult.FAIL,
                              "Nenhuma resposta")
                success = False
            
            sock.close()
            return success
            
        except Exception as e:
            self.print_test("LIST", TestResult.FAIL, f"Erro: {e}")
            return False
    
    def test_09_names_command(self):
        """Testa comando NAMES"""
        print(f"\n{Color.BLUE}[9] Testando NAMES...{Color.END}")
        
        sock = self.connect_client()
        if not sock:
            return False
        
        try:
            self.register_client(sock, "namesuser")
            time.sleep(0.2)
            
            # Entrar em um canal
            self.send_command(sock, "JOIN #nameschannel")
            time.sleep(0.2)
            self.receive_response(sock)  # Limpar JOIN message
            
            # Executar NAMES
            self.send_command(sock, "NAMES #nameschannel")
            time.sleep(0.3)
            response = self.receive_response(sock)
            
            # Verifica respostas NAMES
            has_353 = "353" in response  # RPL_NAMREPLY
            has_366 = "366" in response  # RPL_ENDOFNAMES
            
            if has_353 and has_366:
                self.print_test("Comando NAMES", TestResult.PASS)
                success = True
            elif response:
                self.print_test("Comando NAMES", TestResult.WARNING,
                              f"Resposta parcial: {response[:100]}")
                success = True
            else:
                self.print_test("Comando NAMES", TestResult.FAIL,
                              "Nenhuma resposta")
                success = False
            
            sock.close()
            return success
            
        except Exception as e:
            self.print_test("NAMES", TestResult.FAIL, f"Erro: {e}")
            return False
    
    def test_10_topic_command(self):
        """Testa comando TOPIC"""
        print(f"\n{Color.BLUE}[10] Testando TOPIC...{Color.END}")
        
        sock = self.connect_client()
        if not sock:
            return False
        
        try:
            self.register_client(sock, "topicuser")
            time.sleep(0.2)
            
            # Entrar em canal
            self.send_command(sock, "JOIN #topicchan")
            time.sleep(0.2)
            self.receive_response(sock)
            
            # Definir tópico
            self.send_command(sock, "TOPIC #topicchan :Welcome to our channel!")
            time.sleep(0.3)
            set_response = self.receive_response(sock)
            
            has_topic_set = "TOPIC" in set_response or "332" in set_response
            if has_topic_set:
                self.print_test("Definir TOPIC", TestResult.PASS)
            else:
                self.print_test("Definir TOPIC", TestResult.WARNING,
                              f"Resposta: {set_response[:100]}")
            
            # Ver tópico
            self.send_command(sock, "TOPIC #topicchan")
            time.sleep(0.2)
            get_response = self.receive_response(sock)
            
            has_topic_get = "TOPIC" in get_response or "332" in get_response or "331" in get_response
            if has_topic_get:
                self.print_test("Ver TOPIC", TestResult.PASS)
                success = True
            else:
                self.print_test("Ver TOPIC", TestResult.WARNING,
                              f"Resposta: {get_response[:100]}")
                success = True
            
            sock.close()
            return success
            
        except Exception as e:
            self.print_test("TOPIC", TestResult.FAIL, f"Erro: {e}")
            return False
    
    def test_11_quit_command(self):
        """Testa comando QUIT"""
        print(f"\n{Color.BLUE}[11] Testando QUIT...{Color.END}")
        
        sock = self.connect_client()
        if not sock:
            return False
        
        try:
            self.register_client(sock, "quituser")
            time.sleep(0.2)
            
            # Enviar QUIT
            self.send_command(sock, "QUIT :Goodbye!")
            time.sleep(0.2)
            
            # Verificar se socket foi fechado
            try:
                response = sock.recv(1, socket.MSG_DONTWAIT)
                # Se chegou aqui, socket ainda está aberto
                self.print_test("Comando QUIT", TestResult.FAIL, "Socket não fechado")
                sock.close()
                return False
            except (socket.error, ConnectionResetError):
                # Socket foi fechado (esperado)
                self.print_test("Comando QUIT", TestResult.PASS)
                return True
                
        except Exception as e:
            self.print_test("QUIT", TestResult.FAIL, f"Erro: {e}")
            return False
    
    def test_12_error_handling(self):
        """Testa tratamento de erros"""
        print(f"\n{Color.BLUE}[12] Testando tratamento de erros...{Color.END}")
        
        sock = self.connect_client()
        if not sock:
            return False
        
        try:
            # Tentar comando sem registrar
            self.send_command(sock, "JOIN #channel")
            time.sleep(0.2)
            response = self.receive_response(sock)
            
            has_error = "451" in response or "not registered" in response.lower() or "error" in response.lower()
            if has_error:
                self.print_test("Erro: comando sem registro", TestResult.PASS)
                success = True
            else:
                self.print_test("Erro: comando sem registro", TestResult.WARNING,
                              f"Resposta: {response[:100]}")
                success = True
            
            sock.close()
            return success
            
        except Exception as e:
            self.print_test("Erro handling", TestResult.FAIL, f"Erro: {e}")
            return False
    
    def run_all_tests(self):
        """Executa todos os testes"""
        self.print_banner()
        
        # Verificar se servidor está rodando
        print(f"{Color.YELLOW}⚠️  Certifique-se que o servidor está rodando:{Color.END}")
        print(f"   ./ircserv 6667 passw")
        print(f"{Color.YELLOW}⚠️  Pressione Enter para iniciar os testes...{Color.END}")
        input()
        
        start_time = time.time()
        
        # Executar todos os testes
        tests = [
            (self.test_01_connection, "Conexão"),
            (self.test_02_registration, "Registro"),
            (self.test_03_ping_pong, "PING/PONG"),
            (self.test_04_channel_join_part, "JOIN/PART"),
            (self.test_05_privmsg_basic, "PRIVMSG básico"),
            (self.test_06_channel_privmsg, "PRIVMSG canal"),
            (self.test_07_whois_command, "WHOIS"),
            (self.test_08_list_command, "LIST"),
            (self.test_09_names_command, "NAMES"),
            (self.test_10_topic_command, "TOPIC"),
            (self.test_11_quit_command, "QUIT"),
            (self.test_12_error_handling, "Tratamento de erros"),
        ]
        
        results = []
        for test_func, test_name in tests:
            print(f"\n{Color.CYAN}▶ Executando: {test_name}{Color.END}")
            try:
                result = test_func()
                results.append(result)
            except Exception as e:
                print(f"{Color.RED}❌ Erro inesperado no teste: {e}{Color.END}")
                results.append(False)
            time.sleep(0.5)  # Pequena pausa entre testes
        
        # Estatísticas
        end_time = time.time()
        duration = end_time - start_time
        
        passed_tests = sum(1 for r in results if r)
        failed_tests = len(results) - passed_tests
        
        print(f"\n{Color.CYAN}{'='*60}{Color.END}")
        print(f"{Color.CYAN}📊 RESUMO FINAL{Color.END}")
        print(f"{Color.CYAN}{'='*60}{Color.END}")
        
        print(f"\n{Color.GREEN}✅ Testes aprovados: {self.pass_count}/{self.test_count}{Color.END}")
        print(f"{Color.YELLOW}⚠️  Testes com aviso: {self.warning_count}/{self.test_count}{Color.END}")
        print(f"{Color.RED}❌ Testes falhados: {self.fail_count}/{self.test_count}{Color.END}")
        print(f"⏱️  Tempo total: {duration:.2f} segundos")
        
        # Status final
        print(f"\n{Color.CYAN}{'='*60}{Color.END}")
        if self.fail_count == 0:
            print(f"{Color.GREEN}🎉 EXCELENTE! Todos os testes principais passaram!{Color.END}")
            print(f"{Color.GREEN}   Seu servidor IRC está funcionando corretamente.{Color.END}")
        elif self.fail_count <= 3:
            print(f"{Color.YELLOW}⚠️  BOM! A maioria dos testes passou.{Color.END}")
            print(f"{Color.YELLOW}   Algumas funcionalidades precisam de ajustes.{Color.END}")
        else:
            print(f"{Color.RED}❌ ATENÇÃO! Muitos testes falharam.{Color.END}")
            print(f"{Color.RED}   Revise a implementação dos comandos.{Color.END}")
        
        print(f"{Color.CYAN}{'='*60}{Color.END}")

def main():
    """Função principal"""
    tester = IRCServTester(host='127.0.0.1', port=6667, password='passw')
    tester.run_all_tests()

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print(f"\n{Color.YELLOW}⏹️  Teste interrompido pelo usuário.{Color.END}")
        sys.exit(1)
    except Exception as e:
        print(f"{Color.RED}❌ Erro fatal: {e}{Color.END}")
        sys.exit(1)