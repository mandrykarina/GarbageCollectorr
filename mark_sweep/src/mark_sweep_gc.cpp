#include "mark_sweep_gc.h"
#include <algorithm>
#include <sstream>
#include <iostream>
#include <chrono>

// ===========================
// КОНСТРУКТОР И ДЕСТРУКТОР
// ===========================

MarkSweepGC::MarkSweepGC(
    size_t max_heap_size,
    size_t collection_threshold,
    const std::string& log_file_path)
    : next_object_id(0),
      max_heap_size(max_heap_size),
      collection_threshold(collection_threshold),
      collection_count(0),
      total_objects_collected(0),
      total_memory_freed(0),
      total_collection_time(0),
      current_step(0)
{
    // Открыть файл логирования
    log_file.open(log_file_path, std::ios::app);
    if (log_file.is_open()) {
        log_file << "\n=== Mark-Sweep GC Session Started ===" << std::endl;
    }
    
    log_operation("GC initialized with max_heap=" + std::to_string(max_heap_size));
}

MarkSweepGC::~MarkSweepGC() {
    if (log_file.is_open()) {
        log_file << "=== Mark-Sweep GC Session Ended ===" << std::endl;
        log_file.close();
    }
}

// ===========================
// ОСНОВНОЙ API
// ===========================

/**
 * @brief Выделить память на heap'е
 * 
 * Алгоритм:
 * 1. Проверить, достаточно ли свободной памяти
 * 2. Если нет, запустить сборку мусора
 * 3. Создать новый объект с уникальным ID
 * 4. Добавить в heap
 * 5. Залогировать операцию
 */
int MarkSweepGC::allocate(size_t size) {
    // Проверка границ
    if (size == 0 || size > max_heap_size) {
        log_operation("ALLOCATE FAILED: invalid size " + std::to_string(size));
        return -1;
    }

    // Если мало памяти, запустить сборку
    if (!has_enough_memory(size)) {
        log_operation("ALLOCATE: memory low, triggering collection...");
        collect();
    }

    // Если всё ещё не хватает — ошибка
    if (!has_enough_memory(size)) {
        log_operation("ALLOCATE FAILED: out of memory");
        return -1;
    }

    // Создать новый объект
    int object_id = next_object_id++;
    HeapObject obj(object_id, size, false);
    obj.allocation_step = current_step;
    
    heap[object_id] = obj;

    // Логирование
    std::ostringstream oss;
    oss << "ALLOCATE: obj_" << object_id << " (size=" << size << " bytes)";
    log_operation(oss.str());

    return object_id;
}

/**
 * @brief Создать ссылку от одного объекта к другому
 * 
 * Алгоритм:
 * 1. Проверить, существуют ли оба объекта
 * 2. Проверить, нет ли уже такой ссылки (избежать дубликатов)
 * 3. Добавить ссылку в исходящий граф источника
 * 4. Добавить ссылку в входящий граф назначения
 * 5. Логировать
 */
bool MarkSweepGC::add_reference(int from_id, int to_id) {
    // Проверки
    if (!object_exists(from_id)) {
        std::ostringstream oss;
        oss << "ADD_REF FAILED: source object_" << from_id << " not found";
        log_operation(oss.str());
        return false;
    }

    if (!object_exists(to_id)) {
        std::ostringstream oss;
        oss << "ADD_REF FAILED: target object_" << to_id << " not found";
        log_operation(oss.str());
        return false;
    }

    // Получить объекты
    HeapObject& source = heap[from_id];
    HeapObject& target = heap[to_id];

    // Проверить, нет ли уже такой ссылки
    if (source.outgoing_references.count(to_id) > 0) {
        std::ostringstream oss;
        oss << "ADD_REF SKIPPED: edge obj_" << from_id << " -> obj_" << to_id 
            << " already exists";
        log_operation(oss.str());
        return true; // Уже есть
    }

    // Добавить ссылку
    source.add_reference_to(to_id);
    target.add_reference_from(from_id);

    // Логирование
    std::ostringstream oss;
    oss << "ADD_REF: obj_" << from_id << " -> obj_" << to_id;
    log_operation(oss.str());

    return true;
}

/**
 * @brief Удалить ссылку от одного объекта к другому
 * 
 * Алгоритм:
 * 1. Проверить, существуют ли оба объекта
 * 2. Проверить, есть ли такая ссылка
 * 3. Удалить ссылку из исходящего графа источника
 * 4. Удалить ссылку из входящего графа назначения
 * 5. Логировать
 */
bool MarkSweepGC::remove_reference(int from_id, int to_id) {
    // Проверки
    if (!object_exists(from_id)) {
        std::ostringstream oss;
        oss << "REM_REF FAILED: source object_" << from_id << " not found";
        log_operation(oss.str());
        return false;
    }

    if (!object_exists(to_id)) {
        std::ostringstream oss;
        oss << "REM_REF FAILED: target object_" << to_id << " not found";
        log_operation(oss.str());
        return false;
    }

    // Получить объекты
    HeapObject& source = heap[from_id];
    HeapObject& target = heap[to_id];

    // Проверить, есть ли такая ссылка
    if (source.outgoing_references.count(to_id) == 0) {
        std::ostringstream oss;
        oss << "REM_REF FAILED: edge obj_" << from_id << " -> obj_" << to_id 
            << " doesn't exist";
        log_operation(oss.str());
        return false;
    }

    // Удалить ссылку
    source.remove_reference_to(to_id);
    target.remove_reference_from(from_id);

    // Логирование
    std::ostringstream oss;
    oss << "REM_REF: obj_" << from_id << " -X-> obj_" << to_id;
    log_operation(oss.str());

    return true;
}

/**
 * @brief Запустить цикл Mark-and-Sweep
 * 
 * ГЛАВНЫЙ АЛГОРИТМ:
 * 
 * Фаза 1 - Mark:
 *   - Найти все root объекты
 *   - Для каждого root запустить DFS
 *   - DFS помечает (mark=true) все достижимые объекты
 * 
 * Фаза 2 - Sweep:
 *   - Обойти все объекты на heap'е
 *   - Если объект not marked и not root → удалить его
 *   - Обновить статистику
 * 
 * Сложность: O(n + m), где n - объекты, m - рёбра
 */
size_t MarkSweepGC::collect() {
    auto start_time = std::chrono::high_resolution_clock::now();

    // Логирование начала сборки
    std::ostringstream oss;
    oss << "\n[COLLECTION #" << (collection_count + 1) << "] Starting Mark-Sweep...";
    log_operation(oss.str());

    // === MARK PHASE ===
    log_operation("  Phase 1: MARK - finding reachable objects via DFS from roots");
    mark_phase();

    // === SWEEP PHASE ===
    log_operation("  Phase 2: SWEEP - removing unreachable objects");
    size_t freed_memory = sweep_phase();

    // Обновить статистику
    collection_count++;
    total_memory_freed += freed_memory;

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time
    ).count();
    total_collection_time += duration;

    // Логирование конца сборки
    std::ostringstream oss_end;
    oss_end << "[COLLECTION #" << collection_count << "] Complete. "
            << "Freed: " << freed_memory << " bytes, "
            << "Live objects: " << get_alive_objects_count();
    log_operation(oss_end.str());

    return freed_memory;
}

/**
 * @brief Получить информацию о heap'е в формате JSON
 * 
 * Формат:
 * {
 *   "total_objects": N,
 *   "alive_objects": M,
 *   "total_memory": X,
 *   "free_memory": Y,
 *   "objects": [
 *     {"id": 0, "size": 100, "marked": true, "refs_to": [1, 2], "refs_from": [3]},
 *     ...
 *   ]
 * }
 */
std::string MarkSweepGC::get_heap_info() const {
    std::ostringstream oss;
    
    oss << "{\n";
    oss << "  \"total_objects\": " << heap.size() << ",\n";
    oss << "  \"alive_objects\": " << get_alive_objects_count() << ",\n";
    oss << "  \"total_memory\": " << get_total_memory() << ",\n";
    oss << "  \"free_memory\": " << get_free_memory() << ",\n";
    oss << "  \"objects\": [\n";

    bool first = true;
    for (const auto& [id, obj] : heap) {
        if (!first) oss << ",\n";
        first = false;

        oss << "    {\n";
        oss << "      \"id\": " << obj.id << ",\n";
        oss << "      \"size\": " << obj.size << ",\n";
        oss << "      \"marked\": " << (obj.is_marked ? "true" : "false") << ",\n";
        oss << "      \"is_root\": " << (obj.is_root ? "true" : "false") << ",\n";
        oss << "      \"alive\": " << (obj.is_alive ? "true" : "false") << ",\n";
        oss << "      \"refs_to\": [";
        
        bool first_ref = true;
        for (int ref_id : obj.outgoing_references) {
            if (!first_ref) oss << ", ";
            first_ref = false;
            oss << ref_id;
        }
        oss << "],\n";
        
        oss << "      \"refs_from\": [";
        first_ref = true;
        for (int ref_id : obj.incoming_references) {
            if (!first_ref) oss << ", ";
            first_ref = false;
            oss << ref_id;
        }
        oss << "]\n";
        oss << "    }";
    }

    oss << "\n  ]\n";
    oss << "}\n";

    return oss.str();
}

/**
 * @brief Получить статистику работы GC
 */
std::string MarkSweepGC::get_gc_stats() const {
    std::ostringstream oss;
    
    oss << "=== Mark-Sweep GC Statistics ===\n";
    oss << "Collections run: " << collection_count << "\n";
    oss << "Total objects collected: " << total_objects_collected << "\n";
    oss << "Total memory freed: " << total_memory_freed << " bytes\n";
    oss << "Total collection time: " << total_collection_time << " µs\n";
    
    if (collection_count > 0) {
        oss << "Average collection time: " 
            << (total_collection_time / collection_count) << " µs\n";
        oss << "Average objects per collection: "
            << (total_objects_collected / collection_count) << "\n";
    }
    
    oss << "Heap usage: " << get_total_memory() << " / " << max_heap_size 
        << " bytes (" 
        << ((get_total_memory() * 100) / max_heap_size) << "%)\n";

    return oss.str();
}

/**
 * @brief Получить общий размер выделенной памяти
 */
size_t MarkSweepGC::get_total_memory() const {
    size_t total = 0;
    for (const auto& [id, obj] : heap) {
        if (obj.is_alive) {
            total += obj.size;
        }
    }
    return total;
}

/**
 * @brief Получить размер свободной памяти
 */
size_t MarkSweepGC::get_free_memory() const {
    return max_heap_size - get_total_memory();
}

// ===========================
// ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ
// ===========================

/**
 * @brief Сделать объект root (всегда достижимым)
 */
void MarkSweepGC::make_root(int object_id) {
    if (object_exists(object_id)) {
        heap[object_id].is_root = true;
        std::ostringstream oss;
        oss << "MAKE_ROOT: obj_" << object_id << " is now a root object";
        log_operation(oss.str());
    }
}

/**
 * @brief Удалить статус root у объекта
 */
void MarkSweepGC::remove_root(int object_id) {
    if (object_exists(object_id)) {
        heap[object_id].is_root = false;
        std::ostringstream oss;
        oss << "REMOVE_ROOT: obj_" << object_id << " is no longer a root";
        log_operation(oss.str());
    }
}

/**
 * @brief Получить объект по ID
 */
HeapObject* MarkSweepGC::get_object(int id) {
    auto it = heap.find(id);
    if (it != heap.end()) {
        return &it->second;
    }
    return nullptr;
}

/**
 * @brief Получить константный объект по ID
 */
const HeapObject* MarkSweepGC::get_object(int id) const {
    auto it = heap.find(id);
    if (it != heap.end()) {
        return &it->second;
    }
    return nullptr;
}

/**
 * @brief Проверить, существует ли объект
 */
bool MarkSweepGC::object_exists(int id) const {
    return heap.find(id) != heap.end() && heap.at(id).is_alive;
}

/**
 * @brief Получить количество живых объектов
 */
int MarkSweepGC::get_alive_objects_count() const {
    int count = 0;
    for (const auto& [id, obj] : heap) {
        if (obj.is_alive) {
            count++;
        }
    }
    return count;
}

// ===========================
// ВНУТРЕННИЕ МЕТОДЫ
// ===========================

/**
 * @brief Mark фаза: пометить все достижимые объекты
 * 
 * Алгоритм:
 * 1. Получить все root объекты
 * 2. Для каждого root запустить DFS
 * 3. DFS помечает все посещённые объекты
 */
void MarkSweepGC::mark_phase() {
    // Сначала разметить всё как недостижимое
    for (auto& [id, obj] : heap) {
        obj.unmark();
    }

    // Получить root объекты
    std::vector<int> roots = get_root_objects();

    std::ostringstream oss;
    oss << "    Found " << roots.size() << " root objects: [";
    for (size_t i = 0; i < roots.size(); i++) {
        if (i > 0) oss << ", ";
        oss << "obj_" << roots[i];
    }
    oss << "]";
    log_operation(oss.str());

    // Запустить DFS из каждого root
    for (int root_id : roots) {
        std::ostringstream oss_dfs;
        oss_dfs << "    Starting DFS from root obj_" << root_id;
        log_operation(oss_dfs.str());
        dfs_mark(root_id);
    }

    // Логирование результата mark
    int marked_count = 0;
    for (const auto& [id, obj] : heap) {
        if (obj.is_marked) {
            marked_count++;
        }
    }
    std::ostringstream oss_result;
    oss_result << "    Mark phase complete. " << marked_count 
               << " objects marked as reachable.";
    log_operation(oss_result.str());
}

/**
 * @brief Sweep фаза: удалить все непомеченные объекты
 * 
 * Алгоритм:
 * 1. Обойти все объекты
 * 2. Если not marked и not root → удалить
 * 3. При удалении обновить граф ссылок
 * 4. Вернуть количество освобождённой памяти
 */
size_t MarkSweepGC::sweep_phase() {
    std::vector<int> to_delete;

    // Найти все объекты для удаления
    for (const auto& [id, obj] : heap) {
        if (!obj.is_marked && !obj.is_root && obj.is_alive) {
            to_delete.push_back(id);
        }
    }

    // Логирование найденных объектов
    std::ostringstream oss;
    oss << "    Found " << to_delete.size() << " objects to delete: [";
    for (size_t i = 0; i < to_delete.size(); i++) {
        if (i > 0) oss << ", ";
        oss << "obj_" << to_delete[i];
    }
    oss << "]";
    log_operation(oss.str());

    size_t freed_memory = 0;

    // Удалить объекты
    for (int id : to_delete) {
        HeapObject& obj = heap[id];
        
        // Удалить все ссылки от других объектов на этот
        for (int source_id : obj.incoming_references) {
            if (object_exists(source_id)) {
                heap[source_id].remove_reference_to(id);
            }
        }

        // Удалить все ссылки от этого объекта на другие
        for (int target_id : obj.outgoing_references) {
            if (object_exists(target_id)) {
                heap[target_id].remove_reference_from(id);
            }
        }

        // Пометить как мёртвый
        obj.is_alive = false;
        obj.collection_step = current_step;
        freed_memory += obj.size;

        // Логирование удаления
        std::ostringstream oss_del;
        oss_del << "    Deleted obj_" << id << " (" << obj.size << " bytes)";
        log_operation(oss_del.str());
    }

    total_objects_collected += to_delete.size();

    std::ostringstream oss_result;
    oss_result << "    Sweep phase complete. Freed " << freed_memory << " bytes.";
    log_operation(oss_result.str());

    return freed_memory;
}

/**
 * @brief DFS для поиска достижимых объектов
 * 
 * Алгоритм:
 * 1. Пометить текущий объект
 * 2. Для каждой исходящей ссылки
 * 3. Если целевой объект не помечен, запустить DFS рекурсивно
 */
void MarkSweepGC::dfs_mark(int object_id) {
    if (!object_exists(object_id)) {
        return;
    }

    HeapObject& obj = heap[object_id];

    // Если уже помечен, избежать бесконечного цикла
    if (obj.is_marked) {
        return;
    }

    // Пометить как достижимый
    obj.is_marked = true;

    std::ostringstream oss;
    oss << "      Mark obj_" << object_id;
    log_operation(oss.str());

    // DFS по всем исходящим ссылкам
    for (int target_id : obj.outgoing_references) {
        if (!heap[target_id].is_marked) {
            dfs_mark(target_id);
        }
    }
}

/**
 * @brief Получить список всех root объектов
 */
std::vector<int> MarkSweepGC::get_root_objects() const {
    std::vector<int> roots;
    for (const auto& [id, obj] : heap) {
        if (obj.is_root && obj.is_alive) {
            roots.push_back(id);
        }
    }
    return roots;
}

/**
 * @brief Проверить, достаточно ли памяти для выделения
 */
bool MarkSweepGC::has_enough_memory(size_t size) const {
    return get_free_memory() >= size;
}

/**
 * @brief Логировать операцию
 */
void MarkSweepGC::log_operation(const std::string& operation) {
    // Добавить в вектор
    operation_logs.push_back(operation);
    last_operation = operation;

    // Записать в файл
    if (log_file.is_open()) {
        log_file << "[Step " << current_step << "] " << operation << std::endl;
        log_file.flush();
    }

    // Вывести в консоль для отладки
    std::cout << "[Step " << current_step << "] " << operation << std::endl;
}
