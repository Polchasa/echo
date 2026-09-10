#ifndef PATIENTREPOSITORY_H
#define PATIENTREPOSITORY_H

#include "models/patient.h"
#include <QVector>

class PatientRepository
{
public:
    bool add(const Patient& patient);
    QVector<Patient> getAll();
    bool remove(int id);
    bool update(const Patient& patient);
};

#endif // PATIENTREPOSITORY_H