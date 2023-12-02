#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "sys/types.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#define MAXLINE  8192

struct Response{
    int statusCode;
    char payload[256];
};
void error(char *msg){
    perror(msg);
    exit(1);
}
static struct sockaddr_in derive_sockaddr(const char* host, const char* port) {//kopiert vom Aufgabenblatt
    struct addrinfo hints = {
            .ai_family = AF_INET,
            };
    struct addrinfo *result_info;

    // Resolve the host (IP address or hostname) into a list of possible addresses.
    int returncode = getaddrinfo(host, port, &hints, &result_info);
    if (returncode) {
        fprintf(stderr, "Error␣parsing␣host/port");
        exit(EXIT_FAILURE);
    }

    // Copy the sockaddr_in structure from the first address in the list
    struct sockaddr_in result = *((struct sockaddr_in*) result_info->ai_addr);

    // Free the allocated memory for the result_info
    freeaddrinfo(result_info);
    return result;
}

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
} //kopiert vom beej's guide

int main(int argc, char *argv[]){

    if(argc < 2){
        fprintf(stderr, "Full address not provided!");
        exit(1);
    }
    int sockfd, server_port, client_fd, recv_Bytes;
    struct  sockaddr_storage client_addr;
    socklen_t client_addr_len;
    char *packet = calloc(MAXLINE + 1, sizeof(char));
    char *test = calloc(MAXLINE + 1, sizeof(char));
    struct sockaddr_in server_addr;
    int yes = 1;
    int isHttp = 0;

    server_addr = derive_sockaddr(argv[1], argv[2]);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0 ){
        error("Error opening socket!");
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }


    if(bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0){
        close(sockfd);
        error("bind error!");
    }

    if(listen(sockfd, 10) < 0){
        error("listen error!");
    }
    struct sigaction sa; //kopiert von Beej's guide
    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    while(1){

        char *buffer = calloc(MAXLINE+1, sizeof(char));
        client_fd = accept(sockfd, (struct sockaddr *) &client_addr, &client_addr_len);
        if(client_fd < 0){
            perror("accept");
            continue;
        }
        if (!fork()) {
            size_t uri_len, ver_len, buffer_len, header_len, method_len, header_name_len, header_value_len;
            char *method = calloc(method_len + 1, sizeof(char));
            char *uri = calloc(uri_len + 1, sizeof(char));
            char *header_name = calloc(256, sizeof(char));
            char *header_value = calloc(256, sizeof(char));
            int not_valid = 0;
            char *content_length = calloc(256, sizeof(char));
            char *resource = calloc(uri_len + 1, sizeof(char));
            char *header = calloc(header_len + 1, sizeof(char));
            int content_len;
            char *payload = calloc(content_len + 1, sizeof(char));

            for(int i = 0; i < 100; i++){
                recv_Bytes = recv(client_fd, buffer, MAXLINE, 0);
                buffer_len = strlen(buffer);
                if (recv_Bytes < 0) {
                    error("recv");
                }
                if(recv_Bytes > 0){
                    strcat(packet, buffer);

                    size_t request_len = strcspn(buffer, "\r\n");
                    method_len = strcspn(buffer, " ");


                    if(method_len < request_len) { //there is method
                        memcpy(method, buffer, method_len);
                        method[method_len] = '\0';
                        buffer += method_len + 1;
                        uri_len = strcspn(buffer, " ");
                        if(uri_len < request_len){ //there is uri
                            memcpy(uri, buffer, uri_len);
                            uri[uri_len] = '\0';
                            buffer += uri_len + 1;
                            ver_len = strcspn(buffer, "\r\n");
                            if(ver_len < request_len){//there is version
                                buffer += ver_len + 2;
                                header_len = strcspn(buffer, "\r\n");
                                if(header_len > 0){//there is header
                                    for(int j = 0; j < 40; j++){
                                        header_len = strcspn(buffer, "\r\n");
                                        if(header_len > 0){
                                            header_name_len = strcspn(buffer, " ");
                                            memcpy(header_name, buffer, header_name_len);
                                            header_name[header_name_len] = '\0';

                                            buffer += header_name_len + 1;
                                            header_value_len = strcspn(buffer, "\r\n");
                                            memcpy(header_value, buffer, header_value_len);
                                            header_value[header_value_len] = '\0';

                                            if(strncmp(header_name, "Content-Length:", strlen("Content-Length:")) == 0){
                                                memcpy(content_length, header_value, header_value_len);
                                                content_length[header_value_len] = '\0';
                                            }
                                            if(header_name_len > header_len || header_value_len == 0){//test header
                                                not_valid += 1;
                                            }
                                            buffer += header_value_len + 2;

                                        }
                                        else{
                                            break;
                                        }
                                    }
                                }
                                if(not_valid > 0){//not a header Semantic
                                    buffer += 2;
                                    send(client_fd, "HTTP/1.1 400\r\nContent-Length: 0\r\n\r\n", strlen("HTTP/1.1 400\r\nContent-Length: 0\r\n\r\n"), 0);
                                }
                                if(buffer[0] == '\r' && buffer[1] == '\n'){//there is no header OR there is a header Semantic
                                    buffer += 2;

                                    if(strlen(content_length) > 0){//there is payload
                                        content_len = atoi(content_length);
                                        memset(content_length, 0, 256);
                                        memcpy(payload, buffer, content_len);
                                        payload[content_len] = '\0';
                                        buffer += content_len;
                                    }

                                    if(strncmp(method, "GET", strlen("GET")) == 0){
                                        if(strncmp(uri, "/static/foo", strlen("/static/foo")) == 0){
                                            send(client_fd, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 3\r\n\r\n", strlen("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 3\r\n\r\n"), 0);
                                            send(client_fd, "Foo", strlen("Foo"), 0 );
                                        }
                                        if(strncmp(uri, "/static/bar", strlen("/static/bar")) == 0){
                                            send(client_fd, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 3\r\n\r\n", strlen("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 3\r\n\r\n"), 0);
                                            send(client_fd, "Bar", strlen("Bar"), 0 );
                                        }
                                        if(strncmp(uri, "/static/baz", strlen("/static/baz")) == 0){
                                            char msg[1024];
                                            strcpy(msg, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 3\r\n\r\n");
                                            send(client_fd, msg, strlen(msg), 0);
                                            send(client_fd, "Baz", strlen("Baz"), 0 );
                                        }
                                        else{//not static Content OR not bar, baz, foo
                                            if(strlen(resource) > 0){
                                                char msg[1024];
                                                strcpy(msg, "HTTP/1.1 200\r\nContent-Type: text/plain\r\nContent-Length: ");
                                                char len[5];
                                                sprintf(len, "%d", content_len);
                                                strcat(msg, len);
                                                strcat(msg, "\r\n\r\n");
                                                send(client_fd, msg, strlen(msg), 0);
                                                send(client_fd, payload, strlen(payload), 0);
                                            }
                                            else{
                                                send(client_fd, "HTTP/1.1 404\r\nContent-Length: 0\r\n\r\n", strlen("HTTP/1.1 404\r\nContent-Length: 0\r\n\r\n"), 0);
                                            }
                                        }
                                    }
                                    else if(strncmp(uri, "/dynamic/", strlen("/dynamic/")) == 0){
                                        if(strncmp(method, "PUT", strlen("PUT")) == 0){
                                            if(strlen(resource) == 0){
                                                memcpy(resource, uri, uri_len);
                                                resource[uri_len] = '\0';
                                                send(client_fd, "HTTP/1.1 201\r\nContent-Length: 0\r\n\r\n", strlen("HTTP/1.1 201\r\nContent-Length: 0\r\n\r\n"), 0);
                                            }
                                            else{
                                                send(client_fd, "HTTP/1.1 204\r\nContent-Length: 0\r\n\r\n", strlen("HTTP/1.1 204\r\nContent-Length: 0\r\n\r\n"), 0);
                                            }

                                        }
                                        if(strncmp(method, "DELETE", strlen("DELETE")) == 0){
                                            if(strlen(resource) > 0){
                                                memset(resource, 0, strlen(resource));
                                                send(client_fd, "HTTP/1.1 204\r\nContent-Length: 0\r\n\r\n", strlen("HTTP/1.1 204\r\nContent-Length: 0\r\n\r\n"), 0);
                                            }
                                            else{
                                                send(client_fd, "HTTP/1.1 404\r\nContent-Length: 0\r\n\r\n", strlen("HTTP/1.1 404\r\nContent-Length: 0\r\n\r\n"), 0);
                                            }
                                        }

                                    }
                                    else{
                                        if(strncmp(method, "PUT", strlen("PUT")) == 0){
                                            send(client_fd, "HTTP/1.1 403\r\nContent-Length: 0\r\n\r\n", strlen("HTTP/1.1 403\r\nContent-Length: 0\r\n\r\n"), 0);
                                        }
                                        else if(strncmp(method, "DELETE", strlen("DELETE")) == 0){
                                            send(client_fd, "HTTP/1.1 403\r\nContent-Length: 0\r\n\r\n", strlen("HTTP/1.1 403\r\nContent-Length: 0\r\n\r\n"), 0);
                                        } else{
                                            send(client_fd, "HTTP/1.1 501\r\nContent-Length: 0\r\n\r\n", strlen("HTTP/1.1 501\r\nContent-Length: 0\r\n\r\n"), 0);
                                        }
                                    }
                                }
                            } else {//there is no version
                                send(client_fd, "HTTP/1.1 400\r\nContent-Length: 0\r\n\r\n", strlen("HTTP/1.1 400\r\nContent-Length: 0\r\n\r\n"), 0);
                            }
                        } else { //there is no uri
                            send(client_fd, "HTTP/1.1 400\r\nContent-Length: 0\r\n\r\n", strlen("HTTP/1.1 400\r\nContent-Length: 0\r\n\r\n"), 0);
                        }
                    } else{ //there is no method
                        send(client_fd, "HTTP/1.1 400\r\nContent-Length: 0\r\n\r\n", strlen("HTTP/1.1 400\r\nContent-Length: 0\r\n\r\n"), 0);
                    }
                }

                memset(buffer, 0, MAXLINE);
            }
            close(client_fd);
            exit(0);
        }
        close(client_fd);
    }

    return 0;
}
