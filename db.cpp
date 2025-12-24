#include "db.h"

using namespace std;

Database::Database() {
}

Database::~Database() {
}

//загрузка данных
void Database::loadData(const string& path) {
    ifstream file(path);
    if (!file.is_open()) return;

    string type, name, key, val;
    
    //считываем файл слово за словом
    while (file >> type >> name >> key >> val) {
        if (type == "HASH") {
            tables[name][key] = val;
        }
    }
    file.close();
}

//сохранение данных в файл
void Database::saveData(const string& path) {
    ofstream file(path);
    if (!file.is_open()) {
        cout << "Ошибка: не удалось открыть файл для записи" << endl;
        return;
    }

    //перебор всех таблиц в базе
    for (const auto& tablePair : tables) {
        const string& tableName = tablePair.first;
        const auto& innerMap = tablePair.second;

        //перебор всех записей (ключ-значение) внутри таблицы
        for (const auto& entry : innerMap) {
            file << "HASH " << tableName << " " << entry.first << " " << entry.second << endl;
        }
    }

    file.close();
}

//парс строки запроса на компоненты
void Database::parseQuery(const string& qStr, string& cmd, string& name, string& key, string& val) {
    cmd = ""; name = ""; key = ""; val = "";
    
    stringstream stream(qStr);
    stream >> cmd;  
    stream >> name; 
    stream >> key;  
    stream >> val; 
}

//выполнение текстовой команды запроса
string Database::executeQuery(string queryStr) {
    string cmd, name, key, val;
    parseQuery(queryStr, cmd, name, key, val);

    if (cmd == "") return "";

    // HSET <имя_таблицы> <ключ> <значение>
    if (cmd == "HSET") {
        if (name == "" || key == "" || val == "") 
            return "ОШИБКА: Неверные аргументы для HSET";
        
        tables[name][key] = val;
        return "OK";
    }

    // HGET <имя_таблицы> <ключ>
    if (cmd == "HGET") {
        if (name == "" || key == "") 
            return "ОШИБКА: Неверные аргументы для HGET";

        if (tables.count(name) && tables[name].count(key)) {
            return tables[name][key];
        } else {
            return "ОШИБКА: Ключ не найден";
        }
    }

    //HGETALL имя
    if (cmd == "HGETALL") {
        if (name == "") 
            return "ОШИБКА: Неверные аргументы для HGETALL";

        if (tables.count(name) == 0) {
            return ""; // Таблица пуста или не существует
        }

        string result = "";
        //итерируемся по всем элементам таблицы
        for (const auto& entry : tables[name]) {
            result += entry.first + "=" + entry.second + ";";
        }
        return result;
    }

    if (cmd == "HDEL") {
        if (name == "" || key == "") 
            return "ОШИБКА: Неверные аргументы для HDEL";

        if (tables.count(name)) {
            tables[name].erase(key);
            if (tables[name].empty()) {
                tables.erase(name);
            }
            return "OK";
        }
        return "ОШИБКА: Таблица не найдена";
    }

    if (cmd == "DEBUG") {
        cout << " DEBUG DUMP " << endl;
        for (auto const& [tName, tMap] : tables) {
            cout << "Table: " << tName << endl;
            for (auto const& [k, v] : tMap) {
                cout << "  " << k << " -> " << v << endl;
            }
        }
        return "DUMP COMPLETE";
    }

    return "ОШИБКА: Неизвестная команда";
}