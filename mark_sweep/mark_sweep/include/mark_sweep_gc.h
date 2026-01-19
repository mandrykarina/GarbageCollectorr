#ifndef MARK_SWEEP_GC_H
#define MARK_SWEEP_GC_H

#include "gc_interface.h"
#include "heap_object.h"
#include <unordered_map>
#include <queue>
#include <fstream>
#include <memory>

/**
 * @brief Реализация сборщика мусора Mark-and-Sweep
 * 
 * Алгоритм:
 * 1. Mark фаза: DFS из root объектов, помечаем достижимые объекты
 * 2. Sweep фаза: Обходим все объекты, удаляем непомеченные
 * 3. Логирование: Каждый шаг логируется для визуализации
 * 
 * Сложность: O(n + m), где n - объекты, m - ссылки
 */
class MarkSweepGC : public GCInterface {
private:
    // === ОСНОВНЫЕ СТРУКТУРЫ ===
    
    /** @brief Хранилище всех объектов на heap'е */
    std::unordered_map<int, HeapObject> heap;
    
    /** @brief Максимальный ID для автоинкремента */
    int next_object_id;
    
    /** @brief Максимальный размер heap'а (в байтах) */
    size_t max_heap_size;
    
    /** @brief Максимальный пороговый размер до принудительной сборки */
    size_t collection_threshold;
    
    // === ЛОГИРОВАНИЕ ===
    
    /** @brief Все логи операций */
    std::vector<std::string> operation_logs;
    
    /** @brief Последняя операция */
    std::string last_operation;
    
    /** @brief Поток для записи в файл */
    std::ofstream log_file;
    
    // === СТАТИСТИКА ===
    
    /** @brief Количество запущенных циклов сборки */
    int collection_count;
    
    /** @brief Общее количество удалённых объектов */
    int total_objects_collected;
    
    /** @brief Общее количество освобождённой памяти */
    size_t total_memory_freed;
    
    /** @brief Общее время на сборку (в условных единицах) */
    int total_collection_time;
    
    // === ТЕКУЩИЙ ШАГ СИМУЛЯЦИИ ===
    
    /** @brief Номер текущего шага */
    int current_step;

public:
    // === КОНСТРУКТОР И ДЕСТРУКТОР ===
    
    /**
     * @brief Конструктор
     * @param max_heap_size Максимальный размер heap'а (по умолчанию 1MB)
     * @param collection_threshold Порог для автоматической сборки (по умолчанию 80%)
     * @param log_file_path Путь для логирования
     */
    MarkSweepGC(
        size_t max_heap_size = 1024 * 1024,
        size_t collection_threshold = (1024 * 1024 * 80) / 100,
        const std::string& log_file_path = "ms_trace.log"
    );
    
    /**
     * @brief Деструктор
     */
    ~MarkSweepGC() override;

    // === ОСНОВНОЙ API (из GCInterface) ===
    
    /**
     * @brief Выделить память на heap'е
     * @param size Размер объекта
     * @return ID объекта
     */
    int allocate(size_t size) override;

    /**
     * @brief Создать ссылку от одного объекта к другому
     * @param from_id ID объекта-источника
     * @param to_id ID объекта-назначения
     * @return true если успешно
     */
    bool add_reference(int from_id, int to_id) override;

    /**
     * @brief Удалить ссылку
     * @param from_id ID объекта-источника
     * @param to_id ID объекта-назначения
     * @return true если успешно
     */
    bool remove_reference(int from_id, int to_id) override;

    /**
     * @brief Запустить цикл Mark-and-Sweep
     * @return Количество освобождённой памяти
     */
    size_t collect() override;

    /**
     * @brief Получить информацию о heap'е
     * @return JSON-подобная строка с информацией
     */
    std::string get_heap_info() const override;

    /**
     * @brief Получить статистику сборки
     * @return Строка со статистикой
     */
    std::string get_gc_stats() const override;

    /**
     * @brief Получить последний лог операции
     */
    std::string get_last_operation_log() const override {
        return last_operation;
    }

    /**
     * @brief Получить все логи
     */
    std::vector<std::string> get_all_logs() const override {
        return operation_logs;
    }

    /**
     * @brief Очистить логи
     */
    void clear_logs() override {
        operation_logs.clear();
        last_operation = "";
    }

    /**
     * @brief Получить общий размер выделенной памяти
     */
    size_t get_total_memory() const override;

    /**
     * @brief Получить размер свободной памяти
     */
    size_t get_free_memory() const override;

    // === ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ ===

    /**
     * @brief Сделать объект root (всегда достижимым)
     * @param object_id ID объекта
     */
    void make_root(int object_id);

    /**
     * @brief Удалить статус root у объекта
     * @param object_id ID объекта
     */
    void remove_root(int object_id);

    /**
     * @brief Получить объект по ID
     * @return Указатель на объект или nullptr
     */
    HeapObject* get_object(int id);

    /**
     * @brief Получить константный указатель на объект
     */
    const HeapObject* get_object(int id) const;

    /**
     * @brief Проверить, существует ли объект на heap'е
     */
    bool object_exists(int id) const;

    /**
     * @brief Получить количество живых объектов
     */
    int get_alive_objects_count() const;

    /**
     * @brief Получить все объекты (для визуализации)
     */
    const std::unordered_map<int, HeapObject>& get_all_objects() const {
        return heap;
    }

    /**
     * @brief Установить текущий шаг симуляции
     */
    void set_current_step(int step) {
        current_step = step;
    }

    /**
     * @brief Получить текущий шаг симуляции
     */
    int get_current_step() const {
        return current_step;
    }

private:
    // === ВНУТРЕННИЕ МЕТОДЫ ===

    /**
     * @brief Mark фаза: пометить все достижимые объекты
     */
    void mark_phase();

    /**
     * @brief Sweep фаза: удалить все непомеченные объекты
     * @return Количество освобождённой памяти
     */
    size_t sweep_phase();

    /**
     * @brief DFS для поиска всех достижимых объектов (из root)
     * @param object_id ID текущего объекта
     */
    void dfs_mark(int object_id);

    /**
     * @brief Логировать операцию
     * @param operation Описание операции
     */
    void log_operation(const std::string& operation);

    /**
     * @brief Получить список всех root объектов
     */
    std::vector<int> get_root_objects() const;

    /**
     * @brief Проверить, достаточно ли памяти для выделения
     */
    bool has_enough_memory(size_t size) const;
};

#endif // MARK_SWEEP_GC_H
