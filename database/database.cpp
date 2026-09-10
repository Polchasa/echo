#include "database.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QCoreApplication>
#include <QDebug>

QSqlDatabase Database::m_db = QSqlDatabase();

bool Database::init()
{
    // подключаем SQLite
    m_db = QSqlDatabase::addDatabase("QSQLITE");

    QString dbPath =
        QCoreApplication::applicationDirPath()
        + "/database.db";

    m_db.setDatabaseName(dbPath);

    if (!m_db.open())
    {
        qDebug() << "DB open error:" << m_db.lastError();
        return false;
    }

    qDebug() << "Database opened:" << dbPath;

    createTables();

    return true;
}

QSqlDatabase Database::db()
{
    return m_db;
}

void Database::createTables()
{
    QSqlQuery query;

    QString sql = R"(
        CREATE TABLE IF NOT EXISTS patients (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            full_name TEXT NOT NULL,
            birth_date TEXT NOT NULL
        )
    )";

    if (!query.exec(sql))
    {
        qDebug() << "Create table error:"
                 << query.lastError();
    }
    else
    {
        qDebug() << "Tables ready";
    }
}