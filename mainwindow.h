#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMap>
#include <QMainWindow>
#include "models/patient.h"

class QComboBox;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void on_Save_clicked();
    void openPatientDialog();
    void on_pushButton_clicked();
    QMap<QString, QString> collectTablePlaceholders() const;


private:
    QMap<QString, QString> collectProtocolPlaceholders() const;
    void addLJTypeUnderlinePlaceholders(QMap<QString, QString>& placeholders) const;
    void applyLJTypeUnderlineRanges(QString& content) const;
    QString underlineWordRuns(const QString& content) const;
    bool replacePlaceholdersInFile(const QString& path,
                                   const QMap<QString, QString>& placeholders);
    bool replacePlaceholdersInXmlFiles(const QString& directoryPath,
                                       const QMap<QString, QString>& placeholders);
    QString xmlEscaped(const QString& value) const;
    bool extractDocxTemplate(const QString& sevenZipPath,
                             const QString& templateDocx,
                             const QString& tempDir);
    bool packDocx(const QString& sevenZipPath,
                  const QString& tempDir,
                  const QString& outputPath);

    // Настраивает QComboBox как поле с историей: включает загрузку сохраненных
    // значений, рисование крестиков удаления и сохранение новых введенных строк.
    void setupHistoryCombo(QComboBox *comboBox,
                           const QString& settingsKey,
                           const QString& placeholderText = QString());
    // Загружает список ранее введенных значений из QSettings по ключу поля.
    void loadComboHistory(QComboBox *comboBox, const QString& settingsKey);
    // Сохраняет текущий текст комбобокса в историю, поднимая его наверх списка.
    void rememberComboText(QComboBox *comboBox, const QString& settingsKey);
    // Перезаписывает историю поля в QSettings текущими элементами комбобокса.
    void saveComboHistory(QComboBox *comboBox, const QString& settingsKey);
    // Перед сохранением протокола запоминает текущие значения всех полей с историей.
    void rememberAllComboTexts();
    void setupMeasurementsTable();
    void setupValveDescriptionTable();
    void updateKdolzhIndexFromKdo();
    void updateKsoLjIndexFromKso();
    void updateOtsLjFromTzsljAndKdrlj();
    void updateImmLjFromTmjpTzsKdr();
    void updateLpFromDlpPpt();
    void updatePpFromPpPpt();
    void updateKdoljFromKdrlj();
    void updateKsoljFromKsrlj();
    void updateYoljFromKdoKso();
    // Возвращает ключ QSettings для конкретного комбобокса.
    QString historySettingsKey(QComboBox *comboBox) const;
    // Проверяет клик в выпадающем списке и удаляет пункт, если нажали на крестик.
    bool removeClickedComboItem(QComboBox *comboBox,
                                const QString& settingsKey,
                                QEvent *event);

    void calculateImtAndPpt();

    Ui::MainWindow *ui;
    QString templatePath;
    Patient currentPatient;

    QPixmap pixmap;
};
#endif // MAINWINDOW_H
