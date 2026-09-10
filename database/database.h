#ifndef DATABASE_H
#define DATABASE_H

#include <QSqlDatabase>
#include <QString>

class Database
{
public:
    static bool init();          // инициализация БД
    static QSqlDatabase db();    // доступ к соединению

private:
    static void createTables();  // создание таблиц

private:
    static QSqlDatabase m_db;
};

#endif // DATABASE_H