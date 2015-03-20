#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sstream>
#include <vector>
#include <string>
#include <queue>

#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

template <class T>
class SafeQueue {
    mutex m;
    queue<T> q;
    condition_variable cv;
public:
    void push(T v) {
        lock_guard<mutex> lk(m);
        q.push(move(v));
        cv.notify_one();
    }

    bool try_pop(T &v) {
        lock_guard<mutex> lk(m);
        if(q.empty()) {
            return false;
        }
        v = move(q.front());
        q.pop();
        return true;
    }
private:
    /* data */
};

class Pool {
public:
    typedef function<void()> Task;

    Pool() : done(false) {
        unsigned c = thread::hardware_concurrency();
        //unsigned c = 1000;
        printf("Pool() with %d threads\n", c);
        for(unsigned i = 0; i < c; ++i) {
            vt.push_back(thread(&Pool::worker, this));
        }
    }

    ~Pool() {
        done = true;
    }

    void submit(Task t) {
        sq.push(t);
    }
private:
    atomic_bool done;
    SafeQueue<Task> sq;
    vector<thread> vt;

    void worker() {
        while(!done) {
            Task t;
            if(sq.try_pop(t)) {
                t();
            } else {
                this_thread::yield();
            }
        }
    }
};

    const char *response_200 = "HTTP/1.1 200 OK\nConnection: close\nContent-Type: text/html; charset=utf-8\n\n<html><body><i>Hello!</i></body></html>";
    const char *response_400 = "HTTP/1.1 400 Bad Request\nConnection: close\nContent-Type: text/html; charset=utf-8\n\n<html><body><i>Bad Request!</i></body></html>";
    const char *response_404 = "HTTP/1.1 404 Not Found\nConnection: close\nContent-Type: text/html; charset=utf-8\n\n<html><body><i>Not Found!</i></body></html>";

void handle_request(int cliefd) 
{
    ssize_t n;
    char buffer[255];
    const char *response;

    n = recv(cliefd, buffer, sizeof(buffer), 0);
    if(n < 0) {
        perror("recv()");
        return;
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

    n = send(cliefd, response, strlen(response), 0);
    if(n < 0) {
        perror("write()"); 
        return;
    }

    close(cliefd);
    return;
}

int main(int argc, const char *argv[])
{
    int sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in servaddr;

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

    Pool p;

    while(true) {

        cliesize = sizeof(clieaddr);
        cliefd = accept(sockfd, (struct sockaddr *)&clieaddr, &cliesize);
        if(cliefd < 0) {
            perror("accept()");
            continue;
        }

        /*
         * std::thread::hardware_concurrency
         */

        inet_ntop(clieaddr.ss_family, (void *)&((struct sockaddr_in *)&clieaddr)->sin_addr, s, sizeof(s));
        printf("accept() %s\n", s);

        p.submit(bind(handle_request, cliefd));
    }

    return 0;
}
