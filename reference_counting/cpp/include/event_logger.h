#ifndef EVENT_LOGGER_H
#define EVENT_LOGGER_H

#include <fstream>
#include <string>
#include <iostream>
#include <ctime>

/**
 * @class EventLogger
 * @brief Логирует все события изменения памяти в JSON формате
 *
 * Создаёт файл логов при инициализации и записывает каждое событие
 * в формате JSON для последующего анализа и визуализации.
 */
class EventLogger
{
public:
    /**
     * @brief Конструктор с указанием пути к файлу логов
     * @param filename Путь к файлу для логов
     * @throw std::runtime_error если файл не удаётся открыть
     */
    explicit EventLogger(const std::string &filename);

    /**
     * @brief Деструктор, закрывает файл логов
     */
    ~EventLogger();

    /**
     * @brief Логировать выделение памяти
     * @param obj_id ID выделенного объекта
     */
    void log_allocate(int obj_id);

    /**
     * @brief Логировать добавление ссылки
     * @param from ID объекта-источника
     * @param to ID объекта-цели
     * @param new_ref_count Новое значение ref_count цели
     */
    void log_add_ref(int from, int to, int new_ref_count);

    /**
     * @brief Логировать удаление ссылки
     * @param from ID объекта-источника
     * @param to ID объекта-цели
     * @param new_ref_count Новое значение ref_count цели
     */
    void log_remove_ref(int from, int to, int new_ref_count);

    /**
     * @brief Логировать удаление объекта
     * @param obj_id ID удалённого объекта
     */
    void log_delete(int obj_id);

    /**
     * @brief Логировать утечку памяти (объект не удалён из-за цикла)
     * @param obj_id ID объекта с утечкой
     */
    void log_leak(int obj_id);

    /**
     * @brief Проверить, успешно ли открыт файл логов
     * @return true, если файл открыт
     */
    bool is_open() const { return file.is_open(); }

private:
    std::ofstream file;

    /**
     * @brief Получить текущее время в ISO формате
     * @return Строка с временем
     */
    std::string get_timestamp() const;

    /**
     * @brief Записать строку JSON в файл логов
     * @param json JSON строка для записи
     */
    void write(const std::string &json);

    /**
     * @brief Создать директорию логов если её не существует
     * @param path Путь к директории
     * @return true если директория создана или существует
     */
    static bool ensure_directory_exists(const std::string &path);
};

#endif // EVENT_LOGGER_H
