#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 4096

int main(int argc, char *argv[]) {
    
    // Debug can be removed 
    printf("Number of arguments is: %d\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("Argument %d: %s\n", i, argv[i]);
    }

    // 
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <IP> <PORT>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Read ip and port from arguments
    char ip[sizeof(argv[1])];
    strcpy(ip, argv[1]);
    int port = atoi(argv[2]);

    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    // check if socket creation worked 
    if (socket_fd < 0) {
        perror("Socket creation failed.");
        exit(EXIT_FAILURE);
    }

    // server object ?
    struct sockaddr_in server_addr;

    // client object ?
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    int opt = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    // bind socket
    if (bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Binding failed!");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }
    
    //int newsocket_fd = accept(socket_fd, (struct sockaddr *)&server_addr, &client_len);
    
    // start listening 
    if (listen(socket_fd, 5) < 0) {
        perror("Listen failed");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on %s:%d\n", ip, port);

    

    while(1) {
        int client_fd = accept(socket_fd, (struct sockaddr *)&client_addr, &client_len);

        if (client_fd < 0) {
            perror("accept failed");
            continue;
        } else {

            // Setting up buffer 
            char buffer[BUFFER_SIZE];
            int received_length = 0; // Wie viel im Puffer bereits gespeichert ist

            // Buffer handling 
            while (1) {
                // check available buffer space 
                if (BUFFER_SIZE - received_length -1 <= 0) {
                    //TODO: handle buffer overflow (close connection)
                    break;
                }

                // Read pakets 
                // Empfangen von Daten und Hinzufügen zum Puffer
                int length = read(client_fd, buffer + received_length, BUFFER_SIZE - received_length - 1);
                if (length < 0) {
                    perror("Read failed");
                    continue;
                } else if(length == 0) {
                    // NO more data end of request 
                    break;
                }

                received_length += length;
                buffer[received_length] = '\0'; // Null-Terminator hinzufügen
                printf("Buffer contents: \n");
                printf(buffer);



                // Überprüfen, ob der Puffer das Ende der Nachricht enthält (\r\n\r\n)
                char *end_of_message = strstr(buffer, "\r\n\r\n");
                if (end_of_message != NULL) {
                    // Behandeln Sie die Anfrage hier
                    char *response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nReply\r\n\r\n";
                    //write(client_fd, response, strlen(response));
                    // wait till transmition is complete
                    ssize_t bytes_sent = write(client_fd, response, strlen(response));
                    if (bytes_sent < 0) {
                        perror("Failed to send message");
                    } else {
                        printf("Wrote %zd bytes to client!\n", bytes_sent);
                    }
                    // wait till transmition is complete
                    // Shutdown server side of the connection to send all data.
                    shutdown(client_fd, SHUT_WR);
                    printf("Wrote to client!\n");
                    
                    // Verschieben Sie alle unverarbeiteten Daten an den Anfang des Puffers
                    int remaining = received_length - (end_of_message - buffer + 4);
                    memmove(buffer, end_of_message + 4, remaining);
                    received_length = remaining;
                }


            }

            close(client_fd);

            // Wenn der Puffer voll ist und kein Ende der Nachricht gefunden wurde, 
            // haben Sie ein Problem, da Ihre Anfrage zu groß ist oder nicht korrekt beendet wird.
            // if (received_length == BUFFER_SIZE - 1) {
            //     // Behandeln Sie diesen Fall, möglicherweise durch Schließen der Verbindung
            //     break;
            // }

            
        }
        
    }
    
    close(socket_fd);
    return 0;

    
}