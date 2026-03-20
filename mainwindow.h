#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QPushButton>
// Предварительные объявления
class YLSNetwork;
class PortFinder;
class ConfigManager;
class QLabel;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Слоты для кнопок
    void on_connectButton_clicked();
    void on_disconnectButton_clicked();
    void on_testButton_clicked();
    void on_findPortButton_clicked();
    void on_periodicCheckBox_toggled(bool checked);
    void on_saveConfigButton_clicked();


    // Слоты для сети
    void onResponseReceived(int block, uint8_t mask);
    void onNetworkError(const QString& message);
    void onNetworkConnected();
    void onNetworkDisconnected();
    void onPeriodicSendingChanged(bool enabled);

    // Слоты для поиска порта
    void onPortSearchStarted();
    void onPortSearchProgress(int current, int total);
    void onPortFound(uint16_t port);
    void onPortSearchFinished(bool success, const QString& message);

    // Слоты для конфигурации
    void onConfigChanged();

    //кнопки-лампочки
    void setupInputLamps(); //инициализация лампочек ввода
    void updateInputLamps(); //обновление лампочек-кнопок
    void onInputLampToggled(QAbstractButton* button, bool checked); // Обработка клика
    //QString updateStatusLabel(bool connected);    // Обновление статуса кнопки

private:
    // Методы инициализации
    void setupUI();
    void setupConnections();
    void loadConfigToUI();
    void saveConfigFromUI();

    // Вспомогательные методы
    void updateUIState(bool connected);
    void updateLamps(uint8_t mask);

    uint16_t calculateInputMask();//рассчет маски исходя из нажатых лампочек
    void setLampButtonState(int bit, bool state); // Установка состояния кнопки
    QString getLampButtonState(int bit, bool state); // получение состояния кнопки
    QString getLampButtonStyle(bool check, bool enable);

    QString maskToBinaryString(uint8_t mask);

    // UI
    Ui::MainWindow *ui;

    // Компоненты
    YLSNetwork* m_network = nullptr;
    PortFinder* m_portFinder = nullptr;
    ConfigManager* m_configManager = nullptr;

    // Лампочки
    QList<QLabel*> m_lamps;

    //кнопки-лампочки
    QList <QPushButton*> m_InputLamps;
    uint16_t m_inputMask = 0x00; //текущая маска (с лампочек)
    QButtonGroup* m_inputLampGroup = nullptr;

    // Состояние
    bool m_connected = false;
    uint8_t m_currentMask = 0x00;
    int m_currentBlock = 4;
};

#endif // MAINWINDOW_H
