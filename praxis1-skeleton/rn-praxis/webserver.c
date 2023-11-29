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
static struct sockaddr_in derive_sockaddr(const char* host, const char* port) {
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

    /*bzero(&server_addr, sizeof(server_addr));
    server_port = atoi(argv[2]);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(server_port);*/
    char *statusCode = "HTTP/1.1 400\r\n\r\n";
    int status_len = strlen(statusCode);

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
            size_t uri_len, ver_len, buffer_len, header_len, method_len;
            char *method = calloc(method_len + 1, sizeof(char));
            char *uri = calloc(uri_len + 1, sizeof(char));
            char *header_name = calloc(256, sizeof(char));
            char *header_value = calloc(256, sizeof(char));

            for(int i = 0; i < 100; i++){
                recv_Bytes = recv(client_fd, buffer, MAXLINE, 0);
                buffer_len = strlen(buffer);
                if (recv_Bytes < 0) {
                    error("recv");
                }
                if(recv_Bytes > 0){
                    strcat(packet, buffer);
                    printf("%s\n",buffer);

                    size_t request_len = strcspn(buffer, "\r\n");
                    printf("%ld\n",request_len);
                    method_len = strcspn(buffer, " ");


                    if(method_len < request_len) { //there is method
                        printf("%ld\n",method_len);
                        memcpy(method, buffer, method_len);
                        method[method_len] = '\0';
                        buffer += method_len + 1;
                        uri_len = strcspn(buffer, " ");
                        if(uri_len < request_len){ //there is uri
                            printf("%ld\n",uri_len);
                            memcpy(uri, buffer, uri_len);
                            uri[uri_len] = '\0';
                            printf("%s\n",uri);
                            buffer += uri_len + 1;
                            ver_len = strcspn(buffer, "\r\n");
                            if(ver_len < request_len){//there is version
                                buffer += ver_len + 2;
                                header_len = strcspn(buffer, "\r\n");
                                if(header_len > 0){//there is header
                                    for(int j = 0; j < 40; j++){
                                        header_len = strcspn(buffer, "\r\n");
                                        if(header_len > 0){
                                            buffer += header_len + 2;
                                        }else{
                                            break;
                                        }
                                    }
                                }
                                if(buffer[0] == '\r' && buffer[1] == '\n'){//there is no header OR header is parsed
                                    buffer += 2;
                                    if(strncmp(uri, "/static/foo", strlen("/static/foo")) == 0){
                                        send(client_fd, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 3\r\n\r\n", strlen("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 3\r\n\r\n"), 0);
                                        send(client_fd, "Foo", strlen("Foo"), 0 );
                                    }
                                    if(strncmp(uri, "/static/bar", strlen("/static/bar")) == 0){
                                        send(client_fd, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 3\r\n\r\n", strlen("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 3\r\n\r\n"), 0);
                                        send(client_fd, "Bar", strlen("Bar"), 0 );
                                    }
                                    if(strncmp(uri, "/static/baz", strlen("/static/baz")) == 0){
                                        send(client_fd, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 3\r\n\r\n", strlen("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 3\r\n\r\n"), 0);
                                        send(client_fd, "Baz", strlen("Baz"), 0 );
                                    }
                                    else{
                                        if(strncmp(method, "GET", strlen("GET")) == 0){//not static Content OR not bar, baz, foo
                                            send(client_fd, "HTTP/1.1 404\r\nContent-Length: 0\r\n\r\n", strlen("HTTP/1.1 404\r\nContent-Length: 0\r\n\r\n"), 0);
                                        }
                                        else{
                                            send(client_fd, "HTTP/1.1 501\r\n\r\n", status_len, 0);
                                        }
                                    }
                                }
                            } else {//there is no version
                                send(client_fd, "HTTP/1.1 400\r\n\r\n", status_len, 0);
                            }
                        } else { //there is no uri
                            send(client_fd, "HTTP/1.1 400\r\n\r\n", status_len, 0);
                        }
                    } else{ //there is no method
                        send(client_fd, "HTTP/1.1 400\r\n\r\n", status_len, 0);
                    }
                }


                /*if(packet[0] == '\r' && packet[1] == '\n'){
                    packet += 2;
                }
                method_len = strcspn(packet, " ");
                size_t request_len = strcspn(packet, "\r\n");

                if(method_len < request_len){ //there is method
                        memcpy(method, packet, method_len);
                        method[method_len] = '\0';
                        packet += request_len + 2;

                        if(strlen(packet) == 2){ //Anfrage besteht nur aus Request
                            packet += 2;
                            if(strncmp(method, "GET", strlen("GET")) == 0){
                                send(client_fd, "HTTP/1.1 404\r\n\r\n", status_len, 0);
                            }
                            else{
                                send(client_fd, "HTTP/1.1 501\r\n\r\n", status_len, 0);
                            }
                        }
                        while(packet[0] != '\r' && packet[1] != '\n'){
                            header_len = strcspn(packet, "\r\n");
                            packet += header_len + 2;
                        }
                        if(packet[0] == '\r' && packet[1] == '\n'){
                            packet += 2;
                            if(strncmp(method, "GET", strlen("GET")) == 0){
                                send(client_fd, "HTTP/1.1 404\r\n\r\n", status_len, 0);
                            }
                            else{
                                send(client_fd, "HTTP/1.1 501\r\n\r\n", status_len, 0);
                            }
                        }
                    }
                    else{ //there is no method
                        packet += request_len + 2;
                        if(packet[0] == '\r' && packet[1] == '\n'){ //Anfrage besteht nur aus Request
                            packet += 2;
                            send(client_fd, "HTTP/1.1 400\r\n\r\n", status_len, 0);
                        }
                    }*/
                memset(buffer, 0, MAXLINE);
            }
            printf("**%s\n",packet);
            close(client_fd);
            exit(0);
        }
        close(client_fd);
    }

    return 0;
}
/*if(method_len > request_len){ //there is no method
                    send(client_fd, "HTTP/1.1 400\r\n\r\n", status_len, 0);
                }
                else{
                    packet += request_len + 2;
                    if(packet[0] == '\r' && packet[1] == '\n'){
                        send(client_fd, "HTTP/1.1 400\r\n\r\n", status_len, 0);
                    }
                    else{
                        while(packet[0] != '\r' || packet[1] != '\n'){
                            header_len = strcspn(packet, "\r\n");
                            packet += header_len + 2;
                        }
                        send(client_fd, "HTTP/1.1 400\r\n\r\n", status_len, 0);
                    }
                } */
/*if(packet[0] == '\r' && packet[1] == '\n'){
    send(client_fd, "HTTP/1.1 400\r\n\r\n", status_len, 0);
}
size_t request_len = strcspn(packet, "\r\n");
char *request = calloc(request_len + 1, sizeof(char));
memcpy(request, packet, request_len);
request[request_len] = '\0';
packet += request_len + 2;
if(packet[0] == '\r' && packet[1] == '\n'){
    send(client_fd, "HTTP/1.1 400\r\n\r\n", status_len, 0);
}
else{
    while(packet[0] != '\r' || packet[1] != '\n'){
        size_t header_len = strcspn(packet, "\r\n");
        packet += header_len + 2;
    }
    if(packet [0] == '\r' && packet[1] == '\n'){
        if(strncmp(method, "GET", strlen("GET")) == 0){
            send(client_fd, "HTTP/1.1 404\r\n\r\n", status_len, 0);

        }
        if(strncmp(method, "HEAD", strlen("HEAD")) == 0){
            send(client_fd, "HTTP/1.1 501\r\n\r\n", status_len, 0);
        }
        else{
            send(client_fd, "HTTP/1.1 400\r\n\r\n", status_len, 0);
        }
    }
    else{
        continue;
    }
}*/
                /*method_len = strcspn(packet, " ");
                if(method_len > 7){
                    send(client_fd, "HTTP/1.1 400\r\n\r\n", status_len, 0);
                }
                else{
                    method = calloc(method_len + 1, sizeof(char));
                    memcpy(method, packet, method_len);
                    method[method_len] = '\0';
                    packet += method_len + 1;
                    uri_len = strcspn(packet, " ");
                    uri = calloc(uri_len + 1, sizeof(char));
                    memcpy(uri, packet, uri_len);
                    uri[uri_len] = '\0';
                    packet += uri_len + 1;
                    size_t ver_len = strcspn(packet, "\r\n");
                    char *ver = calloc(ver_len + 1, sizeof(char));
                    memcpy(ver, packet, ver_len);
                    ver[ver_len] = '\0';
                    packet += ver_len + 2;

                    while(packet[0] != '\r' || packet[1] != '\n'){
                        size_t header_name_len = strcspn(packet, ":");
                        char *header_name = calloc(header_name_len + 1, sizeof(char));
                        memcpy(header_name, packet, header_name_len);
                        header_name[header_name_len] = '\0';
                        packet += header_name_len + 1;
                        size_t header_value_len = strcspn(packet, "\r\n");
                        char *header_value = calloc(header_value_len + 1, sizeof(char));
                        memcpy(header_value, packet, header_value_len);
                        header_value[header_value_len] = '\0';
                        packet += header_value_len + 2;
                    }
                    if(strlen(packet) == 0){

                    }
                    else{
                        if(packet[0] == '\r' && packet[1] == '\n'){
                            if(strncmp(method, "GET", strlen("GET")) == 0){
                                send(client_fd, "HTTP/1.1 404\r\n\r\n", status_len, 0);

                            }
                            if(strncmp(method, "HEAD", strlen("HEAD")) == 0){
                                send(client_fd, "HTTP/1.1 501\r\n\r\n", status_len, 0);
                            }
                            else{
                                send(client_fd, "HTTP/1.1 400\r\n\r\n", status_len, 0);
                            }
                        }
                    }
                }*/

/*                size_t method_len = strcspn(buffer, " ");
                char *method = calloc(method_len + 1, sizeof(char));
                memcpy(method, buffer, method_len);
                for(int x = 0; x < strlen(buffer); x++){
                    if(buffer[x] == '\r'){
                        if(buffer[x+1] == '\n'){
                            isHttp++;
                        }
                    }
                    if (isHttp == 2) {
                        if(strcmp(method, "GET") == 0){
                            send(client_fd, "HTTP/1.1 404\r\n\r\n", status_len, 0);

                        }
                        if(strcmp(method, "HEAD") == 0){
                            send(client_fd, "HTTP/1.1 501\r\n\r\n", status_len, 0);
                        }
                        else{
                            send(client_fd, "HTTP/1.1 400\r\n\r\n", status_len, 0);
                        }
                    }
                } */








/* char *httpReply = "Reply\r\n\r\n";
            int httpPeply_len = strlen(httpReply);
            int j = 0; //end of paket
            char *statusCode = "HTTP/1.1 400\r\n\r\n";
            int status_len = strlen(statusCode);
            int isHttp = 0;

            size_t method_len;
            size_t uri_len;
            char *method = calloc(method_len + 1, sizeof(char));
            size_t buffer_len = strlen(buffer);
            size_t request_len = strcspn(buffer, "\r\n");
            size_t header_len = strlen(buffer) - (request_len + 2);

            printf("%ld\n\n", request_len);
            printf("%ld\n", header_len);
            printf("%ld\n", strlen(buffer));

            char *request = calloc(request_len + 1, sizeof(char));

            memcpy(request, buffer, request_len);
            printf("%s\n\n", request);
            method_len = strcspn(buffer, " ");

            if (method_len == buffer_len) {

                for(int i = 0; i < buffer_len; i++){
                    if(buffer[i] == '\r'){
                        if(buffer[i+1] == '\n'){
                            isHttp++;
                        }
                    }
                    if (isHttp == 2) {
                        send(client_fd, "HTTP/1.1 400\r\n\r\n", status_len, 0);
                    }
                }


            } else {
                memcpy(method, buffer, method_len);
                method[method_len] = '\0';
                buffer += method_len + 1;
                printf("%s\n\n", buffer);

                uri_len = strcspn(buffer, " ");
                char *uri = calloc(uri_len + 1, sizeof(char));
                if (uri_len != strlen(buffer)) {
                    memcpy(uri, buffer, uri_len);
                    uri[uri_len] = '\0';
                    buffer += uri_len + 1;
                }

                size_t version_len = strcspn(buffer, "\r\n");
                char *version = calloc(version_len + 1, sizeof(char));
                if (version_len != strlen(buffer)) {
                    memcpy(version, buffer, version_len);
                    version[version_len] = '\0';
                    buffer += version_len + 2;
                }
                memcpy(buffer, packet, strlen(packet));
                printf("test buffer: %ld\n", strlen(buffer));

                while(buffer[0] != '\r' ||  buffer[1] != '\n'){
                    size_t header_name_len = strcspn(buffer, " ");
                    buffer += header_name_len + 1;
                    size_t  header_value_len = strcspn(buffer, "\r\n");
                    if(strlen(buffer) == header_value_len + 2){
                        buffer += header_value_len + 2;
                    }
                }
                if (memcmp(method, "GET", strlen("GET")) == 0) {
                    send(client_fd, "HTTP/1.1 404\r\n\r\n", status_len, 0);
                }
                else if (memcmp(method, "HEAD", strlen("HEAD")) == 0) {
                    send(client_fd, "HTTP/1.1 501\r\n\r\n", status_len, 0);
                } else {
                    send(client_fd, "HTTP/1.1 400\r\n\r\n", status_len, 0);
                }*/

/*recv_Bytes = recv(client_fd, buffer, MAXLINE, 0);
if(recv_Bytes < 0){
    error("recv error\n");
}*/


        /*if(checkPaket(buffer) == 2){
            write(client_fd, "Reply\r\n\r\n", 10);

        }*/


        /*if(recv_Bytes == 0){
            error("client is disconnected");
        }
        if(checkPaket(buffer) == 2){
            write(client_fd, "Reply\r\n\r\n", 10);

        }*/



    /*client_addr_len = sizeof(client_addr);
    accepted_sockfd = accept(sockfd, (struct sockaddr *) &client_addr, &client_addr_len);
    if(accepted_sockfd < 0){
        error("Error on Accept");
    }
    while(1){
        nachricht = read(accepted_sockfd, buffer, 255);
        if(nachricht<0){
            error("Error on reading!");
        }
        printf("client: %s\n", buffer);
        bzero(buffer, 255);
        fgets(buffer, 255, stdin); //reading bytes from string

        nachricht = write(accepted_sockfd, buffer, strlen(buffer));
        if(nachricht<0){
            error("error on writing!");
        }
        int cmp = strncmp("Bye", buffer, 3);
        if(cmp == 0){
            break;
        }
    }
    close(accepted_sockfd);
    close(sockfd);*/

    //return printf("%d\n", client_addr_len);
