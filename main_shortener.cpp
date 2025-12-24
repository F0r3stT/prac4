#include "c_server.h"
#include "shortener.h"
#include <iostream>
#include <cstdlib> // для atoi

using namespace std;

int main(int argc, char* argv[]) {
    int dbPort = 6379;
    string dbHost = "127.0.0.1";
    int serverPort = 8080;

    //./shortener_app <порт> <порт бд>
    if (argc > 1) {
        serverPort = atoi(argv[1]);
    }
    if (argc > 2) {
        dbPort = atoi(argv[2]);
    }

    cout << "Инициализация сервиса сокращения ссылок..." << endl;
    cout << "HTTP Сервер будет запущен на порту: " << serverPort << endl;
    cout << "Подключение к БД: " << dbHost << ":" << dbPort << endl;

    try {
        // Создаем логику сокращателя, подключаясь к БД
        UrlShortener shortener(dbHost, dbPort);

        // Создаем HTTP сервер
        HttpServer server(serverPort, shortener);
        
        // Запускаем сервер
        server.run();
    } 
    catch (const exception& e) {
        cout << "Критическая ошибка запуска: " << e.what() << endl;
        return 1;
    }

    return 0;
}