#ifndef HEAP_OBJECT_H
#define HEAP_OBJECT_H

#include <cstddef>
#include <vector>
#include <set>

/**
 * @brief Представляет объект, выделенный на heap'е
 * 
 * Каждый объект содержит:
 * - Уникальный ID
 * - Размер
 * - Флаг Mark для M&S
 * - Счётчик ссылок для RC
 * - Список ссылок на другие объекты
 * - Счётчик входящих ссылок (для общей статистики)
 */
struct HeapObject {
    int id;                              // Уникальный ID объекта
    size_t size;                         // Размер объекта в байтах
    
    // Mark-and-Sweep специфично
    bool is_marked;                      // Флаг: объект помечен на mark-фазе?
    
    // Reference Counting специфично
    int reference_count;                 // Счётчик входящих ссылок
    
    // Граф ссылок
    std::set<int> outgoing_references;   // На какие объекты ссылаемся
    std::set<int> incoming_references;   // На нас ссылаются из каких объектов
    
    // Метаинформация
    bool is_root;                        // Это root объект (всегда достижим)?
    bool is_alive;                       // Объект ещё на heap'е?
    int allocation_step;                 // На каком шаге симуляции был выделен
    int collection_step;                 // На каком шаге симуляции был удален (-1 если жив)

    /**
     * @brief Конструктор по умолчанию
     */
    HeapObject() 
        : id(-1), 
          size(0), 
          is_marked(false), 
          reference_count(0),
          is_root(false), 
          is_alive(true), 
          allocation_step(-1),
          collection_step(-1) 
    {}

    /**
     * @brief Конструктор с инициализацией
     */
    HeapObject(int id, size_t size, bool is_root = false)
        : id(id),
          size(size),
          is_marked(false),
          reference_count(is_root ? 1 : 0),  // Root объекты имеют "виртуальную" ссылку
          is_root(is_root),
          is_alive(true),
          allocation_step(-1),
          collection_step(-1)
    {}

    /**
     * @brief Добавить исходящую ссылку на объект
     */
    void add_reference_to(int target_id) {
        outgoing_references.insert(target_id);
    }

    /**
     * @brief Удалить исходящую ссылку на объект
     */
    void remove_reference_to(int target_id) {
        outgoing_references.erase(target_id);
    }

    /**
     * @brief Добавить входящую ссылку от объекта
     */
    void add_reference_from(int source_id) {
        incoming_references.insert(source_id);
    }

    /**
     * @brief Удалить входящую ссылку от объекта
     */
    void remove_reference_from(int source_id) {
        incoming_references.erase(source_id);
    }

    /**
     * @brief Получить количество входящих ссылок
     */
    int get_incoming_reference_count() const {
        return static_cast<int>(incoming_references.size());
    }

    /**
     * @brief Получить количество исходящих ссылок
     */
    int get_outgoing_reference_count() const {
        return static_cast<int>(outgoing_references.size());
    }

    /**
     * @brief Сбросить флаг Mark для новой итерации сборки
     */
    void unmark() {
        is_marked = false;
    }

    /**
     * @brief Проверить, имеет ли объект ссылки
     */
    bool has_references() const {
        return !outgoing_references.empty();
    }
};

#endif // HEAP_OBJECT_H
