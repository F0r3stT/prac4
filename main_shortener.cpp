#include "c_server.h"
#include "shortener.h"
#include <iostream>

using namespace std;

int main() {
    // Порт, на котором работает база данных (запущенная через ./db_server)
    int dbPort = 6379;
    string dbHost = "127.0.0.1";

    // Порт, на котором будет работать наш HTTP сервер (сокращатель)
    int serverPort = 8080;

    cout << "Инициализация сервиса сокращения ссылок..." << endl;

    try {
        // Создаем логику сокращателя, подключаясь к БД
        UrlShortener shortener(dbHost, dbPort);

        // Создаем HTTP сервер, передавая ему логику сокращателя
        HttpServer server(serverPort, shortener);
        
        // Запускаем сервер (блокирует выполнение)
        server.run();
    } 
    catch (const exception& e) {
        cout << "Критическая ошибка запуска: " << e.what() << endl;
        return 1;
    }

    return 0;
}