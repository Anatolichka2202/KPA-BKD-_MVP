#include "portfinder.h"
#include "YPSProtocol.h"
#include <QNetworkDatagram>
#include <QDebug>
#include <QThread>

PortFinder::PortFinder(QObject *parent)
    : QObject(parent)
{
    // Инициализируем таймеры
    m_searchTimeoutTimer = new QTimer(this);
    m_searchTimeoutTimer->setInterval(1000); // 1 секунда ожидания ответа
    m_searchTimeoutTimer->setSingleShot(true);

    m_searchRetryTimer = new QTimer(this);
    m_searchRetryTimer->setInterval(500); // 500 мс между попытками
    m_searchRetryTimer->setSingleShot(true);

    // Подключаем таймеры к слотам
    connect(m_searchTimeoutTimer, &QTimer::timeout, this, &PortFinder::onSearchTimeout);
    connect(m_searchRetryTimer, &QTimer::timeout, this, &PortFinder::onSearchRetry);
}

PortFinder::~PortFinder()
{
    stopSearch();
}

void PortFinder::startSearch(const QHostAddress &ylsAddress, quint16 ylsPort,
                             quint16 startPort, quint16 endPort)
{
    // Останавливаем предыдущий поиск, если был
    stopSearch();

    // Сохраняем параметры поиска
    m_ylsAddress = ylsAddress;
    m_ylsPort = ylsPort;
    m_searchStartPort = startPort;
    m_searchEndPort = endPort;
    m_currentSearchPort = startPort;
    m_searchRetryCount = 0;
    m_searching = true;

    qDebug() << "Начинаем поиск порта в диапазоне" << startPort << "-" << endPort;
    emit searchStarted();

    // Создаём сокеты
    m_testSocket = new QUdpSocket(this);
    m_listenSocket = new QUdpSocket(this);

    // Начинаем поиск с первого порта
    tryNextPort();
}

void PortFinder::stopSearch()
{
    if (!m_searching) return;

    m_searching = false;
    m_searchTimeoutTimer->stop();
    m_searchRetryTimer->stop();

    // Закрываем и удаляем сокеты
    if (m_testSocket) {
        m_testSocket->close();
        m_testSocket->deleteLater();
        m_testSocket = nullptr;
    }

    if (m_listenSocket) {
        m_listenSocket->close();
        m_listenSocket->deleteLater();
        m_listenSocket = nullptr;
    }

    qDebug() << "Поиск порта остановлен";
}

void PortFinder::tryNextPort()
{
    if (!m_searching) return;

    // Проверяем, не вышли ли за пределы диапазона
    if (m_currentSearchPort > m_searchEndPort) {
        qDebug() << "Поиск порта завершен: порт не найден в диапазоне";
        emit searchFinished(false, "Порт не найден в указанном диапазоне");
        stopSearch();
        return;
    }

    // Закрываем предыдущий сокет, если он был открыт
    if (m_listenSocket->state() == QUdpSocket::BoundState) {
        m_listenSocket->close();
        QThread::msleep(50); // Даём время на освобождение порта
    }

    // Пытаемся привязаться к текущему порту
    if (!m_listenSocket->bind(QHostAddress::Any, m_currentSearchPort)) {
        qDebug() << "Порт" << m_currentSearchPort << "занят или недоступен";

        // Переходим к следующему порту
        m_currentSearchPort++;
        m_searchRetryCount = 0;

        // Обновляем прогресс
        int totalPorts = m_searchEndPort - m_searchStartPort + 1;
        int currentPort = m_currentSearchPort - m_searchStartPort;
        emit searchProgress(currentPort, totalPorts);

        // Пробуем следующий порт через небольшой интервал
        QTimer::singleShot(100, this, &PortFinder::tryNextPort);
        return;
    }

    // Подключаем слот для приёма данных
    connect(m_listenSocket, &QUdpSocket::readyRead, this, &PortFinder::onReadyRead);

    qDebug() << "Проверяем порт" << m_currentSearchPort
             << "(попытка" << (m_searchRetryCount + 1) << "из" << MAX_SEARCH_RETRIES << ")";

    // Отправляем тестовый пакет
    sendTestPacket();

    // Запускаем таймер ожидания ответа
    m_searchTimeoutTimer->start();
}

void PortFinder::sendTestPacket()
{
    if (!m_testSocket || !m_searching) return;

    // Создаём тестовый запрос (запрос состояния блока 4 с маской 0)
    YPSProtocol::Request request = YPSProtocol::createRequest(0x00);
    request.cell_number = 4;

    // Отправляем тестовую датаграмму
    qint64 bytesSent = m_testSocket->writeDatagram(
        reinterpret_cast<const char*>(&request),
        sizeof(request),
        m_ylsAddress,
        m_ylsPort
        );

    if (bytesSent == -1) {
        qDebug() << "Ошибка отправки тестового пакета на порт" << m_currentSearchPort
                 << ":" << m_testSocket->errorString();
    } else {
        qDebug() << "Тестовый пакет отправлен на порт" << m_currentSearchPort;
    }
}

void PortFinder::onReadyRead()
{
    if (!m_listenSocket || !m_searching) return;

    // Читаем все доступные датаграммы
    while (m_listenSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_listenSocket->receiveDatagram();
        QByteArray data = datagram.data();

        if (data.isEmpty()) continue;

        qDebug() << "Получен ответ при поиске порта:" << data.size() << "байт";

        // Останавливаем таймеры поиска
        m_searchTimeoutTimer->stop();
        m_searchRetryTimer->stop();

        // Парсим ответ
        auto result = YPSProtocol::parseResponse(
            reinterpret_cast<const uint8_t*>(data.constData()),
            data.size()
            );

        if (result) {
            // Порт найден!
            quint16 foundPort = m_currentSearchPort;
            m_searching = false;

            qDebug() << "Порт найден:" << foundPort;
            emit portFound(foundPort);
            emit searchFinished(true, QString("Порт найден: %1").arg(foundPort));

            // Останавливаем поиск
            stopSearch();
        } else {
            qDebug() << "Получен некорректный ответ на порту" << m_currentSearchPort;
            // Продолжаем поиск
            onSearchTimeout();
        }
    }
}

void PortFinder::onSearchTimeout()
{
    if (!m_searching) return;

    m_searchRetryCount++;

    if (m_searchRetryCount >= MAX_SEARCH_RETRIES) {
        // Превышено количество попыток для этого порта
        qDebug() << "Порт" << m_currentSearchPort << "не ответил после"
                 << MAX_SEARCH_RETRIES << "попыток";

        // Переходим к следующему порту
        m_currentSearchPort++;
        m_searchRetryCount = 0;

        // Обновляем прогресс
        int totalPorts = m_searchEndPort - m_searchStartPort + 1;
        int currentPort = m_currentSearchPort - m_searchStartPort;
        emit searchProgress(currentPort, totalPorts);

        // Пробуем следующий порт
        tryNextPort();
    } else {
        // Повторная попытка для текущего порта
        qDebug() << "Повторная попытка для порта" << m_currentSearchPort
                 << "(попытка" << (m_searchRetryCount + 1) << "из" << MAX_SEARCH_RETRIES << ")";
        m_searchRetryTimer->start();
    }
}

void PortFinder::onSearchRetry()
{
    if (m_searching) {
        sendTestPacket();
        m_searchTimeoutTimer->start();
    }
}
