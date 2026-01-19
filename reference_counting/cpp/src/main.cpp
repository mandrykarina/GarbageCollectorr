#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include "rc_heap.h"
#include "event_logger.h"
#include "scenario_loader.h"

namespace fs = std::filesystem;

int main(int argc, char *argv[])
{

    std::cout << "\n════════════════════════════════════════════\n";
    std::cout << "🗑️ Reference Counting GC Tester\n";
    std::cout << "════════════════════════════════════════════\n";

    // Директории ОТНОСИТЕЛЬНО exe (из build/)
    std::string scenariosDir = "..\\scenarios";
    std::string logsDir = "..\\logs";

    std::cout << "Scenarios dir: " << scenariosDir << "\n";
    std::cout << "Logs dir: " << logsDir << "\n";

    // Какой сценарий нужен
    std::string testType = (argc > 1) ? argv[1] : "basic";
    std::cout << "Test type: " << testType << "\n";
    std::cout << "════════════════════════════════════════════\n\n";

    std::vector<Scenario> scenarios;

    try
    {
        // Создаём logs директорию
        if (!fs::exists(logsDir))
        {
            fs::create_directories(logsDir);
            std::cout << "📁 Created logs directory\n";
        }

        // ЗАГРУЖАЕМ ТОЛЬКО НУЖНЫЙ СЦЕНАРИЙ
        if (testType == "basic")
        {
            try
            {
                std::string path = (fs::path(scenariosDir) / "basic.json").string();
                std::cout << "Loading: " << path << "\n";
                scenarios.push_back(ScenarioLoader::loadScenario(path));
                std::cout << "✅ Loaded\n\n";
            }
            catch (const std::exception &e)
            {
                std::cerr << "❌ Error: " << e.what() << "\n\n";
            }
        }
        else if (testType == "cascade")
        {
            try
            {
                std::string path = (fs::path(scenariosDir) / "cascade_delete.json").string();
                std::cout << "Loading: " << path << "\n";
                scenarios.push_back(ScenarioLoader::loadScenario(path));
                std::cout << "✅ Loaded\n\n";
            }
            catch (const std::exception &e)
            {
                std::cerr << "❌ Error: " << e.what() << "\n\n";
            }
        }
        else if (testType == "cycle")
        {
            try
            {
                std::string path = (fs::path(scenariosDir) / "cycle_leak.json").string();
                std::cout << "Loading: " << path << "\n";
                scenarios.push_back(ScenarioLoader::loadScenario(path));
                std::cout << "✅ Loaded\n\n";
            }
            catch (const std::exception &e)
            {
                std::cerr << "❌ Error: " << e.what() << "\n\n";
            }
        }
        else if (testType == "all")
        {
            try
            {
                std::string path = (fs::path(scenariosDir) / "basic.json").string();
                scenarios.push_back(ScenarioLoader::loadScenario(path));
                std::cout << "✅ basic.json loaded\n";
            }
            catch (const std::exception &e)
            {
                std::cerr << "⚠️ basic: " << e.what() << "\n";
            }

            try
            {
                std::string path = (fs::path(scenariosDir) / "cascade_delete.json").string();
                scenarios.push_back(ScenarioLoader::loadScenario(path));
                std::cout << "✅ cascade_delete.json loaded\n";
            }
            catch (const std::exception &e)
            {
                std::cerr << "⚠️ cascade: " << e.what() << "\n";
            }

            try
            {
                std::string path = (fs::path(scenariosDir) / "cycle_leak.json").string();
                scenarios.push_back(ScenarioLoader::loadScenario(path));
                std::cout << "✅ cycle_leak.json loaded\n";
            }
            catch (const std::exception &e)
            {
                std::cerr << "⚠️ cycle: " << e.what() << "\n";
            }
        }

        if (scenarios.empty())
        {
            std::cerr << "❌ No scenarios loaded!\n";
            return 1;
        }

        std::cout << "✅ Total scenarios: " << scenarios.size() << "\n\n";

        // ВЫПОЛНЯЕМ КАЖДЫЙ СЦЕНАРИЙ
        for (const auto &scenario : scenarios)
        {
            std::cout << "\n════════════════════════════════════════════\n";
            std::cout << "Running: " << scenario.name << "\n";
            std::cout << "Description: " << scenario.description << "\n";
            std::cout << "════════════════════════════════════════════\n\n";

            std::string logFile = (fs::path(logsDir) / "rc_events.log").string();
            std::cout << "Log file: " << logFile << "\n";

            // Очищаем старый лог
            if (fs::exists(logFile))
            {
                fs::remove(logFile);
                std::cout << "🗑️ Cleaned old log\n";
            }

            EventLogger logger(logFile);

            if (!logger.is_open())
            {
                std::cerr << "❌ Cannot open log: " << logFile << "\n";
                return 1;
            }

            std::cout << "✅ Log file opened\n\n";

            RCHeap heap(logger);

            // ВЫПОЛНЯЕМ ОПЕРАЦИИ
            for (size_t i = 0; i < scenario.operations.size(); i++)
            {
                const auto &op = scenario.operations[i];

                if (op.type == "allocate")
                {
                    std::cout << "[" << (i + 1) << "/" << scenario.operations.size()
                              << "] allocate(" << op.object_id << ")\n";
                    heap.allocate(op.object_id);
                }
                else if (op.type == "add_ref")
                {
                    if (op.from_id == 0)
                    {
                        std::cout << "[" << (i + 1) << "/" << scenario.operations.size()
                                  << "] add_root(" << op.to_id << ")\n";
                        heap.add_root(op.to_id);
                    }
                    else if (op.from_id > 0 && op.to_id > 0)
                    {
                        std::cout << "[" << (i + 1) << "/" << scenario.operations.size()
                                  << "] add_ref(" << op.from_id << " → " << op.to_id << ")\n";
                        heap.add_ref(op.from_id, op.to_id);
                    }
                }
                else if (op.type == "remove_ref")
                {
                    if (op.from_id == 0)
                    {
                        std::cout << "[" << (i + 1) << "/" << scenario.operations.size()
                                  << "] remove_root(" << op.to_id << ")\n";
                        heap.remove_root(op.to_id);
                    }
                    else if (op.from_id > 0 && op.to_id > 0)
                    {
                        std::cout << "[" << (i + 1) << "/" << scenario.operations.size()
                                  << "] remove_ref(" << op.from_id << " → " << op.to_id << ")\n";
                        heap.remove_ref(op.from_id, op.to_id);
                    }
                }
            }

            std::cout << "\n🔍 Detecting memory leaks...\n";
            heap.detect_and_log_leaks();

            std::cout << "📊 Final heap state:\n";
            heap.dump_state();

            std::cout << "\n✅ Scenario completed!\n";

            // Проверяем что лог создан
            if (fs::exists(logFile))
            {
                auto size = fs::file_size(logFile);
                std::cout << "✅ Log file created: " << size << " bytes\n";
            }
            else
            {
                std::cerr << "❌ Log file NOT created!\n";
                return 1;
            }
        }

        std::cout << "\n════════════════════════════════════════════\n";
        std::cout << "🎉 All tests completed!\n";
        std::cout << "✅ Logs ready at: " << logsDir << "/rc_events.log\n";
        std::cout << "════════════════════════════════════════════\n\n";

        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "❌ Fatal error: " << e.what() << "\n";
        return 1;
    }
}
