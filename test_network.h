#pragma once

#include <QObject>
#include <cstdint>
#include <QHostAddress>
#include <QTimer>

class QUdpSocket;

class YLSNetwork : public QObject
{
    Q_OBJECT

signals:
    void responseReceived(int block, uint8_t mask);  // Сигнал при получении ответа
    void errorOccurred(const QString& message);     // Сигнал при ошибке
    void connected();                               // Сигнал успешного подключения
    void disconnected();                            // Сигнал отключения
    void periodicSendingChanged(bool enabled);      // Сигнал изменения режима периодической отправки

public:
    explicit YLSNetwork(QObject *parent = nullptr);
    ~YLSNetwork();

    // Подключение и отключение
    bool connectToYLS(const QString& ylsIP, uint16_t ylsPort, uint16_t listenPort);
    void disconnect();

    // Отправка команд
    void sendCommand(int block, uint8_t mask);
    void sendCurrentCommand();  // Отправка текущей команды (для периодической отправки)

    // Управление периодической отправкой
    void setPeriodicSending(bool enabled, int intervalMs = 100);
    bool isPeriodicSending() const { return m_periodicTimer && m_periodicTimer->isActive(); }

    // Проверка состояния
    bool isConnected() const { return m_connected; }
    uint16_t getListenPort() const { return m_listenPort; }

    void setCurrentBlock(int block) { m_currentBlock = block; }
    void setCurrentMask(uint8_t mask) { m_currentMask = mask; }

private slots:
    void onReadyRead();  // Слот для приёма данных
    void onPeriodicTimer();  // Слот для периодической отправки
     void onConnectionTimeout();

private:
    // Сокеты
    QUdpSocket* m_sendSocket = nullptr;
    QUdpSocket* m_listenSocket = nullptr;

    // Таймер для периодической отправки
    QTimer* m_periodicTimer = nullptr;

    // Параметры подключения
    QHostAddress m_ylsAddress;
    uint16_t m_ylsPort = 0;
    uint16_t m_listenPort = 0;

    // Текущая команда для периодической отправки
    int m_currentBlock = 4;
    uint8_t m_currentMask = 0x00;

    // Состояние
    bool m_connected = false;
    QTimer* m_connectionTimer = nullptr; //таймер подключения
};
