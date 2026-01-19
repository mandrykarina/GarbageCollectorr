#include <iostream>
#include <iomanip>

#include "rc_heap.h"
#include "event_logger.h"

using std::cout;
using std::endl;

/* =======================
   Scenario A: Basic test
   ======================= */
void scenario_basic(RCHeap &heap)
{
    cout << "\n";
    cout << "╔════════════════════════════════════════════════════════════╗\n";
    cout << "║ Scenario A: Basic allocate / add_ref / remove_ref          ║\n";
    cout << "║ Демонстрирует простое выделение, добавление и удаление    ║\n";
    cout << "║ ссылок                                                     ║\n";
    cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    // Шаг 1: Выделить объект 1
    cout << " Allocate object 1\n";
    heap.allocate(1);
    heap.dump_state();

    // Шаг 2: Выделить объект 2
    cout << " Allocate object 2\n";
    heap.allocate(2);
    heap.dump_state();

    // Шаг 3: Объект 1 ссылается на объект 2
    cout << " Object 1 -> Object 2 (add reference)\n";
    heap.add_ref(1, 2);
    heap.dump_state();

    // Шаг 4: Удалить ссылку от 1 к 2
    cout << " Remove reference 1 -> 2\n";
    cout << " Expected: Object 2 should be deleted (ref_count becomes 0)\n";
    heap.remove_ref(1, 2);
    heap.dump_state();

    cout << "✓ Scenario A completed\n\n";
}

/* =======================
   Scenario B: Deep cascade
   ======================= */
void scenario_cascade(RCHeap &heap)
{
    cout << "\n";
    cout << "╔════════════════════════════════════════════════════════════╗\n";
    cout << "║ Scenario B: Deep cascade deletion                          ║\n";
    cout << "║ Демонстрирует каскадное удаление цепочки объектов         ║\n";
    cout << "║ 1 -> 2 -> 3 -> 4                                           ║\n";
    cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    // Выделить цепочку объектов
    cout << " Allocate objects 1, 2, 3, 4\n";
    heap.allocate(1);
    heap.allocate(2);
    heap.allocate(3);
    heap.allocate(4);
    heap.dump_state();

    // Создать цепочку ссылок
    cout << " Create reference chain: 1 -> 2 -> 3 -> 4\n";
    heap.add_ref(1, 2);
    heap.add_ref(2, 3);
    heap.add_ref(3, 4);
    heap.dump_state();

    // Удалить корневую ссылку - должна произойти каскадная очистка
    cout << " Remove reference 1 -> 2\n";
    cout << " Expected: All objects should be deleted (cascade)\n";
    cout << " Because: Object 2 has no refs, so cascade deletes 2\n";
    cout << " Object 3 only referenced by 2, so it gets deleted\n";
    cout << " Object 4 only referenced by 3, so it gets deleted\n";
    heap.remove_ref(1, 2);
    heap.dump_state();

    cout << "✓ Scenario B completed\n\n";
}

/* =======================
   Scenario C: RC leak (cycle) WITH ROOTS
   ======================= */
void scenario_cycle_leak(RCHeap &heap)
{
    cout << "\n";
    cout << "╔════════════════════════════════════════════════════════════╗\n";
    cout << "║ Scenario C: Reference Counting cycle leak WITH ROOTS       ║\n";
    cout << "║ Демонстрирует ОСНОВНУЮ ПРОБЛЕМУ RC:                        ║\n";
    cout << "║ циклические ссылки вызывают УТЕЧКУ ПАМЯТИ 💥              ║\n";
    cout << "║ root -> 1 <-> 2 (циклическая ссылка)                      ║\n";
    cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    // Выделить два объекта
    cout << " Allocate objects 1, 2\n";
    heap.allocate(1);
    heap.allocate(2);
    heap.dump_state();

    // Добавить корень на объект 1
    cout << " Add root -> 1 (now object 1 is reachable from root)\n";
    heap.add_root(1);
    heap.dump_state();

    // Создать циклическую ссылку
    cout << " Create cycle: 1 -> 2, 2 -> 1\n";
    heap.add_ref(1, 2);
    heap.add_ref(2, 1);
    heap.dump_state();

    // Удалить корень - это КРИТИЧНО для демонстрации утечки
    cout << " Remove root (root -> 1 deleted)\n";
    cout << " Expected: Object 1 ref_count drops to 1 (still referenced by 2)\n";
    heap.remove_root(1);
    heap.dump_state();

    cout << " ⚠ WARNING: Objects 1 and 2 still exist with ref_count=1!\n";
    cout << " This is a MEMORY LEAK! RC cannot delete them!\n";
    cout << " Reason: 1 references 2, 2 references 1 (cycle)\n";
    cout << " They have ref_count > 0, but are UNREACHABLE!\n";
    cout << " RC limitation: Cannot handle circular references 💥\n";
    cout << " Solution: Mark & Sweep GC can handle cycles ✓\n\n";

    cout << "✓ Scenario C completed (with memory leak)\n\n";
}

/* =======================
   Scenario D: Multiple references
   ======================= */
void scenario_multiple_refs(RCHeap &heap)
{
    cout << "\n";
    cout << "╔════════════════════════════════════════════════════════════╗\n";
    cout << "║ Scenario D: Multiple references                            ║\n";
    cout << "║ Несколько объектов ссылаются на один объект               ║\n";
    cout << "║ root -> 1, root -> 3, 2 -> 3 (ref_count = 3)              ║\n";
    cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    cout << " Allocate objects 1, 2, 3\n";
    heap.allocate(1);
    heap.allocate(2);
    heap.allocate(3);
    heap.dump_state();

    cout << " Add roots for objects 1, 3\n";
    heap.add_root(1);
    heap.add_root(3);
    heap.dump_state();

    cout << " Create reference: 2 -> 3\n";
    heap.add_ref(2, 3);
    heap.dump_state();

    cout << " Remove root from 3 (ref_count: 2 -> 1)\n";
    cout << " Expected: Object 3 survives (still has root and 2 references it)\n";
    heap.remove_root(3);
    heap.dump_state();

    cout << " Remove reference 2 -> 3\n";
    cout << " Expected: Object 3 survives (still has root)\n";
    heap.remove_ref(2, 3);
    heap.dump_state();

    cout << " Remove root from 3\n";
    cout << " Expected: Object 3 is deleted (no more references)\n";
    heap.remove_root(3);
    heap.dump_state();

    cout << "✓ Scenario D completed\n\n";
}

/* =======================
   MAIN
   ======================= */
int main()
{
    cout << "\n";
    cout << "╔════════════════════════════════════════════════════════════╗\n";
    cout << "║ Reference Counting GC Simulator (WITH ROOTS)               ║\n";
    cout << "║ Version 2.0.0 - Now with proper cycle leak demonstration  ║\n";
    cout << "╚════════════════════════════════════════════════════════════╝\n";

    try
    {
        // Инициализировать логгер
        EventLogger logger("logs/rc_events.log");
        if (!logger.is_open())
        {
            std::cerr << "Failed to open log file\n";
            return 1;
        }

        // Инициализировать кучу
        RCHeap heap(logger);

        // Запустить все сценарии
        scenario_basic(heap);
        scenario_cascade(heap);
        scenario_cycle_leak(heap);
        scenario_multiple_refs(heap);

        // Обнаружить и залогировать остающиеся утечки
        heap.detect_and_log_leaks();

        cout << "╔════════════════════════════════════════════════════════════╗\n";
        cout << "║ ✓ All scenarios finished successfully                       ║\n";
        cout << "║ 📝 Check logs/rc_events.log for detailed event log          ║\n";
        cout << "║ 💾 Final heap size: " << std::setw(38) << heap.get_heap_size()
             << " objects ║\n";
        cout << "║ 📌 Final roots count: " << std::setw(36) << heap.get_roots_count()
             << " ║\n";
        cout << "╚════════════════════════════════════════════════════════════╝\n";

        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
