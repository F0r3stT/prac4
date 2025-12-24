#include "stat_sender.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>

using namespace std;


void StatSender::sendStat(const string& statHost, int statPort, 
                          const string& ip, const string& originalUrl, 
                          const string& shortCode, long timestamp) {
    
     cout << "Попытка отправить статистику на " << statHost << ":" << statPort << "..." << endl;
    int sock = socket(AF_INET, SOCK_STREAM, 0); //создать сокет
    if (sock < 0) 
        cout << "Ошибка: Не удалось создать сокет для статистики" << endl;
        return; 

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr)); //зануление сокета
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(statPort);

    //СТРОКА В бинарный айпишник
    if (inet_pton(AF_INET, statHost.c_str(), &serv_addr.sin_addr) <= 0) {
        cout << "Ошибка: Неверный IP адрес статистики" << endl;
        close(sock);
        return;
    }

    //tcp соединение с сервисом статистики
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        cout << "Не удалось подключиться к сервису статистики";
        close(sock);
        return;
    }

    //айпи|урл|код|время
    string body = ip + "|" + originalUrl + "|" + shortCode + "|" + to_string(timestamp);
    
    //post запрос
    stringstream request;
    request << "POST / HTTP/1.1\r\n";
    request << "Host: " << statHost << "\r\n";
    request << "Content-Length: " << body.length() << "\r\n";
    request << "Connection: close\r\n\r\n";
    request << body;

    string reqStr = request.str();
    send(sock, reqStr.c_str(), reqStr.length(), 0); //отправляем запрос

    cout << "Статистика успешно отправлена" << endl;
    close(sock);
}