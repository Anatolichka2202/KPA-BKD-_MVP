#ifndef PORTFINDER_H
#define PORTFINDER_H

#pragma once

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QHostAddress>

class PortFinder : public QObject
{
    Q_OBJECT

public:
    explicit PortFinder(QObject *parent = nullptr);
    ~PortFinder();

    // Запуск поиска порта
    void startSearch(const QHostAddress &ylsAddress, quint16 ylsPort,
                     quint16 startPort, quint16 endPort);

    // Остановка поиска
    void stopSearch();

signals:
    // Сигналы для UI
    void searchStarted();
    void searchProgress(int current, int total);
    void portFound(quint16 port);
    void searchFinished(bool success, const QString &message);
    void errorOccurred(const QString &message);

private slots:
    // Слоты для внутренней логики
    void onReadyRead();
    void onSearchTimeout();
    void onSearchRetry();

private:
    // Внутренние методы
    void tryNextPort();
    void sendTestPacket();

    // Сокеты и таймеры
    QUdpSocket *m_testSocket = nullptr;
    QUdpSocket *m_listenSocket = nullptr;
    QTimer *m_searchTimeoutTimer = nullptr;
    QTimer *m_searchRetryTimer = nullptr;

    // Параметры поиска
    QHostAddress m_ylsAddress;
    quint16 m_ylsPort = 0;
    quint16 m_currentSearchPort = 0;
    quint16 m_searchStartPort = 0;
    quint16 m_searchEndPort = 0;

    // Состояние поиска
    bool m_searching = false;
    int m_searchRetryCount = 0;
    const int MAX_SEARCH_RETRIES = 3;
};
#endif // PORTFINDER_H
