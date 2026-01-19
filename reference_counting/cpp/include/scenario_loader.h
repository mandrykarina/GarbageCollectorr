#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct Operation
{
    std::string type;        // "allocate", "add_ref", "remove_ref"
    int object_id;           // ID объекта (для allocate, add_root, remove_root)
    int from_id;             // ID источника ссылки (для add_ref 1→2, from=1)
    int to_id;               // ID цели ссылки (для add_ref 1→2, to=2)
    int ref_count;           // (deprecated - для совместимости)
    std::string description; // описание операции
};

struct Scenario
{
    std::string name;
    std::string description;
    std::vector<Operation> operations;
};

class ScenarioLoader
{
public:
    // Загрузить один JSON сценарий
    static Scenario loadScenario(const std::string &jsonPath);

    // Загрузить все JSON сценарии из папки
    static std::vector<Scenario> loadAllScenarios(const std::string &scenariosDir);

private:
    static Operation parseOperation(const json &opJson);
};