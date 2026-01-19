#ifndef GC_INTERFACE_H
#define GC_INTERFACE_H

#include <string>
#include <vector>
#include <cstddef>

/**
 * @brief Абстрактный интерфейс для всех сборщиков мусора
 * 
 * Определяет единую сигнатуру методов, которую должны реализовать
 * как Reference Counting, так и Mark-and-Sweep
 */
class GCInterface {
public:
    virtual ~GCInterface() = default;
    
    /**
     * @brief Выделить память на heap'е
     * @param size Размер объекта в байтах
     * @return ID выделенного объекта (-1 если ошибка)
     */
    virtual int allocate(size_t size) = 0;
    
    /**
     * @brief Создать ссылку от одного объекта к другому
     * @param from_id ID объекта-источника
     * @param to_id ID объекта-назначения
     * @return true если ссылка создана, false если ошибка
     */
    virtual bool add_reference(int from_id, int to_id) = 0;
    
    /**
     * @brief Удалить ссылку от одного объекта к другому
     * @param from_id ID объекта-источника
     * @param to_id ID объекта-назначения
     * @return true если ссылка удалена, false если ошибка
     */
    virtual bool remove_reference(int from_id, int to_id) = 0;
    
    /**
     * @brief Запустить цикл сборки мусора
     * @return Количество освобождённых байтов
     */
    virtual size_t collect() = 0;
    
    /**
     * @brief Получить текущее состояние heap'а
     * @return Строка с информацией об объектах
     */
    virtual std::string get_heap_info() const = 0;
    
    /**
     * @brief Получить статистику работы GC
     * @return Статистика (формат определяется реализацией)
     */
    virtual std::string get_gc_stats() const = 0;
    
    /**
     * @brief Получить лог последней операции
     * @return Строка-лог для визуализации
     */
    virtual std::string get_last_operation_log() const = 0;
    
    /**
     * @brief Получить все логи операций
     * @return Вектор строк-логов
     */
    virtual std::vector<std::string> get_all_logs() const = 0;
    
    /**
     * @brief Очистить все логи
     */
    virtual void clear_logs() = 0;
    
    /**
     * @brief Получить общий размер выделенной памяти
     * @return Размер в байтах
     */
    virtual size_t get_total_memory() const = 0;
    
    /**
     * @brief Получить размер свободной памяти
     * @return Размер в байтах
     */
    virtual size_t get_free_memory() const = 0;
    
    // === НОВЫЕ МЕТОДЫ ДЛЯ СИМУЛЯТОРА ===
    
    /**
     * @brief Установить текущий номер шага симуляции
     * @param step Номер шага
     */
    virtual void set_current_step(int step) = 0;
    
    /**
     * @brief Получить текущий номер шага симуляции
     * @return Номер шага
     */
    virtual int get_current_step() const = 0;
    
    /**
     * @brief Получить количество живых объектов на heap'е
     * @return Количество объектов
     */
    virtual int get_alive_objects_count() const = 0;
};

#endif // GC_INTERFACE_H
