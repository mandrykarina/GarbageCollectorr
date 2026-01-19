#include "mark_sweep_gc.h"
#include "cascade_deletion_gc.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>

struct Operation {
    std::string type;
    int param1 = 0;
    int param2 = 0;
    std::string collection_type = "mark_sweep";
};

std::vector<Operation> parse_json_scenario(const std::string& filename) {
    std::vector<Operation> operations;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Cannot open: " << filename << std::endl;
        return operations;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();
    
    // Читаем тип сборки
    std::string collection_type = "mark_sweep";
    size_t type_pos = content.find("\"collection_type\":");
    if (type_pos != std::string::npos) {
        size_t start = content.find("\"", type_pos + 18);
        size_t end = content.find("\"", start + 1);
        if (start != std::string::npos && end != std::string::npos) {
            collection_type = content.substr(start + 1, end - start - 1);
        }
    }
    
    // Парсим операции
    size_t pos = 0;
    while (pos < content.length()) {
        pos = content.find('{', pos);
        if (pos == std::string::npos) break;
        
        size_t end = content.find('}', pos);
        if (end == std::string::npos) break;
        
        std::string obj = content.substr(pos, end - pos + 1);
        pos = end + 1;
        
        if (obj.find("\"op\"") == std::string::npos) continue;
        
        Operation op;
        op.collection_type = collection_type;
        
        if (obj.find("\"op\": \"allocate\"") != std::string::npos ||
            obj.find("\"op\":\"allocate\"") != std::string::npos) {
            op.type = "allocate";
            size_t size_pos = obj.find("\"size\"");
            if (size_pos != std::string::npos) {
                sscanf(obj.c_str() + size_pos, "\"size\": %d", &op.param1);
            }
            if (op.param1 > 0) operations.push_back(op);
        }
        else if (obj.find("\"op\": \"make_root\"") != std::string::npos ||
                 obj.find("\"op\":\"make_root\"") != std::string::npos) {
            op.type = "make_root";
            size_t id_pos = obj.find("\"id\"");
            if (id_pos != std::string::npos) {
                sscanf(obj.c_str() + id_pos, "\"id\": %d", &op.param1);
            }
            operations.push_back(op);
        }
        else if (obj.find("\"op\": \"add_ref\"") != std::string::npos ||
                 obj.find("\"op\":\"add_ref\"") != std::string::npos) {
            op.type = "add_ref";
            size_t from_pos = obj.find("\"from\"");
            size_t to_pos = obj.find("\"to\"");
            if (from_pos != std::string::npos) {
                sscanf(obj.c_str() + from_pos, "\"from\": %d", &op.param1);
            }
            if (to_pos != std::string::npos) {
                sscanf(obj.c_str() + to_pos, "\"to\": %d", &op.param2);
            }
            operations.push_back(op);
        }
        else if (obj.find("\"op\": \"remove_ref\"") != std::string::npos ||
                 obj.find("\"op\":\"remove_ref\"") != std::string::npos) {
            op.type = "remove_ref";
            size_t from_pos = obj.find("\"from\"");
            size_t to_pos = obj.find("\"to\"");
            if (from_pos != std::string::npos) {
                sscanf(obj.c_str() + from_pos, "\"from\": %d", &op.param1);
            }
            if (to_pos != std::string::npos) {
                sscanf(obj.c_str() + to_pos, "\"to\": %d", &op.param2);
            }
            operations.push_back(op);
        }
        else if (obj.find("\"op\": \"collect\"") != std::string::npos ||
                 obj.find("\"op\":\"collect\"") != std::string::npos) {
            op.type = "collect";
            operations.push_back(op);
        }
    }
    
    return operations;
}

void run_simulation(const std::string& scenario_file) {
    std::cout << "\n========================================" << std::endl;
    std::cout << " Garbage Collector Simulator" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    std::vector<Operation> operations = parse_json_scenario(scenario_file);
    std::cout << "Parsed " << operations.size() << " operations\n" << std::endl;
    
    if (operations.empty()) {
        std::cerr << "ERROR: No operations!" << std::endl;
        return;
    }
    
    // Определяем тип GC
    std::string gc_type = operations[0].collection_type;
    
    std::unique_ptr<GCInterface> gc;
    
    if (gc_type == "cascade") {
        std::cout << "Using: Cascade Deletion GC\n" << std::endl;
        gc = std::make_unique<CascadeDeletionGC>();
    } else {
        std::cout << "Using: Mark-and-Sweep GC\n" << std::endl;
        gc = std::make_unique<MarkSweepGC>();
    }
    
    // Выполняем операции
    for (size_t step = 0; step < operations.size(); step++) {
        const Operation& op = operations[step];
        gc->set_current_step(step);
        
        std::cout << "\n--- Step " << step << " ---" << std::endl;
        
        if (op.type == "allocate") {
            int id = gc->allocate(op.param1);
            std::cout << "ALLOCATE " << op.param1 << " bytes -> object_" << id << std::endl;
        }
        else if (op.type == "make_root") {
            if (auto* ms_gc = dynamic_cast<MarkSweepGC*>(gc.get())) {
                ms_gc->make_root(op.param1);
            } else if (auto* c_gc = dynamic_cast<CascadeDeletionGC*>(gc.get())) {
                c_gc->make_root(op.param1);
            }
            std::cout << "MAKE_ROOT object_" << op.param1 << std::endl;
        }
        else if (op.type == "add_ref") {
            gc->add_reference(op.param1, op.param2);
            std::cout << "ADD_REF object_" << op.param1 << " -> object_" << op.param2 << std::endl;
        }
        else if (op.type == "remove_ref") {
            gc->remove_reference(op.param1, op.param2);
            std::cout << "REMOVE_REF object_" << op.param1 << " -X-> object_" << op.param2 << std::endl;
        }
        else if (op.type == "collect") {
            size_t freed = gc->collect();
            std::cout << "COLLECT -> freed " << freed << " bytes" << std::endl;
        }
        
        std::cout << "Heap: " << gc->get_alive_objects_count() << " objects, "
                  << gc->get_total_memory() << " bytes" << std::endl;
    }
    
    std::cout << "\n========================================" << std::endl;
    std::cout << " Simulation Complete" << std::endl;
    std::cout << "========================================\n" << std::endl;
    std::cout << gc->get_gc_stats() << std::endl;
}

void show_menu() {
    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << " Garbage Collector" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    std::cout << "[1] Basic (Mark-Sweep)\n[2] Cyclic (Mark-Sweep)\n[3] Cascade Deletion\n[4] Performance\n[5] All\n[6] Exit\n" << std::endl;
}

bool file_exists(const std::string& f) {
    std::ifstream file(f);
    return file.good();
}

std::string find_scenario(const std::string& name) {
    std::vector<std::string> paths = {
        name,
        "../scenarios/" + name,
        "scenarios/" + name,
        "../../scenarios/" + name
    };
    
    for (const auto& p : paths) {
        if (file_exists(p)) return p;
    }
    
    return "../scenarios/" + name;
}

void run_scenario(int choice) {
    std::string file;
    
    if (choice == 1) {
        file = find_scenario("scenario_basic.json");
    }
    else if (choice == 2) {
        file = find_scenario("scenario_cycle.json");
    }
    else if (choice == 3) {
        file = find_scenario("scenario_cascade.json");
    }
    else if (choice == 4) {
        file = find_scenario("scenario_performance.json");
    }
    else if (choice == 5) {
        // Run all
        std::vector<std::string> scenarios = {
            "scenario_basic.json",
            "scenario_cycle.json",
            "scenario_cascade.json",
            "scenario_performance.json"
        };
        
        for (const auto& scenario : scenarios) {
            std::string path = find_scenario(scenario);
            std::cout << "\n\n" << std::string(70, '#') << std::endl;
            std::cout << "Running: " << scenario << std::endl;
            std::cout << std::string(70, '#') << std::endl;
            run_simulation(path);
            
            std::cout << "Press ENTER to continue..." << std::endl;
            std::cin.ignore();
            std::cin.get();
        }
        return;
    }
    else {
        return;
    }
    
    run_simulation(file);
}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        run_simulation(argv[1]);
        return 0;
    }
    
    while (true) {
        show_menu();
        std::cout << "Choice: ";
        int choice;
        std::cin >> choice;
        
        if (choice == 6) return 0;
        if (choice >= 1 && choice <= 5) {
            run_scenario(choice);
        }
        
        std::cout << "Press ENTER..." << std::endl;
        std::cin.ignore();
        std::cin.get();
    }
    
    return 0;
}
