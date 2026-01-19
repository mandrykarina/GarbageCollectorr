import json
import os
from pathlib import Path


class LogParser:
    """Парсер логов Reference Counting из JSON"""

    @staticmethod
    def parse_logs(log_file):
        """
        Парсит лог файл и возвращает список событий
        Args:
            log_file: Путь к файлу логов
        Returns:
            list: Список событий с деталями
        """
        events = []

        # Проверка существования файла
        if not os.path.exists(log_file):
            print(f"⚠️ Log file not found: {log_file}")
            return events

        try:
            with open(log_file, 'r', encoding='utf-8') as f:
                lines = f.readlines()

            # Если файл пуст
            if not lines:
                print(f"⚠️ Log file is empty: {log_file}")
                return events

            # Парсим каждую строку как JSON
            for i, line in enumerate(lines):
                line = line.strip()
                if not line:
                    continue

                try:
                    event_data = json.loads(line)
                    event = LogParser._convert_event(event_data, i + 1)
                    if event:
                        events.append(event)
                except json.JSONDecodeError as e:
                    print(f"⚠️ Error parsing line {i + 1}: {e}")
                    print(f" Content: {line[:100]}")
                    continue

            print(f"✅ Parsed {len(events)} events from log file")
            return events

        except Exception as e:
            print(f"❌ Error reading log file: {e}")
            return events

    @staticmethod
    def _convert_event(event_data, index):
        """
        Преобразует сырое событие логов в нужный формат
        Args:
            event_data: Словарь с данными события
            index: Номер события
        Returns:
            dict: Преобразованное событие
        """
        if not isinstance(event_data, dict):
            return None

        # Определяем тип события - может быть 'event' или 'type'
        event_type = event_data.get('event', event_data.get('type', 'unknown'))

        # Общая структура события
        event = {
            'index': index,
            'type': event_type,
            'timestamp': event_data.get('timestamp', ''),
            'description': '',
            'object_id': None,
            'from_id': None,
            'to_id': None,
            'ref_count': None,
            'status': 'info',
            'icon': '📌'
        }

        # Парсим в зависимости от типа
        if event_type == 'allocate':
            event['object_id'] = event_data.get('object')
            event['description'] = f"Allocate object #{event['object_id']}"
            event['icon'] = '🔵'
            event['status'] = 'success'

        elif event_type == 'add_ref':
            event['from_id'] = event_data.get('from', 0)
            event['to_id'] = event_data.get('to')
            event['ref_count'] = event_data.get('ref_count')

            if event['from_id'] == 0:
                event['description'] = f"Add root reference to object #{event['to_id']} (rc={event['ref_count']})"
                event['icon'] = '🟡'
            else:
                event['description'] = f"Object #{event['from_id']} → #{event['to_id']} (rc={event['ref_count']})"
                event['icon'] = '➡️'

            event['status'] = 'info'

        elif event_type == 'remove_ref':
            event['from_id'] = event_data.get('from', 0)
            event['to_id'] = event_data.get('to')
            event['ref_count'] = event_data.get('ref_count')

            if event['from_id'] == 0:
                event['description'] = f"Remove root reference from object #{event['to_id']} (rc={event['ref_count']})"
                event['icon'] = '⬅️'
            else:
                event['description'] = f"Object #{event['from_id']} ← #{event['to_id']} (rc={event['ref_count']})"
                event['icon'] = '⬅️'

            event['status'] = 'warning'

        elif event_type == 'delete':
            event['object_id'] = event_data.get('object')
            event['description'] = f"Delete object #{event['object_id']} (freed)"
            event['icon'] = '⚫'
            event['status'] = 'success'

        elif event_type == 'leak':
            event['object_id'] = event_data.get('object')
            event['description'] = f"⚠️ MEMORY LEAK: Object #{event['object_id']}"
            event['icon'] = '🔴'
            event['status'] = 'error'

        else:
            event['description'] = json.dumps(event_data)

        return event

    @staticmethod
    def get_summary(events):
        """
        Вычисляет статистику на основе событий
        Args:
            events: Список событий
        Returns:
            dict: Статистика
        """
        summary = {
            'total_events': len(events),
            'allocated': 0,
            'deleted': 0,
            'leaks': 0,
            'add_refs': 0,
            'remove_refs': 0,
            'root_refs': 0,
            'objects_alive': set(),
            'objects_deleted': set(),
            'status': '🔵 Running'
        }

        for event in events:
            event_type = event.get('type', '')

            if event_type == 'allocate':
                summary['allocated'] += 1
                obj_id = event.get('object_id')
                if obj_id:
                    summary['objects_alive'].add(obj_id)
                    summary['objects_deleted'].discard(obj_id)

            elif event_type == 'delete':
                summary['deleted'] += 1
                obj_id = event.get('object_id')
                if obj_id:
                    summary['objects_alive'].discard(obj_id)
                    summary['objects_deleted'].add(obj_id)

            elif event_type == 'leak':
                summary['leaks'] += 1

            elif event_type == 'add_ref':
                if event.get('from_id') == 0:
                    summary['root_refs'] += 1
                summary['add_refs'] += 1

            elif event_type == 'remove_ref':
                summary['remove_refs'] += 1

        # Вычисляем статус
        if summary['leaks'] > 0:
            summary['status'] = '🔴 MEMORY LEAK DETECTED!'
        elif summary['objects_alive']:
            summary['status'] = f"⚠️ {len(summary['objects_alive'])} objects still alive"
        else:
            summary['status'] = '✅ ALL FREED'

        # Конвертим sets в списки для JSON сериализации
        summary['objects_alive'] = list(summary['objects_alive'])
        summary['objects_deleted'] = list(summary['objects_deleted'])

        return summary

    @staticmethod
    def get_object_graph(events):
        """
        Строит граф объектов на основе событий
        Args:
            events: Список событий
        Returns:
            dict: Данные для визуализации (nodes, edges)
        """
        nodes = {}
        edges = []
        object_status = {}  # Отслеживаем статус объектов

        for event in events:
            event_type = event.get('type', '')

            if event_type == 'allocate':
                obj_id = event.get('object_id')
                if obj_id:
                    nodes[obj_id] = {
                        'id': f'obj{obj_id}',
                        'label': f'Object {obj_id}',
                        'status': 'alive',
                        'rc': 0,
                        'is_root': False
                    }
                    object_status[obj_id] = 'alive'

            elif event_type == 'delete':
                obj_id = event.get('object_id')
                if obj_id and obj_id in nodes:
                    nodes[obj_id]['status'] = 'deleted'
                    object_status[obj_id] = 'deleted'

            elif event_type == 'leak':
                obj_id = event.get('object_id')
                if obj_id and obj_id in nodes:
                    nodes[obj_id]['status'] = 'leak'
                    object_status[obj_id] = 'leak'

            elif event_type == 'add_ref':
                from_id = event.get('from_id')
                to_id = event.get('to_id')
                ref_count = event.get('ref_count', 0)

                if from_id == 0:  # Root reference
                    if to_id in nodes:
                        nodes[to_id]['is_root'] = True
                        nodes[to_id]['rc'] = ref_count
                else:  # Object reference
                    if to_id in nodes:
                        nodes[to_id]['rc'] = ref_count

                    if from_id and to_id:
                        edge_id = f'{from_id}-{to_id}'
                        if not any(e['id'] == edge_id for e in edges):
                            edges.append({
                                'id': edge_id,
                                'source': f'obj{from_id}',
                                'target': f'obj{to_id}',
                                'label': f'→ {ref_count}'
                            })

        # Конвертим в нужный формат
        graph_nodes = list(nodes.values())

        return {
            'nodes': graph_nodes,
            'edges': edges,
            'object_count': len(graph_nodes),
            'edge_count': len(edges)
        }