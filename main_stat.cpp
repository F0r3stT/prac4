#include "stat_server.h"
#include <iostream>
#include <string>

using namespace std;

int main(int argc, char* argv[]) {
    int port = 8082;
    int dbPort = 6379; 

    if (argc > 1) 
        port = atoi(argv[1]);

    cout << "Запуск сервиса статистики..." << endl;
    
    try {
        StatServer server(port, "127.0.0.1", dbPort);
        server.run();
    } catch (const exception& e) {
        cout << "Критическая ошибка: " << e.what() << endl;
    }

    return 0;
}