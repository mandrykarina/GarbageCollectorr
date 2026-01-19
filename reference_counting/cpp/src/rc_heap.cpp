#include "rc_heap.h"
#include <iostream>
#include <algorithm>

RCHeap::RCHeap(EventLogger &logger_)
    : rc(objects, logger_), logger(logger_) {}

bool RCHeap::allocate(int obj_id)
{
    // Проверить, не существует ли уже объект с таким ID
    if (objects.count(obj_id) > 0)
    {
        std::cerr << "Error: Object " << obj_id << " already exists\n";
        return false;
    }

    // Проверить валидность ID
    if (obj_id < 0)
    {
        std::cerr << "Error: Invalid object ID " << obj_id << "\n";
        return false;
    }

    // Выделить новый объект
    objects.emplace(obj_id, RCObject(obj_id));
    logger.log_allocate(obj_id);

    return true;
}

bool RCHeap::add_root(int obj_id)
{
    // Проверить, существует ли объект
    if (!object_exists(obj_id))
    {
        std::cerr << "Error: Object " << obj_id << " does not exist\n";
        return false;
    }

    // Проверить, не является ли объект уже корнем
    if (roots.count(obj_id) > 0)
    {
        std::cerr << "Warning: Object " << obj_id << " is already a root\n";
        return false;
    }

    // Добавить в корни и увеличить ref_count
    roots.insert(obj_id);
    objects[obj_id].ref_count++;
    logger.log_add_ref(0, obj_id, objects[obj_id].ref_count); // 0 = root

    return true;
}

bool RCHeap::remove_root(int obj_id)
{
    // Проверить, существует ли объект
    if (!object_exists(obj_id))
    {
        std::cerr << "Error: Object " << obj_id << " does not exist\n";
        return false;
    }

    // Проверить, является ли объект корнем
    if (roots.count(obj_id) == 0)
    {
        std::cerr << "Error: Object " << obj_id << " is not a root\n";
        return false;
    }

    // Удалить из корней и уменьшить ref_count
    roots.erase(obj_id);
    objects[obj_id].ref_count--;

    if (objects[obj_id].ref_count < 0)
    {
        std::cerr << "Error: ref_count became negative for root object " << obj_id << "\n";
        objects[obj_id].ref_count = 0;
        return false;
    }

    logger.log_remove_ref(0, obj_id, objects[obj_id].ref_count); // 0 = root

    // ✅ Если ref_count == 0, начать каскадное удаление
    if (objects[obj_id].ref_count == 0)
    {
        std::unordered_set<int> visited;
        rc.cascade_delete(obj_id, visited);
    }

    return true;
}

bool RCHeap::add_ref(int from, int to)
{
    // Валидация ID'ов
    if (from < 0 || to < 0)
    {
        std::cerr << "Error: Invalid object IDs\n";
        return false;
    }

    // Проверить, существуют ли оба объекта
    if (!object_exists(from))
    {
        std::cerr << "Error: Source object " << from << " does not exist\n";
        return false;
    }

    if (!object_exists(to))
    {
        std::cerr << "Error: Target object " << to << " does not exist\n";
        return false;
    }

    // Запретить саморефренцию
    if (from == to)
    {
        std::cerr << "Error: Self-reference not allowed\n";
        return false;
    }

    // Делегировать ReferenceCounter
    return rc.add_ref(from, to);
}

bool RCHeap::remove_ref(int from, int to)
{
    // Валидация ID'ов
    if (from < 0 || to < 0)
    {
        std::cerr << "Error: Invalid object IDs\n";
        return false;
    }

    // Проверить, существует ли source объект (может быть удален во время cascade)
    if (!object_exists(from))
    {
        std::cerr << "Error: Source object " << from << " does not exist\n";
        return false;
    }

    // Проверить, существует ли target объект
    if (!object_exists(to))
    {
        std::cerr << "Error: Target object " << to << " does not exist\n";
        return false;
    }

    // Делегировать ReferenceCounter
    return rc.remove_ref(from, to);
}

void RCHeap::dump_state() const
{
    std::cout << "=== HEAP STATE ===\n";

    // Вывести корни
    std::cout << "ROOTS: ";
    if (roots.empty())
    {
        std::cout << "[none]";
    }
    else
    {
        for (int root : roots)
        {
            std::cout << root << " ";
        }
    }
    std::cout << "\n\n";

    if (objects.empty())
    {
        std::cout << "[empty]\n";
    }
    else
    {
        // Вывести объекты отсортированные по ID для консистентности
        std::vector<int> ids;
        for (const auto &[id, _] : objects)
        {
            ids.push_back(id);
        }

        std::sort(ids.begin(), ids.end());

        for (int id : ids)
        {
            const RCObject &obj = objects.at(id);
            std::cout << "Object " << id
                      << " | ref_count=" << obj.ref_count
                      << " | refs: ";

            // Вывести исходящие ссылки
            for (int ref : obj.references)
            {
                std::cout << ref << " ";
            }

            std::cout << "\n";
        }
    }

    std::cout << "=================\n\n";
}

void RCHeap::run_scenario(const ScenarioOp ops[], int size)
{
    for (int i = 0; i < size; ++i)
    {
        const ScenarioOp &op = ops[i];

        if (op.op == "allocate")
        {
            allocate(op.id);
        }
        else if (op.op == "add_root")
        {
            add_root(op.id);
        }
        else if (op.op == "remove_root")
        {
            remove_root(op.id);
        }
        else if (op.op == "add_ref")
        {
            add_ref(op.from, op.to);
        }
        else if (op.op == "remove_ref")
        {
            remove_ref(op.from, op.to);
        }
        else
        {
            std::cerr << "Unknown operation: " << op.op << "\n";
        }

        dump_state();
    }
}

int RCHeap::get_ref_count(int obj_id) const
{
    auto it = objects.find(obj_id);
    if (it != objects.end())
    {
        return it->second.ref_count;
    }

    return -1; // Объект не существует
}

void RCHeap::detect_and_log_leaks()
{
    for (const auto &[id, obj] : objects)
    {
        // Логировать объекты с ref_count > 0 (утечка памяти!)
        if (obj.ref_count > 0)
        {
            logger.log_leak(id);
        }
    }
}

RCObject *RCHeap::get_object(int obj_id)
{
    auto it = objects.find(obj_id);
    if (it != objects.end())
    {
        return &it->second;
    }

    return nullptr;
}

const RCObject *RCHeap::get_object(int obj_id) const
{
    auto it = objects.find(obj_id);
    if (it != objects.end())
    {
        return &it->second;
    }

    return nullptr;
}