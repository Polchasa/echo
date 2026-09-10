#ifndef PATIENT_H
#define PATIENT_H

#include <QString>
#include <QDate>

struct Patient
{
    int id;
    QString fullName;
    QDate birthDate;
};

#endif // PATIENT_H