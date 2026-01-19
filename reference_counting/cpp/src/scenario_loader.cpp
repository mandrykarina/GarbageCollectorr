#include "scenario_loader.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

Scenario ScenarioLoader::loadScenario(const std::string &jsonPath)
{
    std::ifstream file(jsonPath);
    if (!file.is_open())
    {
        throw std::runtime_error("Cannot open scenario file: " + jsonPath);
    }

    json j;
    try
    {
        file >> j;
    }
    catch (const std::exception &e)
    {
        throw std::runtime_error("Invalid JSON in " + jsonPath + ": " + e.what());
    }

    Scenario scenario;
    scenario.name = j.value("name", "Unknown");
    scenario.description = j.value("description", "");

    // Парсим операции
    if (j.contains("operations") && j["operations"].is_array())
    {
        for (const auto &opJson : j["operations"])
        {
            scenario.operations.push_back(parseOperation(opJson));
        }
    }

    return scenario;
}

Operation ScenarioLoader::parseOperation(const json &opJson)
{
    Operation op;
    op.type = opJson.value("type", "");
    op.object_id = opJson.value("object_id", -1);
    op.from_id = opJson.value("from_id", -1);
    op.to_id = opJson.value("to_id", -1);
    op.ref_count = opJson.value("ref_count", 1);
    op.description = opJson.value("description", "");

    if (op.type.empty())
    {
        throw std::runtime_error("Operation type cannot be empty");
    }

    return op;
}

std::vector<Scenario> ScenarioLoader::loadAllScenarios(const std::string &scenariosDir)
{
    std::vector<Scenario> scenarios;

    try
    {
        for (const auto &entry : fs::directory_iterator(scenariosDir))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".json")
            {
                try
                {
                    std::cout << "Loading: " << entry.path().filename().string() << std::endl;
                    scenarios.push_back(loadScenario(entry.path().string()));
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Error loading " << entry.path().filename().string()
                              << ": " << e.what() << std::endl;
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        throw std::runtime_error("Cannot read scenarios directory: " + std::string(e.what()));
    }

    if (scenarios.empty())
    {
        std::cerr << "Warning: No scenarios found in " << scenariosDir << std::endl;
    }

    return scenarios;
}