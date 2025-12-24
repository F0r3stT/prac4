#ifndef DB_H
#define DB_H

#include <iostream>
#include <string>
#include <fstream>
#include <unordered_map> 
#include <sstream>

using namespace std;

class Database {
private:
    
    unordered_map<string, unordered_map<string, string>> tables;
    //библоитечна бд чтобы основной операцией было найти по ключу, вставтиь удалить

    void parseQuery(const string& qStr, string& cmd, string& name, string& key, string& val);

public:
    // Конструктор
    Database();
    ~Database();

    // Загрузка данных из файла
    void loadData(const string& path);
    // Сохранение данных в файл
    void saveData(const string& path);
    
    // Главный метод выполнения команд (HSET, HGET, HGETALL)
    string executeQuery(string queryStr);
};

#endif