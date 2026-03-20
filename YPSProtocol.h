#pragma once
#include <cstdint>
#include <array>
#include <optional>
#include <QDebug>

namespace YPSProtocol {

// Размеры пакетов
constexpr size_t MIN_REQUEST_SIZE = 16;
constexpr size_t MIN_RESPONSE_SIZE = 40;

// Структура запроса к ЯПС
struct Request {
    uint8_t cell_number;     // [0] - номер ячейки
    uint8_t command;         // [1] - команда
    uint8_t reserved[8];     // [2-9] - мусор
    uint8_t mask;            // [10] - битовая маска
    // остальное мусор или нули
};

// ответ от ЯПС
struct Response {
    uint8_t cell_number;     // [0] - номер ячейки (4)
    uint8_t command;         // [1] - команда (2 - ответ)
    uint8_t reserved[12];    // [2-13] - резерв
    uint8_t mask;            // [14] - обновленная маска
    // [15-39] - мусор
};

//  запрос
inline Request createRequest(uint8_t mask) {
    Request req;
    req.cell_number = 4;
    req.command = 1;

    // Заполняем reserved нулями
    for (int i = 0; i < 8; i++) {
        req.reserved[i] = 0;
    }

    req.mask = mask;
    return req;
}

// Парсинг ответа (нам важно только первые 15 байт)
inline std::optional<uint8_t> parseResponse(const uint8_t* data, size_t length) {
    if (length < 15) {
        qDebug() << "Слишком короткий ответ:" << length << "байт";
        return std::nullopt;
    }

    const Response* response = reinterpret_cast<const Response*>(data);

    qDebug() << "\n????????????????????????????????????????????????";
    qDebug() << "ПОЛНЫЙ ОТВЕТ ОТ ЯЛС:";
    qDebug() << "????????????????????????????????????????????????";
    qDebug() << "Размер пакета:" << length << "байт";
    qDebug() << "Номер ячейки [0]:" << response->cell_number
             << "(ожидали 4)";
    qDebug() << "Команда [1]:" << response->command
             << "(2 = ответ, 1 = запрос)";
    qDebug() << "Маска [14]:" << QString("0x%1").arg(response->mask, 2, 16, QChar('0'))
             << "(бинарно: " << QString::number(response->mask, 2).rightJustified(8, '0') << ")";
    qDebug() << "???????????????????????????????????????????????";
    qDebug() << "Первые 20 байт ответа:";
    for (size_t i = 0; i < length && i < 20; i++) {
        qDebug() << QString("  [%1] = 0x%2 (%3) | %4")
        .arg(i, 2)
            .arg(data[i], 2, 16, QChar('0'))
            .arg(data[i], 3)
            .arg((i == 0) ? "? номер ячейки" :
                     (i == 1) ? "? команда" :
                     (i == 14) ? "? маска" : "");
    }
    qDebug() << "????????????????????????????????????????????????\n";

    if (response->cell_number == 4 && response->command == 2) {
        return response->mask;
    }

    qDebug() << "!x!x! Некорректный ответ: ожидали cell_number=4, command=2";
    return std::nullopt;
}

} // namespace YPSProtocol
