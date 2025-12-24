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

    void parseQuery(const string& qStr, string& cmd, string& name, string& key, string& val);

public:
    Database();
    ~Database();

    void loadData(const string& path);
    void saveData(const string& path);
    
    string executeQuery(string queryStr);
};

#endif