cmake_minimum_required(VERSION 3.16)

project(test_yls_qt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Находим Qt с компонентами Network
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Core Widgets Network REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core5Compat)

# Исходные файлы
set(SOURCES
    main.cpp
    mainwindow.cpp
    test_network.cpp
    portfinder.cpp
    configmanager.cpp
)

set(HEADERS
    mainwindow.h
    test_network.h
    YPSProtocol.h
    portfinder.h
    configmanager.h
)

set(FORMS
    mainwindow.ui
)

# Создаем исполняемый файл
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${FORMS}
    configmanager.h configmanager.cpp
    portfinder.h portfinder.cpp)

# Подключаем зависимости
target_link_libraries(${PROJECT_NAME}
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Core5Compat
)


# Включаем поддержку UTF-8 для Windows
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        UNICODE
        _UNICODE
    )
endif()
