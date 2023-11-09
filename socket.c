#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>


int create_socket(int port, int *server_fd, struct sockaddr_in *server_addr) {
    *server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (*server_fd == -1) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

	//configure socket
    int opt = 1;
    if (setsockopt(*server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

	//bind socket 
    server_addr->sin_family = AF_INET;
    server_addr->sin_addr.s_addr = INADDR_ANY;
    server_addr->sin_port = htons(port);

	return 0;
}


// accept connection
void accept_connection(int client_addr, int client_fd, int server_fd, int addr_len) {
	addr_len = sizeof(client_addr);
    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_fd < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }
}


void handle_client(int client_fd) {
	// handle client 
    char *message = "HTTP/1.1 200 OK\nContent-Type: text/plain\n\nHello, World!";
    write(client_fd, message, strlen(message));
}

int get_port(int argc, char *argv[]) {
	if (argc < 2) {
		printf("Please procide a port as a argument. \n");
		return -1;
	}
	char *end;
	int port = strtol(argv[1], &end, 10);

	if (*end) {
        printf("Conversion error, non-convertible part: %s\n", end);
        return -1;
    }

	return port;
}

int main(int argc, char *argv[]) {

	//init variables
    int server_fd, client_fd; //File decriptors 
    struct sockaddr_in server_addr, client_addr; //structs to hold socket adress info
    socklen_t addr_len;// struct size 

	int port = get_port(argc, argv);
	if (port == -1) {
		perror("no port as argument");
        exit(EXIT_FAILURE);
	}

	// 1. create and bind a new socket 
	if (create_socket(port, &server_fd, &server_addr) != 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    } else {
        printf("Socket created!\n");
    }

	addr_len = sizeof(client_addr); 

	// start to listen 
	if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
	printf("listening...\n");
	while (1) 
	{
		printf("new loop run\n");
		client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
		if (client_fd < 0) {
			perror("accept");
			continue;
		} else {
			handle_client(client_fd);
		}

    	close(client_fd);
		printf("end loop \n");
	}

	// close server socket
    close(server_fd);

    return 0;
}
