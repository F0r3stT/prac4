#include "shortener.h"
#include <random>
#include <iostream>

using namespace std;

const string charac = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

UrlShortener::UrlShortener(const string& dbHost, int dbPort)
    : dbClient(dbHost, dbPort) {}

string UrlShortener::generateShortCode() {
    random_device device;
    mt19937 generator(device()); 
    uniform_int_distribution<int> distribution(0, charac.size() - 1); 
    
    string code;
    for (int idx = 0; idx < short_len; ++idx) {
        code += charac[distribution(generator)]; //генерация кода, для сокращения
    }
    return code;
}

string UrlShortener::shortenUrl(const string& originalUrl) {
    if (originalUrl.empty()) {
        throw runtime_error("URL пуст");
    }
    
    for (int iter = 0; iter < 10; ++iter) {
        string code = generateShortCode();
        if (dbClient.getUrl(code).empty()) {  //проверка кода на уникальность
            // Указываем таблицу "urls" явно
            if (dbClient.saveUrl("urls", code, originalUrl)) {
                return code;
            }
        }
    }
    throw runtime_error("Не удалось сгенерировать код");
}

string UrlShortener::getOriginalUrl(const string& shortCode) {
    return dbClient.getUrl(shortCode);
}