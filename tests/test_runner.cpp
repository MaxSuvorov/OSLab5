#include <iostream>
#include <cassert>
#include "temperature_calculator.h"
#include "logger.h"

void test_temperature_calculator() {
    std::cout << "Testing TemperatureCalculator..." << std::endl;
    
    TemperatureCalculator calc;
    
    // Добавляем тестовые данные
    std::time_t now = std::time(nullptr);
    calc.add_measurement(now - 1800, 20.0f); // 30 минут назад
    calc.add_measurement(now - 900, 22.0f);  // 15 минут назад
    calc.add_measurement(now, 24.0f);        // сейчас
    
    // Тест среднего за час
    float avg = calc.calculate_hourly_average(now - 3600);
    assert(avg > 21.9f && avg < 22.1f);
    
    std::cout << "All tests passed!" << std::endl;
}

int main() {
    test_temperature_calculator();
    return 0;
}
