#ifndef PATIENTEDITDIALOG_H
#define PATIENTEDITDIALOG_H

#include <QDialog>
#include "models/patient.h"

namespace Ui {
class PatientEditDialog;
}

class PatientEditDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PatientEditDialog(QWidget *parent = nullptr);
    ~PatientEditDialog();

    void setPatient(const Patient& p);
    Patient getPatient() const;

private:
    Ui::PatientEditDialog *ui;
};

#endif // PATIENTEDITDIALOG_H