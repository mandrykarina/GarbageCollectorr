#include "cascade_deletion_gc.h"
#include <chrono>
#include <sstream>
#include <iostream>

CascadeDeletionGC::CascadeDeletionGC(size_t max_heap_size, size_t collection_threshold, const std::string& log_file_path)
    : next_object_id(0), max_heap_size(max_heap_size), collection_threshold(collection_threshold),
      collection_count(0), total_objects_collected(0), total_memory_freed(0), total_collection_time(0), current_step(0)
{
    log_file.open(log_file_path, std::ios::app);
    if (log_file.is_open()) log_file << "\n=== Cascade Deletion GC Session Started ===" << std::endl;
    log_operation("GC initialized with max_heap=" + std::to_string(max_heap_size));
}

CascadeDeletionGC::~CascadeDeletionGC() {
    if (log_file.is_open()) {
        log_file << "=== Cascade Deletion GC Session Ended ===" << std::endl;
        log_file.close();
    }
}

int CascadeDeletionGC::allocate(size_t size) {
    if (size == 0 || size > max_heap_size) {
        log_operation("ALLOCATE FAILED: invalid size " + std::to_string(size));
        return -1;
    }
    
    if (!has_enough_memory(size)) {
        log_operation("ALLOCATE: memory low, triggering collection...");
        collect();
    }
    
    if (!has_enough_memory(size)) {
        log_operation("ALLOCATE FAILED: out of memory");
        return -1;
    }
    
    int object_id = next_object_id++;
    HeapObject obj(object_id, size, false);
    obj.allocation_step = current_step;
    heap[object_id] = obj;
    
    std::ostringstream oss;
    oss << "ALLOCATE: obj_" << object_id << " (size=" << size << " bytes)";
    log_operation(oss.str());
    
    return object_id;
}

bool CascadeDeletionGC::add_reference(int from_id, int to_id) {
    if (!object_exists(from_id)) {
        std::ostringstream oss;
        oss << "ADD_REF FAILED: source object_" << from_id << " not found";
        log_operation(oss.str());
        return false;
    }
    
    if (!object_exists(to_id)) {
        std::ostringstream oss;
        oss << "ADD_REF FAILED: target object_" << to_id << " not found";
        log_operation(oss.str());
        return false;
    }
    
    HeapObject& source = heap[from_id];
    HeapObject& target = heap[to_id];
    
    if (source.outgoing_references.count(to_id) > 0) {
        std::ostringstream oss;
        oss << "ADD_REF SKIPPED: edge obj_" << from_id << " -> obj_" << to_id << " already exists";
        log_operation(oss.str());
        return true;
    }
    
    source.add_reference_to(to_id);
    target.add_reference_from(from_id);
    
    std::ostringstream oss;
    oss << "ADD_REF: obj_" << from_id << " -> obj_" << to_id;
    log_operation(oss.str());
    
    return true;
}

bool CascadeDeletionGC::remove_reference(int from_id, int to_id) {
    if (!object_exists(from_id)) {
        std::ostringstream oss;
        oss << "REM_REF FAILED: source object_" << from_id << " not found";
        log_operation(oss.str());
        return false;
    }
    
    if (!object_exists(to_id)) {
        std::ostringstream oss;
        oss << "REM_REF FAILED: target object_" << to_id << " not found";
        log_operation(oss.str());
        return false;
    }
    
    HeapObject& source = heap[from_id];
    HeapObject& target = heap[to_id];
    
    if (source.outgoing_references.count(to_id) == 0) {
        std::ostringstream oss;
        oss << "REM_REF FAILED: edge obj_" << from_id << " -> obj_" << to_id << " doesn't exist";
        log_operation(oss.str());
        return false;
    }
    
    source.remove_reference_to(to_id);
    target.remove_reference_from(from_id);
    
    std::ostringstream oss;
    oss << "REM_REF: obj_" << from_id << " -X-> obj_" << to_id;
    log_operation(oss.str());
    
    // ТРИГГЕР КАСКАДА!
    if (should_be_deleted(to_id)) {
        log_operation(" [CASCADE] Triggering cascade deletion chain...");
        cascade_delete(to_id);
    }
    
    return true;
}

size_t CascadeDeletionGC::collect() {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::ostringstream oss;
    oss << "\n[COLLECTION #" << (collection_count + 1) << "] Starting Cascade Deletion...";
    log_operation(oss.str());
    
    log_operation(" Phase 1: SCAN - finding orphan objects");
    
    std::vector<int> orphans;
    for (auto& [id, obj] : heap) {
        if (obj.is_alive && !obj.is_root && obj.get_incoming_reference_count() == 0) {
            orphans.push_back(id);
        }
    }
    
    std::ostringstream oss_orphans;
    oss_orphans << " Found " << orphans.size() << " orphans: [";
    for (size_t i = 0; i < orphans.size(); i++) {
        if (i > 0) oss_orphans << ", ";
        oss_orphans << "obj_" << orphans[i];
    }
    oss_orphans << "]";
    log_operation(oss_orphans.str());
    
    log_operation(" Phase 2: CASCADE - deleting cascade chains");
    
    size_t total_freed = 0;
    for (int orphan_id : orphans) {
        if (object_exists(orphan_id)) {
            total_freed += cascade_delete(orphan_id);
        }
    }
    
    collection_count++;
    total_memory_freed += total_freed;
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time
    ).count();
    total_collection_time += duration;
    
    std::ostringstream oss_end;
    oss_end << "[COLLECTION #" << collection_count << "] Complete. "
            << "Freed: " << total_freed << " bytes, "
            << "Live objects: " << get_alive_objects_count();
    log_operation(oss_end.str());
    
    return total_freed;
}

std::string CascadeDeletionGC::get_heap_info() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << " \"total_objects\": " << heap.size() << ",\n";
    oss << " \"alive_objects\": " << get_alive_objects_count() << ",\n";
    oss << " \"total_memory\": " << get_total_memory() << ",\n";
    oss << " \"free_memory\": " << get_free_memory() << ",\n";
    oss << " \"objects\": [\n";
    
    bool first = true;
    for (const auto& [id, obj] : heap) {
        if (!first) oss << ",\n";
        first = false;
        
        oss << " {\n";
        oss << "  \"id\": " << obj.id << ",\n";
        oss << "  \"size\": " << obj.size << ",\n";
        oss << "  \"is_root\": " << (obj.is_root ? "true" : "false") << ",\n";
        oss << "  \"alive\": " << (obj.is_alive ? "true" : "false") << "\n";
        oss << " }";
    }
    
    oss << "\n ]\n}\n";
    return oss.str();
}

std::string CascadeDeletionGC::get_gc_stats() const {
    std::ostringstream oss;
    oss << "=== Cascade Deletion GC Statistics ===\n";
    oss << "Collections run: " << collection_count << "\n";
    oss << "Total objects collected: " << total_objects_collected << "\n";
    oss << "Total memory freed: " << total_memory_freed << " bytes\n";
    oss << "Total collection time: " << total_collection_time << " µs\n";
    
    if (collection_count > 0) {
        oss << "Average collection time: "
            << (total_collection_time / collection_count) << " µs\n";
        oss << "Average objects per collection: "
            << (total_objects_collected / collection_count) << "\n";
    }
    
    size_t total_mem = get_total_memory();
    int percentage = (max_heap_size > 0) ? ((total_mem * 100) / max_heap_size) : 0;
    oss << "Heap usage: " << total_mem << " / " << max_heap_size
        << " bytes (" << percentage << "%)\n";
    
    return oss.str();
}

size_t CascadeDeletionGC::get_total_memory() const {
    size_t total = 0;
    for (const auto& [id, obj] : heap) {
        if (obj.is_alive) {
            total += obj.size;
        }
    }
    return total;
}

size_t CascadeDeletionGC::get_free_memory() const {
    return max_heap_size - get_total_memory();
}

void CascadeDeletionGC::make_root(int object_id) {
    if (object_exists(object_id)) {
        heap[object_id].is_root = true;
        std::ostringstream oss;
        oss << "MAKE_ROOT: obj_" << object_id << " is now a root object";
        log_operation(oss.str());
    }
}

void CascadeDeletionGC::remove_root(int object_id) {
    if (object_exists(object_id)) {
        heap[object_id].is_root = false;
        std::ostringstream oss;
        oss << "REMOVE_ROOT: obj_" << object_id << " is no longer a root";
        log_operation(oss.str());
    }
}

HeapObject* CascadeDeletionGC::get_object(int id) {
    auto it = heap.find(id);
    if (it != heap.end()) {
        return &it->second;
    }
    return nullptr;
}

const HeapObject* CascadeDeletionGC::get_object(int id) const {
    auto it = heap.find(id);
    if (it != heap.end()) {
        return &it->second;
    }
    return nullptr;
}

bool CascadeDeletionGC::object_exists(int id) const {
    return heap.find(id) != heap.end() && heap.at(id).is_alive;
}

size_t CascadeDeletionGC::cascade_delete(int object_id) {
    if (!object_exists(object_id)) {
        return 0;
    }
    
    size_t freed_memory = 0;
    
    deletion_queue = std::queue<int>();
    processed_in_cascade.clear();
    
    deletion_queue.push(object_id);
    
    while (!deletion_queue.empty()) {
        int current_id = deletion_queue.front();
        deletion_queue.pop();
        
        if (processed_in_cascade[current_id]) {
            continue;
        }
        processed_in_cascade[current_id] = true;
        
        if (!object_exists(current_id)) {
            continue;
        }
        
        HeapObject& obj = heap[current_id];
        
        if (obj.is_root) {
            log_operation(" [CASCADE] Stopping at root object obj_" + std::to_string(current_id));
            continue;
        }
        
        std::vector<int> targets_to_check(obj.outgoing_references.begin(),
                                          obj.outgoing_references.end());
        
        for (int source_id : obj.incoming_references) {
            if (object_exists(source_id)) {
                heap[source_id].remove_reference_to(current_id);
            }
        }
        
        for (int target_id : obj.outgoing_references) {
            if (object_exists(target_id)) {
                heap[target_id].remove_reference_from(current_id);
                
                if (should_be_deleted(target_id)) {
                    deletion_queue.push(target_id);
                }
            }
        }
        
        obj.is_alive = false;
        obj.collection_step = current_step;
        freed_memory += obj.size;
        total_objects_collected++;
        
        std::ostringstream oss;
        oss << " Cascade deleted obj_" << current_id << " (" << obj.size << " bytes)";
        log_operation(oss.str());
    }
    
    return freed_memory;
}

bool CascadeDeletionGC::should_be_deleted(int object_id) const {
    if (!object_exists(object_id)) {
        return false;
    }
    
    const HeapObject& obj = heap.at(object_id);
    
    if (obj.is_root) {
        return false;
    }
    
    return obj.get_incoming_reference_count() == 0;
}

void CascadeDeletionGC::log_operation(const std::string& operation) {
    operation_logs.push_back(operation);
    last_operation = operation;
    
    if (log_file.is_open()) {
        log_file << "[Step " << current_step << "] " << operation << std::endl;
        log_file.flush();
    }
    
    std::cout << "[Step " << current_step << "] " << operation << std::endl;
}

bool CascadeDeletionGC::has_enough_memory(size_t size) const {
    return get_free_memory() >= size;
}
