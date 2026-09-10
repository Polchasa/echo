#include "patienteditdialog.h"
#include "ui_patienteditdialog.h"

PatientEditDialog::PatientEditDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::PatientEditDialog)
{
    ui->setupUi(this);

    ui->deBirthDate->setCalendarPopup(true);
    ui->deBirthDate->setDate(QDate::currentDate());
}

PatientEditDialog::~PatientEditDialog()
{
    delete ui;
}

void PatientEditDialog::setPatient(const Patient &p)
{
    ui->leFullName->setText(p.fullName);
    ui->deBirthDate->setDate(p.birthDate);
}

Patient PatientEditDialog::getPatient() const
{
    Patient p;

    p.fullName = ui->leFullName->text();
    p.birthDate = ui->deBirthDate->date();

    return p;
}