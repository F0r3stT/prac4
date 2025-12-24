#ifndef C_SERVER_H
#define C_SERVER_H

#include <string>
#include "shortener.h" 

using namespace std;

class HttpServer {
private:
    int port;
    int serverSocket; //файловый дескриптор
    
    UrlShortener& shortener; //ссылка на логику сокращателя

    void handleClient(int clientSocket);

public:
    HttpServer(int port, UrlShortener& shortener);
    ~HttpServer();
    void run();
};

#endif