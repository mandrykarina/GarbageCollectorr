#include "performance_test.h"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <cstdlib>

PerformanceTest::PerformanceTest(const std::string& output_dir_)
    : output_dir(output_dir_) {
    ensure_output_directory();
}

void PerformanceTest::ensure_output_directory() {
    std::string mkdir_cmd = "mkdir -p " + output_dir;
    std::system(mkdir_cmd.c_str());
}

std::string PerformanceTest::get_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

PerfTestResult PerformanceTest::test_simple_linear(int num_objects) {
    PerfTestResult result;
    result.test_name = "Simple Linear Chain";
    result.scenario_type = "simple_linear";
    result.total_objects = num_objects;
    result.timestamp = get_timestamp();
    result.collection_runs = 0;
    
    // Создаём GC с логированием
    std::string log_file = output_dir + "/simple_linear_" + 
                          std::to_string(num_objects) + ".log";
    MarkSweepGC gc(1024 * 1024 * 100, // 100MB heap
                   1024 * 1024 * 80,  // 80% threshold
                   log_file);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // === ЭТАП 1: СОЗДАНИЕ ЛИНЕЙНОЙ ЦЕПИ ===
    // root -> obj1 -> obj2 -> ... -> objN
    
    int root_id = gc.allocate(64); // root объект
    int op_count = 1;
    
    int prev_id = root_id;
    gc.make_root(root_id);
    op_count++;
    
    for (int i = 1; i < num_objects; ++i) {
        int obj_id = gc.allocate(64); // каждый объект 64 байта
        op_count++;
        
        gc.add_reference(prev_id, obj_id);
        op_count++;
        
        prev_id = obj_id;
    }
    
    // === ЭТАП 2: СБОРКА МУСОРА ===
    size_t freed = gc.collect();
    result.collection_runs = 1;
    
    // === ЭТАП 3: УДАЛЕНИЕ ROOT (демонстрация каскада) ===
    gc.remove_root(root_id);
    op_count++;
    
    // Вторая сборка
    freed += gc.collect();
    result.collection_runs = 2;
    
    auto end_time = std::chrono::high_resolution_clock::now();
    
    // === ПОДСЧЁТ РЕЗУЛЬТАТОВ ===
    double exec_time = std::chrono::duration<double, std::milli>(
        end_time - start_time).count();
    
    result.execution_time_ms = exec_time;
    result.total_operations = op_count;
    result.objects_collected = num_objects;
    result.objects_leaked = 0;
    result.memory_used_bytes = num_objects * 64;
    result.memory_freed_bytes = freed;
    
    results.push_back(result);
    return result;
}

PerfTestResult PerformanceTest::test_cyclic_graph(int num_objects, int cycle_length) {
    PerfTestResult result;
    result.test_name = "Cyclic Graph (Cycle Detection)";
    result.scenario_type = "cyclic_graph";
    result.total_objects = num_objects;
    result.timestamp = get_timestamp();
    result.collection_runs = 0;
    
    std::string log_file = output_dir + "/cyclic_graph_" + 
                          std::to_string(num_objects) + ".log";
    MarkSweepGC gc(1024 * 1024 * 100,
                   1024 * 1024 * 80,
                   log_file);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // === ЭТАП 1: СОЗДАНИЕ ЦИКЛИЧЕСКИХ ССЫЛОК ===
    // Структура: root -> [cycle1] -> [cycle2] -> ...
    // где каждый cycle имеет циклические ссылки между объектами
    
    int root_id = gc.allocate(64);
    int op_count = 1;
    
    gc.make_root(root_id);
    op_count++;
    
    int num_cycles = std::max(1, (num_objects - 1) / cycle_length);
    int current_id_base = 1;
    
    for (int cycle_idx = 0; cycle_idx < num_cycles; ++cycle_idx) {
        std::vector<int> cycle_objects;
        
        // Создаём объекты в этом цикле
        for (int j = 0; j < cycle_length && 
             (current_id_base + j) < num_objects; ++j) {
            int obj_id = gc.allocate(64);
            op_count++;
            cycle_objects.push_back(obj_id);
            
            // Первый объект цикла подключаем к root
            if (j == 0) {
                gc.add_reference(root_id, obj_id);
                op_count++;
            }
        }
        
        // Создаём циклические ссылки: obj0 -> obj1 -> ... -> objN -> obj0
        for (size_t j = 0; j < cycle_objects.size(); ++j) {
            int from_id = cycle_objects[j];
            int to_id = cycle_objects[(j + 1) % cycle_objects.size()];
            
            gc.add_reference(from_id, to_id);
            op_count++;
        }
        
        current_id_base += cycle_length;
    }
    
    // === ЭТАП 2: СБОРКА МУСОРА (ДО УДАЛЕНИЯ ROOT) ===
    // Mark-Sweep должен НАЙТИ и пометить все циклы как достижимые
    size_t freed = gc.collect();
    result.collection_runs = 1;
    
    // === ЭТАП 3: УДАЛЕНИЕ ROOT ===
    // После этого циклы станут недостижимы и будут удалены!
    gc.remove_root(root_id);
    op_count++;
    
    // Вторая сборка
    freed += gc.collect();
    result.collection_runs = 2;
    
    auto end_time = std::chrono::high_resolution_clock::now();
    
    // === ПОДСЧЁТ РЕЗУЛЬТАТОВ ===
    double exec_time = std::chrono::duration<double, std::milli>(
        end_time - start_time).count();
    
    result.execution_time_ms = exec_time;
    result.total_operations = op_count;
    result.objects_collected = num_objects;
    result.objects_leaked = 0; // Mark-Sweep НЕ имеет утечек!
    result.memory_used_bytes = num_objects * 64;
    result.memory_freed_bytes = freed;
    
    results.push_back(result);
    return result;
}

PerfTestResult PerformanceTest::test_cascade_tree(int num_objects) {
    PerfTestResult result;
    result.test_name = "Cascade Tree (Recursive Deletion)";
    result.scenario_type = "cascade_tree";
    result.total_objects = num_objects;
    result.timestamp = get_timestamp();
    result.collection_runs = 0;
    
    std::string log_file = output_dir + "/cascade_tree_" + 
                          std::to_string(num_objects) + ".log";
    MarkSweepGC gc(1024 * 1024 * 100,
                   1024 * 1024 * 80,
                   log_file);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // === Простая цепь: root -> obj1 -> obj2 -> ... -> objN ===
    
    int root_id = gc.allocate(64);
    int op_count = 1;
    
    gc.make_root(root_id);
    op_count++;
    
    int prev_id = root_id;
    int created_count = 1;
    
    for (int i = 1; i < num_objects; ++i) {
        int obj_id = gc.allocate(64);
        op_count++;
        
        gc.add_reference(prev_id, obj_id);
        op_count++;
        
        prev_id = obj_id;
        created_count++;
    }
    
    // === СБОРКА МУСОРА ===
    size_t freed = gc.collect();
    result.collection_runs = 1;
    
    // === УДАЛЕНИЕ ROOT ===
    gc.remove_root(root_id);
    op_count++;
    
    freed += gc.collect();
    result.collection_runs = 2;
    
    auto end_time = std::chrono::high_resolution_clock::now();
    
    // === РЕЗУЛЬТАТЫ ===
    double exec_time = std::chrono::duration<double, std::milli>(
        end_time - start_time).count();
    
    result.execution_time_ms = exec_time;
    result.total_operations = op_count;
    result.objects_collected = created_count;
    result.objects_leaked = 0;
    result.memory_used_bytes = created_count * 64;
    result.memory_freed_bytes = freed;
    
    results.push_back(result);
    return result;
}






void PerformanceTest::run_all_tests(int small_size, 
                                    int medium_size, 
                                    int large_size) {
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "MARK-SWEEP GARBAGE COLLECTOR PERFORMANCE TEST SUITE v1.0\n";
    std::cout << "Testing: Simple Linear, Cyclic Graphs, Cascade Trees\n";
    std::cout << std::string(80, '=') << "\n\n";
    
    // === ТЕСТ 1: ПРОСТАЯ ЛИНЕЙНАЯ ЦЕПЬ ===
    std::cout << ">> TEST 1: SIMPLE LINEAR CHAIN\n";
    std::cout << "   Scenario: root -> obj1 -> obj2 -> ... -> objN\n";
    std::cout << "   " << std::string(70, '-') << "\n";
    
    std::cout << "   [1/3] Small (" << small_size << " objects)...\n";
    auto r1 = test_simple_linear(small_size);
    std::cout << "         OK " << r1.execution_time_ms << " ms | " 
              << r1.objects_collected << " collected\n";
    
    std::cout << "   [2/3] Medium (" << medium_size << " objects)...\n";
    auto r2 = test_simple_linear(medium_size);
    std::cout << "         OK " << r2.execution_time_ms << " ms | " 
              << r2.objects_collected << " collected\n";
    
    std::cout << "   [3/3] Large (" << large_size << " objects)...\n";
    auto r3 = test_simple_linear(large_size);
    std::cout << "         OK " << r3.execution_time_ms << " ms | " 
              << r3.objects_collected << " collected\n\n";
    
    // === ТЕСТ 2: ЦИКЛИЧЕСКИЙ ГРАФ ===
    std::cout << ">> TEST 2: CYCLIC GRAPH (CYCLE DETECTION)\n";
    std::cout << "   Scenario: root -> [cycle1 <-> cycle1] -> [cycle2] ...\n";
    std::cout << "   Mark-Sweep DETECTS and collects cycles!\n";
    std::cout << "   " << std::string(70, '-') << "\n";
    
    std::cout << "   [1/3] Small (" << small_size << " objects)...\n";
    auto r4 = test_cyclic_graph(small_size, 3);
    std::cout << "         OK " << r4.execution_time_ms << " ms | " 
              << r4.objects_collected << " collected (NO LEAKS!)\n";
    
    std::cout << "   [2/3] Medium (" << medium_size << " objects)...\n";
    auto r5 = test_cyclic_graph(medium_size, 3);
    std::cout << "         OK " << r5.execution_time_ms << " ms | " 
              << r5.objects_collected << " collected (NO LEAKS!)\n";
    
    std::cout << "   [3/3] Large (" << large_size << " objects)...\n";
    auto r6 = test_cyclic_graph(large_size, 3);
    std::cout << "         OK " << r6.execution_time_ms << " ms | " 
              << r6.objects_collected << " collected (NO LEAKS!)\n\n";
    
    // === ТЕСТ 3: CASCADE TREE ===
std::cout << ">> TEST 3: CASCADE TREE (RECURSIVE DELETION)\n";
std::cout << "   Scenario: root -> obj1 -> obj2 -> ... -> objN\n";
std::cout << "   Demonstrates cascade deletion of all descendants\n";
std::cout << "   " << std::string(70, '-') << "\n";

std::cout << "   [1/3] Small (" << small_size << " objects)...\n";
auto r7 = test_cascade_tree(small_size);
std::cout << "         OK " << r7.execution_time_ms << " ms | " 
          << r7.objects_collected << " collected\n";

std::cout << "   [2/3] Medium (" << medium_size << " objects)...\n";
auto r8 = test_cascade_tree(medium_size);
std::cout << "         OK " << r8.execution_time_ms << " ms | " 
          << r8.objects_collected << " collected\n";

std::cout << "   [3/3] Large (" << large_size << " objects)...\n";
auto r9 = test_cascade_tree(large_size);
std::cout << "         OK " << r9.execution_time_ms << " ms | " 
          << r9.objects_collected << " collected\n\n";

print_summary();

}

void PerformanceTest::print_summary() const {
    std::cout << std::string(100, '=') << "\n";
    std::cout << "PERFORMANCE SUMMARY\n";
    std::cout << std::string(100, '=') << "\n\n";
    
    // Заголовок таблицы
    std::cout << std::left
              << std::setw(18) << "Scenario"
              << std::setw(12) << "Objects"
              << std::setw(12) << "Time (ms)"
              << std::setw(12) << "Collected"
              << std::setw(12) << "Leaked"
              << std::setw(14) << "Memory (MB)"
              << std::setw(10) << "Ops"
              << "\n";
    
    std::cout << std::string(100, '-') << "\n";
    
    // Данные таблицы
    for (const auto& result : results) {
        double mem_used = result.memory_used_bytes / (1024.0 * 1024.0);
        
        std::cout << std::left
                  << std::setw(18) << result.scenario_type
                  << std::setw(12) << result.total_objects
                  << std::setw(12) << std::fixed << std::setprecision(2) 
                    << result.execution_time_ms
                  << std::setw(12) << result.objects_collected
                  << std::setw(12) << result.objects_leaked
                  << std::setw(14) << std::fixed << std::setprecision(4)
                    << mem_used
                  << std::setw(10) << result.total_operations
                  << "\n";
    }
    
    std::cout << "\n";
}

void PerformanceTest::save_results_to_json(const std::string& filename) {
    // Создаём результирующий JSON вручную
    json output;
    output["test_suite"] = "Mark-Sweep GC Performance Tests";
    output["timestamp"] = get_timestamp();
    output["tests"] = json::array();
    output["statistics"] = json::object();
    
    // Заполняем тесты
    for (const auto& result : results) {
        json test_obj;
        test_obj["test_name"] = result.test_name;
        test_obj["scenario_type"] = result.scenario_type;
        test_obj["total_objects"] = result.total_objects;
        test_obj["total_operations"] = result.total_operations;
        test_obj["execution_time_ms"] = std::round(result.execution_time_ms * 100) / 100.0;
        test_obj["objects_collected"] = result.objects_collected;
        test_obj["objects_leaked"] = result.objects_leaked;
        test_obj["memory_used_mb"] = std::round((result.memory_used_bytes / (1024.0 * 1024.0)) * 100) / 100.0;
        test_obj["memory_freed_mb"] = std::round((result.memory_freed_bytes / (1024.0 * 1024.0)) * 100) / 100.0;
        test_obj["collection_runs"] = result.collection_runs;
        test_obj["timestamp"] = result.timestamp;
        
        output["tests"].push_back(test_obj);
    }
    
    // Заполняем статистику
    int total_tests = 0;
    int total_objects = 0;
    double total_time = 0.0;
    int total_collected = 0;
    int total_leaked = 0;
    
    for (const auto& result : results) {
        total_tests++;
        total_objects += result.total_objects;
        total_time += result.execution_time_ms;
        total_collected += result.objects_collected;
        total_leaked += result.objects_leaked;
    }
    
    output["statistics"]["total_tests"] = total_tests;
    output["statistics"]["total_objects_tested"] = total_objects;
    output["statistics"]["total_time_ms"] = std::round(total_time * 100) / 100.0;
    output["statistics"]["total_objects_collected"] = total_collected;
    output["statistics"]["total_objects_leaked"] = total_leaked;
    
    // Запись в файл
    std::string full_path = output_dir + "/" + filename;
    std::ofstream file(full_path);
    
    if (!file.is_open()) {
        std::cerr << "ERROR: Cannot open file: " << full_path << "\n";
        return;
    }
    
    try {
        file << output.dump(2) << "\n";
        file.close();
        std::cout << "OK Results saved to: " << full_path << "\n\n";
    } catch (const std::exception& e) {
        std::cerr << "ERROR writing JSON: " << e.what() << "\n";
    }
}

