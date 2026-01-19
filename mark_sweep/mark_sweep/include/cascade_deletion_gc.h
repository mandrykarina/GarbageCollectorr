#ifndef CASCADE_DELETION_GC_H
#define CASCADE_DELETION_GC_H

#include "gc_interface.h"
#include "heap_object.h"
#include <unordered_map>
#include <vector>
#include <queue>
#include <memory>
#include <fstream>
#include <string>

class CascadeDeletionGC : public GCInterface {
private:
    std::unordered_map<int, HeapObject> heap;
    int next_object_id;
    size_t max_heap_size;
    size_t collection_threshold;
    std::vector<std::string> operation_logs;
    std::string last_operation;
    std::ofstream log_file;
    int collection_count;
    int total_objects_collected;
    size_t total_memory_freed;
    int total_collection_time;
    int current_step;
    std::queue<int> deletion_queue;
    std::unordered_map<int, bool> processed_in_cascade;
    
public:
    CascadeDeletionGC(
        size_t max_heap_size = 1024 * 1024,
        size_t collection_threshold = (1024 * 1024 * 80) / 100,
        const std::string& log_file_path = "cascade_trace.log"
    );
    
    ~CascadeDeletionGC() override;
    
    int allocate(size_t size) override;
    bool add_reference(int from_id, int to_id) override;
    bool remove_reference(int from_id, int to_id) override;
    size_t collect() override;
    std::string get_heap_info() const override;
    std::string get_gc_stats() const override;
    
    std::string get_last_operation_log() const override { return last_operation; }
    std::vector<std::string> get_all_logs() const override { return operation_logs; }
    void clear_logs() override { operation_logs.clear(); last_operation = ""; }
    
    size_t get_total_memory() const override;
    size_t get_free_memory() const override;
    
    void set_current_step(int step) override { current_step = step; }
    int get_current_step() const override { return current_step; }
    int get_alive_objects_count() const override {
        int count = 0;
        for (const auto& [id, obj] : heap) {
            if (obj.is_alive) count++;
        }
        return count;
    }
    
    void make_root(int object_id);
    void remove_root(int object_id);
    HeapObject* get_object(int id);
    const HeapObject* get_object(int id) const;
    bool object_exists(int id) const;
    
    const std::unordered_map<int, HeapObject>& get_all_objects() const { return heap; }
    
private:
    size_t cascade_delete(int object_id);
    bool should_be_deleted(int object_id) const;
    void log_operation(const std::string& operation);
    bool has_enough_memory(size_t size) const;
};

#endif
