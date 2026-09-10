#include "patientrepository.h"
#include "database.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

bool PatientRepository::add(const Patient& patient)
{
    QSqlQuery query(Database::db());

    query.prepare(R"(
        INSERT INTO patients(full_name, birth_date)
        VALUES(:name, :date)
    )");

    query.bindValue(":name", patient.fullName);
    query.bindValue(":date", patient.birthDate.toString("yyyy-MM-dd"));

    if (!query.exec())
    {
        qDebug() << "Add patient error:" << query.lastError();
        return false;
    }

    return true;
}

QVector<Patient> PatientRepository::getAll()
{
    QVector<Patient> list;

    QSqlQuery query(Database::db());

    if (!query.exec("SELECT id, full_name, birth_date FROM patients"))
    {
        qDebug() << "Get all error:" << query.lastError();
        return list;
    }

    while (query.next())
    {
        Patient p;

        p.id = query.value("id").toInt();
        p.fullName = query.value("full_name").toString();
        p.birthDate = QDate::fromString(
            query.value("birth_date").toString(),
            "yyyy-MM-dd"
            );

        list.append(p);
    }

    return list;
}

bool PatientRepository::remove(int id)
{
    QSqlQuery query(Database::db());

    query.prepare("DELETE FROM patients WHERE id = :id");
    query.bindValue(":id", id);

    if (!query.exec())
    {
        qDebug() << "Delete error:" << query.lastError();
        return false;
    }

    return true;
}

bool PatientRepository::update(const Patient& patient)
{
    QSqlQuery query(Database::db());

    query.prepare(R"(
        UPDATE patients
        SET full_name = :name,
            birth_date = :date
        WHERE id = :id
    )");

    query.bindValue(":name", patient.fullName);
    query.bindValue(":date", patient.birthDate.toString("yyyy-MM-dd"));
    query.bindValue(":id", patient.id);

    if (!query.exec())
    {
        qDebug() << "Update error:" << query.lastError();
        return false;
    }

    return true;
}