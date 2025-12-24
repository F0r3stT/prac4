#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <vector>
#include <thread> 
#include <mutex>  
#include <netinet/in.h> 

using namespace std;

class Database;

class SimpleServer {
private:
    int port;                  
    string filename;      
    Database* db;         
    
    int serverSocket;     
    bool isRunning;            
    
    //список потоков клиентов
    vector<thread> clientThreads;
    
    //мьютекс для синхронизации доступа к бд из разных потоков
    mutex dbMutex;

    void handleClient(int clientSocket, struct sockaddr_in clientAddr);

public:
    SimpleServer(const string& dbFile, int port = 6379);
    ~SimpleServer();

    bool start();
    
    void run();
    
    void stop();
};

#endif