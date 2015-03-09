#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sstream>
#include <vector>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

using namespace std;

    const char *response_200 = "HTTP/1.1 200 OK\nContent-Type: text/html; charset=utf-8\n\n<html><body><i>Hello!</i></body></html>";
    const char *response_400 = "HTTP/1.1 400 Bad Request\nContent-Type: text/html; charset=utf-8\n\n<html><body><i>Bad Request!</i></body></html>";
    const char *response_404 = "HTTP/1.1 404 Not Found\nContent-Type: text/html; charset=utf-8\n\n<html><body><i>Not Found!</i></body></html>";

void *handle_request(void *pcliefd) 
{
    int cliefd = *(int*)pcliefd;
    delete (int *)pcliefd;

    ssize_t n;
    char buffer[255];
    const char *response;

    n = recv(cliefd, buffer, sizeof(buffer), 0);
    if(n < 0) {
        perror("recv()");
        return 0;
    }

    buffer[n] = 0;
    //printf("recv() %s\n", buffer);

    response = response_400;

    string s(buffer), token;
    istringstream ss(s);
    vector<string> token_list;
    for(int i = 0; i < 3 && ss; i++) {
        ss >> token;
        //printf("token %d %s\n", i, token.c_str());
        token_list.push_back(token);
    } 

    if(token_list.size() == 3 
            && token_list[0] == "GET" 
            && token_list[2].substr(0, 4) == "HTTP") {
        if(token_list[1] == "/index.html") {
            response = response_200;
        } else {
            response = response_404;
        }
    }

    n = write(cliefd, response, strlen(response));
    if(n < 0) {
        perror("write()"); 
        return 0;
    }

    close(cliefd);
    return 0;
}

int main(int argc, const char *argv[])
{
    int sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in servaddr;
    pthread_t thread;

    if(sockfd < 0) {
        perror("socket() error");
        exit(EXIT_FAILURE); 
    }

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8080);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind()");
        exit(EXIT_FAILURE); 
    }
    

    if(listen(sockfd, 1000) < 0) {
        perror("listen()");
        exit(EXIT_FAILURE); 
    }


    struct sockaddr_storage clieaddr;
    int cliefd;
    char s[INET_ADDRSTRLEN];
    socklen_t cliesize;

    while(true) {

        cliesize = sizeof(clieaddr);
        cliefd = accept(sockfd, (struct sockaddr *)&clieaddr, &cliesize);
        if(cliefd < 0) {
            perror("accept()");
            continue;
        }

        inet_ntop(clieaddr.ss_family, (void *)&((struct sockaddr_in *)&clieaddr)->sin_addr, s, sizeof(s));
        printf("accept() %s\n", s);


        int *pcliefd = new int;
        *pcliefd = cliefd;
        if(true) {
            if(pthread_create(&thread, 0, handle_request, pcliefd) < 0) {
                perror("pthread_create()");
            } 
        } else {
            handle_request(pcliefd);
        }
    }

    return 0;
}
