#pragma once
#include <QString>

namespace Studio {
class Credentials {
public:
    static bool available();
    static bool store(const QString &key, QString *error = nullptr);
    static QString load(QString *error = nullptr);
    static bool valid(const QString &key);
private:
    static QString service();
};
}
