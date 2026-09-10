#ifndef PATIENTDIALOG_H
#define PATIENTDIALOG_H

#include <QDialog>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>

#include "models/patient.h"
#include "database/patientrepository.h"

namespace Ui {
class PatientDialog;
}

class PatientDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PatientDialog(QWidget *parent = nullptr);
    ~PatientDialog();

    Patient selectedPatient() const;

private slots:
    void loadPatients();
    void onTableDoubleClicked(const QModelIndex &index);
    void onTableClicked(const QModelIndex &index);

    void on_btnAdd_clicked();
    void on_btnDelete_clicked();
    void on_btnEdit_clicked();

private:
    Ui::PatientDialog *ui;

    PatientRepository repo;

    QVector<Patient> patients;
    Patient selected;

    QStandardItemModel *model;

    QSortFilterProxyModel *proxyModel;

    int selectedRow = -1;
};

#endif // PATIENTDIALOG_H