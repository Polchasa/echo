#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "QAbstractItemView"
#include "QComboBox"
#include "QCoreApplication"
#include "QDir"
#include "QDirIterator"
#include "QEvent"
#include "QFile"
#include "QFileDialog"
#include "QFileInfo"
#include "QLineEdit"
#include "QMouseEvent"
#include "QPainter"
#include "QProcess"
#include "QMessageBox"
#include "QRegularExpression"
#include "QSettings"
#include "QStringList"
#include "QSignalBlocker"
#include "QStyledItemDelegate"
#include "QHeaderView"
#include "QTableWidgetItem"
#include "QTemporaryDir"
#include "QVector"
#include "QScrollArea"
#include "patientdialog.h"
#include <cmath>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>

namespace {
// Максимальное количество вариантов, которые храним в истории каждого поля.
constexpr int MaxComboHistoryItems = 20;

// Правая часть строки выпадающего списка зарезервирована под крестик удаления.
constexpr int DeleteAreaWidth = 28;

// Отдельный ключ QSettings для каждого поля нужен, чтобы истории не смешивались.
const QString NapravilHistoryKey = "history/napravil";
const QString VidObrHistoryKey = "history/vid_obr";
const QString IssledovanieHistoryKey = "history/issledovanie";
const QString NaprDiagnozHistoryKey = "history/napr_diagnoz";
const QString ApparatModelHistoryKey = "history/apparat_model";
const QString ComboBoxHistoryKey = "history/combo_box";
const QString ComboBox2HistoryKey = "history/combo_box_2";

// Delegate отвечает только за внешний вид пункта в выпадающем списке.
// Стандартный QComboBox не умеет рисовать кнопку удаления в каждой строке,
// поэтому мы сами дорисовываем крестик справа, а клик обрабатываем отдельно.
class ComboDeleteDelegate : public QStyledItemDelegate
{
public:
    explicit ComboDeleteDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        // Сначала рисуем обычный пункт списка, но сужаем область текста,
        // чтобы длинная строка не залезала под крестик справа.
        QStyleOptionViewItem itemOption(option);
        initStyleOption(&itemOption, index);
        itemOption.rect.adjust(0, 0, -DeleteAreaWidth, 0);

        QStyle *style = option.widget
                            ? option.widget->style()
                            : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem,
                           &itemOption,
                           painter,
                           option.widget);

        // Затем рисуем простой крестик в зарезервированной правой зоне строки.
        QRect deleteRect(option.rect.right() - DeleteAreaWidth,
                         option.rect.top(),
                         DeleteAreaWidth,
                         option.rect.height());
        QPoint center = deleteRect.center();

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(QPen(QColor(120, 120, 120), 2));
        painter->drawLine(center.x() - 4,
                          center.y() - 4,
                          center.x() + 4,
                          center.y() + 4);
        painter->drawLine(center.x() + 4,
                          center.y() - 4,
                          center.x() - 4,
                          center.y() + 4);
        painter->restore();
    }
    };

    // Делегат для ячеек с плейсхолдером "Описание"
    class PlaceholderDelegate : public QStyledItemDelegate
    {
    public:
        explicit PlaceholderDelegate(QObject *parent = nullptr)
            : QStyledItemDelegate(parent)
        {
        }

        void paint(QPainter *painter,
                   const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override
        {
            QString text = index.data(Qt::DisplayRole).toString();
            
            if (text.isEmpty() && !option.state.testFlag(QStyle::State_Editing))
            {
                // Рисуем плейсхолдер серым цветом
                QStyleOptionViewItem itemOption(option);
                initStyleOption(&itemOption, index);
                itemOption.text = "Описание";
                itemOption.palette.setColor(QPalette::Text, QColor(128, 128, 128));
                
                QStyle *style = option.widget
                                    ? option.widget->style()
                                    : QApplication::style();
                style->drawControl(QStyle::CE_ItemViewItem,
                                   &itemOption,
                                   painter,
                                   option.widget);
            }
            else
            {
                // Стандартное отображение
                QStyledItemDelegate::paint(painter, option, index);
            }
        }
    };
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QWidget *oldCentralWidget = centralWidget();
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    oldCentralWidget->setParent(scrollArea);
    scrollArea->setWidget(oldCentralWidget);
    setCentralWidget(scrollArea);

    templatePath = QCoreApplication::applicationDirPath() + "/template/Shablon.docx";
    pixmap = QCoreApplication::applicationDirPath() + "/template/visualJK.jpg";
    ui->LJVisual->setPixmap(pixmap);
    qDebug() << "Шаблон: " << templatePath;

    connect(ui->allPatients,
            &QPushButton::clicked,
            this,
            &MainWindow::openPatientDialog);

    ui->dateEdit->setDate(QDate::currentDate());

    // Все эти поля остаются редактируемыми: в них можно печатать новый текст,
    // но также можно выбрать ранее введенное значение из выпадающего списка.
    setupHistoryCombo(ui->napravilLE, NapravilHistoryKey, "Какого врача?");
    setupHistoryCombo(ui->vidObrLE, VidObrHistoryKey);
    setupHistoryCombo(ui->issledovanieLE, IssledovanieHistoryKey);
    setupHistoryCombo(ui->naprDiagnozLE, NaprDiagnozHistoryKey);
    setupHistoryCombo(ui->apparatModelLE, ApparatModelHistoryKey);
    setupHistoryCombo(ui->comboBox, ComboBoxHistoryKey);
    setupHistoryCombo(ui->comboBox_2, ComboBox2HistoryKey);

    ui->rostLE->setValidator(new QIntValidator(1, 999, this));
    ui->vesLE->setValidator(new QIntValidator(1, 999, this));
    ui->CHSSLE->setValidator(new QIntValidator(1, 999, this));

    connect(ui->rostLE, &QLineEdit::textEdited, this, &MainWindow::calculateImtAndPpt);
    connect(ui->vesLE, &QLineEdit::textEdited, this, &MainWindow::calculateImtAndPpt);

    setupMeasurementsTable();
    setupValveDescriptionTable();
    connect(ui->tableWidget,
            &QTableWidget::itemChanged,
            this,
            [this](QTableWidgetItem *item)
            {
                if (item && item->row() == 6 && item->column() == 1) {
                    updateKdolzhIndexFromKdo();
                    updateYoljFromKdoKso();
                }
                else if (item && item->row() == 2 && item->column() == 4) {
                    updateKsoLjIndexFromKso();
                    updateYoljFromKdoKso();
                }

                /*else if (item && ((item->row() == 3 && item->column() == 1) || (item->row() == 4 && item->column() == 1)))
                    updateOtsLjFromTzsljAndKdrlj();*/
                else if ((item && ((item->row() == 2 && item->column() == 1) || (item->row() == 3 && item->column() == 1) || (item->row() == 4 && item->column() == 1)))) {
                    updateImmLjFromTmjpTzsKdr();
                    updateOtsLjFromTzsljAndKdrlj();
                    updateKdoljFromKdrlj();
                }
                else if (item && item->row() == 13 && item->column() == 1)
                    updateLpFromDlpPpt();
                else if (item && item->row() == 21 && item->column() == 1)
                    updatePpFromPpPpt();
                else if (item && item->row() == 5 && item->column() == 1)
                    updateKsoljFromKsrlj();
                else
                    return;

            });

    ui->label_9->setVisible(false);
    ui->comboBox->setVisible(false);
}

MainWindow::~MainWindow()
{
    delete ui;
}

QMap<QString, QString> MainWindow::collectProtocolPlaceholders() const
{
    QMap<QString, QString> placeholders;

    // Все значения сразу готовим в XML-безопасном виде, потому что подстановка
    // идет напрямую в файлы docx, которые внутри являются XML-документами.
    placeholders.insert("{{FIO}}", xmlEscaped(currentPatient.fullName));
    placeholders.insert("{{DATA}}",
                        xmlEscaped(ui->dateEdit->date().toString("dd.MM.yyyy")));
    placeholders.insert("{{NAPRAVIL}}", xmlEscaped(ui->napravilLE->currentText()));
    placeholders.insert("{{VID_OBR}}", xmlEscaped(ui->vidObrLE->currentText()));
    placeholders.insert("{{ISSLEDOVANIE}}",
                        xmlEscaped(ui->issledovanieLE->currentText()));
    placeholders.insert("{{NAPR_DIAGNOZ}}",
                        xmlEscaped(ui->naprDiagnozLE->currentText()));
    placeholders.insert("{{APPARAT_MODEL}}",
                        xmlEscaped(ui->apparatModelLE->currentText()));
    placeholders.insert("{{MESTO}}", xmlEscaped(ui->mestoLE->text()));
    placeholders.insert("{{KACHESTVO}}", xmlEscaped(ui->kachestvoLE->text()));
    placeholders.insert("{{PPT}}", xmlEscaped(ui->PPTResultLabel->text()));
    placeholders.insert("{{ROST}}", xmlEscaped(ui->rostLE->text()));
    placeholders.insert("{{VES}}", xmlEscaped(ui->vesLE->text()));
    placeholders.insert("{{IMT}}", xmlEscaped(ui->IMTResultLabel->text()));
    placeholders.insert("{{CHSS}}", xmlEscaped(ui->CHSSLE->text()));
    placeholders.insert("{{RITHM}}", xmlEscaped(ui->rithmLE->text()));
    placeholders.insert("{{DESCRIPTION}}", xmlEscaped(ui->lineEdit->text()));
    placeholders.insert("{{PERIKARD}}", xmlEscaped(ui->perikardLE->text()));
    placeholders.insert("{{PEREGOROD1}}", xmlEscaped(ui->peregorod1LE->text()));
    placeholders.insert("{{PEREGOROD2}}", xmlEscaped(ui->peregorod2LE->text()));
    placeholders.insert("{{CONCLUSION}}", xmlEscaped(ui->textEdit->toPlainText()));
    placeholders.insert("{{MEDICAL_SERVICES}}", xmlEscaped(ui->comboBox->currentText()));
    placeholders.insert("{{CARDIOLOGIST}}", xmlEscaped(ui->comboBox_2->currentText()));

    addLJTypeUnderlinePlaceholders(placeholders);
    placeholders.insert(collectTablePlaceholders());

    return placeholders;
}

QMap<QString, QString> MainWindow::collectTablePlaceholders() const
{
    QMap<QString, QString> placeholders;

    // Собираем данные из ui->tableWidget
    // "Левый желудочек"
    placeholders.insert("{{MEASUREMENTS_TMJP}}", xmlEscaped(ui->tableWidget->item(2, 1)->text()));
    placeholders.insert("{{MEASUREMENTS_KSO_LJ}}", xmlEscaped(ui->tableWidget->item(2, 4)->text()));
    placeholders.insert("{{MEASUREMENTS_TZSLJ}}", xmlEscaped(ui->tableWidget->item(3, 1)->text()));
    placeholders.insert("{{MEASUREMENTS_KSO_LJ_INDEX}}", xmlEscaped(ui->tableWidget->item(3, 4)->text()));
    placeholders.insert("{{MEASUREMENTS_KDRLJ}}", xmlEscaped(ui->tableWidget->item(4, 1)->text()));
    placeholders.insert("{{MEASUREMENTS_FV_LJ_SIMPSON}}", xmlEscaped(ui->tableWidget->item(4, 4)->text()));
    placeholders.insert("{{MEASUREMENTS_KSRLJ}}", xmlEscaped(ui->tableWidget->item(5, 1)->text()));
    placeholders.insert("{{MEASUREMENTS_UO_LJ}}", xmlEscaped(ui->tableWidget->item(5, 4)->text()));
    placeholders.insert("{{MEASUREMENTS_KDO_LJ}}", xmlEscaped(ui->tableWidget->item(6, 1)->text()));
    placeholders.insert("{{MEASUREMENTS_OTS_LJ}}", xmlEscaped(ui->tableWidget->item(6, 4)->text()));
    placeholders.insert("{{MEASUREMENTS_KDO_LJ_INDEX}}", xmlEscaped(ui->tableWidget->item(7, 1)->text()));
    placeholders.insert("{{MEASUREMENTS_IMMLJ}}", xmlEscaped(ui->tableWidget->item(7, 4)->text()));

    // "Диастолическая функция левого желудочка / расчетное давление наполнения левого желудочка"
    placeholders.insert("{{MEASUREMENTS_PIK_EMK}}", xmlEscaped(ui->tableWidget->item(9, 1)->text()));
    placeholders.insert("{{MEASUREMENTS_E_E}}", xmlEscaped(ui->tableWidget->item(9, 4)->text()));
    placeholders.insert("{{MEASUREMENTS_E_A_MK}}", xmlEscaped(ui->tableWidget->item(10, 1)->text()));

    // "Левое предсердие"
    placeholders.insert("{{MEASUREMENTS_DIAMETR_LP}}", xmlEscaped(ui->tableWidget->item(12, 1)->text()));
    placeholders.insert("{{MEASUREMENTS_OBJEM_LP_INDEX}}", xmlEscaped(ui->tableWidget->item(12, 4)->text()));
    placeholders.insert("{{MEASUREMENTS_OBJEM_LP}}", xmlEscaped(ui->tableWidget->item(13, 1)->text()));

    // "Аорта"
    placeholders.insert("{{MEASUREMENTS_AOSV}}", xmlEscaped(ui->tableWidget->item(15, 1)->text()));
    placeholders.insert("{{MEASUREMENTS_DUGA_AO}}", xmlEscaped(ui->tableWidget->item(15, 4)->text()));
    placeholders.insert("{{MEASUREMENTS_VOSX_AO}}", xmlEscaped(ui->tableWidget->item(16, 1)->text()));
    placeholders.insert("{{AORTA_DESCRIPTION}}", xmlEscaped(ui->tableWidget->item(16, 4)->text()));

    // "Правый желудочек"
    placeholders.insert("{{MEASUREMENTS_PJ_PZR}}", xmlEscaped(ui->tableWidget->item(18, 1)->text()));
    placeholders.insert("{{MEASUREMENTS_TAPSE}}", xmlEscaped(ui->tableWidget->item(18, 4)->text()));
    placeholders.insert("{{MEASUREMENTS_BAZ_PJ}}", xmlEscaped(ui->tableWidget->item(19, 1)->text()));
    placeholders.insert("{{MEASUREMENTS_TOLSHINA_STENKI_PJ}}", xmlEscaped(ui->tableWidget->item(19, 4)->text()));

    // "Правое предсердие"
    placeholders.insert("{{MEASUREMENTS_OBJEM_PP}}", xmlEscaped(ui->tableWidget->item(21, 1)->text()));
    placeholders.insert("{{MEASUREMENTS_PLOSHAD_PP}}", xmlEscaped(ui->tableWidget->item(21, 4)->text()));
    placeholders.insert("{{MEASUREMENTS_OBJEM_PP_INDEX}}", xmlEscaped(ui->tableWidget->item(22, 1)->text()));

    // "Нижняя полая вена"
    placeholders.insert("{{MEASUREMENTS_NPV_VIDOX}}", xmlEscaped(ui->tableWidget->item(24, 1)->text()));
    placeholders.insert("{{MEASUREMENTS_NPV_VDOH}}", xmlEscaped(ui->tableWidget->item(24, 4)->text()));

    // "Расчетное систолическое давление в легочной артерии"
    placeholders.insert("{{MEASUREMENTS_MAKS_GRADIENT_TR}}", xmlEscaped(ui->tableWidget->item(26, 1)->text()));
    placeholders.insert("{{MEASUREMENTS_SDLA}}", xmlEscaped(ui->tableWidget->item(26, 4)->text()));

    // Собираем данные из ui->tableWidget_2
    // "Митральный клапан" и "Аортальный клапан"
    placeholders.insert("{{VALVE_MITRAL_REGURGITATION}}", xmlEscaped(ui->tableWidget_2->item(3, 1)->text()));
    placeholders.insert("{{VALVE_AORTAL_REGURGITATION}}", xmlEscaped(ui->tableWidget_2->item(3, 3)->text()));
    placeholders.insert("{{VALVE_MITRAL_STENOSIS}}", xmlEscaped(ui->tableWidget_2->item(4, 1)->text()));
    placeholders.insert("{{VALVE_AORTAL_STENOSIS}}", xmlEscaped(ui->tableWidget_2->item(4, 3)->text()));
    placeholders.insert("{{VALVE_SR_GRADIENT_MK}}", xmlEscaped(ui->tableWidget_2->item(5, 1)->text()));
    placeholders.insert("{{VALVE_PIK_SKOROST_AK}}", xmlEscaped(ui->tableWidget_2->item(5, 3)->text()));

    placeholders.insert("{{DESCRIPTION_MITRAL}}", xmlEscaped(ui->tableWidget_2->item(2, 0)->text()));
    placeholders.insert("{{DESCRIPTION_AORTAL}}", xmlEscaped(ui->tableWidget_2->item(2, 2)->text()));

    // "Трикуспидальный клапан" и "Пульмональный клапан"
    placeholders.insert("{{VALVE_TRIKUSPIDAL_REGURGITATION}}", xmlEscaped(ui->tableWidget_2->item(8, 1)->text()));
    placeholders.insert("{{VALVE_PULMONAL_REGURGITATION}}", xmlEscaped(ui->tableWidget_2->item(8, 3)->text()));
    placeholders.insert("{{VALVE_TRIKUSPIDAL_STENOSIS}}", xmlEscaped(ui->tableWidget_2->item(9, 1)->text()));
    placeholders.insert("{{VALVE_PULMONAL_STENOSIS}}", xmlEscaped(ui->tableWidget_2->item(9, 3)->text()));
    placeholders.insert("{{VALVE_PIK_SKOROST_TR}}", xmlEscaped(ui->tableWidget_2->item(10, 1)->text()));
    placeholders.insert("{{VALVE_PIK_SKOROST_PK}}", xmlEscaped(ui->tableWidget_2->item(10, 3)->text()));
    placeholders.insert("{{VALVE_PIK_SKOROST_TK}}", xmlEscaped(ui->tableWidget_2->item(11, 1)->text()));

    placeholders.insert("{{DESCRIPTION_TRIKUSPIDAL}}", xmlEscaped(ui->tableWidget_2->item(7, 0)->text()));
    placeholders.insert("{{DESCRIPTION_PULMONAL}}", xmlEscaped(ui->tableWidget_2->item(7, 2)->text()));

    return placeholders;
}

void MainWindow::addLJTypeUnderlinePlaceholders(QMap<QString, QString>& placeholders) const
{
    for (int i = 0; i < 5; ++i)
    {
        const QString prefix = QString("{{LJ%1_").arg(i + 1);
        placeholders.insert(prefix + "START}}", QString());
        placeholders.insert(prefix + "END}}", QString());
    }
}

void MainWindow::applyLJTypeUnderlineRanges(QString& content) const
{
    const bool checked[] = {
        ui->LJType1->isChecked(),
        ui->LJType2->isChecked(),
        ui->LJType3->isChecked(),
        ui->LJType4->isChecked(),
        ui->LJType5->isChecked()
    };

    for (int i = 0; i < 5; ++i)
    {
        if (!checked[i])
            continue;

        const QString startMarker = QString("{{LJ%1_START}}").arg(i + 1);
        const QString endMarker = QString("{{LJ%1_END}}").arg(i + 1);

        int startPos = content.indexOf(startMarker);

        while (startPos >= 0)
        {
            const int rangeStart = startPos + startMarker.length();
            const int endPos = content.indexOf(endMarker, rangeStart);

            if (endPos < 0)
                break;

            QString rangeContent = content.mid(rangeStart, endPos - rangeStart);

            if (rangeContent.contains("<w:r"))
            {
                rangeContent = underlineWordRuns(rangeContent);
                content.replace(rangeStart, endPos - rangeStart, rangeContent);
            }
            else
            {
                const QString underlineStart =
                    "</w:t></w:r><w:r><w:rPr><w:u w:val=\"single\"/></w:rPr><w:t>";
                const QString underlineEnd =
                    "</w:t></w:r><w:r><w:t>";

                content.replace(endPos, endMarker.length(), underlineEnd + endMarker);
                content.replace(startPos, startMarker.length(), startMarker + underlineStart);
            }

            startPos = content.indexOf(startMarker, rangeStart + rangeContent.length());
        }
    }
}

QString MainWindow::underlineWordRuns(const QString& content) const
{
    QString result = content;
    const QString underlineXml = "<w:u w:val=\"single\"/>";

    QRegularExpression runRegex(
        "(<w:r(?:\\s[^>]*)?>)(.*?)(</w:r>)",
        QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression rPrOpenRegex("<w:rPr(?:\\s[^>]*)?>");

    auto matches = runRegex.globalMatch(result);
    QVector<QRegularExpressionMatch> runMatches;

    while (matches.hasNext())
        runMatches.append(matches.next());

    for (int i = runMatches.size() - 1; i >= 0; --i)
    {
        const QRegularExpressionMatch match = runMatches[i];
        QString runBody = match.captured(2);

        if (!runBody.contains("<w:t") || runBody.contains("<w:u "))
            continue;

        int bodyInsertPos = -1;
        QRegularExpressionMatch rPrMatch = rPrOpenRegex.match(runBody);

        if (rPrMatch.hasMatch())
        {
            bodyInsertPos = rPrMatch.capturedEnd();
        }
        else if (runBody.contains("<w:rPr/>"))
        {
            runBody.replace("<w:rPr/>", QString("<w:rPr>") + underlineXml + "</w:rPr>");
        }
        else
        {
            runBody.prepend(QString("<w:rPr>") + underlineXml + "</w:rPr>");
        }

        if (bodyInsertPos >= 0)
            runBody.insert(bodyInsertPos, underlineXml);

        result.replace(match.capturedStart(2), match.capturedLength(2), runBody);
    }

    return result;
}

void MainWindow::calculateImtAndPpt(){
    //ИМТ = вес * 10000 / рост * 2
    //ППТ = 0,007184 * вес^0.425 * рост^0.725

    int ves = ui->vesLE->text().toInt();
    int rost = ui->rostLE->text().toInt();

    double ppt = 0.007184 * std::pow(ves, 0.425) * std::pow(rost, 0.725);
    ui->PPTResultLabel->setText(QString::number(ppt));
    updateKdolzhIndexFromKdo();
    updateKsoLjIndexFromKso();
    updateLpFromDlpPpt();
    updatePpFromPpPpt();

    double imt = (ves * 10000.0) / (rost * rost);
    ui->IMTResultLabel->setText(QString::number(imt));
}

bool MainWindow::replacePlaceholdersInFile(
    const QString& path,
    const QMap<QString, QString>& placeholders)
{
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly))
        return false;

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    applyLJTypeUnderlineRanges(content);

    // Один проход по карте заменяет каждый плейсхолдер во всех местах файла.
    // Поэтому {{DATA}} заменится и во втором, и в десятом месте документа.
    for (auto it = placeholders.cbegin(); it != placeholders.cend(); ++it)
        content.replace(it.key(), it.value());

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    file.write(content.toUtf8());
    file.close();

    return true;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // Крестик удаления находится внутри popup-списка QComboBox. У него свой
    // viewport, поэтому клики ловим через eventFilter и проверяем каждое поле.
    QComboBox *comboBoxes[] = {
        ui->napravilLE,
        ui->vidObrLE,
        ui->issledovanieLE,
        ui->naprDiagnozLE,
        ui->apparatModelLE,
        ui->comboBox,
        ui->comboBox_2
    };

    for (QComboBox *comboBox : comboBoxes)
    {
        if (watched == comboBox->view()->viewport()
            && removeClickedComboItem(comboBox,
                                      historySettingsKey(comboBox),
                                      event))
        {
            return true;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::setupHistoryCombo(QComboBox *comboBox,
                                   const QString& settingsKey,
                                   const QString& placeholderText)
{
    // Placeholder нужен только некоторым полям. Для остальных оставляем пустым.
    if (!placeholderText.isEmpty())
        comboBox->lineEdit()->setPlaceholderText(placeholderText);

    // Delegate рисует крестики, eventFilter обрабатывает клики по ним.
    comboBox->setItemDelegate(new ComboDeleteDelegate(comboBox));
    comboBox->view()->viewport()->installEventFilter(this);

    // При открытии окна сразу подставляем ранее сохраненные значения.
    loadComboHistory(comboBox, settingsKey);

    // editingFinished срабатывает, когда пользователь закончил ввод:
    // ушел из поля, нажал Enter или начал другое действие.
    connect(comboBox->lineEdit(),
            &QLineEdit::editingFinished,
            this,
            [this, comboBox, settingsKey]()
            {
                rememberComboText(comboBox, settingsKey);
            });
}

QString MainWindow::historySettingsKey(QComboBox *comboBox) const
{
    // eventFilter получает только QObject, поэтому по указателю на комбобокс
    // восстанавливаем, в какой ключ QSettings надо сохранять изменения.
    if (comboBox == ui->napravilLE)
        return NapravilHistoryKey;
    if (comboBox == ui->vidObrLE)
        return VidObrHistoryKey;
    if (comboBox == ui->issledovanieLE)
        return IssledovanieHistoryKey;
    if (comboBox == ui->naprDiagnozLE)
        return NaprDiagnozHistoryKey;
    if (comboBox == ui->apparatModelLE)
        return ApparatModelHistoryKey;
    if (comboBox == ui->comboBox)
        return ComboBoxHistoryKey;
    if (comboBox == ui->comboBox_2)
        return ComboBox2HistoryKey;

    return QString();
}

void MainWindow::rememberAllComboTexts()
{
    // Это нужно на случай, если пользователь ввел текст и сразу нажал
    // "Сохранить протокол", не успев уйти фокусом из поля.
    rememberComboText(ui->napravilLE, NapravilHistoryKey);
    rememberComboText(ui->vidObrLE, VidObrHistoryKey);
    rememberComboText(ui->issledovanieLE, IssledovanieHistoryKey);
    rememberComboText(ui->naprDiagnozLE, NaprDiagnozHistoryKey);
    rememberComboText(ui->apparatModelLE, ApparatModelHistoryKey);
    rememberComboText(ui->comboBox, ComboBoxHistoryKey);
    rememberComboText(ui->comboBox_2, ComboBox2HistoryKey);
}

void MainWindow::setupMeasurementsTable()
{
    ui->tableWidget->clear();
    ui->tableWidget->setColumnCount(6);
    ui->tableWidget->setRowCount(27);
    ui->tableWidget->horizontalHeader()->setVisible(false);
    ui->tableWidget->verticalHeader()->setVisible(false);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->tableWidget->setWordWrap(false);
    ui->tableWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->tableWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectItems);
    ui->tableWidget->setAlternatingRowColors(true);

    auto setReadOnlyItem = [this](int row, int column, const QString& text, bool centerText = false)
    {
        auto *item = new QTableWidgetItem(text);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        if (centerText)
            item->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row, column, item);
    };

    auto setValueItem = [this](int row, int column)
    {
        ui->tableWidget->setItem(row, column, new QTableWidgetItem());
    };

    const QStringList headers = {
        "Параметр",
        "Значение",
        "Референсный интервал",
        "Параметр",
        "Значение",
        "Референсный интервал"
    };

    for (int column = 0; column < headers.size(); ++column)
        setReadOnlyItem(0, column, headers[column], true);

    ui->tableWidget->setSpan(1, 0, 1, 6);
    setReadOnlyItem(1, 0, "Левый желудочек", true);

    const QVector<QStringList> rows = {
        {"ТМЖП, см", "", "М 0.6-1см, Ж 0.6-0.9см", "КСО ЛЖ, мл", "", "М 21-61мл, Ж 14-42мл"},
        {"ТЗСЛЖ, см", "", "М 0.6-1см, Ж 0.6-0.9см", "КСО ЛЖ индекс, мл/м²", "", "М 11-31мл/м², Ж 8-24мл/м²"},
        {"КДРЛЖ, см", "", "М 4.2-5.8см, Ж 3.8-5.2см", "ФВ ЛЖ Симпсон, %", "", "М 52-72%, Ж 52-75%"},
        {"КСРЛЖ, см", "", "М 2.5-4см, Ж 2.2-3.5см", "УО ЛЖ (метод дисков), мл", "", "-"},
        {"КДО ЛЖ, мл", "", "М 62-150мл, Ж 46-106мл", "ОТС ЛЖ", "", "≤0,42"},
        {"КДОЛЖ индекс, мл/м²", "", "М 34-72мл/м², Ж 29-61мл/м²", "ИММЛЖ, г/м²", "", "М 49-115г/м², Ж 43-95г/м²"}
    };

    for (int row = 0; row < rows.size(); ++row)
    {
        const int tableRow = row + 2;
        const QStringList values = rows[row];

        setReadOnlyItem(tableRow, 0, values[0]);
        setValueItem(tableRow, 1);
        setReadOnlyItem(tableRow, 2, values[2]);
        setReadOnlyItem(tableRow, 3, values[3]);
        setValueItem(tableRow, 4);
        setReadOnlyItem(tableRow, 5, values[5]);
    }

    ui->tableWidget->setSpan(8, 0, 1, 6);
    setReadOnlyItem(8, 0,
                   "Диастолическая функция левого желудочка / расчетное давление наполнения левого желудочка",
                   true);

    const QVector<QStringList> diastolicRows = {
        {"ПикЕМК, см/с", "", "-", "Е/е’", "", "<10"},
        {"E/AМК", "", ">=0,8<2,0", "", "", ""}
    };

    for (int row = 0; row < diastolicRows.size(); ++row)
    {
        const int tableRow = row + 9;
        const QStringList values = diastolicRows[row];

        setReadOnlyItem(tableRow, 0, values[0]);
        setValueItem(tableRow, 1);
        setReadOnlyItem(tableRow, 2, values[2]);
        setReadOnlyItem(tableRow, 3, values[3]);
        setValueItem(tableRow, 4);
        setReadOnlyItem(tableRow, 5, values[5]);
    }

    ui->tableWidget->setSpan(11, 0, 1, 6);
    setReadOnlyItem(11, 0, "Левое предсердие", true);

    const QVector<QStringList> leftAtriumRows = {
        {"Диаметр ЛП, мм", "", "М 30-40мм, Ж 27-38мм", "Объем ЛП индекс, мл/м²", "", "16-34мл/м²"},
        {"Объем ЛП, мл", "", "М 22-52мл, Ж 18-58мл", "", "", ""}
    };

    for (int row = 0; row < leftAtriumRows.size(); ++row)
    {
        const int tableRow = row + 12;
        const QStringList values = leftAtriumRows[row];

        setReadOnlyItem(tableRow, 0, values[0]);
        setValueItem(tableRow, 1);
        setReadOnlyItem(tableRow, 2, values[2]);
        setReadOnlyItem(tableRow, 3, values[3]);
        setValueItem(tableRow, 4);
        setReadOnlyItem(tableRow, 5, values[5]);
    }

    ui->tableWidget->setSpan(14, 0, 1, 6);
    setReadOnlyItem(14, 0, "Аорта", true);

    const QVector<QStringList> aortaRows = {
        {"АоСВ, мм", "", "М 31-37мм, Ж 27-33мм", "ДугаАо, мм", "", ""},
        {"ВосхАо, мм", "", "М 26-34мм, Ж 23-31мм", "Описание", "", ""}
    };

    for (int row = 0; row < aortaRows.size(); ++row)
    {
        const int tableRow = row + 15;
        const QStringList values = aortaRows[row];

        setReadOnlyItem(tableRow, 0, values[0]);
        setValueItem(tableRow, 1);
        setReadOnlyItem(tableRow, 2, values[2]);
        setReadOnlyItem(tableRow, 3, values[3]);
        setValueItem(tableRow, 4);
        setReadOnlyItem(tableRow, 5, values[5]);
    }

    ui->tableWidget->setSpan(17, 0, 1, 6);
    setReadOnlyItem(17, 0, "Правый желудочек", true);

    const QVector<QStringList> rightVentricleRows = {
        {"ПЖ(ПЗР), мм", "", "20-30мм", "TAPSE, мм", "", ">17мм"},
        {"Баз ПЖ, мм", "", "25-42мм", "Толщина стенки ПЖ", "", "1-5 мм"}
    };

    for (int row = 0; row < rightVentricleRows.size(); ++row)
    {
        const int tableRow = row + 18;
        const QStringList values = rightVentricleRows[row];

        setReadOnlyItem(tableRow, 0, values[0]);
        setValueItem(tableRow, 1);
        setReadOnlyItem(tableRow, 2, values[2]);
        setReadOnlyItem(tableRow, 3, values[3]);
        setValueItem(tableRow, 4);
        setReadOnlyItem(tableRow, 5, values[5]);
    }

    ui->tableWidget->setSpan(20, 0, 1, 6);
    setReadOnlyItem(20, 0, "Правое предсердие", true);

    const QVector<QStringList> rightAtriumRows = {
        {"Объем ПП, мл", "", "-", "Площадь ПП, см²", "", "<18см²"},
        {"Объем ПП индекс, мл/м²", "", "М 18-32мл/м², Ж15-27мл/м²", "", "", ""}
    };

    for (int row = 0; row < rightAtriumRows.size(); ++row)
    {
        const int tableRow = row + 21;
        const QStringList values = rightAtriumRows[row];

        setReadOnlyItem(tableRow, 0, values[0]);
        setValueItem(tableRow, 1);
        setReadOnlyItem(tableRow, 2, values[2]);
        setReadOnlyItem(tableRow, 3, values[3]);
        setValueItem(tableRow, 4);
        setReadOnlyItem(tableRow, 5, values[5]);
    }

    ui->tableWidget->setSpan(23, 0, 1, 6);
    setReadOnlyItem(23, 0, "Нижняя полая вена", true);

    const QVector<QStringList> ivcRows = {
        {"НПВ выдох, мм", "", "<21мм", "НПВ вдох, мм", "", "-"}
    };

    for (int row = 0; row < ivcRows.size(); ++row)
    {
        const int tableRow = row + 24;
        const QStringList values = ivcRows[row];

        setReadOnlyItem(tableRow, 0, values[0]);
        setValueItem(tableRow, 1);
        setReadOnlyItem(tableRow, 2, values[2]);
        setReadOnlyItem(tableRow, 3, values[3]);
        setValueItem(tableRow, 4);
        setReadOnlyItem(tableRow, 5, values[5]);
    }

    ui->tableWidget->setSpan(25, 0, 1, 6);
    setReadOnlyItem(25, 0, "Расчетное систолическое давление в легочной артерии", true);

    const QVector<QStringList> pulmonaryRows = {
        {"Макс.градиентТР, мм рт.ст.", "", "", "СДЛА, мм рт.ст.", "", "<31мм.рт.ст."}
    };

    for (int row = 0; row < pulmonaryRows.size(); ++row)
    {
        const int tableRow = row + 26;
        const QStringList values = pulmonaryRows[row];

        setReadOnlyItem(tableRow, 0, values[0]);
        setValueItem(tableRow, 1);
        setReadOnlyItem(tableRow, 2, values[2]);
        setReadOnlyItem(tableRow, 3, values[3]);
        setValueItem(tableRow, 4);
        setReadOnlyItem(tableRow, 5, values[5]);
    }

    const int defaultRowHeight = 28;

    for (int row = 0; row < ui->tableWidget->rowCount(); ++row)
        ui->tableWidget->setRowHeight(row, defaultRowHeight);

    int tableHeight = ui->tableWidget->frameWidth() * 2;

    for (int row = 0; row < ui->tableWidget->rowCount(); ++row)
        tableHeight += ui->tableWidget->rowHeight(row);

    ui->tableWidget->setMinimumHeight(tableHeight);
    ui->tableWidget->setMaximumHeight(tableHeight + 80);

    ui->tableWidget->item(7,1)->setFlags(ui->tableWidget->item(7,1)->flags() & ~Qt::ItemIsEditable);
    ui->tableWidget->item(3,4)->setFlags(ui->tableWidget->item(3,4)->flags() & ~Qt::ItemIsEditable);
    ui->tableWidget->item(6,4)->setFlags(ui->tableWidget->item(6,4)->flags() & ~Qt::ItemIsEditable);
    ui->tableWidget->item(7,4)->setFlags(ui->tableWidget->item(7,4)->flags() & ~Qt::ItemIsEditable);
    ui->tableWidget->item(10,3)->setFlags(ui->tableWidget->item(10,3)->flags() & ~Qt::ItemIsEditable);
    ui->tableWidget->item(10,4)->setFlags(ui->tableWidget->item(10,4)->flags() & ~Qt::ItemIsEditable);
    ui->tableWidget->item(10,5)->setFlags(ui->tableWidget->item(10,5)->flags() & ~Qt::ItemIsEditable);
    ui->tableWidget->item(13,3)->setFlags(ui->tableWidget->item(13,3)->flags() & ~Qt::ItemIsEditable);
    ui->tableWidget->item(13,4)->setFlags(ui->tableWidget->item(13,4)->flags() & ~Qt::ItemIsEditable);
    ui->tableWidget->item(13,5)->setFlags(ui->tableWidget->item(13,5)->flags() & ~Qt::ItemIsEditable);
    ui->tableWidget->item(16,3)->setFlags(ui->tableWidget->item(16,3)->flags() & ~Qt::ItemIsEditable);
    // ui->tableWidget->item(16,4)->setFlags(ui->tableWidget->item(16,4)->flags() & ~Qt::ItemIsEditable); // Удаляем read-only для объединенной ячейки
    ui->tableWidget->item(16,5)->setFlags(ui->tableWidget->item(16,5)->flags() & ~Qt::ItemIsEditable);
    ui->tableWidget->item(22,3)->setFlags(ui->tableWidget->item(22,3)->flags() & ~Qt::ItemIsEditable);
    ui->tableWidget->item(22,4)->setFlags(ui->tableWidget->item(22,4)->flags() & ~Qt::ItemIsEditable);
    ui->tableWidget->item(22,5)->setFlags(ui->tableWidget->item(22,5)->flags() & ~Qt::ItemIsEditable);
    ui->tableWidget->item(12,4)->setFlags(ui->tableWidget->item(12,4)->flags() & ~Qt::ItemIsEditable);
    ui->tableWidget->item(22,1)->setFlags(ui->tableWidget->item(22,1)->flags() & ~Qt::ItemIsEditable);
    
    // Объединяем столбцы 4 и 5 строки 16 для ввода текста описания аорты
    ui->tableWidget->setSpan(16, 4, 1, 2);
    // Удаляем ячейку в столбце 5 строки 16, так как она теперь объединена с столбцом 4
    delete ui->tableWidget->takeItem(16, 5);
    // Создаем новую ячейку для объединенной области
    QTableWidgetItem *aortaDescriptionItem = new QTableWidgetItem();
    ui->tableWidget->setItem(16, 4, aortaDescriptionItem);
}

void MainWindow::setupValveDescriptionTable()
{
    ui->tableWidget_2->clear();
    ui->tableWidget_2->setColumnCount(4);
    ui->tableWidget_2->setRowCount(12);
    ui->tableWidget_2->horizontalHeader()->setVisible(false);
    ui->tableWidget_2->verticalHeader()->setVisible(false);
    ui->tableWidget_2->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableWidget_2->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->tableWidget_2->setWordWrap(false);
    ui->tableWidget_2->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->tableWidget_2->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->tableWidget_2->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableWidget_2->setSelectionBehavior(QAbstractItemView::SelectItems);
    ui->tableWidget_2->setAlternatingRowColors(true);

    auto setReadOnlyItem = [this](int row, int column, const QString& text, bool centerText = false)
    {
        auto *item = new QTableWidgetItem(text);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        if (centerText)
            item->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget_2->setItem(row, column, item);
    };

    auto setValueItem = [this](int row, int column)
    {
        ui->tableWidget_2->setItem(row, column, new QTableWidgetItem());
    };

    const QStringList headers = {
        "Параметр",
        "Значение",
        "Параметр",
        "Значение"
    };

    for (int column = 0; column < headers.size(); ++column)
        setReadOnlyItem(0, column, headers[column], true);

    ui->tableWidget_2->setSpan(1, 0, 1, 2);
    setReadOnlyItem(1, 0, "Митральный клапан", true);

    ui->tableWidget_2->setSpan(1, 2, 1, 2);
    setReadOnlyItem(1, 2, "Аортальный клапан", true);

    // Добавляем строку с описанием для митрального и аортального клапанов
    ui->tableWidget_2->setSpan(2, 0, 1, 2);
    QTableWidgetItem *mitralDescriptionItem = new QTableWidgetItem("");
    mitralDescriptionItem->setFlags(mitralDescriptionItem->flags() | Qt::ItemIsEditable);
    ui->tableWidget_2->setItem(2, 0, mitralDescriptionItem);

    ui->tableWidget_2->setSpan(2, 2, 1, 2);
    QTableWidgetItem *aortalDescriptionItem = new QTableWidgetItem("");
    aortalDescriptionItem->setFlags(aortalDescriptionItem->flags() | Qt::ItemIsEditable);
    ui->tableWidget_2->setItem(2, 2, aortalDescriptionItem);

    const QVector<QStringList> firstGroupRows = {
        {"Митральная регургитация, ст", "", "Аортальная регургитация, ст", ""},
        {"Митральный стеноз, ст", "", "Аортальный стеноз, ст", ""},
        {"Ср.градиентМК, мм рт.ст.", "", "Пик.скоростьАК, м/с", ""}
    };

    for (int row = 0; row < firstGroupRows.size(); ++row)
    {
        const int tableRow = row + 3;
        const QStringList values = firstGroupRows[row];

        setReadOnlyItem(tableRow, 0, values[0]);
        setValueItem(tableRow, 1);
        setReadOnlyItem(tableRow, 2, values[2]);
        setValueItem(tableRow, 3);
    }

    ui->tableWidget_2->setSpan(6, 0, 1, 2);
    setReadOnlyItem(6, 0, "Трикуспидальный клапан", true);

    ui->tableWidget_2->setSpan(6, 2, 1, 2);
    setReadOnlyItem(6, 2, "Пульмональный клапан", true);

    // Добавляем строку с описанием для трикуспидального и пульмонального клапанов
    ui->tableWidget_2->setSpan(7, 0, 1, 2);
    QTableWidgetItem *trikuspidalDescriptionItem = new QTableWidgetItem("");
    trikuspidalDescriptionItem->setFlags(trikuspidalDescriptionItem->flags() | Qt::ItemIsEditable);
    ui->tableWidget_2->setItem(7, 0, trikuspidalDescriptionItem);

    ui->tableWidget_2->setSpan(7, 2, 1, 2);
    QTableWidgetItem *pulmonalDescriptionItem = new QTableWidgetItem("");
    pulmonalDescriptionItem->setFlags(pulmonalDescriptionItem->flags() | Qt::ItemIsEditable);
    ui->tableWidget_2->setItem(7, 2, pulmonalDescriptionItem);

    // Применяем делегат плейсхолдера к ячейкам "Описание"
    PlaceholderDelegate *placeholderDelegate = new PlaceholderDelegate(this);
    ui->tableWidget_2->setItemDelegateForRow(2, placeholderDelegate);
    ui->tableWidget_2->setItemDelegateForRow(7, placeholderDelegate);

    const QVector<QStringList> secondGroupRows = {
        {"Трикуспидальная регургитация, ст", "", "Пульмональная регургитация, ст", ""},
        {"Трикуспидальный стеноз, ст", "", "Пульмональный стеноз, ст", ""},
        {"Пик.скорость ТР, м/с", "", "Пик.скорость ПК, м/с", ""},
        {"Пик.скорость ТК, м/с", "", "", ""}
    };

    for (int row = 0; row < secondGroupRows.size(); ++row)
    {
        const int tableRow = row + 8;
        const QStringList values = secondGroupRows[row];

        setReadOnlyItem(tableRow, 0, values[0]);
        setValueItem(tableRow, 1);
        setReadOnlyItem(tableRow, 2, values[2]);
        setValueItem(tableRow, 3);
    }

    const int defaultRowHeight = 28;

    for (int row = 0; row < ui->tableWidget_2->rowCount(); ++row)
        ui->tableWidget_2->setRowHeight(row, defaultRowHeight);

    int tableHeight = ui->tableWidget_2->frameWidth() * 2;

    for (int row = 0; row < ui->tableWidget_2->rowCount(); ++row)
        tableHeight += ui->tableWidget_2->rowHeight(row);

    ui->tableWidget_2->setMinimumHeight(tableHeight);
    ui->tableWidget_2->setMaximumHeight(tableHeight + 20);

    ui->tableWidget_2->item(11,3)->setFlags(ui->tableWidget_2->item(11,3)->flags() & ~Qt::ItemIsEditable);
}

void MainWindow::updateKdolzhIndexFromKdo()
{
    QTableWidgetItem *kdoItem = ui->tableWidget->item(6, 1);
    QTableWidgetItem *indexItem = ui->tableWidget->item(7, 1);

    if (!kdoItem || !indexItem)
        return;

    bool kdoOk = false;
    bool pptOk = false;
    double kdoValue = kdoItem->text().trimmed().replace(',', '.').toDouble(&kdoOk);
    double pptValue = ui->PPTResultLabel->text().trimmed().replace(',', '.').toDouble(&pptOk);

    if (!kdoOk || !pptOk || pptValue == 0.0)
    {
        QSignalBlocker blocker(ui->tableWidget);
        indexItem->setText(QString());
        return;
    }

    const double indexValue = kdoValue / pptValue;

    QSignalBlocker blocker(ui->tableWidget);
    indexItem->setText(QString::number(indexValue, 'f', 2));
}

void MainWindow::updateKsoLjIndexFromKso()
{
    QTableWidgetItem *ksoItem = ui->tableWidget->item(2, 4);
    QTableWidgetItem *indexItem = ui->tableWidget->item(3, 4);

    if (!ksoItem || !indexItem)
        return;

    bool tmjpOk = false;
    bool pptOk = false;
    double ksoValue = ksoItem->text().trimmed().replace(',', '.').toDouble(&tmjpOk);
    double pptValue = ui->PPTResultLabel->text().trimmed().replace(',', '.').toDouble(&pptOk);

    if (!tmjpOk || !pptOk || pptValue == 0.0)
    {
        QSignalBlocker blocker(ui->tableWidget);
        indexItem->setText(QString());
        return;
    }

    const double indexValue = ksoValue / pptValue;

    QSignalBlocker blocker(ui->tableWidget);
    indexItem->setText(QString::number(indexValue, 'f', 2));
}

void MainWindow::updateOtsLjFromTzsljAndKdrlj()
{
    QTableWidgetItem *tzsItem = ui->tableWidget->item(3, 1);
    QTableWidgetItem *kdrItem = ui->tableWidget->item(4, 1);
    QTableWidgetItem *indexItem = ui->tableWidget->item(6, 4);

    if (!tzsItem || !kdrItem || !indexItem)
        return;

    bool tzsOk = false;
    bool kdrOk = false;
    double tzsValue = tzsItem->text().trimmed().replace(',', '.').toDouble(&tzsOk);
    double kdrValue = kdrItem->text().trimmed().replace(',', '.').toDouble(&kdrOk);

    if (!tzsOk || !kdrOk)
    {
        QSignalBlocker blocker(ui->tableWidget);
        indexItem->setText(QString());
        return;
    }

    const double indexValue = 2 * tzsValue / kdrValue;

    QSignalBlocker blocker(ui->tableWidget);
    indexItem->setText(QString::number(indexValue, 'f', 2));
}

void MainWindow::updateImmLjFromTmjpTzsKdr()
{
    QTableWidgetItem *tmjpItem = ui->tableWidget->item(2, 1);
    QTableWidgetItem *tzsItem = ui->tableWidget->item(3, 1);
    QTableWidgetItem *kdrItem = ui->tableWidget->item(4, 1);
    QTableWidgetItem *indexItem = ui->tableWidget->item(7, 4);

    if (!tmjpItem || !tzsItem || !kdrItem || !indexItem)
        return;

    bool tmjpOk = false;
    bool tzsOk = false;
    bool kdrOk = false;
    double tmjpValue = tmjpItem->text().trimmed().replace(',', '.').toDouble(&tmjpOk);
    double tzsValue = tzsItem->text().trimmed().replace(',', '.').toDouble(&tzsOk);
    double kdrValue = kdrItem->text().trimmed().replace(',', '.').toDouble(&kdrOk);

    if (!tmjpOk || !tzsOk || !kdrOk)
    {
        QSignalBlocker blocker(ui->tableWidget);
        indexItem->setText(QString());
        return;
    }

    const double indexValue = 0.832 * ((std::pow((tmjpValue + tzsValue + kdrValue), 3)) - std::pow(kdrValue, 3)) + 0.6;

    QSignalBlocker blocker(ui->tableWidget);
    indexItem->setText(QString::number(indexValue, 'f', 3));
}

void MainWindow::updateLpFromDlpPpt()
{
    QTableWidgetItem *dlpItem = ui->tableWidget->item(13, 1);
    QTableWidgetItem *indexItem = ui->tableWidget->item(12, 4);

    if (!dlpItem || !indexItem)
        return;

    bool dlpOk = false;
    bool pptOk = false;

    double dlpValue = dlpItem->text().trimmed().replace(',', '.').toDouble(&dlpOk);
    double pptValue = ui->PPTResultLabel->text().trimmed().replace(',', '.').toDouble(&pptOk);

    if (!dlpOk || !pptOk || pptValue == 0.0)
    {
        QSignalBlocker blocker(ui->tableWidget);
        indexItem->setText(QString());
        return;
    }

    const double indexValue = dlpValue / pptValue;

    QSignalBlocker blocker(ui->tableWidget);
    indexItem->setText(QString::number(indexValue, 'f', 3));
}

void MainWindow::updatePpFromPpPpt()
{
    QTableWidgetItem *ppItem = ui->tableWidget->item(21, 1);
    QTableWidgetItem *indexItem = ui->tableWidget->item(22, 1);

    if (!ppItem || !indexItem)
        return;

    bool ppOk = false;
    bool pptOk = false;

    double ppValue = ppItem->text().trimmed().replace(',', '.').toDouble(&ppOk);
    double pptValue = ui->PPTResultLabel->text().trimmed().replace(',', '.').toDouble(&pptOk);

    if (!ppOk || !pptOk || pptValue == 0.0)
    {
        QSignalBlocker blocker(ui->tableWidget);
        indexItem->setText(QString());
        return;
    }

    const double indexValue = ppValue / pptValue;

    QSignalBlocker blocker(ui->tableWidget);
    indexItem->setText(QString::number(indexValue, 'f', 3));
}

void MainWindow::updateKdoljFromKdrlj()
{
    QTableWidgetItem *KdrljItem = ui->tableWidget->item(4, 1);
    QTableWidgetItem *KdoljItem = ui->tableWidget->item(6, 1);

    if (!KdrljItem)
        return;

    bool KdrljOk = false;

    double KdrljValue = KdrljItem->text().trimmed().replace(',', '.').toDouble(&KdrljOk);

    if (!KdrljOk)
    {
        QSignalBlocker blocker(ui->tableWidget);
        KdoljItem->setText(QString());
        return;
    }

    const double indexValue = (7/(2.4 + KdrljValue)) * std::pow(KdrljValue, 3);
    //std::pow(kdrValue, 3))

    QSignalBlocker blocker(ui->tableWidget);
    KdoljItem->setText(QString::number(indexValue, 'f', 3));
    updateYoljFromKdoKso();
    updateKdolzhIndexFromKdo();
}

void MainWindow::updateKsoljFromKsrlj()
{
    QTableWidgetItem *KsrljItem = ui->tableWidget->item(5, 1);
    QTableWidgetItem *KsoljItem = ui->tableWidget->item(2, 4);

    if (!KsrljItem || !KsoljItem)
        return;

    bool KsrljOk = false;

    double KsrljValue = KsrljItem->text().trimmed().replace(',', '.').toDouble(&KsrljOk);

    if (!KsrljOk)
    {
        QSignalBlocker blocker(ui->tableWidget);
        KsoljItem->setText(QString());
        return;
    }

    const double indexValue = (7/(2.4 + KsrljValue)) * std::pow(KsrljValue, 3);
    //std::pow(kdrValue, 3))

    QSignalBlocker blocker(ui->tableWidget);
    KsoljItem->setText(QString::number(indexValue, 'f', 3));
    updateYoljFromKdoKso();
    updateKsoLjIndexFromKso();
}

void MainWindow::updateYoljFromKdoKso()
{
    QTableWidgetItem *KdoljItem = ui->tableWidget->item(6, 1);
    QTableWidgetItem *KsoljItem = ui->tableWidget->item(2, 4);
    QTableWidgetItem *YoljItem = ui->tableWidget->item(5, 4);

    if (!KdoljItem || !KsoljItem || !YoljItem)
        return;

    bool KdoljOk = false;
    bool KsrljOk = false;

    double KdoljValue = KdoljItem->text().trimmed().replace(',', '.').toDouble(&KdoljOk);
    double KsoljValue = KsoljItem->text().trimmed().replace(',', '.').toDouble(&KsrljOk);

    if (!KdoljOk || !KsrljOk)
    {
        QSignalBlocker blocker(ui->tableWidget);
        YoljItem->setText(QString());
        return;
    }

    const double indexValue = KdoljValue - KsoljValue;
    //std::pow(kdrValue, 3))

    QSignalBlocker blocker(ui->tableWidget);
    YoljItem->setText(QString::number(indexValue, 'f', 3));
}

void MainWindow::loadComboHistory(QComboBox *comboBox,
                                  const QString& settingsKey)
{
    // Пустой ключ защищает от случайного вызова для поля без истории.
    if (settingsKey.isEmpty())
    {
        comboBox->clear();
        comboBox->setCurrentText("");
        return;
    }

    QSettings settings;
    comboBox->clear();

    // QSettings хранит QStringList, поэтому можно напрямую восстановить
    // элементы выпадающего списка.
    comboBox->addItems(settings.value(settingsKey).toStringList());

    // Для поля "Модель ультразвукового аппарата" добавляем две фиксированные записи,
    // которые нельзя удалить
    if (settingsKey == ApparatModelHistoryKey)
    {
        const QStringList fixedItems = {"MINDRAY DC-70 PRO", "RESONA 7S"};
        
        // Добавляем фиксированные записи, если их еще нет в списке
        for (const QString &fixedItem : fixedItems)
        {
            if (comboBox->findText(fixedItem) == -1)
            {
                comboBox->addItem(fixedItem);
            }
        }
    }

    // После загрузки истории само поле оставляем пустым: сохраненные значения
    // должны быть вариантами выбора, а не автоматически выбранным текстом.
    comboBox->setCurrentText("");
}

void MainWindow::rememberComboText(QComboBox *comboBox,
                                   const QString& settingsKey)
{
    if (settingsKey.isEmpty())
        return;

    QString text = comboBox->currentText().trimmed();

    if (text.isEmpty())
        return;

    QStringList values;

    // Берем текущий список из комбобокса. Так мы сохраняем и удаленные через
    // крестик изменения, и порядок, который уже видит пользователь.
    for (int i = 0; i < comboBox->count(); i++)
        values.append(comboBox->itemText(i));

    // Убираем дубль и добавляем новое/повторно введенное значение наверх.
    values.removeAll(text);
    values.prepend(text);

    // Чтобы список не разрастался бесконечно, храним только последние значения.
    while (values.size() > MaxComboHistoryItems)
        values.removeLast();

    // Пересобираем элементы комбобокса из нового списка и возвращаем введенный
    // текст в поле, чтобы пользователь не увидел сброс ввода.
    comboBox->clear();
    comboBox->addItems(values);
    comboBox->setCurrentText(text);

    saveComboHistory(comboBox, settingsKey);
}

void MainWindow::saveComboHistory(QComboBox *comboBox,
                                  const QString& settingsKey)
{
    if (settingsKey.isEmpty())
        return;

    QStringList values;

    // Сохраняем именно текущие элементы комбобокса. После удаления крестиком
    // ненужного пункта его здесь уже не будет.
    for (int i = 0; i < comboBox->count(); i++)
        values.append(comboBox->itemText(i));

    QSettings settings;
    settings.setValue(settingsKey, values);
}

bool MainWindow::removeClickedComboItem(QComboBox *comboBox,
                                        const QString& settingsKey,
                                        QEvent *event)
{
    if (event->type() != QEvent::MouseButtonPress
        && event->type() != QEvent::MouseButtonRelease)
    {
        return false;
    }

    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    QAbstractItemView *view = comboBox->view();
    QModelIndex index = view->indexAt(mouseEvent->pos());

    if (!index.isValid())
        return false;

    // Крестик рисуется в правой части popup-строки. Если клик левее этой зоны,
    // это обычный выбор значения, его должен обработать стандартный QComboBox.
    bool clickedDelete =
        mouseEvent->pos().x() >= view->viewport()->width() - DeleteAreaWidth;

    if (!clickedDelete)
        return false;

    if (event->type() == QEvent::MouseButtonRelease)
    {
        // На release удаляем пункт и сразу пишем новый список в QSettings.
        // MouseButtonPress тоже перехватываем, чтобы QComboBox не успел выбрать
        // удаляемый пункт как обычное значение.
        QString removedText = comboBox->itemText(index.row());
        
        // Для поля "Модель ультразвукового аппарата" запрещаем удаление фиксированных записей
        if (settingsKey == ApparatModelHistoryKey)
        {
            const QStringList fixedItems = {"MINDRAY DC-70 PRO", "RESONA 7S"};
            if (fixedItems.contains(removedText))
            {
                // Не удаляем фиксированные записи
                return true;
            }
        }
        
        comboBox->removeItem(index.row());
        saveComboHistory(comboBox, settingsKey);

        // Если удалили то, что сейчас отображалось в поле, очищаем поле.
        if (comboBox->currentText() == removedText)
            comboBox->setCurrentText("");

        // После удаления Qt закрывает popup. Открываем обрано, чтобы можно
        // было быстро удалить несколько старых вариантов подряд.
        comboBox->showPopup();
    }

    return true;
}

bool MainWindow::replacePlaceholdersInXmlFiles(
    const QString& directoryPath,
    const QMap<QString, QString>& placeholders)
{
    QDirIterator iterator(directoryPath,
                          QStringList() << "*.xml",
                          QDir::Files,
                          QDirIterator::Subdirectories);

    // Плейсхолдеры в docx могут лежать не только в word/document.xml, но и в
    // колонтитулах, сносках, таблицах и других XML-файлах пакета.
    while (iterator.hasNext())
    {
        if (!replacePlaceholdersInFile(iterator.next(), placeholders))
            return false;
    }

    return true;
}

QString MainWindow::xmlEscaped(const QString& value) const
{
    return value.toHtmlEscaped();
}

bool MainWindow::extractDocxTemplate(const QString& sevenZipPath,
                                     const QString& templateDocx,
                                     const QString& tempDir)
{
    QStringList extractArgs;
    extractArgs << "x"
                << templateDocx
                << "-o" + tempDir
                << "-y";

    QProcess process;
    process.start(sevenZipPath, extractArgs);

    if (!process.waitForFinished())
        return false;

    if (process.exitCode() != 0)
    {
        qDebug() << "Extract error:" << process.readAllStandardError();
        return false;
    }

    return true;
}

bool MainWindow::packDocx(const QString& sevenZipPath,
                          const QString& tempDir,
                          const QString& outputPath)
{
    QStringList archiveArgs;
    archiveArgs << "a"
                << "-tzip"
                << outputPath
                << ".";

    QProcess process;
    process.setWorkingDirectory(tempDir);
    process.start(sevenZipPath, archiveArgs);

    if (!process.waitForFinished())
        return false;

    if (process.exitCode() != 0)
    {
        qDebug() << "Archive error:" << process.readAllStandardError();
        return false;
    }

    return true;
}

void MainWindow::on_Save_clicked()
{
    rememberAllComboTexts();

    QString exeDir = QCoreApplication::applicationDirPath();
    QString templateDocx = exeDir + "/template/Shablon.docx";
    QString sevenZipPath = exeDir + "/7z.exe";

    if (!QFileInfo::exists(templateDocx))
    {
        QMessageBox::critical(this,
                              "Ошибка",
                              "Не найден шаблон протокола:\n" + templateDocx);
        return;
    }

    if (!QFileInfo::exists(sevenZipPath))
    {
        QMessageBox::critical(this,
                              "Ошибка",
                              "Не найден архиватор 7z.exe:\n" + sevenZipPath);
        return;
    }

    QString fileName;
    if(currentPatient.fullName.isEmpty()) {
        fileName = "Пациент не указан_" + ui->dateEdit->date().toString("dd.MM.yyyy");
    }
    else {
        fileName = currentPatient.fullName + "_" + ui->dateEdit->date().toString("dd.MM.yyyy");
    }

    QString outputPath = QFileDialog::getSaveFileName(
        this,
        "Сохранить протокол",
        fileName,
        "Word (*.docx)"
        );

    if (outputPath.isEmpty())
        return;

    if (QFileInfo(outputPath).suffix().isEmpty())
        outputPath += ".docx";

    QTemporaryDir tempDir;

    if (!tempDir.isValid())
    {
        QMessageBox::critical(this,
                              "Ошибка",
                              "Не удалось создать временную папку.");
        return;
    }

    if (!extractDocxTemplate(sevenZipPath, templateDocx, tempDir.path()))
    {
        QMessageBox::critical(this,
                              "Ошибка",
                              "Не удалось распаковать шаблон протокола.");
        return;
    }

    QMap<QString, QString> placeholders = collectProtocolPlaceholders();

    if (!replacePlaceholdersInXmlFiles(tempDir.path(), placeholders))
    {
        QMessageBox::critical(this,
                              "Ошибка",
                              "Не удалось заменить плейсхолдеры в шаблоне.");
        return;
    }

    QFile::remove(outputPath);

    if (!packDocx(sevenZipPath, tempDir.path(), outputPath))
    {
        QMessageBox::critical(this,
                              "Ошибка",
                              "Не удалось сохранить готовый протокол.");
        return;
    }

    QMessageBox::information(this,
                             "Готово",
                             "Протокол сохранен:\n" + outputPath);
}

void MainWindow::openPatientDialog()
{
    PatientDialog dialog(this);

    if (dialog.exec() == QDialog::Accepted)
    {
        Patient p = dialog.selectedPatient();
        ui->currentPatient->setText(p.fullName);

        currentPatient = p;
    }
}

void MainWindow::on_pushButton_clicked()
{
    rememberAllComboTexts();

    QString exeDir = QCoreApplication::applicationDirPath();
    QString templateDocx = exeDir + "/template/Shablon.docx";
    QString sevenZipPath = exeDir + "/7z.exe";

    if (!QFileInfo::exists(templateDocx))
    {
        QMessageBox::critical(this, "Ошибка", "Не найден шаблон протокола:\n" + templateDocx);
        return;
    }

    if (!QFileInfo::exists(sevenZipPath))
    {
        QMessageBox::critical(this, "Ошибка", "Не найден архиватор 7z.exe:\n" + sevenZipPath);
        return;
    }

    QTemporaryDir tempDir;
    tempDir.setAutoRemove(false);
    if (!tempDir.isValid())
    {
        QMessageBox::critical(this, "Ошибка", "Не удалось создать временную папку.");
        return;
    }

    QString outputPath = tempDir.path() + "/Предпросмотр.docx";

    if (!extractDocxTemplate(sevenZipPath, templateDocx, tempDir.path()))
    {
        QMessageBox::critical(this, "Ошибка", "Не удалось распаковать шаблон протокола.");
        return;
    }

    QMap<QString, QString> placeholders = collectProtocolPlaceholders();

    if (!replacePlaceholdersInXmlFiles(tempDir.path(), placeholders))
    {
        QMessageBox::critical(this, "Ошибка", "Не удалось заменить плейсхолдеры в шаблоне.");
        return;
    }

    if (!packDocx(sevenZipPath, tempDir.path(), outputPath))
    {
        QMessageBox::critical(this, "Ошибка", "Не удалось сохранить готовый протокол.");
        return;
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(outputPath)))
    {
        QMessageBox::critical(this, "Ошибка", "Не удалось открыть предпросмотр протокола.");
    }
}
