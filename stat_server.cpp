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
#include <chrono> 

using namespace std;

StatServer::StatServer(int port, const string& dbHost, int dbPort) 
    : port(port), serverSocket(-1), dbClient(dbHost, dbPort), globalIdCounter(0) {}

StatServer::~StatServer() {
    if (serverSocket >= 0) close(serverSocket); 
}

string StatServer::generateId() {
    auto now = chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    long long millis = chrono::duration_cast<chrono::milliseconds>(duration).count();

    random_device dev; 
    mt19937 generator(dev()); 
    uniform_int_distribution<int> dist(0, 9999);
    
    return to_string(millis) + "_" + to_string(dist(generator)); 
}

vector<StatRecord> StatServer::loadAllRecords() {
    vector<StatRecord> records;
    const string raw = dbClient.getAll(tableName);

    int st = 0;
    while (st < raw.size()) {
        int end = raw.find(';', st); 
        if (end == string::npos) 
            end = raw.size();

        string segment = raw.substr(st, end - st); 
        st = end + 1;

        if (segment.empty()) continue;

        int eq = segment.find('='); 
        if (eq == string::npos) continue;

        string value = segment.substr(eq + 1); 

        int p1 = value.find('|');
        if (p1 == string::npos) continue;
        int p2 = value.find('|', p1 + 1);
        if (p2 == string::npos) continue;
        int p3 = value.find('|', p2 + 1);
        if (p3 == string::npos) continue; 

        StatRecord rec;
        rec.ip = value.substr(0, p1); 
        rec.originalUrl = value.substr(p1 + 1, p2 - (p1 + 1));
        rec.shortCode = value.substr(p2 + 1, p3 - (p2 + 1));
        string timeVal = value.substr(p3 + 1);

        try {
            rec.timestamp = stol(timeVal);
        } catch (...) {
            rec.timestamp = 0;
        }

        records.push_back(rec);
    }
    return records;
}

string StatServer::formatTimeInterval(long timestamp) {
    time_t t = (time_t)timestamp;
    struct tm* tm_info = localtime(&t); 
    
    char buffer[20];
    strftime(buffer, 20, "%H:%M", tm_info);
    string st = string(buffer);
    
    t += 60;
    tm_info = localtime(&t);
    strftime(buffer, 20, "%H:%M", tm_info);
    string end = string(buffer);
    
    return st + "-" + end;
}

vector<string> StatServer::parseDimensions(const string& jsonBody) {
    vector<string> dims;
    size_t stPos = jsonBody.find("[");
    size_t endPos = jsonBody.find("]");
    
    if (stPos == string::npos || endPos == string::npos) {
        return {"URL", "SourceIP", "TimeInterval"};
    }
    
    string content = jsonBody.substr(stPos + 1, endPos - stPos - 1);
    stringstream ss(content);
    string segment;
    
    while(getline(ss, segment, ',')) {
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

void StatServer::buildTreeRecursive(ReportNode* node, const vector<StatRecord>& records, const vector<string>& dimensions, int dimIndex) {
    node->count = records.size(); 
    if (dimIndex >= dimensions.size() || records.empty()) {
        return;
    }

    string currentDim = dimensions[dimIndex];
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

    for (const auto& entry : groups) {
        string groupKey = entry.first;
        const vector<StatRecord>& groupRecords = entry.second;
        
        ReportNode* child = new ReportNode();
        child->dimensionType = currentDim;
        child->value = groupKey;
        
        node->child[groupKey] = child;
        
        buildTreeRecursive(child, groupRecords, dimensions, dimIndex + 1);
    }
}

void StatServer::flattenTreeToJson(ReportNode* root, stringstream& jsonStream) {
    globalIdCounter = 0;
    vector<ReportNode*> flatList;

    auto assignIds = [&](auto&& self, ReportNode* n) -> void {
         for (auto const& [key, child] : n->child) {
             globalIdCounter++;
             child->id = globalIdCounter;
             child->pid = (n->id == 0) ? 0 : n->id; 
             
             flatList.push_back(child);
             self(self, child);
         }
    };
    
    assignIds(assignIds, root);

    // Генерация JSON
    jsonStream << "[";
    for (size_t i = 0; i < flatList.size(); ++i) {
        ReportNode* node = flatList[i];
        if (i > 0) jsonStream << ",";
        
        jsonStream << "{";
        jsonStream << "\"Id\": " << node->id << ",";
        
        if (node->pid == 0) jsonStream << "\"Pid\": null,";
        else jsonStream << "\"Pid\": " << node->pid << ",";
        
        string u = "null", s = "null", t = "null";
        if (node->dimensionType == "URL") 
            u = "\"" + node->value + "\"";
        if (node->dimensionType == "SourceIP") 
            s = "\"" + node->value + "\"";
        if (node->dimensionType == "TimeInterval") 
            t = "\"" + node->value + "\"";
        
        jsonStream << "\"URL\": " << u << ",";
        jsonStream << "\"SourceIP\": " << s << ",";
        jsonStream << "\"TimeInterval\": " << t << ",";
        jsonStream << "\"Count\": " << node->count;
        jsonStream << "}";
    }
    jsonStream << "]";
}

void StatServer::handleClient(int clientSocket) {
    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    
    size_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0); 
    if (bytesRead <= 0) { 
        close(clientSocket); 
        return; 
    }

    string request(buffer);
    string body;
    int bodyPos = request.find("\r\n\r\n"); 
    if (bodyPos != string::npos) 
        body = request.substr(bodyPos + 4);

    stringstream ss(request);
    string meth, path;
    ss >> meth >> path;

    string response;

    if (meth == "POST" && path == "/") {
        if (!body.empty()) {
            string uid = generateId();
            // Логируем сохранение
            cout << "[LOG] Save Stat: " << body << " -> ID: " << uid << endl;
            dbClient.saveUrl(tableName, uid, body);
            response = "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nOK";
        } else {
            response = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\nEmpty";
        }
    }
    else if (meth == "POST" && path == "/report") {
        try {
            cout << "[LOG] Report Request: " << body << endl;
            
            vector<string> dimensions = parseDimensions(body);
            if (dimensions.empty()) 
                dimensions = {"URL", "SourceIP", "TimeInterval"};

            vector<StatRecord> records = loadAllRecords();
            cout << "[LOG] Records found: " << records.size() << endl;

            ReportNode root;
            root.id = 0; 
            
            buildTreeRecursive(&root, records, dimensions, 0);

            stringstream json;
            flattenTreeToJson(&root, json);

            response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n" + json.str();
            
        } catch (const exception& e) {
            cout << "[ERR] " << e.what() << endl;
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
    if (serverSocket < 0) return;
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr;
    addr.sin_family = AF_INET; 
    addr.sin_addr.s_addr = INADDR_ANY; 
    addr.sin_port = htons(port);

    if (bind(serverSocket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        cout << "Ошибка bind порта " << port << endl;
        return;
    }
    if (listen(serverSocket, 10) < 0) return;

    cout << "Сервер статистики запущен на порту " << port << endl;
    while (true) {
        struct sockaddr_in clientAddr;
        socklen_t len = sizeof(clientAddr);
        int clientSock = accept(serverSocket, (struct sockaddr*)&clientAddr, &len);
        if (clientSock >= 0) 
            handleClient(clientSock);
    }
}