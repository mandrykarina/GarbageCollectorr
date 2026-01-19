#ifndef REFERENCE_COUNTER_H
#define REFERENCE_COUNTER_H

#include <unordered_map>
#include <unordered_set>
#include "rc_object.h"
#include "event_logger.h"

class RCHeap; // Forward declaration

/**
 * Reference Counting Garbage Collector
 *
 * АЛГОРИТМ:
 * 1. allocate(id) - создать объект с rc=0
 * 2. add_ref(from, to) - добавить from→to, to.rc++
 * 3. remove_ref(from, to) - удалить from→to, to.rc--
 *    - Если to.rc == 0 → cascade_delete(to)
 * 4. cascade_delete(id):
 *    - Рекурсивно удаляем все исходящие ссылки
 *    - Затем удаляем сам объект
 *
 * ЦИКЛИЧЕСКИЕ ССЫЛКИ: Остаются живыми (ref_count > 0) - это УТЕЧКА
 */
class ReferenceCounter
{
public:
    ReferenceCounter(std::unordered_map<int, RCObject> &heap,
                     EventLogger &logger);

    /**
     * @brief Добавить ссылку от одного объекта к другому
     * @param from ID объекта-источника
     * @param to ID объекта-цели
     * @return true если успешно
     */
    bool add_ref(int from, int to);

    /**
     * @brief Удалить ссылку между объектами
     * @param from ID объекта-источника
     * @param to ID объекта-цели
     * @return true если успешно
     */
    bool remove_ref(int from, int to);

    /**
     * @brief Каскадное удаление объекта
     * Удаляет все исходящие ссылки рекурсивно, затем сам объект
     */
    void cascade_delete(int obj_id, std::unordered_set<int> &visited);

private:
    std::unordered_map<int, RCObject> &heap;
    EventLogger &logger;

    friend class RCHeap;
};

#endif