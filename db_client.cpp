#include "db_client.h"
#include <iostream>
#include <arpa/inet.h>
#include <netdb.h>

using namespace std;

//конструктор клиента базы данных
DatabaseClient::DatabaseClient(const string& host, int port) 
    : host(host), port(port), sockfd(-1) { //сохраняем хост и порт
    

    memset(&serverAddr, 0, sizeof(serverAddr)); //обнуление
    serverAddr.sin_family = AF_INET; //айпи адрес
    serverAddr.sin_port = htons(port); //в байты
    
    //преобразование IP адреса из текста в бинарный вид
    if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) <= 0) {
        struct hostent* server = gethostbyname(host.c_str());
        if (server == nullptr) {
            throw runtime_error("Не удалось найти хост БД: " + host);
        }
        memcpy(&serverAddr.sin_addr.s_addr, server->h_addr, server->h_length);
    }
}

DatabaseClient::~DatabaseClient() {
    closeConnection();
}

//установка соединения с сервером
bool DatabaseClient::connectToServer() {
    if (sockfd >= 0) {
        return true; 
    }
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0); //создание сокета
    if (sockfd < 0) 
        return false;
    
    //подключение к адресу
    if (connect(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        close(sockfd);
        sockfd = -1;
        return false;
    }
    
    return true;
}

void DatabaseClient::closeConnection() {
    if (sockfd >= 0) {
        close(sockfd);
        sockfd = -1;
    }
}

//выполнение команды на сервере БД
string DatabaseClient::executeCommand(const string& command) {
    if (!connectToServer()) {
        throw runtime_error("Нет соединения с сервером БД");
    }
    
    string cmd = command + "\n";
    
    if (send(sockfd, cmd.c_str(), cmd.length(), 0) < 0) {
        closeConnection();
        throw runtime_error("Ошибка отправки команды в БД");
    }
    
    char buffer[40961]; //читаем данные из большого буфера
    memset(buffer, 0, sizeof(buffer));
    
    size_t bytesRead = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead <= 0) {
        closeConnection();
        //переподключение
        if (connectToServer()) {
             return executeCommand(command);
        }
        throw runtime_error("Сервер БД разорвал соединение");
    }
    
    string response(buffer);
    
    //очистка от лишних символов переноса строки
    while (!response.empty() && (response.back() == '\n' || response.back() == '\r')) {
        response.pop_back();
    }
    
    return response;
}

//сохраняем пары ключ значение в бд
bool DatabaseClient::saveUrl(const string& tableName, const string& key, const string& value) {
    string cmd = "HSET " + tableName + " " + key + " " + value;
    string response = executeCommand(cmd);
    
    return response.find("ERROR") == string::npos && response.find("ОШИБКА") == string::npos;
}

//получение ссылки , по urls таблице
string DatabaseClient::getUrl(const string& shortCode) {
    string cmd = "HGET urls " + shortCode;
    string response = executeCommand(cmd);
    
    if (response.find("ОШИБКА") != string::npos || response.find("ERROR") != string::npos) {
        return "";
    }

    if (response.empty() || response == "EMPTY") {
        return "";
    }
    
    return response;
}

//получение данных из всей таблицы
string DatabaseClient::getAll(const string& tableName) {
    string cmd = "HGETALL " + tableName;
    return executeCommand(cmd);
}