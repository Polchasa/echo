#include "patientdialog.h"
#include "ui_patientdialog.h"
#include "patienteditdialog.h"

#include <QDebug>
#include <QMessageBox>

PatientDialog::PatientDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::PatientDialog)
{
    ui->setupUi(this);

    model = new QStandardItemModel(this);

    proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(model);

    ui->tableView->setModel(proxyModel);

    // 2 колонки: ФИО и дата
    model->setColumnCount(2);
    model->setHorizontalHeaderLabels({"ФИО", "Дата рождения"});

    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(ui->tableView,
            &QTableView::doubleClicked,
            this,
            &PatientDialog::onTableDoubleClicked);

    connect(ui->tableView,
            &QTableView::clicked,
            this,
            &PatientDialog::onTableClicked);


    connect(ui->leSearch, &QLineEdit::textChanged,
            this, [=](const QString &text)
            {
                QRegularExpression rx(
                    ".*" + QRegularExpression::escape(text) + ".*",
                    QRegularExpression::CaseInsensitiveOption
                    );

                proxyModel->setFilterKeyColumn(0);
                proxyModel->setFilterRegularExpression(rx);
            });

    loadPatients();
}

void PatientDialog::loadPatients()
{
    patients = repo.getAll();

    model->removeRows(0, model->rowCount());

    for (int i = 0; i < patients.size(); i++)
    {
        model->insertRow(i);

        model->setItem(i, 0,
                       new QStandardItem(patients[i].fullName));

        model->setItem(i, 1,
                       new QStandardItem(
                           patients[i].birthDate.toString("dd.MM.yyyy")
                           ));
    }

    ui->tableView->resizeColumnsToContents();
}

void PatientDialog::onTableClicked(const QModelIndex &index)
{
    QModelIndex sourceIndex =
        proxyModel->mapToSource(index);

    int row = sourceIndex.row();

    selectedRow = row;
}

void PatientDialog::onTableDoubleClicked(const QModelIndex &index)
{
    QModelIndex sourceIndex =
        proxyModel->mapToSource(index);

    int row = sourceIndex.row();

    selected = patients[row];

    accept();
}

Patient PatientDialog::selectedPatient() const
{
    return selected;
}

void PatientDialog::on_btnAdd_clicked()
{
    PatientEditDialog dlg(this);
    dlg.setWindowTitle("Добавить пациента");

    if (dlg.exec() == QDialog::Accepted)
    {
        Patient p = dlg.getPatient();

        repo.add(p);

        loadPatients();
    }
}

void PatientDialog::on_btnDelete_clicked()
{
    if (selectedRow < 0 || selectedRow >= patients.size())
        return;

    int id = patients[selectedRow].id;

    repo.remove(id);

    loadPatients();
}

void PatientDialog::on_btnEdit_clicked()
{
    if (selectedRow < 0)
        return;

    Patient p = patients[selectedRow];

    PatientEditDialog dlg(this);
    dlg.setWindowTitle("Редактировать пациента");
    dlg.setPatient(p);

    if (dlg.exec() == QDialog::Accepted)
    {
        Patient updated = dlg.getPatient();

        updated.id = p.id;

        repo.update(updated); // позже добавим update()

        loadPatients();
    }
}

PatientDialog::~PatientDialog()
{
    delete ui;
}