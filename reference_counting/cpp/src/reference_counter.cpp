#include "reference_counter.h"
#include <iostream>
#include <algorithm>

ReferenceCounter::ReferenceCounter(
    std::unordered_map<int, RCObject> &heap_,
    EventLogger &logger_)
    : heap(heap_), logger(logger_) {}

bool ReferenceCounter::add_ref(int from, int to)
{
    // Проверяем что оба объекта существуют
    if (!heap.count(from) || !heap.count(to))
    {
        std::cerr << "add_ref error: object not found (from=" << from
                  << ", to=" << to << ")\n";
        return false;
    }

    RCObject &src = heap[from];
    RCObject &dst = heap[to];

    // Проверяем что такой ссылки ещё нет
    if (src.has_reference_to(to))
    {
        return false;
    }

    // Добавляем исходящую ссылку и увеличиваем счётчик целевого объекта
    src.add_outgoing_ref(to);
    dst.ref_count++;

    logger.log_add_ref(from, to, dst.ref_count);
    return true;
}

bool ReferenceCounter::remove_ref(int from, int to)
{
    // ⚠️ ВАЖНО: from может быть удалён во время cascade_delete
    // Но to ДОЛЖЕН существовать!

    if (!heap.count(to))
    {
        std::cerr << "remove_ref error: target object not found (to=" << to << ")\n";
        return false;
    }

    // Если from удалён - это нормально для cascade_delete, пропускаем
    if (!heap.count(from))
    {
        return false;
    }

    RCObject &src = heap[from];
    RCObject &dst = heap[to];

    // Проверяем что ссылка существует
    if (!src.has_reference_to(to))
    {
        return false;
    }

    // Удаляем исходящую ссылку и уменьшаем счётчик целевого объекта
    src.remove_outgoing_ref(to);
    dst.ref_count--;

    if (dst.ref_count < 0)
    {
        std::cerr << "ERROR: ref_count became negative for object " << to << "\n";
        dst.ref_count = 0;
        return false;
    }

    logger.log_remove_ref(from, to, dst.ref_count);

    // ✅ КЛЮЧЕВОЕ ИЗМЕНЕНИЕ: Cascade delete только если ref_count стал 0
    if (dst.ref_count == 0)
    {
        std::unordered_set<int> visited;
        cascade_delete(to, visited);
    }

    return true;
}

void ReferenceCounter::cascade_delete(int obj_id, std::unordered_set<int> &visited)
{
    // Проверяем что объект существует
    if (!heap.count(obj_id))
    {
        return;
    }

    // Защита от бесконечной рекурсии при циклах
    if (visited.count(obj_id))
    {
        return;
    }
    visited.insert(obj_id);

    RCObject &obj = heap[obj_id];

    // ⚠️ КЛЮЧЕВАЯ ПРОВЕРКА: удаляем только если ref_count == 0
    if (obj.ref_count != 0)
    {
        std::cerr << "WARNING: cascade_delete called on object " << obj_id
                  << " with ref_count=" << obj.ref_count << "\n";
        return;
    }

    // 1️⃣ КОПИРУЕМ исходящие ссылки (потому что удалим их)
    auto children = obj.references;

    // 2️⃣ УДАЛЯЕМ ВСЕ исходящие ссылки (это уменьшит ref_count дочерних объектов)
    for (int child : children)
    {
        if (heap.count(child))
        {
            // ⚠️ Рекурсивное удаление ссылки
            RCObject &child_obj = heap[child];
            child_obj.ref_count--;

            if (child_obj.ref_count < 0)
            {
                std::cerr << "ERROR: ref_count became negative for child "
                          << child << "\n";
                child_obj.ref_count = 0;
            }

            logger.log_remove_ref(obj_id, child, child_obj.ref_count);

            // 3️⃣ Если у дочернего объекта ref_count стал 0, удаляем его рекурсивно
            if (child_obj.ref_count == 0)
            {
                cascade_delete(child, visited);
            }
        }
    }

    // 4️⃣ УДАЛЯЕМ САМ ОБЪЕКТ из кучи (в конце!)
    heap.erase(obj_id);
    logger.log_delete(obj_id);
}