#ifndef DATABASE_CLIENT_H
#define DATABASE_CLIENT_H

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>
#include <vector>

using namespace std;

class DatabaseClient {
private:
    string host;
    int port;
    int sockfd;
    struct sockaddr_in serverAddr;
    
    //подключение к сокету сервера БД
    bool connectToServer();
    //отправка команды
    string executeCommand(const string& command);
    
public:
    DatabaseClient(const string& host = "127.0.0.1", int port = 6379);
    ~DatabaseClient();
    
    //сохранить короткую ссылку 
    bool saveUrl(const string& tableName, const string& key, const string& value);
    
    //получить значение по ключу
    string getUrl(const string& key);

    //получить все значения из таблицы 
    string getAll(const string& tableName);
    
    void closeConnection();
};

#endif