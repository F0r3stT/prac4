#include "server.h"
#include "db.h"         
#include <iostream>
#include <cstring>      
#include <string>       
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>  
#include <unistd.h>     
#include <signal.h>     

using namespace std;

//указатель для обработчика сигналов
SimpleServer* globalServer = nullptr;

void signalHandler(int signum) {
    cout << "\nВыключаем сервер..." << endl;
    if (globalServer != nullptr) {
        globalServer->stop();
    }
    exit(0); 
}

SimpleServer::SimpleServer(const string& dbFile, int p) {
    this->port = p;
    this->filename = dbFile;
    this->db = new Database(); 
    this->serverSocket = -1;
    this->isRunning = false;
}

SimpleServer::~SimpleServer() {
    stop();     
    if (db != nullptr) {
        delete db;
        db = nullptr;
    }
}

bool SimpleServer::start() {
    try {
        db->loadData(this->filename);
        cout << "БД: Данные загружены из " << this->filename << endl;
    } catch (...) {
        cout << "БД: Ошибка чтения файла, будет создан новый." << endl;
    }

    //создание сокета
    this->serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (this->serverSocket < 0) {
        cout << "Ошибка создания сокета" << endl;
        return false;
    }

    //разрешение повторного использования порта
    int opt = 1;
    setsockopt(this->serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    
    serverAddr.sin_family = AF_INET;           
    serverAddr.sin_addr.s_addr = INADDR_ANY;   
    serverAddr.sin_port = htons(this->port);   

    //бинд
    if (bind(this->serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        cout << "Ошибка bind (порт занят?)" << endl;
        close(this->serverSocket);
        return false;
    }

    //читаем
    if (listen(this->serverSocket, 5) < 0) {
        cout << "Ошибка listen" << endl;
        close(this->serverSocket);
        return false;
    }

    this->isRunning = true;
    globalServer = this;
    
    signal(SIGINT, signalHandler);

    cout << "DB Server запущен на порту " << this->port << endl;
    return true;
}

void SimpleServer::stop() {
    if (!isRunning) return;
    
    this->isRunning = false;

    if (this->serverSocket >= 0) {
        close(this->serverSocket);
        this->serverSocket = -1;
    }
    
    //ждем завершения потоков 
    for (size_t i = 0; i < clientThreads.size(); i++) {
        if (clientThreads[i].joinable()) {
            clientThreads[i].detach(); //отпускаем потоки
        }
    }
    clientThreads.clear();

    // Сохранение при выходе
    if (this->db != nullptr) {
        cout << "Сохранение данных..." << endl;
        this->db->saveData(this->filename);
    }
}

void SimpleServer::run() {
    if (!start()) 
        return;

    while (this->isRunning) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        
        int clientSocket = accept(this->serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        
        if (clientSocket < 0) {
            //если сервер остановлен, accept вернет ошибку
            if (this->isRunning) 
                cout << "Ошибка accept" << endl;
                    continue; 
        }

        //запуск потока для клиента
        try {
            thread newThread(&SimpleServer::handleClient, this, clientSocket, clientAddr);
            clientThreads.push_back(move(newThread));
        } catch (...) {
            cout << "Ошибка создания потока" << endl;
            close(clientSocket);
        }
    }
}

void SimpleServer::handleClient(int clientSocket, struct sockaddr_in clientAddr) {
    char buffer[4096]; 
    
    while (this->isRunning) {
        memset(buffer, 0, sizeof(buffer));

        int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) 
            break; 

        string request = string(buffer);

        //чистка мусора в конце строки
        while (!request.empty()) {
            char last = request.back();
            if (last == '\n' || last == '\r' || last == ' ') {
                request.pop_back(); 
            } else {
                break; 
            }
        }

        if (request == "") 
            continue;

        string response = "";

        this->dbMutex.lock();
        
        //выполняем запрос
        response = this->db->executeQuery(request);
        
        //сразу сохраняем 
        this->db->saveData(this->filename);
        
        this->dbMutex.unlock(); 

        if (response == "") response = "OK";
        response += "\n"; 

        int bytesSent = send(clientSocket, response.c_str(), response.length(), 0);
        if (bytesSent < 0) break;
    }

    close(clientSocket);
}

int main(int argc, char* argv[]) {
    string dbFile = "data.db";
    int port = 6379;

    if (argc > 1) 
        dbFile = argv[1];
    if (argc > 2) 
        port = atoi(argv[2]);

    SimpleServer server(dbFile, port);
    server.run();

    return 0;
}