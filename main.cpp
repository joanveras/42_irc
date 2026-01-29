#include "./include/Server.hpp"

namespace {
const int DEFAULT_PORT = 6667;
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  try {
    Server server(DEFAULT_PORT, "passw");
    server.run();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}
