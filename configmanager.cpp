#include "configmanager.h"
#include <QDebug>

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
{
    // Используем INI-формат для конфигурации
    // Файл будет создаваться в папке с приложением: config.ini
    m_settings = new QSettings("config.ini", QSettings::IniFormat, this);

    // Загружаем конфигурацию при создании
    loadConfig();
}

void ConfigManager::loadConfig()
{
    qDebug() << "Загрузка конфигурации из файла:" << m_settings->fileName();

    // Загружаем значения из конфига, используя текущие как значения по умолчанию
    m_ylsIp = m_settings->value("network/yls_ip", m_ylsIp).toString();
    m_ylsPort = m_settings->value("network/yls_port", m_ylsPort).toUInt();
    m_listenPort = m_settings->value("network/listen_port", m_listenPort).toUInt();
    m_searchStartPort = m_settings->value("search/start_port", m_searchStartPort).toUInt();
    m_searchEndPort = m_settings->value("search/end_port", m_searchEndPort).toUInt();
    m_autoConnect = m_settings->value("behavior/auto_connect", m_autoConnect).toBool();

    qDebug() << "Конфигурация загружена:";
    qDebug() << "  ЯЛС IP:" << m_ylsIp;
    qDebug() << "  ЯЛС порт:" << m_ylsPort;
    qDebug() << "  Порт приёма:" << m_listenPort;
    qDebug() << "  Диапазон поиска:" << m_searchStartPort << "-" << m_searchEndPort;
    qDebug() << "  Автоподключение:" << m_autoConnect;
}

void ConfigManager::saveConfig()
{
    qDebug() << "Сохранение конфигурации в файл:" << m_settings->fileName();

    // Сохраняем значения в конфиг
    m_settings->setValue("network/yls_ip", m_ylsIp);
    m_settings->setValue("network/yls_port", m_ylsPort);
    m_settings->setValue("network/listen_port", m_listenPort);
    m_settings->setValue("search/start_port", m_searchStartPort);
    m_settings->setValue("search/end_port", m_searchEndPort);
    m_settings->setValue("behavior/auto_connect", m_autoConnect);

    // Синхронизируем с файлом
    m_settings->sync();

    qDebug() << "Конфигурация сохранена";
    emit configChanged();
}
