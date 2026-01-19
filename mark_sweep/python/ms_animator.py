#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Mark-and-Sweep GC Visualizer
Интерактивная визуализация алгоритма Mark-and-Sweep с анимацией.

Функции:
- Визуализировать граф объектов и ссылок
- Показать Mark фазу (пошаговая анимация DFS)
- Показать Sweep фазу (удаление непомеченных объектов)
- Интерактивные элементы управления (пауза, быстро вперёд, назад)
"""

import json
import sys
import os
import re
from pathlib import Path
from dataclasses import dataclass, field
from typing import Dict, List, Tuple, Optional, Set
from collections import deque
import math

try:
    import matplotlib.pyplot as plt
    import matplotlib.patches as patches
    from matplotlib.animation import FuncAnimation
    from matplotlib.widgets import Button, Slider
    import networkx as nx
except ImportError:
    print("ERROR: Required packages not installed!")
    print("Install with: pip install matplotlib networkx")
    sys.exit(1)


# ===========================
# СТРУКТУРЫ ДАННЫХ
# ===========================

@dataclass
class GCObject:
    """Представляет объект на heap'е"""
    id: int
    size: int
    is_marked: bool = False
    is_root: bool = False
    is_alive: bool = True
    allocation_step: int = -1
    collection_step: int = -1
    references_to: Set[int] = field(default_factory=set)
    references_from: Set[int] = field(default_factory=set)


@dataclass
class VisualizationState:
    """Состояние визуализации на определённом шаге"""
    step_number: int
    operation_type: str  # allocate, add_ref, remove_ref, make_root, collect, mark, sweep
    operation_description: str
    objects: Dict[int, GCObject]
    phase: str = "idle"  # idle, marking, sweeping
    marked_objects: Set[int] = field(default_factory=set)
    deleted_objects: Set[int] = field(default_factory=set)
    current_marking: Optional[int] = None  # Текущий объект при DFS


class MarkSweepVisualizer:
    """Главный класс визуализации Mark-and-Sweep"""

    def __init__(self, log_file_path: str):
        """
        Инициализировать визуализатор
        
        Args:
            log_file_path: Путь к файлу логирования от симулятора
        """
        self.log_file_path = log_file_path
        self.states: List[VisualizationState] = []
        self.current_state_index = 0
        self.is_playing = False
        self.animation_speed = 500  # Миллисекунды между кадрами

        # Цвета для визуализации
        self.colors = {
            'root': '#FF6B6B',          # Красный для root объектов
            'alive': '#4ECDC4',         # Бирюзовый для живых объектов
            'marked': '#95E1D3',        # Светлый бирюзовый для помеченных
            'dead': '#D3D3D3',          # Серый для удалённых
            'reference': '#2C3E50',     # Тёмный синий для ссылок
            'marked_reference': '#F39C12', # Оранжевый для ссылок помеченных объектов
        }

        self.fig = None
        self.ax_graph = None
        self.ax_info = None
        self.anim = None

    def parse_logs(self) -> bool:
        """
        Парсить логи из файла симулятора
        
        Returns:
            True если успешно, False если ошибка
        """
        if not os.path.exists(self.log_file_path):
            print(f"ERROR: Log file not found: {self.log_file_path}")
            return False

        try:
            with open(self.log_file_path, 'r', encoding='utf-8') as f:
                lines = f.readlines()
        except Exception as e:
            print(f"ERROR: Cannot read log file: {e}")
            return False

        # Начальное состояние
        current_objects: Dict[int, GCObject] = {}
        current_step = 0

        for line in lines:
            line = line.strip()
            if not line or line.startswith('==='):
                continue

            # Парсить строку формата "[Step N] ..."
            match = re.match(r'\[Step (\d+)\]\s+(.*)', line)
            if not match:
                continue

            step = int(match.group(1))
            operation_text = match.group(2)

            current_step = step

            # ===== ALLOCATE =====
            if operation_text.startswith('ALLOCATE:') and 'FAILED' not in operation_text:
                match = re.match(r'ALLOCATE:\s+obj_(\d+)\s+\(size=(\d+)', operation_text)
                if match:
                    obj_id = int(match.group(1))
                    size = int(match.group(2))
                    
                    obj = GCObject(id=obj_id, size=size, allocation_step=step)
                    current_objects[obj_id] = obj

                    state = VisualizationState(
                        step_number=step,
                        operation_type='allocate',
                        operation_description=f'Allocated object_{obj_id} ({size} bytes)',
                        objects={k: self._copy_object(v) for k, v in current_objects.items()},
                        phase='idle'
                    )
                    self.states.append(state)

            # ===== MAKE_ROOT =====
            elif 'MAKE_ROOT:' in operation_text and 'FAILED' not in operation_text:
                match = re.match(r'MAKE_ROOT:\s+obj_(\d+)', operation_text)
                if match:
                    obj_id = int(match.group(1))
                    if obj_id in current_objects:
                        current_objects[obj_id].is_root = True

                    state = VisualizationState(
                        step_number=step,
                        operation_type='make_root',
                        operation_description=f'Made object_{obj_id} a root',
                        objects={k: self._copy_object(v) for k, v in current_objects.items()},
                        phase='idle'
                    )
                    self.states.append(state)

            # ===== ADD_REF =====
            elif operation_text.startswith('ADD_REF:') and 'FAILED' not in operation_text and 'SKIPPED' not in operation_text:
                match = re.match(r'ADD_REF:\s+obj_(\d+)\s+->\s+obj_(\d+)', operation_text)
                if match:
                    from_id = int(match.group(1))
                    to_id = int(match.group(2))

                    if from_id in current_objects and to_id in current_objects:
                        current_objects[from_id].references_to.add(to_id)
                        current_objects[to_id].references_from.add(from_id)

                    state = VisualizationState(
                        step_number=step,
                        operation_type='add_ref',
                        operation_description=f'Added reference: object_{from_id} → object_{to_id}',
                        objects={k: self._copy_object(v) for k, v in current_objects.items()},
                        phase='idle'
                    )
                    self.states.append(state)

            # ===== REMOVE_REF =====
            elif operation_text.startswith('REM_REF:') and 'FAILED' not in operation_text:
                match = re.match(r'REM_REF:\s+obj_(\d+)\s+-X->\s+obj_(\d+)', operation_text)
                if match:
                    from_id = int(match.group(1))
                    to_id = int(match.group(2))

                    if from_id in current_objects and to_id in current_objects:
                        current_objects[from_id].references_to.discard(to_id)
                        current_objects[to_id].references_from.discard(from_id)

                    state = VisualizationState(
                        step_number=step,
                        operation_type='remove_ref',
                        operation_description=f'Removed reference: object_{from_id} -X-> object_{to_id}',
                        objects={k: self._copy_object(v) for k, v in current_objects.items()},
                        phase='idle'
                    )
                    self.states.append(state)

            # ===== MARK PHASE =====
            elif 'Mark obj_' in operation_text:
                match = re.match(r'Mark\s+obj_(\d+)', operation_text)
                if match:
                    obj_id = int(match.group(1))
                    if obj_id in current_objects:
                        current_objects[obj_id].is_marked = True

                    state = VisualizationState(
                        step_number=step,
                        operation_type='mark',
                        operation_description=f'Marked object_{obj_id} as reachable',
                        objects={k: self._copy_object(v) for k, v in current_objects.items()},
                        phase='marking',
                        marked_objects={k for k, v in current_objects.items() if v.is_marked},
                        current_marking=obj_id
                    )
                    self.states.append(state)

            # ===== DELETED =====
            elif operation_text.startswith('Deleted obj_'):
                match = re.match(r'Deleted obj_(\d+)', operation_text)
                if match:
                    obj_id = int(match.group(1))
                    if obj_id in current_objects:
                        current_objects[obj_id].is_alive = False
                        current_objects[obj_id].collection_step = step

                    state = VisualizationState(
                        step_number=step,
                        operation_type='sweep',
                        operation_description=f'Deleted object_{obj_id}',
                        objects={k: self._copy_object(v) for k, v in current_objects.items()},
                        phase='sweeping',
                        deleted_objects={obj_id}
                    )
                    self.states.append(state)

        print(f"✓ Parsed {len(self.states)} visualization states")
        return len(self.states) > 0

    def _copy_object(self, obj: GCObject) -> GCObject:
        """Создать копию объекта"""
        return GCObject(
            id=obj.id,
            size=obj.size,
            is_marked=obj.is_marked,
            is_root=obj.is_root,
            is_alive=obj.is_alive,
            allocation_step=obj.allocation_step,
            collection_step=obj.collection_step,
            references_to=obj.references_to.copy(),
            references_from=obj.references_from.copy()
        )

    def build_graph(self, objects: Dict[int, GCObject]) -> Tuple[nx.DiGraph, Dict]:
        """
        Построить граф NetworkX из объектов
        
        Returns:
            (граф, позиции узлов)
        """
        G = nx.DiGraph()

        # Добавить узлы
        for obj_id, obj in objects.items():
            if obj.is_alive:
                G.add_node(obj_id, obj=obj)

        # Добавить рёбра (ссылки)
        for obj_id, obj in objects.items():
            if obj.is_alive:
                for target_id in obj.references_to:
                    if target_id in objects and objects[target_id].is_alive:
                        G.add_edge(obj_id, target_id)

        # Расположить узлы
        if len(G.nodes()) > 0:
            # Иерархическое расположение: root вверху, остальные внизу
            root_objects = [oid for oid, obj in objects.items() 
                          if obj.is_root and obj.is_alive]
            other_objects = [oid for oid, obj in objects.items() 
                           if not obj.is_root and obj.is_alive]

            pos = {}

            # Root объекты в верхней части
            for i, obj_id in enumerate(root_objects):
                x = (i - len(root_objects) / 2) * 2
                pos[obj_id] = (x, 2)

            # Остальные объекты в нижней части
            num_other = len(other_objects)
            for i, obj_id in enumerate(other_objects):
                x = (i - num_other / 2) * 1.5
                y = 0
                pos[obj_id] = (x, y)

            # Если граф пуст, использовать пустую позицию
            if len(pos) == 0:
                pos = {}
        else:
            pos = {}

        return G, pos

    def create_figure(self):
        """Создать matplotlib фигуру с интерактивными элементами"""
        self.fig = plt.figure(figsize=(16, 10))
        self.fig.suptitle('Mark-and-Sweep GC Visualizer', fontsize=16, fontweight='bold')

        # Главный граф
        self.ax_graph = plt.subplot(1, 2, 1)
        self.ax_graph.set_title('Heap Graph Visualization')
        self.ax_graph.set_aspect('equal')

        # Информация справа
        self.ax_info = plt.subplot(1, 2, 2)
        self.ax_info.axis('off')

        # Кнопки управления внизу
        ax_play = plt.axes([0.35, 0.05, 0.1, 0.04])
        ax_pause = plt.axes([0.47, 0.05, 0.1, 0.04])
        ax_reset = plt.axes([0.59, 0.05, 0.1, 0.04])

        btn_play = Button(ax_play, 'Play')
        btn_pause = Button(ax_pause, 'Pause')
        btn_reset = Button(ax_reset, 'Reset')

        btn_play.on_clicked(self._on_play)
        btn_pause.on_clicked(self._on_pause)
        btn_reset.on_clicked(self._on_reset)

        # Слайдер для выбора шага
        ax_slider = plt.axes([0.2, 0.12, 0.6, 0.02])
        self.slider = Slider(
            ax_slider, 'Step', 0, max(len(self.states) - 1, 0),
            valinit=0, valstep=1
        )
        self.slider.on_changed(self._on_slider_change)

        plt.tight_layout(rect=[0, 0.15, 1, 0.96])

    def _on_play(self, event):
        """Нажата кнопка Play"""
        self.is_playing = True

    def _on_pause(self, event):
        """Нажата кнопка Pause"""
        self.is_playing = False

    def _on_reset(self, event):
        """Нажата кнопка Reset"""
        self.current_state_index = 0
        self.slider.set_val(0)
        self.is_playing = False

    def _on_slider_change(self, val):
        """Слайдер изменился"""
        self.current_state_index = int(val)

    def _update_frame(self, frame_num):
        """Обновить фрейм анимации"""
        if self.is_playing and self.current_state_index < len(self.states) - 1:
            self.current_state_index += 1
            self.slider.set_val(self.current_state_index)

        if self.current_state_index >= len(self.states):
            self.current_state_index = len(self.states) - 1

        self.ax_graph.clear()
        self.ax_info.clear()

        if len(self.states) == 0:
            self.ax_graph.text(0.5, 0.5, 'No data', ha='center', va='center')
            return

        state = self.states[self.current_state_index]

        # === Рисовать граф ===
        G, pos = self.build_graph(state.objects)

        # Цвета узлов
        node_colors = []
        for node_id in G.nodes():
            obj = state.objects[node_id]
            if obj.is_root:
                node_colors.append(self.colors['root'])
            elif obj.is_marked:
                node_colors.append(self.colors['marked'])
            elif node_id == state.current_marking:
                node_colors.append('#FFD700')  # Жёлтый для текущего
            else:
                node_colors.append(self.colors['alive'])

        # Рисовать рёбра
        edge_colors = []
        for edge in G.edges():
            from_id, to_id = edge
            from_obj = state.objects[from_id]
            if from_obj.is_marked:
                edge_colors.append(self.colors['marked_reference'])
            else:
                edge_colors.append(self.colors['reference'])

        # Рисовать граф
        nx.draw_networkx_nodes(
            G, pos, ax=self.ax_graph, node_color=node_colors,
            node_size=1500, alpha=0.9
        )
        nx.draw_networkx_labels(
            G, pos, ax=self.ax_graph, font_size=10, font_weight='bold'
        )
        nx.draw_networkx_edges(
            G, pos, ax=self.ax_graph, edge_color=edge_colors,
            arrows=True, arrowsize=20, arrowstyle='->', width=2, alpha=0.7
        )

        self.ax_graph.set_title(
            f'Heap Graph - Step {state.step_number}',
            fontsize=12, fontweight='bold'
        )
        self.ax_graph.axis('off')

        # === Информация справа ===
        info_text = f"""
OPERATION: {state.operation_type.upper()}
Description: {state.operation_description}

PHASE: {state.phase.upper()}

HEAP STATUS:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Total objects: {len([o for o in state.objects.values() if o.is_alive])}
Root objects: {len([o for o in state.objects.values() if o.is_root and o.is_alive])}
Marked objects: {len(state.marked_objects)}
Total memory: {sum(o.size for o in state.objects.values() if o.is_alive)} bytes

OBJECT DETAILS:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
"""

        for obj_id, obj in sorted(state.objects.items()):
            if obj.is_alive:
                status = []
                if obj.is_root:
                    status.append('ROOT')
                if obj.is_marked:
                    status.append('MARKED')
                status_str = ' | '.join(status) if status else 'UNMARKED'

                info_text += f"\nobject_{obj_id}: {obj.size} bytes [{status_str}]"
                if len(obj.references_to) > 0:
                    refs = ', '.join(f"obj_{r}" for r in sorted(obj.references_to))
                    info_text += f"\n  → {refs}"

        self.ax_info.text(0.05, 0.95, info_text, transform=self.ax_info.transAxes,
                         fontsize=9, verticalalignment='top', fontfamily='monospace',
                         bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.3))

    def animate(self):
        """Запустить анимацию"""
        self.create_figure()

        self.anim = FuncAnimation(
            self.fig, self._update_frame,
            interval=self.animation_speed,
            repeat=False,
            cache_frame_data=False
        )

        plt.show()


def main():
    """Entry point"""
    if len(sys.argv) < 2:
        print("Usage: python ms_animator.py <log_file>")
        print("Example: python ms_animator.py ms_trace.log")
        sys.exit(1)

    log_file = sys.argv[1]

    print("Starting Mark-and-Sweep Visualizer...")
    print(f"Loading logs from: {log_file}\n")

    visualizer = MarkSweepVisualizer(log_file)

    if not visualizer.parse_logs():
        print("Failed to parse logs!")
        sys.exit(1)

    print("\nStarting animation...")
    visualizer.animate()


if __name__ == '__main__':
    main()
