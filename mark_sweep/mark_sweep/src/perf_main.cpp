#include <iostream>
#include "performance_test.h"

int main(int argc, char* argv[]) {
    std::cout << "\n";
    std::cout << "====================================================================\n";
    std::cout << "       Mark-Sweep GC Performance Test Suite\n";
    std::cout << "                   v1.0\n";
    std::cout << "  Scenario 1: Simple Linear Chain (basic cascade)\n";
    std::cout << "  Scenario 2: Cyclic Graph (cycle detection & collection)\n";
    std::cout << "  Scenario 3: Cascade Tree (recursive deletion)\n";
    std::cout << "====================================================================\n\n";
    
    // Параметры из аргументов командной строки
    int small_size = 1000;
    int medium_size = 10000;
    int large_size = 100000;
    
    if (argc > 1) {
        try {
            small_size = std::stoi(argv[1]);
            if (argc > 2) medium_size = std::stoi(argv[2]);
            if (argc > 3) large_size = std::stoi(argv[3]);
        } catch (...) {
            std::cerr << "Invalid arguments. Using defaults.\n";
        }
    }
    
    std::cout << "Configuration:\n";
    std::cout << "  Small:  " << small_size << " objects\n";
    std::cout << "  Medium: " << medium_size << " objects\n";
    std::cout << "  Large:  " << large_size << " objects\n";
    std::cout << "\nStarting tests...\n\n";
    
    // Создаём и запускаем тесты
    PerformanceTest perf_test("./perf_results");
    perf_test.run_all_tests(small_size, medium_size, large_size);
    
    // Сохраняем результаты
    perf_test.save_results_to_json("performance_results.json");
    
    std::cout << std::string(100, '=') << "\n";
    std::cout << "OK All tests completed successfully!\n";
    std::cout << "OK Logs saved to:     ./perf_results/*.log\n";
    std::cout << "OK Results saved to:  ./perf_results/performance_results.json\n";
    std::cout << std::string(100, '=') << "\n\n";
    
    return 0;
}
