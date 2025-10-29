#include "./include/Server.hpp"

int	main( int argc, char **argv) {
	(void)argc;
	(void)argv;

	try {
		Server	server( 6667, "passw" );
		server.run();
	} catch ( const std::exception& e ) {
		std::cerr << e.what() << std::endl;
	}

	return 0;
}
