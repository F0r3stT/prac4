#ifndef STAT_SERVER_H
#define STAT_SERVER_H

#include <string>
#include <vector>
#include <map>
#include "db_client.h"

using namespace std;

struct StatRecord { //сырая запись
    string ip;
    string originalUrl;
    string shortCode;
    long timestamp;
};

//узел дерева для построения иерархии
struct ReportNode {
    int id;                 //айди узлов для отчёта
    int pid;                //айди родителя
    string dimensionType;   //тип группы
    string value;           //значение 
    int count;             
    map<string, ReportNode*> child; //дети по ключу значения
    
    ReportNode() : id(0), pid(0), count(0) {}
    ~ReportNode() {
        for(auto& [key, val] : child) { //деструктор 
            delete val;
        }
    }
};

class StatServer {
private:
    int port;
    int serverSocket;
    DatabaseClient dbClient;
    const string tableName = "statistics"; //таблица хранения
    
    //счётчик для генерации ID
    int globalIdCounter;

    void handleClient(int clientSocket);
    string generateId();

    //все записи из бд в сырую запись
    vector<StatRecord> loadAllRecords();
    string formatTimeInterval(long timestamp); //интервал времени
    vector<string> parseDimensions(const string& jsonBody);
    
    //дерево группировок
    void buildTreeRecursive(ReportNode* node, const vector<StatRecord>& records, const vector<string>& dimensions, int dimIndex);
    
    //преобразования дерева в JSon
    void flattenTreeToJson(ReportNode* node, stringstream& jsonStream);

public:
    StatServer(int port, const string& dbHost, int dbPort);
    ~StatServer();

    void run();
};

#endif