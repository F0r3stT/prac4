#include <iostream>
#include <string>
#include <cstring>      
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>  
#include <unistd.h>     

using namespace std;

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cout << "Нужно ввести: ./client <IP> <PORT>" << endl;
        return 1;
    }

    const char* ipStr = argv[1];
    int port = atoi(argv[2]); 

    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        cout << "Ошибка создания сокета";
        return 1;
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr)); 
    serverAddr.sin_family = AF_INET;           
    serverAddr.sin_port = htons(port);          
    
    if (inet_pton(AF_INET, ipStr, &serverAddr.sin_addr) <= 0) {
        cout << "Неверный IP адрес" << endl;
        return 1;
    }
    
    cout << "Подключение к " << ipStr << ":" << port << endl;
    
    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        cout << "Не удалось подключиться" << endl;
        close(clientSocket);
        return 1;
    }

    cout << "Успешно. Введите команду (например: HGETALL urls):" << endl;

    char buffer[4096]; 
    
    while (true) {
        cout << "> ";
        string message;
        if (!getline(cin, message) || message == "exit") break;
        if (message == "") continue; 
        
        message += "\n";
        send(clientSocket, message.c_str(), message.length(), 0);

        memset(buffer, 0, sizeof(buffer));
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) break;

        cout << "Ответ: " << buffer;
    }

    close(clientSocket);
    return 0;
}