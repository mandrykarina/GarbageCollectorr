#ifndef RC_OBJECT_H
#define RC_OBJECT_H

#include <vector>
#include <algorithm>
#include <iostream>

/**
 * @struct RCObject
 * @brief Объект в управляемой памяти с подсчётом ссылок
 *
 * Содержит счётчик ссылок и список объектов, на которые данный объект ссылается.
 */
struct RCObject
{
    int id;                      ///< Уникальный идентификатор объекта
    int ref_count;               ///< Количество входящих ссылок
    std::vector<int> references; ///< Список объектов, на которые ссылается данный объект

    /**
     * @brief Конструктор по умолчанию
     */
    RCObject() : id(-1), ref_count(0) {}

    /**
     * @brief Конструктор с инициализацией ID
     * @param id_ Идентификатор объекта
     */
    explicit RCObject(int id_) : id(id_), ref_count(0) {}

    /**
     * @brief Проверить, ссылается ли этот объект на другой объект
     * @param target_id ID объекта-цели
     * @return true, если ссылка существует
     */
    bool has_reference_to(int target_id) const
    {
        return std::find(references.begin(), references.end(), target_id) != references.end();
    }

    /**
     * @brief Добавить исходящую ссылку
     * @param target_id ID объекта-цели
     * @return true, если ссылка была добавлена (её не было ранее)
     */
    bool add_outgoing_ref(int target_id)
    {
        if (!has_reference_to(target_id))
        {
            references.push_back(target_id);
            return true;
        }
        return false;
    }

    /**
     * @brief Удалить исходящую ссылку
     * @param target_id ID объекта-цели
     * @return true, если ссылка была удалена (она существовала)
     */
    bool remove_outgoing_ref(int target_id)
    {
        auto it = std::find(references.begin(), references.end(), target_id);
        if (it != references.end())
        {
            references.erase(it);
            return true;
        }
        return false;
    }

    /**
     * @brief Получить количество исходящих ссылок
     * @return Размер массива ссылок
     */
    size_t get_outgoing_count() const
    {
        return references.size();
    }
};

#endif // RC_OBJECT_H
