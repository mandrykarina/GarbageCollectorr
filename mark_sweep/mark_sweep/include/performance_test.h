#ifndef PERFORMANCE_TEST_H
#define PERFORMANCE_TEST_H

#include "mark_sweep_gc.h"
#include <chrono>
#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * @struct PerfTestResult
 * @brief Результаты одного теста производительности
 */
struct PerfTestResult {
    std::string test_name;
    std::string scenario_type; // "simple_linear", "cyclic_graph", "cascade_tree"
    int total_objects;
    int total_operations;
    double execution_time_ms;
    int objects_collected;
    int objects_leaked;
    size_t memory_used_bytes;
    size_t memory_freed_bytes;
    int collection_runs;
    std::string timestamp;
    
    json to_json() const {
        json j;
        j["test_name"] = test_name;
        j["scenario_type"] = scenario_type;
        j["total_objects"] = total_objects;
        j["total_operations"] = total_operations;
        j["execution_time_ms"] = std::round(execution_time_ms * 100) / 100.0;
        j["objects_collected"] = objects_collected;
        j["objects_leaked"] = objects_leaked;
        j["memory_used_mb"] = std::round((memory_used_bytes / (1024.0 * 1024.0)) * 100) / 100.0;
        j["memory_freed_mb"] = std::round((memory_freed_bytes / (1024.0 * 1024.0)) * 100) / 100.0;
        j["collection_runs"] = collection_runs;
        j["timestamp"] = timestamp;
        return j;
    }
};

/**
 * @class PerformanceTest
 * @brief Framework для тестирования Mark-Sweep GC с тремя сценариями
 * 
 * Сценарии:
 * 1. Simple Linear - линейная цепь объектов
 * 2. Cyclic Graph - граф с циклическими ссылками (демонстрирует обнаружение циклов)
 * 3. Cascade Tree - дерево объектов (демонстрирует каскадное удаление)
 */
class PerformanceTest {
public:
    /**
     * @brief Конструктор
     * @param output_dir Директория для сохранения логов и результатов
     */
    explicit PerformanceTest(const std::string& output_dir = "./perf_results");
    
    /**
     * @brief Сценарий 1: Простая линейная цепь
     * Структура: root -> obj1 -> obj2 -> ... -> objN
     * После Mark-Sweep all удаляются, т.к. достижимы из root
     * 
     * @param num_objects Количество объектов в цепи
     * @return PerfTestResult с результатами
     */
    PerfTestResult test_simple_linear(int num_objects);
    
    /**
     * @brief Сценарий 2: Циклический граф
     * Структура:
     *   root -> [cycle1: obj1 <-> obj2 <-> ... <-> objK]
     *        -> [cycle2: objK+1 <-> objK+2 <-> ... ]
     * 
     * Mark-Sweep ОБНАРУЖИВАЕТ и удаляет циклы! (в отличие от обычного RC)
     * Демонстрирует преимущество перед Reference Counting
     * 
     * @param num_objects Количество объектов с циклами
     * @param cycle_length Длина каждого цикла (по умолчанию 3)
     * @return PerfTestResult с результатами
     */
    PerfTestResult test_cyclic_graph(int num_objects, int cycle_length = 3);
    
    /**
     * @brief Сценарий 3: Древовидная структура с каскадным удалением
     * Структура:
     *        root
     *       / | \ (branches ветвей)
     *      c1 c2 c3
     *     /| /| /|
     *    ...  (depth уровней)
     * 
     * Демонстрирует эффективность каскадного удаления потомков
     * 
     * @param num_objects Количество объектов (распределяются по дереву)
     * @param branches Количество ветвей из каждого узла
     * @param depth Глубина дерева
     * @return PerfTestResult с результатами
     */
    PerfTestResult test_cascade_tree(int num_objects);
    
    /**
     * @brief Запустить все три сценария с тремя размерами
     * @param small_size Маленький набор (~1K объектов)
     * @param medium_size Средний набор (~10K объектов)
     * @param large_size Большой набор (~100K объектов)
     */
    void run_all_tests(int small_size = 100, 
                       int medium_size = 1000, 
                       int large_size = 10000);
    
    /**
     * @brief Получить все результаты тестов
     */
    const std::vector<PerfTestResult>& get_results() const { 
        return results; 
    }
    
    /**
     * @brief Сохранить результаты в JSON файл
     * @param filename Имя файла в output_dir
     */
    void save_results_to_json(const std::string& filename = "performance_results.json");
    
    /**
     * @brief Вывести краткую таблицу результатов в консоль
     */
    void print_summary() const;
    
private:
    std::string output_dir;
    std::vector<PerfTestResult> results;
    
    /**
     * @brief Получить текущее время в ISO формате
     */
    std::string get_timestamp() const;
    
    /**
     * @brief Убедиться что директория существует
     */
    void ensure_output_directory();
};

#endif // PERFORMANCE_TEST_H
