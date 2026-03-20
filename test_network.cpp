#include "test_network.h"
#include "YPSProtocol.h"
#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QTimer>
#include <QDebug>

YLSNetwork::YLSNetwork(QObject *parent) :
    QObject(parent),
    m_sendSocket(nullptr),
    m_listenSocket(nullptr),
    m_periodicTimer(nullptr),
    m_connectionTimer(nullptr)
{
}

YLSNetwork::~YLSNetwork()
{
    disconnect();
}

void YLSNetwork::onConnectionTimeout()
{
    if (!m_connected) {
        qDebug() << "Таймаут подключения: ЯЛС не ответила за 3 секунды";
        emit errorOccurred("ЯЛС не ответила на тестовый запрос. Проверьте подключение.");
        disconnect();
    }
}


bool YLSNetwork::connectToYLS(const QString& ylsIP, uint16_t ylsPort, uint16_t listenPort)
{
    // Если уже подключены, отключаемся
    if (m_connected) {
        disconnect();
    }

    // Пытаемся распарсить IP-адрес
    QHostAddress address(ylsIP);
    if (address.isNull()) {
        qDebug() << "Неверный IP-адрес:" << ylsIP;
        emit errorOccurred(QString("Неверный IP-адрес: %1").arg(ylsIP));
        return false;
    }

    m_ylsAddress = address;
    m_ylsPort = ylsPort;
    m_listenPort = listenPort;

    // Создаём сокет для отправки
    m_sendSocket = new QUdpSocket(this);
    if (!m_sendSocket->bind()) {
        qDebug() << "Не удалось привязать сокет отправки:" << m_sendSocket->errorString();
        emit errorOccurred(QString("Не удалось привязать сокет отправки: %1").arg(m_sendSocket->errorString()));
        delete m_sendSocket;
        m_sendSocket = nullptr;
        return false;
    }

    // Создаём сокет для приёма
    m_listenSocket = new QUdpSocket(this);
    if (!m_listenSocket->bind(QHostAddress::Any, m_listenPort)) {
        qDebug() << "Не удалось привязать сокет приёма к порту" << m_listenPort
                 << ":" << m_listenSocket->errorString();
        emit errorOccurred(QString("Не удалось привязать сокет приёма к порту %1: %2")
                               .arg(m_listenPort).arg(m_listenSocket->errorString()));
        delete m_listenSocket;
        m_listenSocket = nullptr;
        delete m_sendSocket;
        m_sendSocket = nullptr;
        return false;
    }

    // Если сокеты не созданы - возвращаем false
    if (!m_sendSocket || !m_listenSocket) {
        return false;
    }

    // Подключаем слот для приёма данных
    connect(m_listenSocket, &QUdpSocket::readyRead, this, &YLSNetwork::onReadyRead);

    // Создаём таймер для периодической отправки (но не запускаем)
    m_periodicTimer = new QTimer(this);
    m_periodicTimer->setInterval(100); // По умолчанию 100 мс
    connect(m_periodicTimer, &QTimer::timeout, this, &YLSNetwork::onPeriodicTimer);

    m_connected = true;

    qDebug() << "Подключено к ЯЛС:" << ylsIP << ":" << ylsPort
             << "Порт приёма:" << m_listenPort;

    emit connected();


    YPSProtocol::Request request = YPSProtocol::createRequest(0x00);
    request.cell_number = 4;

    qint64 bytesSent = m_sendSocket->writeDatagram(
        reinterpret_cast<const char*>(&request),
        sizeof(request),
        m_ylsAddress,
        m_ylsPort
        );

    if (bytesSent == -1) {
        qDebug() << "Ошибка отправки тестового пакета:" << m_sendSocket->errorString();
        emit errorOccurred(QString("Ошибка отправки тестового пакета: %1").arg(m_sendSocket->errorString()));
        disconnect();
        return false;
    }

    qDebug() << "Тестовый пакет отправлен, ожидание ответа от ЯЛС...";

    // Устанавливаем таймер ожидания ответа
    m_connectionTimer = new QTimer(this);
    m_connectionTimer->setSingleShot(true);
    m_connectionTimer->setInterval(3000); // 3 секунды на ответ

    connect(m_connectionTimer, &QTimer::timeout, this, [this]() {
        if (!m_connected) {
            qDebug() << "Таймаут подключения: ЯЛС не ответила";
            emit errorOccurred("ЯЛС не ответила на тестовый запрос. Проверьте подключение.");
            disconnect();
        }
    });

    m_connectionTimer->start();
     m_connected = false;
    return true; // Сокеты созданы, тестовый пакет отправлен
}

void YLSNetwork::disconnect()
{
    if (!m_connected) return;
    // Останавливаем таймер подключения
    if (m_connectionTimer) {
        m_connectionTimer->stop();
        m_connectionTimer->deleteLater();
        m_connectionTimer = nullptr;
    }
    // Останавливаем периодическую отправку
    if (m_periodicTimer) {
        m_periodicTimer->stop();
    }

    // Закрываем сокеты
    if (m_sendSocket) {
        m_sendSocket->close();
        m_sendSocket->deleteLater();
        m_sendSocket = nullptr;
    }

    if (m_listenSocket) {
        m_listenSocket->close();
        m_listenSocket->deleteLater();
        m_listenSocket = nullptr;
    }

    // Удаляем таймер
    if (m_periodicTimer) {
        m_periodicTimer->deleteLater();
        m_periodicTimer = nullptr;
    }

    m_connected = false;

    qDebug() << "Отключено от ЯЛС";
    emit disconnected();
}

void YLSNetwork::sendCommand(int block, uint8_t mask)
{
    if (!m_sendSocket) {
        emit errorOccurred("Сеть не подключена");
        return;
    }

    // Сохраняем текущую команду для периодической отправки
    m_currentBlock = block;
    m_currentMask = mask;

    // Создаём запрос по протоколу
    YPSProtocol::Request request = YPSProtocol::createRequest(mask);
    request.cell_number = static_cast<uint8_t>(block);

    // Отправляем датаграмму
    qint64 bytesSent = m_sendSocket->writeDatagram(
        reinterpret_cast<const char*>(&request),
        sizeof(request),
        m_ylsAddress,
        m_ylsPort
        );

    if (bytesSent == -1) {
        qDebug() << "Ошибка отправки:" << m_sendSocket->errorString();
        emit errorOccurred(QString("Ошибка отправки: %1").arg(m_sendSocket->errorString()));
    } else {
        qDebug() << "Отправлена команда. Блок:" << block
                 << "Маска: 0x" << QString::number(mask, 16)
                 << "Байт отправлено:" << bytesSent;
    }
}

void YLSNetwork::sendCurrentCommand()
{
    if (m_connected) {
        sendCommand(m_currentBlock, m_currentMask);
    }
}

void YLSNetwork::setPeriodicSending(bool enabled, int intervalMs)
{
    if (!m_periodicTimer) return;

    if (enabled) {
        m_periodicTimer->setInterval(intervalMs);
        m_periodicTimer->start();
        qDebug() << "Периодическая отправка включена, интервал:" << intervalMs << "мс";
    } else {
        m_periodicTimer->stop();
        qDebug() << "Периодическая отправка выключена";
    }

    emit periodicSendingChanged(enabled);
}

void YLSNetwork::onReadyRead()
{
    if (!m_listenSocket) return;
    int count =0;
    // Читаем все доступные датаграммы
    while (m_listenSocket->hasPendingDatagrams()) {
        bool check_count = count % 100;
        QNetworkDatagram datagram = m_listenSocket->receiveDatagram();
        QByteArray data = datagram.data();

        if (data.isEmpty()) {
            continue;
        }

        if(check_count) qDebug() << "Получено данных:" << data.size() << "байт от"
                 << datagram.senderAddress().toString() << ":" << datagram.senderPort();

        // ПЕРВОЕ получение данных от ЯЛС = мы подключены!
        if (!m_connected) {
            m_connected = true;

            // Останавливаем таймер ожидания
            if (m_connectionTimer) {
                m_connectionTimer->stop();
                m_connectionTimer->deleteLater();
                m_connectionTimer = nullptr;
            }

            qDebug() << "Получен ответ от ЯЛС - подключение установлено!";
            emit connected();
        }


        // Парсим ответ
        auto result = YPSProtocol::parseResponse(
            reinterpret_cast<const uint8_t*>(data.constData()),
            data.size()
            );

        if (result) {
            uint8_t mask = *result;

            // Пытаемся определить номер блока из ответа
            int block = 4;  // По умолчанию
            if (data.size() > 0) {
                block = static_cast<uint8_t>(data[0]);
            }

            if(check_count)qDebug() << "Получен ответ. Блок:" << block
                     << "Маска: 0x" << QString::number(mask, 16);

            emit responseReceived(block, mask);
        } else {
            if(check_count)qDebug() << "Не удалось распарсить ответ. Данные:" << data.toHex();
            // Не отправляем ошибку, так как могут приходить пакеты других протоколов
        }
        count++;
    }
}

void YLSNetwork::onPeriodicTimer()
{
    if (m_connected) {
        sendCurrentCommand();
    }
}
