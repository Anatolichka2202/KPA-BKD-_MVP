#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#pragma once

#include <QObject>
#include <QSettings>
#include <QString>

class ConfigManager : public QObject
{
    Q_OBJECT

public:
    explicit ConfigManager(QObject *parent = nullptr);

    // Загрузка и сохранение конфигурации
    void loadConfig();
    void saveConfig();

    // Геттеры и сеттеры для параметров
    QString ylsIp() const { return m_ylsIp; }
    void setYlsIp(const QString &ip) { m_ylsIp = ip; }

    quint16 ylsPort() const { return m_ylsPort; }
    void setYlsPort(quint16 port) { m_ylsPort = port; }

    quint16 listenPort() const { return m_listenPort; }
    void setListenPort(quint16 port) { m_listenPort = port; }

    quint16 searchStartPort() const { return m_searchStartPort; }
    void setSearchStartPort(quint16 port) { m_searchStartPort = port; }

    quint16 searchEndPort() const { return m_searchEndPort; }
    void setSearchEndPort(quint16 port) { m_searchEndPort = port; }

    bool autoConnect() const { return m_autoConnect; }
    void setAutoConnect(bool enable) { m_autoConnect = enable; }

signals:
    void configChanged();  // Сигнал при изменении конфигурации

private:
    // Параметры конфигурации
    QString m_ylsIp = "192.168.0.230";
    quint16 m_ylsPort = 1080;
    quint16 m_listenPort = 100;  // Порт по умолчанию
    quint16 m_searchStartPort = 95;
    quint16 m_searchEndPort = 105;
    bool m_autoConnect = false;

    QSettings *m_settings = nullptr;
};
#endif // CONFIGMANAGER_H
