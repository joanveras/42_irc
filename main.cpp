#include "./include/Server.hpp"

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cout << "Error: invalid input, below example of correct input.\nExample: ./ircserv <port> <password>" << std::endl;
    return 1;
  }
  try {
    std::string password = static_cast<std::string>(argv[2]);
    unsigned int port = atoi(argv[1]);
    Server server(port, password);
    server.run();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}
