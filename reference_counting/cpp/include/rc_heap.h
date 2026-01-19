#ifndef RC_HEAP_H
#define RC_HEAP_H

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include "rc_object.h"
#include "reference_counter.h"
#include "event_logger.h"

/**
 * @struct ScenarioOp
 * @brief Представляет одну операцию в сценарии тестирования
 */
struct ScenarioOp
{
    std::string op; // "allocate", "add_ref", "remove_ref", "delete_root"
    int id;         // Для allocate
    int from;       // Для add_ref и remove_ref
    int to;         // Для add_ref и remove_ref

    ScenarioOp() : op(""), id(-1), from(-1), to(-1) {}
    ScenarioOp(const std::string &op_, int id_, int from_ = -1, int to_ = -1)
        : op(op_), id(id_), from(from_), to(to_) {}
};

/**
 * @class RCHeap
 * @brief Управляет кучей объектов с подсчётом ссылок и корнями
 *
 * Инкапсулирует управление памятью, добавление/удаление ссылок,
 * управление корнями (roots) и визуализацию состояния кучи.
 *
 * **ВАЖНО: RC ONLY! Только объекты с ref_count == 0 удаляются!**
 */
class RCHeap
{
public:
    /**
     * @brief Конструктор с логгером
     * @param logger Ссылка на логгер событий
     */
    explicit RCHeap(EventLogger &logger);

    /**
     * @brief Выделить новый объект в куче
     * @param obj_id ID выделяемого объекта
     * @return true, если объект успешно выделен
     */
    bool allocate(int obj_id);

    /**
     * @brief Добавить объект в корни (root)
     * @param obj_id ID объекта для добавления в корни
     * @return true, если объект добавлен в корни
     */
    bool add_root(int obj_id);

    /**
     * @brief Удалить объект из корней (root)
     * @param obj_id ID объекта для удаления из корней
     * @return true, если объект удалён из корней
     */
    bool remove_root(int obj_id);

    /**
     * @brief Добавить ссылку от одного объекта к другому
     * @param from ID объекта-источника
     * @param to ID объекта-цели
     * @return true, если ссылка успешно добавлена
     */
    bool add_ref(int from, int to);

    /**
     * @brief Удалить ссылку между объектами
     * @param from ID объекта-источника
     * @param to ID объекта-цели
     * @return true, если ссылка успешно удалена
     */
    bool remove_ref(int from, int to);

    /**
     * @brief Вывести текущее состояние кучи в консоль
     */
    void dump_state() const;

    /**
     * @brief Выполнить последовательность операций из сценария
     * @param ops Массив операций сценария
     * @param size Размер массива операций
     */
    void run_scenario(const ScenarioOp ops[], int size);

    /**
     * @brief Получить количество объектов в куче
     * @return Размер кучи
     */
    size_t get_heap_size() const { return objects.size(); }

    /**
     * @brief Проверить, существует ли объект в куче
     * @param obj_id ID проверяемого объекта
     * @return true, если объект существует
     */
    bool object_exists(int obj_id) const { return objects.count(obj_id) > 0; }

    /**
     * @brief Получить ref_count объекта
     * @param obj_id ID объекта
     * @return ref_count, или -1 если объект не существует
     */
    int get_ref_count(int obj_id) const;

    /**
     * @brief Обнаружить и зарегистрировать утечки памяти
     *
     * Проверяет все объекты в куче с ref_count > 0 (которые не удалены)
     * Это объекты, участвующие в циклических ссылках!
     */
    void detect_and_log_leaks();

    /**
     * @brief Получить количество корней
     * @return Размер множества корней
     */
    size_t get_roots_count() const { return roots.size(); }

    // ReferenceCounter должен иметь доступ к private методам
    friend class ReferenceCounter;

private:
    std::unordered_map<int, RCObject> objects; ///< Куча объектов
    std::unordered_set<int> roots;             ///< Корни (root объекты)
    ReferenceCounter rc;                       ///< Управление ссылками
    EventLogger &logger;                       ///< Логгер событий

    /**
     * @brief Получить объект по ID (внутренняя функция)
     * @param obj_id ID объекта
     * @return Указатель на объект, или nullptr
     */
    RCObject *get_object(int obj_id);

    /**
     * @brief Получить объект по ID (константная версия)
     * @param obj_id ID объекта
     * @return Указатель на константный объект, или nullptr
     */
    const RCObject *get_object(int obj_id) const;
};

#endif // RC_HEAP_H