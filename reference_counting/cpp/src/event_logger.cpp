#include "event_logger.h"
#include <sstream>
#include <iomanip>
#include <filesystem>

// Используем std::filesystem если доступна (C++17)
#ifdef __cplusplus
#if __cplusplus >= 201703L
namespace fs = std::filesystem;
#else
#include <sys/stat.h>
#endif
#endif

EventLogger::EventLogger(const std::string &filename)
{
    // Извлечь директорию из пути к файлу
    size_t last_slash = filename.find_last_of("/\\");
    if (last_slash != std::string::npos)
    {
        std::string directory = filename.substr(0, last_slash);

#ifdef __cplusplus
#if __cplusplus >= 201703L
        try
        {
            fs::create_directories(directory);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Warning: Could not create directory: " << e.what() << std::endl;
        }
#else
        // Fallback для старых компиляторов (Windows)
        std::string mkdir_cmd = "mkdir " + directory;
        system(mkdir_cmd.c_str());
#endif
#endif
    }

    // Открыть файл логов в режиме перезаписи
    file.open(filename, std::ios::out | std::ios::trunc);

    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open log file: " + filename);
    }
}

EventLogger::~EventLogger()
{
    if (file.is_open())
    {
        file.close();
    }
}

std::string EventLogger::get_timestamp() const
{
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::stringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void EventLogger::write(const std::string &json)
{
    if (file.is_open())
    {
        file << json << "\n";
        file.flush(); // Убедиться, что данные записаны немедленно
    }
}

void EventLogger::log_allocate(int obj_id)
{
    write("{\"event\":\"allocate\",\"object\":" + std::to_string(obj_id) + "}");
}

void EventLogger::log_add_ref(int from, int to, int new_ref_count)
{
    write("{\"event\":\"add_ref\",\"from\":" + std::to_string(from) +
          ",\"to\":" + std::to_string(to) +
          ",\"ref_count\":" + std::to_string(new_ref_count) + "}");
}

void EventLogger::log_remove_ref(int from, int to, int new_ref_count)
{
    write("{\"event\":\"remove_ref\",\"from\":" + std::to_string(from) +
          ",\"to\":" + std::to_string(to) +
          ",\"ref_count\":" + std::to_string(new_ref_count) + "}");
}

void EventLogger::log_delete(int obj_id)
{
    write("{\"event\":\"delete\",\"object\":" + std::to_string(obj_id) + "}");
}

void EventLogger::log_leak(int obj_id)
{
    write("{\"event\":\"leak\",\"object\":" + std::to_string(obj_id) + "}");
}

bool EventLogger::ensure_directory_exists(const std::string &path)
{
#ifdef __cplusplus
#if __cplusplus >= 201703L
    try
    {
        fs::create_directories(path);
        return true;
    }
    catch (...)
    {
        return false;
    }
#endif
#endif
    return true;
}
