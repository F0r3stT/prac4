#ifndef STAT_SENDER_H
#define STAT_SENDER_H

#include <string>

using namespace std;

//отправка статистики
struct StatSender {
    //статик чтобы ограничить видимость функции
    static void sendStat(const string& statHost, int statPort, 
                         const string& ip, const string& originalUrl, 
                         const string& shortCode, long timestamp);
};

#endif