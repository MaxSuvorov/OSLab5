cmake_minimum_required(VERSION 3.10)
project(TemperatureMonitoringSystem)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Настройки для SQLite
option(USE_SYSTEM_SQLITE "Use system SQLite" OFF)
if(USE_SYSTEM_SQLITE)
    find_package(SQLite3 REQUIRED)
else()
    # Используем встроенный SQLite
    add_subdirectory(thirdparty/sqlite)
endif()

# Основной сервер
add_executable(temperature_server
    temperature_server/main.cpp
    temperature_server/temperature_server.cpp
    temperature_server/http_server.cpp
    temperature_server/port_reader.cpp
    temperature_server/database_manager.cpp
    temperature_server/temperature_calculator.cpp
)

# Веб-сервер для статических файлов
add_executable(web_server
    web_client/webserver.cpp
)

# Симулятор устройства
add_executable(device_simulator
    device_simulation/device_simulation.cpp
    device_simulation/device_simulation_main.cpp
)

# Линковка SQLite
if(USE_SYSTEM_SQLITE)
    target_link_libraries(temperature_server SQLite::SQLite3)
else()
    target_link_libraries(temperature_server sqlite)
endif()

# Настройки для Windows
if(WIN32)
    target_link_libraries(temperature_server ws2_32)
    target_link_libraries(web_server ws2_32)
    target_link_libraries(device_simulator ws2_32)
endif()