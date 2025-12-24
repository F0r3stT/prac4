#ifndef URL_SHORTENER_H
#define URL_SHORTENER_H

#include <string>
#include "db_client.h" 

using namespace std;

class UrlShortener {
private:
    DatabaseClient dbClient;
    
    const int short_len = 6;
    
    string generateShortCode();
    
public:
    //Конструктор
    UrlShortener(const string& dbHost, int dbPort);

    string shortenUrl(const string& originalUrl);

    string getOriginalUrl(const string& shortCode);
};

#endif