#include "c_server.h"
#include "stat_sender.h" 
#include <iostream>
#include <sstream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <ctime>
#include <arpa/inet.h>

using namespace std;

//Конструктор сервера
HttpServer::HttpServer(int port, UrlShortener& shortener) 
    : port(port), serverSocket(-1), shortener(shortener) {}

HttpServer::~HttpServer() {
    if (serverSocket >= 0) 
        close(serverSocket);
}

// Обработка одного http клиента
void HttpServer::handleClient(int clientSocket) {
    
    struct sockaddr_in addr; //переменная для записи адреса клиента
    socklen_t addr_size = sizeof(struct sockaddr_in); //хранит буфер для адреса
    getpeername(clientSocket, (struct sockaddr *)&addr, &addr_size); //берём адрес подключаемого клиента
    char* clientIp = inet_ntoa(addr.sin_addr); //перевод айпи в строку
    string ipStr(clientIp);

    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    
    size_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0); //читаем байты из соединения 4095
    if (bytesRead <= 0) {
        close(clientSocket);
        return;
    }
    
    string request(buffer);

    //поток строк, 
    istringstream iss(request); 
    string meth, path;
    iss >> meth >> path;
    
    string response;
    
    //post - для короткой ссылки
    if (meth == "POST" && path == "/") {
        string body;
        size_t bodyPos = request.find("\r\n\r\n");

        //берём тело запроса http
        if (bodyPos != string::npos) {
            body = request.substr(bodyPos + 4);
        }
        
        //удалять пробелы
        if (!body.empty() && body.back() == '\n') 
            body.pop_back();
        if (!body.empty() && body.back() == '\r') 
            body.pop_back();

        //пустой url
        if (body.empty()) {
            response = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\nURL is empty";
        } else {
            try {
                string code = shortener.shortenUrl(body); //сокращаем ссылку
                string fullShortUrl = "http://localhost:" + to_string(port) + "/" + code + "\n"; //добавляем к локалке
                
                response = "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/plain\r\n"
                           "Connection: close\r\n\r\n" + fullShortUrl + "\n";
                           
                cout << "сокращено: " << body << " -> " << code << endl;
            } 
            catch (const exception& e) {
                response = "HTTP/1.1 500 Internal Error\r\nConnection: close\r\n\r\n" + string(e.what());
            }
        }
    }
    //get - переход по ссылке
    else if (meth == "GET" && path.length() > 1 && path[0] == '/') { //проверяем, чтобы все было, все слеши работали
        string code = path.substr(1); //достать код 
        string originalUrl = shortener.getOriginalUrl(code); 
        
        if (!originalUrl.empty()) {
            long now = time(0); //время возвращения
            StatSender::sendStat("127.0.0.1", 8082, ipStr, originalUrl, code, now); //прсыоает статистику серверу, куда ведёт ссылка айпи время итд

            response = "HTTP/1.1 302 Found\r\n"
                       "Location: " + originalUrl + "\r\n"
                       "Connection: close\r\n\r\n";
            cout << "Редирект: " << code << " -> " << originalUrl << endl;
        } else {
            response = "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\nLink not found";
        }
    }
    else {
        response = "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\nNot Found";
    }

    send(clientSocket, response.c_str(), response.length(), 0); //отправка клиенту ответа
    close(clientSocket);
}

void HttpServer::run() {
    //AF_INET - адреса айпи4
    //SOCK_STREAM поток сокет
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        cout << "Ошибка создания соединения" << endl;
        return;
    }
    
    int opt = 1;
    //SOL_SOCKET - уровень сокета
    //SO_REUSEADDR - быстрый бинд на порт
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); //настройку на сокет ставим
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET; //все адреса ipv4
    addr.sin_addr.s_addr = INADDR_ANY; //слушаем на всех интерфейсах
    addr.sin_port = htons(port); //кладём порт в байтовый формат
    
    if (bind(serverSocket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        cout << "Ошибка бинда" << endl;
        return;
    }
    
    if (listen(serverSocket, 10) < 0) {
        cout << "Ошибка чтения" << endl;
        return;
    }
    
    cout << "HTTP Сервер запущен на порту " << port << endl;
    
    //цикл приёма клиентов
    while (true) {
        struct sockaddr_in clientAddr;
        socklen_t len = sizeof(clientAddr); //размер буфера
        //берём запрос из очереди покдлючений
        int clientSock = accept(serverSocket, (struct sockaddr*)&clientAddr, &len);
        
        if (clientSock >= 0) {
            handleClient(clientSock);
        }
    }
}