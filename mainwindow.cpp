#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "test_network.h"
#include "portfinder.h"
#include "configmanager.h"
#include <QLabel>
#include <QMessageBox>
#include <QTimer>
#include <QDebug>
#include <QButtonGroup>
#include <winsock.h>
#include <QApplication>
#include <QCoreApplication>
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_network(nullptr),
    m_portFinder(nullptr),
    m_configManager(nullptr),
    m_connected(false)
{
    ui->setupUi(this);

    // Создаём компоненты
    m_configManager = new ConfigManager(this);
    m_network = new YLSNetwork(this);
    m_portFinder = new PortFinder(this);

    // Настраиваем UI
    setupUI();
    setupInputLamps();

    // Настраиваем соединения
    setupConnections();

    // Загружаем конфигурацию в UI
    loadConfigToUI();

    // Если включено автоподключение, подключаемся
    if (m_configManager->autoConnect()) {
        on_connectButton_clicked();
    }

     updateUIState(false);
}

MainWindow::~MainWindow()
{
    if (m_network) {
        m_network->disconnect();
    }

    if (m_portFinder) {
        m_portFinder->stopSearch();
    }

    delete ui;
}

void MainWindow::setupUI()
{
    // Собираем все лампочки в список
    m_lamps.clear();
    m_lamps.append(ui->lamp0);
    m_lamps.append(ui->lamp1);
    m_lamps.append(ui->lamp2);
    m_lamps.append(ui->lamp3);
    m_lamps.append(ui->lamp4);
    m_lamps.append(ui->lamp5);
    m_lamps.append(ui->lamp6);
    m_lamps.append(ui->lamp7);

    // Устанавливаем начальный стиль
    updateLamps(0x00);

    // Обновляем состояние UI
    updateUIState(false);
}

void MainWindow::setupInputLamps()
{
    m_InputLamps.clear();
    m_InputLamps.append(ui->lampcommandmask01);
    m_InputLamps.append(ui->lampcommandmask02);
    m_InputLamps.append(ui->lampcommandmask03);
    m_InputLamps.append(ui->lampcommandmask04);
    m_InputLamps.append(ui->lampcommandmask05);
    m_InputLamps.append(ui->lampcommandmask06);
    m_InputLamps.append(ui->lampcommandmask07);
    m_InputLamps.append(ui->lampcommandmask08);

    m_inputLampGroup = new QButtonGroup(this);
    m_inputLampGroup->setExclusive(false); // Можно выбирать несколько

    for (int i = 0; i <  m_InputLamps.size(); i++) {
        QPushButton* button =  m_InputLamps[i];

        // Настраиваем кнопку
        button->setCheckable(true);
        button->setChecked(false);
        button->setAutoExclusive(false);

        // Добавляем в группу (ID = номер бита)
        m_inputLampGroup->addButton(button, i);

        // Стиль по умолчанию (серый)
        button->setStyleSheet(getLampButtonStyle(false, false));
    }

    // 4. Подключаем сигнал от группы
    connect(m_inputLampGroup, &QButtonGroup::buttonToggled,
            this, &MainWindow::onInputLampToggled);

    // 5. Инициализируем маску (все биты 0)
    m_inputMask = 0x00;
}

void MainWindow::onInputLampToggled(QAbstractButton* abstractButton, bool checked) {
    //  Преобразуем в QPushButton
    QPushButton* button = qobject_cast<QPushButton*>(abstractButton);
    if (!button) return;



    //  Получаем ID кнопки (номер бита)
    int bitIndex = m_inputLampGroup->id(button);
    if (bitIndex < 0 || bitIndex > 7) return;

    //  Обновляем маску
    if (checked) {
        m_inputMask |= (1 << bitIndex);  // Устанавливаем бит
    } else {
        m_inputMask &= ~(1 << bitIndex); // Сбрасываем бит
    }

    if (m_network && m_network->isConnected()) {
        m_network->setCurrentMask(m_inputMask);
    }

    //  Обновляем вид кнопки
    updateInputLamps();

    //  Если включена периодическая отправка - сразу отправляем
    if (ui->periodicCheckBox->isChecked()) {
        m_network->sendCurrentCommand();
    }

    qDebug() << "Маска обновлена: 0x" << QString::number(m_inputMask, 16)
             << " (бинарно: " << QString::number(m_inputMask, 2).rightJustified(8, '0') << ")";
}



void MainWindow::updateInputLamps()
{
    // Обновляем все кнопки ввода
    for (int i = 0; i < m_InputLamps.size(); i++) {
        QPushButton* button = m_InputLamps[i];
        bool isChecked = (m_inputMask >> i) & 0x01;
        bool isEnabled = button->isEnabled();

        // Устанавливаем стиль в зависимости от состояния
        button->setStyleSheet(getLampButtonStyle(isChecked, isEnabled));
        button->setChecked(isChecked); // Синхронизируем состояние
    }
}

QString MainWindow::getLampButtonStyle(bool checked, bool enabled) {
    QString color;

    if (!enabled) {
        color = "#404040"; // Тёмно-серый - отключено
    } else if (checked) {
        color = "#0000FF"; // Синий - выбрано
    } else {
        color = "#A0A0A0"; // Серый - не выбрано
    }

    return QString(
               "QPushButton {"
               "   background-color: %1;"
               "   border-radius: 25px;"
               "   border: 2px solid black;"
               "   font-weight: bold;"
               "   color: white;"
               "}"
               "QPushButton:hover {"
               "   background-color: %2;"
               "}"
               "QPushButton:pressed {"
               "   background-color: %3;"
               "}"
               ).arg(color)
        .arg(checked ? "#3333FF" : "#C0C0C0")  // Цвет при наведении
        .arg(checked ? "#6666FF" : "#808080"); // Цвет при нажатии
}




void MainWindow::setupConnections()
{
    // Подключаем сигналы от сети
    connect(m_network, &YLSNetwork::responseReceived,
            this, &MainWindow::onResponseReceived);
    connect(m_network, &YLSNetwork::errorOccurred,
            this, &MainWindow::onNetworkError);
    connect(m_network, &YLSNetwork::connected,
            this, &MainWindow::onNetworkConnected);
    connect(m_network, &YLSNetwork::disconnected,
            this, &MainWindow::onNetworkDisconnected);
    connect(m_network, &YLSNetwork::periodicSendingChanged,
            this, &MainWindow::onPeriodicSendingChanged);

    // Подключаем сигналы поиска порта
    connect(m_portFinder, &PortFinder::searchStarted,
            this, &MainWindow::onPortSearchStarted);
    connect(m_portFinder, &PortFinder::searchProgress,
            this, &MainWindow::onPortSearchProgress);
    connect(m_portFinder, &PortFinder::portFound,
            this, &MainWindow::onPortFound);
    connect(m_portFinder, &PortFinder::searchFinished,
            this, &MainWindow::onPortSearchFinished);
    connect(m_portFinder, &PortFinder::errorOccurred,
            this, &MainWindow::onNetworkError);

    // Подключаем сигналы конфигурации
    connect(m_configManager, &ConfigManager::configChanged,
            this, &MainWindow::onConfigChanged);
}

void MainWindow::loadConfigToUI()
{
    ui->ipLineEdit->setText(m_configManager->ylsIp());
    ui->portSpinBox->setValue(m_configManager->ylsPort());
    ui->listenPortSpinBox->setValue(m_configManager->listenPort());
    ui->searchStartPortSpinBox->setValue(m_configManager->searchStartPort());
    ui->searchEndPortSpinBox->setValue(m_configManager->searchEndPort());
    ui->autoConnectCheckBox->setChecked(m_configManager->autoConnect());
}

void MainWindow::saveConfigFromUI()
{
    m_configManager->setYlsIp(ui->ipLineEdit->text());
    m_configManager->setYlsPort(ui->portSpinBox->value());
    m_configManager->setListenPort(ui->listenPortSpinBox->value());
    m_configManager->setSearchStartPort(ui->searchStartPortSpinBox->value());
    m_configManager->setSearchEndPort(ui->searchEndPortSpinBox->value());
    m_configManager->setAutoConnect(ui->autoConnectCheckBox->isChecked());

    m_configManager->saveConfig();
}

void MainWindow::updateUIState(bool connected)
{
    m_connected = connected;

    if (connected) {
        // Обновляем заголовок
        ui->lampsGroupBox->setTitle(
            QString("Состояние лампочек (блок %1) - Подключено").arg(m_currentBlock));

        ui->connectButton->setEnabled(false);
        ui->disconnectButton->setEnabled(true);
        ui->testButton->setEnabled(true);
        ui->findPortButton->setEnabled(false);
        ui->lampsGroupBox->setTitle(QString("Состояние лампочек (блок %1) - Подключено").arg(m_currentBlock));
        // Обновляем статус в infoGroupBox
        ui->statusLabel->setText("Подключено");
        ui->statusLabel->setStyleSheet("color: green; font-weight: bold;");

        // Управление кнопками
        ui->connectButton->setEnabled(false);
        ui->disconnectButton->setEnabled(true);
        ui->testButton->setEnabled(!ui->periodicCheckBox->isChecked()); // Если не периодическая
        ui->findPortButton->setEnabled(false);
    }
        else {
        // Обновляем заголовок
        ui->lampsGroupBox->setTitle(
            QString("Состояние лампочек (блок %1) - Нет подключения").arg(m_currentBlock));

        // Обновляем статус в infoGroupBox
        ui->statusLabel->setText("Отключено");
        ui->statusLabel->setStyleSheet("color: red; font-weight: bold;");

        // Управление кнопками
        ui->connectButton->setEnabled(true);
        ui->disconnectButton->setEnabled(false);
        ui->testButton->setEnabled(false);
        ui->findPortButton->setEnabled(true);
        }
    // Блокируем/разблокируем кнопки-лампочки ввода
    for (auto button : m_InputLamps) {
        button->setEnabled(connected);
    }

    // Обновляем стили кнопок ввода (меняем цвет если отключены)
    updateInputLamps();
}

// Слоты для кнопок
void MainWindow::on_connectButton_clicked()
{
    // Сохраняем конфигурацию из UI
    saveConfigFromUI();

    // Получаем параметры подключения
    QString ip = ui->ipLineEdit->text();
    uint16_t ylsPort = static_cast<uint16_t>(ui->portSpinBox->value());
    uint16_t listenPort = static_cast<uint16_t>(ui->listenPortSpinBox->value());

    ui->connectButton->setEnabled(false);
    ui->connectButton->setText("Подключение...");
    ui->statusLabel->setText("Подключение...");
    ui->statusLabel->setStyleSheet("color: orange; font-weight: bold;");

    // Блокируем кнопки ввода
    for (auto button : m_InputLamps) {
        button->setEnabled(false);
    }
    updateInputLamps();

    // Подключаемся к ЯЛС
    if (!m_network->connectToYLS(ip, ylsPort, listenPort)) {
        // Если сразу ошибка (например, неверный IP)
        QMessageBox::warning(this, "Ошибка", "Неверные параметры подключения!");
        // Восстанавливаем UI
        ui->connectButton->setEnabled(true);
        ui->connectButton->setText("Подключиться");
        ui->statusLabel->setText("Отключено");
        ui->statusLabel->setStyleSheet("color: red; font-weight: bold;");
    }
}

void MainWindow::on_disconnectButton_clicked()
{
    m_network->disconnect();
    exit(0);
}

void MainWindow::on_testButton_clicked()
{
     if (!m_network || !m_connected) return;

    // Получаем параметры из UI
    m_currentBlock = ui->blockSpinBox->value();

     // Берем маску из выбранных кнопок (m_inputMask)
         uint8_t mask = m_inputMask;

    qDebug() << "Отправка запроса: блок" << m_currentBlock << "маска:" << QString("0x%1").arg(mask, 2, 16, QChar('0'));

    // Отправляем запрос
    m_network->sendCommand(m_currentBlock, mask);

    // Показываем отправленную маску
    ui->sentMaskLabel->setText(QString("0x%1").arg(mask, 2, 16, QChar('0').toUpper()));

    // Очищаем предыдущий полученный ответ
    ui->receivedMaskLabel->setText("ожидание...");
}

void MainWindow::on_findPortButton_clicked()
{
    // Получаем диапазон поиска из UI
    uint16_t startPort = static_cast<uint16_t>(ui->searchStartPortSpinBox->value());
    uint16_t endPort = static_cast<uint16_t>(ui->searchEndPortSpinBox->value());

    if (startPort > endPort) {
        QMessageBox::warning(this, "Ошибка", "Начальный порт должен быть меньше конечного!");
        return;
    }

    // Получаем IP и порт ЯЛС
    QString ip = ui->ipLineEdit->text();
    QHostAddress ylsAddress(ip);
    if (ylsAddress.isNull()) {
        QMessageBox::warning(this, "Ошибка", "Неверный IP-адрес ЯЛС!");
        return;
    }

    uint16_t ylsPort = static_cast<uint16_t>(ui->portSpinBox->value());

    // Запускаем поиск порта
    m_portFinder->startSearch(ylsAddress, ylsPort, startPort, endPort);
}


void MainWindow::on_periodicCheckBox_toggled(bool checked)
{
    if (!m_network) return;

    int interval = ui->periodicIntervalSpinBox->value();

    if (checked) {
        // Включаем периодическую отправку
        m_network->setPeriodicSending(true, interval);

        // Блокируем кнопку отправки
        ui->testButton->setEnabled(false);
        ui->testButton->setText("Периодическая отправка...");
    } else {
        // Выключаем периодическую отправку
        m_network->setPeriodicSending(false);

        // Разблокируем кнопку отправки (если подключены)
        ui->testButton->setEnabled(m_connected);
        ui->testButton->setText("Отправить команду");
    }

}

void MainWindow::on_saveConfigButton_clicked()
{
    saveConfigFromUI();
    QMessageBox::information(this, "Конфигурация", "Настройки сохранены в файл config.ini");
}

// Слоты для сети
void MainWindow::onResponseReceived(int block, uint8_t mask)
{
    qDebug() << "Получен ответ: блок" << block
             << "маска:" << QString("0x%1").arg(mask, 2, 16, QChar('0'));

    // Обновляем UI только если ответ для текущего блока
    if (block == m_currentBlock) {
        // Обновляем лампочки полученной маской
        updateLamps(mask);

        // Показываем полученную маску
        ui->receivedMaskLabel->setText(
            QString("0x%1 (%2)")
                .arg(mask, 2, 16, QChar('0').toUpper())
                .arg(maskToBinaryString(mask)));

        // Сохраняем текущую маску
        m_currentMask = mask;
    } else {
        qDebug() << "Получен ответ для другого блока:" << block << "ожидали:" << m_currentBlock;
    }
}

void MainWindow::onNetworkError(const QString& message)
{
    QMessageBox::warning(this, "Ошибка сети", message);
    qDebug() << "Ошибка сети:" << message;

    // Восстанавливаем UI при ошибке
    ui->connectButton->setEnabled(true);
    ui->connectButton->setText("Подключиться");
    ui->statusLabel->setText("Отключено");
    ui->statusLabel->setStyleSheet("color: red; font-weight: bold;");
    ui->lampsGroupBox->setTitle(QString("Состояние лампочек (блок %1) - Нет подключения").arg(m_currentBlock));

    // Блокируем кнопки ввода
    for (auto button : m_InputLamps) {
        button->setEnabled(false);
    }
    updateInputLamps();
}

void MainWindow::onNetworkConnected()
{
    qDebug() << "Подключение к сети установлено";
    updateUIState(true);
}

void MainWindow::onNetworkDisconnected()
{
    qDebug() << "Подключение к сети установлено";
    updateUIState(true);

    // Разблокируем кнопки ввода
    for (auto button : m_InputLamps) {
        button->setEnabled(true);
    }
    updateInputLamps();
}
void MainWindow::onPeriodicSendingChanged(bool enabled)
{
    ui->periodicCheckBox->setChecked(enabled);
    qDebug() << "Периодическая отправка:" << (enabled ? "включена" : "выключена");
}

// Слоты для поиска порта
void MainWindow::onPortSearchStarted()
{
    qDebug() << "Поиск порта начат";
    ui->findPortButton->setEnabled(false);
    ui->findPortButton->setText("Поиск...");
    ui->searchProgressLabel->setText("Поиск порта...");
    ui->searchProgressBar->setValue(0);
    ui->searchProgressBar->setVisible(true);
}

void MainWindow::onPortSearchProgress(int current, int total)
{
    if (total > 0) {
        int progress = (current * 100) / total;
        ui->searchProgressBar->setValue(progress);
        ui->searchProgressLabel->setText(QString("Проверка порта %1/%2").arg(current).arg(total));
    }
}

void MainWindow::onPortFound(uint16_t port)
{
    qDebug() << "Порт найден:" << port;

    // Обновляем поле порта приёма
    ui->listenPortSpinBox->setValue(port);

    // Обновляем UI
    ui->searchProgressLabel->setText(QString("Порт найден: %1").arg(port));
    ui->searchProgressBar->setValue(100);

    // Показываем сообщение
    QMessageBox::information(this, "Успех",
                             QString("Порт найден: %1\nТеперь можно подключиться.").arg(port));

    // Сохраняем найденный порт в конфиг
    saveConfigFromUI();
}

void MainWindow::onPortSearchFinished(bool success, const QString& message)
{
    if (!success) {
        qDebug() << "Поиск порта завершился неудачей:" << message;
        ui->searchProgressLabel->setText("Порт не найден");
        ui->searchProgressBar->setValue(0);

        QMessageBox::warning(this, "Поиск порта",
                             QString("%1\nПроверьте:\n"
                                     "1. Подключение к сети\n"
                                     "2. IP-адрес ЯЛС\n"
                                     "3. Порт ЯЛС\n"
                                     "4. Диапазон поиска портов")
                                 .arg(message));
    }

    // Включаем кнопку поиска
    ui->findPortButton->setEnabled(true);
    ui->findPortButton->setText("Найти порт");

    // Через 3 секунды скрываем прогресс-бар
    QTimer::singleShot(3000, this, [this]() {
        ui->searchProgressBar->setVisible(false);
    });
}

// Слоты для конфигурации
void MainWindow::onConfigChanged()
{
    loadConfigToUI();
}

// Вспомогательные методы
void MainWindow::updateLamps(uint8_t mask)
{
    // Обновляем 8 лампочек в соответствии с битами маски
    for (int i = 0; i < 8; i++) {
        bool isActive = (mask >> i) & 0x01;

        if (i < m_lamps.size()) {
            QString color = isActive ? "green" : "red";
            m_lamps[i]->setStyleSheet(
                QString("QLabel {"
                        "background-color: %1;"
                        "border-radius: 20px;"
                        "border: 2px solid black;"
                        "font-weight: bold;"
                        "}").arg(color));

            // Показываем состояние бита
            m_lamps[i]->setText(QString("%1\n%2")
                                    .arg(i + 1)
                                    .arg(isActive ? "1" : "0"));
        }
    }
}

QString MainWindow::maskToBinaryString(uint8_t mask)
{
    QString binary;
    for (int i = 7; i >= 0; i--) {
        binary.append((mask >> i) & 0x01 ? "1" : "0");
    }
    return binary;
}
