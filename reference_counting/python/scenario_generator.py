import json
from pathlib import Path

# scenario_generator.py
BASE_DIR = Path(__file__).resolve().parent  # .../reference_counting/python
PARENT_DIR = BASE_DIR.parent                # .../reference_counting
SCENARIOS_DIR = PARENT_DIR / "scenarios"


class ScenarioGenerator:
    """
    Генератор сценариев для Reference Counting GC
    - Basic RC: все объекты удаляются (НЕТ УТЕЧКИ)
    - Cascade: удаление ROOT → каскадное удаление всей цепочки (НЕТ УТЕЧКИ)
    - Cycles: циклические ссылки → УТЕЧКА (ОЖИДАЕМО)
    """

    @staticmethod
    def generate_basic(num_objects: int = 3) -> dict:
        """
        Простой сценарий: каждый объект - это root.
        Все удаляются сразу когда удаляются roots.
        ✓ ОЖИДАНИЕ: 0 LEAKS
        """
        operations = []

        # 1️⃣ Allocate объекты
        for i in range(1, num_objects + 1):
            operations.append({"type": "allocate", "object_id": i})

        # 2️⃣ Добавить каждый объект как ROOT (rc++)
        # ROOT обозначается как from_id=0
        for i in range(1, num_objects + 1):
            operations.append({"type": "add_ref", "from_id": 0, "to_id": i})

        # 3️⃣ Удалить каждый ROOT (rc-- → 0 → delete)
        for i in range(1, num_objects + 1):
            operations.append({"type": "remove_ref", "from_id": 0, "to_id": i})

        return {
            "name": f"Basic RC (n={num_objects})",
            "description": "Each object is a root and freed immediately when root is removed",
            "operations": operations,
        }

    @staticmethod
    def generate_cascade(depth: int = 4) -> dict:
        """
        Каскадное удаление: цепочка объектов.
        ROOT → 1 → 2 → 3 → ... → N

        Когда удаляем ROOT → 1.rc=0 → delete 1 → delete ссылку 1→2 → 2.rc=0 → ...
        ✓ ОЖИДАНИЕ: 0 LEAKS
        """
        operations = []

        # 1️⃣ Allocate объекты
        for i in range(1, depth + 1):
            operations.append({"type": "allocate", "object_id": i})

        # 2️⃣ ROOT → первый объект (from_id=0)
        operations.append({"type": "add_ref", "from_id": 0, "to_id": 1})

        # 3️⃣ Цепочка: 1 → 2 → 3 → ... → depth
        for i in range(1, depth):
            operations.append({"type": "add_ref", "from_id": i, "to_id": i + 1})

        # 4️⃣ Удаляем ROOT → запускается cascade delete для всей цепочки
        operations.append({"type": "remove_ref", "from_id": 0, "to_id": 1})

        return {
            "name": f"Cascade Delete (depth={depth})",
            "description": "Removing ROOT triggers cascade delete for all linked objects",
            "operations": operations,
        }

    @staticmethod
    def generate_cycle(num_cycles: int = 2) -> dict:
        """
        Циклические ссылки: пары объектов, ссылающихся друг на друга.

        ДО удаления ROOT:
        ROOT → A (rc=1), A → B (rc=1)
        ROOT → B (rc=2), B → A (rc=2)

        ПОСЛЕ удаления ROOT:
        A → B (rc=1), B → A (rc=1)
        → УТЕЧКА! (оба остаются живыми)

        ✗ ОЖИДАНИЕ: num_cycles*2 LEAKS
        """
        operations = []

        start_id = 1
        total_objects = num_cycles * 2

        # 1️⃣ Allocate объекты
        for i in range(start_id, start_id + total_objects):
            operations.append({"type": "allocate", "object_id": i})

        # 2️⃣ ROOT ссылается на ВСЕ объекты (rc++ для каждого)
        for i in range(start_id, start_id + total_objects):
            operations.append({"type": "add_ref", "from_id": 0, "to_id": i})

        # 3️⃣ Создаём циклы: пара объектов ссылается друг на друга
        # A ↔ B означает: A → B и B → A
        for c in range(num_cycles):
            a = start_id + c * 2  # Первый объект цикла
            b = a + 1              # Второй объект цикла

            # A → B (rc[B]++)
            operations.append({"type": "add_ref", "from_id": a, "to_id": b})
            # B → A (rc[A]++)
            operations.append({"type": "add_ref", "from_id": b, "to_id": a})

        # 4️⃣ Удаляем ROOT → объекты остаются живыми (УТЕЧКА)
        for i in range(start_id, start_id + total_objects):
            operations.append({"type": "remove_ref", "from_id": 0, "to_id": i})

        return {
            "name": f"Circular Reference Leak (cycles={num_cycles})",
            "description": "Reference counting cannot collect cycles; intentional leak demonstration",
            "operations": operations,
        }

    @staticmethod
    def save_scenario(scenario: dict, filename: str):
        """Сохраняет JSON сценарий в папку scenarios"""
        SCENARIOS_DIR.mkdir(exist_ok=True)
        path = SCENARIOS_DIR / filename

        with open(path, "w", encoding="utf-8") as f:
            json.dump(scenario, f, indent=2)

        return str(path)

    @staticmethod
    def generate_all(basic_n: int = 3, cascade_depth: int = 4, cycle_count: int = 2):
        """Генерирует и сохраняет все три типа сценариев"""
        ScenarioGenerator.save_scenario(
            ScenarioGenerator.generate_basic(basic_n), "basic.json"
        )

        ScenarioGenerator.save_scenario(
            ScenarioGenerator.generate_cascade(cascade_depth), "cascade_delete.json"
        )

        ScenarioGenerator.save_scenario(
            ScenarioGenerator.generate_cycle(cycle_count), "cycle_leak.json"
        )


if __name__ == "__main__":
    ScenarioGenerator.generate_all()
    print("✅ Сценарии успешно сгенерированы в папке 'scenarios'")