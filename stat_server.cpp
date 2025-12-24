#include "stat_server.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>
#include <random>
#include <ctime>
#include <algorithm>
#include <iomanip>

using namespace std;

//конструктор, запоминающий порт, сокет, в состояние ещё не создан и обнуляет счётчик
StatServer::StatServer(int port, const string& dbHost, int dbPort) 
    : port(port), serverSocket(-1), dbClient(dbHost, dbPort), globalIdCounter(0) {}

StatServer::~StatServer() {
    if (serverSocket >= 0) close(serverSocket); // только валидный
}

string StatServer::generateId() {
    random_device dev; //случайный источник
    mt19937 generator(dev()); // генератор псевдослучайных чисел
    uniform_int_distribution<int> dist(0, 99999999);
    return to_string(dist(generator)); //строка в число
}

// Загрузка и парсинг всех данных из БД
vector<StatRecord> StatServer::loadAllRecords() {
    vector<StatRecord> records;
    //запрос таблицы statistics
    const string raw = dbClient.getAll(tableName);

    int st = 0;

    while (st < raw.size()) {
        int end = raw.find(';', st); //ищет конец фрагмента
        if (end == string::npos) //записи в форме id=val;id=val..
            end = raw.size();

        string segment = raw.substr(st, end - st); //сдвигаем и пропускаем ;
        st = end + 1;

        if (segment.empty()) 
            continue;

        int eq = segment.find('='); //разделитель
        if (eq == string::npos) 
            continue;

        string value = segment.substr(eq + 1); //берём val (без равно)

        int p1 = value.find('|');
        if (p1 == string::npos) 
            continue;
        int p2 = value.find('|', p1 + 1);
        if (p2 == string::npos) 
            continue;
        int p3 = value.find('|', p2 + 1);
        if (p3 == string::npos) 
            continue; //если 3 палки нет 4 поле время

        StatRecord rec;
        rec.ip = value.substr(0, p1); //вырезаем значение от разделителей
        rec.originalUrl = value.substr(p1 + 1, p2 - (p1 + 1));
        rec.shortCode = value.substr(p2 + 1, p3 - (p2 + 1));
        string timeVal = value.substr(p3 + 1);

        try {
            //время в число
            rec.timestamp = stol(timeVal);
        } catch (...) {
            rec.timestamp = 0;
        }

        records.push_back(rec);
    }

    return records;
}


// Преобразование времени в интервал (HH:MM-HH:MM)
string StatServer::formatTimeInterval(long timestamp) {
    time_t t = (time_t)timestamp;
    struct tm* tm_info = localtime(&t); //перевод в локально время
    
    char buffer[20];
    //время с обрезанными секундами
    strftime(buffer, 20, "%H:%M", tm_info);
    string st = string(buffer);
    
    //конец минуты
    t += 60;
    tm_info = localtime(&t);
    strftime(buffer, 20, "%H:%M", tm_info);
    string end = string(buffer);
    
    //интервал
    return st + "-" + end;
}

//парсер для извлечения списка изменения
vector<string> StatServer::parseDimensions(const string& jsonBody) {
    vector<string> dims;
    size_t stPos = jsonBody.find("[");
    size_t endPos = jsonBody.find("]");
    
    if (stPos == string::npos || endPos == string::npos) {
        //сортировка
        return {"URL", "SourceIP", "TimeInterval"};
    }
    
    string content = jsonBody.substr(stPos + 1, endPos - stPos - 1);
    stringstream ss(content);
    string segment;
    
    while(getline(ss, segment, ',')) {
        //удаляем кавычки и пробелы
        string clean;
        for (char c : segment) {
            if (c != '"' && c != ' ' && c != '\n' && c != '\r') {
                clean += c;
            }
        }
        if (!clean.empty()) dims.push_back(clean);
    }
    return dims;
}

// Рекурсивная функция группировки
void StatServer::buildTreeRecursive(ReportNode* node, const vector<StatRecord>& records, const vector<string>& dimensions, int dimIndex) {
    // Базовый случай: кончились измерения
    node->count = records.size(); // Количество записей в этом узле
    if (dimIndex >= dimensions.size() || records.empty()) {
        return;
    }

    string currentDim = dimensions[dimIndex];
    
    // Группируем записи по значению текущего измерения
    map<string, vector<StatRecord>> groups;
    
    for (const auto& rec : records) {
        string key;
        if (currentDim == "URL") {
            key = rec.originalUrl + " (" + rec.shortCode + ")";
        } else if (currentDim == "SourceIP") {
            key = rec.ip;
        } else if (currentDim == "TimeInterval") {
            key = formatTimeInterval(rec.timestamp);
        }
        groups[key].push_back(rec);
    }

    // Для каждой группы создаем дочерний узел и запускаем рекурсию
    for (const auto& entry : groups) {
        string groupKey = entry.first;
        const vector<StatRecord>& groupRecords = entry.second;
        
        ReportNode* child = new ReportNode();
        child->dimensionType = currentDim;
        child->value = groupKey;
        // PID и ID проставим позже при формировании JSON
        
        node->child[groupKey] = child;
        
        // Рекурсия: углубляемся на уровень ниже
        buildTreeRecursive(child, groupRecords, dimensions, dimIndex + 1);
    }
}

//превращаем дерево в JSON
void StatServer::flattenTreeToJson(ReportNode* node, stringstream& jsonStream) {
    // Если это не корень (корень фиктивный), выводим данные
    if (node->id != 0) {
        jsonStream << "{";
        jsonStream << "Id: " << node->id << ",";
        
        if (node->pid == 0) //корень
            jsonStream << "Pid: null,";
        else jsonStream << "Pid: " << node->pid << ",";

        //заполняем поля в зависимости от типа уровня
        if (node->dimensionType == "URL") 
            jsonStream << "URL: " << node->value << ",";
        else 
            jsonStream << "URL: null,";
            
        if (node->dimensionType == "SourceIP") 
            jsonStream << "SourceIP: " << node->value << "\",";
        else 
            jsonStream << "\"SourceIP\": null,";
            
        if (node->dimensionType == "TimeInterval") 
            jsonStream << "\"TimeInterval\": \"" << node->value << "\",";
        else 
            jsonStream << "\"TimeInterval\": null,";
            
        jsonStream << "\"Count\": " << node->count;
        jsonStream << "}";
    }

    //обрабатываем детей (чтоб запятых не было)
    bool isFirst = (node->id == 0); 
    
    for (auto const& [key, child] : node->child) {
        //присвоение айди
        globalIdCounter++;
        child->id = globalIdCounter; //номер
        if (node->id == 0)
            child->pid = 0; //корень, ребёнок родитель
        else
            child->pid = node->id;
        if (!isFirst) {
            jsonStream << ",";
        }
        if (node->id != 0) {
        }
        
        
    }
}

//сбор узлов в список
void collectNodes(ReportNode* node, vector<ReportNode*>& result) {
    if (node->id != 0) {
        result.push_back(node);
    }
    for (auto const& [key, child] : node->child) {
    //назначение айди
        collectNodes(child, result);
    }
}


void StatServer::handleClient(int clientSocket) {
    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    
    size_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0); //читаем запрос
    if (bytesRead <= 0) 
    { 
        close(clientSocket); 
            return; 
    }

    string request(buffer);
    string body;
    int bodyPos = request.find("\r\n\r\n"); //конец http заголовок
    if (bodyPos != string::npos) 
        body = request.substr(bodyPos + 4);

    stringstream ss(request);
    string meth, path;
    ss >> meth >> path;

    string response;

    //сохраняем событие
    if (meth == "POST" && path == "/") {
        if (!body.empty()) {
            dbClient.saveUrl(tableName, generateId(), body);
            response = "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nOK";
        } else {
            response = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\nEmpty";
        }
    }
    //генерация отчёты
    else if (meth == "POST" && path == "/report") {
        try {
            //получение списка измерений из запроса
            vector<string> dimensions = parseDimensions(body);
            if (dimensions.empty()) 
                dimensions = {"URL", "SourceIP", "TimeInterval"};

            //загружаем все данные
            vector<StatRecord> records = loadAllRecords();

            //дерево
            ReportNode root;
            root.id = 0; 
            buildTreeRecursive(&root, records, dimensions, 0);

            //подставляем айди и собираем список
            globalIdCounter = 0;
            vector<ReportNode*> flatList;
            
            //очередь для обхода в ширину
            auto assignIds = [&](auto&& self, ReportNode* n) -> void {
                 for (auto const& [key, child] : n->child) {
                     globalIdCounter++;
                     child->id = globalIdCounter;
                     // Если n - корень (id=0), то у ребенка pid = null (в логике вывода)
                     child->pid = n->id; 
                     
                     flatList.push_back(child);
                     self(self, child);
                 }
            };
            assignIds(assignIds, &root);


            stringstream json;
            json << "[";
            for (size_t i = 0; i < flatList.size(); ++i) {
                ReportNode* node = flatList[i];
                if (i > 0) json << ",";
                
                json << "{";
                json << "\"Id\": " << node->id << ",";
                
                if (node->pid == 0) json << "\"Pid\": null,";
                else json << "\"Pid\": " << node->pid << ",";
                
                // Заполняем поля
                string u = "null", s = "null", t = "null";
                if (node->dimensionType == "URL") 
                    u = "\"" + node->value + "\"";
                if (node->dimensionType == "SourceIP") 
                    s = "\"" + node->value + "\"";
                if (node->dimensionType == "TimeInterval") 
                    t = "\"" + node->value + "\"";
                
                json << "\"URL\": " << u << ",";
                json << "\"SourceIP\": " << s << ",";
                json << "\"TimeInterval\": " << t << ",";
                json << "\"Count\": " << node->count;
                json << "}";
            }
            json << "]";

            response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n" + json.str();
            
        } catch (const exception& e) {
            response = "HTTP/1.1 500 Error\r\nConnection: close\r\n\r\n" + string(e.what());
        }
    }
    else {
        response = "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\nNot Found";
    }

    send(clientSocket, response.c_str(), response.length(), 0);
    close(clientSocket);
}

void StatServer::run() {
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) 
        return;
    int opt = 1;
    //разрешение на переиспользование пота
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr;
    addr.sin_family = AF_INET; 
    addr.sin_addr.s_addr = INADDR_ANY; //слушать на всех сетевых иньерфейсах
    addr.sin_port = htons(port);

    if (bind(serverSocket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        cout << "Ошибка bind порта " << port << endl;
        return;
    }
    if (listen(serverSocket, 10) < 0) 
        return;

    cout << "Сервер статистики запущен на порту " << port << endl;
    while (true) {
        struct sockaddr_in clientAddr;
        socklen_t len = sizeof(clientAddr);
        //ждёт подключения через accept
        int clientSock = accept(serverSocket, (struct sockaddr*)&clientAddr, &len);
        if (clientSock >= 0) 
            handleClient(clientSock);
    }
}