from flask import Flask, render_template, request, jsonify
from flask_cors import CORS
import json
import os
import subprocess
import sys
from scenario_generator import ScenarioGenerator
from log_parser import LogParser

app = Flask(__name__, template_folder='templates', static_folder='static')
CORS(app)

# Пути - ВСЁ ОТНОСИТЕЛЬНО python/
BASE_DIR = os.path.dirname(os.path.abspath(__file__))  # python/
PARENT_DIR = os.path.dirname(BASE_DIR)  # reference_counting/
CPP_DIR = os.path.join(PARENT_DIR, 'cpp')
SCENARIOS_DIR = os.path.join(PARENT_DIR, 'scenarios')
LOGS_DIR = os.path.join(PARENT_DIR, 'logs')
LOGS_FILE = os.path.join(LOGS_DIR, 'rc_events.log')
RC_TESTER = os.path.join(CPP_DIR, 'build', 'rc_tester.exe')

# Debug
print("\n" + "="*70)
print("🗑️ Reference Counting GC Visualizer")
print("="*70)
print(f"BASE_DIR (python): {BASE_DIR}")
print(f"PARENT_DIR (reference_counting): {PARENT_DIR}")
print(f"CPP_DIR: {CPP_DIR}")
print(f"SCENARIOS_DIR: {SCENARIOS_DIR}")
print(f"LOGS_DIR: {LOGS_DIR}")
print(f"RC_TESTER: {RC_TESTER}")
print(f"RC_TESTER exists: {os.path.exists(RC_TESTER)}")
print("="*70 + "\n")

# Создаём директории
os.makedirs(SCENARIOS_DIR, exist_ok=True)
os.makedirs(LOGS_DIR, exist_ok=True)

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/api/run-test', methods=['POST'])
def run_test():
    """Запуск теста"""
    data = request.json
    scenario_type = data.get('type', 'basic')
    params = data.get('params', {})

    try:
        print(f"\n▶️ Running test: type={scenario_type}, params={params}")

        # 1️⃣ ОЧИСТИТЬ СТАРЫЙ ЛОГ
        if os.path.exists(LOGS_FILE):
            os.remove(LOGS_FILE)
            print(f"✅ Cleaned old log file")

        # 2️⃣ ГЕНЕРИРОВАТЬ ТОЛЬКО ВЫБРАННЫЙ СЦЕНАРИЙ
        print(f"📝 Generating ONLY {scenario_type} scenario with params: {params}")

        if scenario_type == 'basic':
            num_objects = params.get('num_objects', 2)
            scenario = ScenarioGenerator.generate_basic(num_objects)
            scenario_file = os.path.join(SCENARIOS_DIR, 'basic.json')
            ScenarioGenerator.save_scenario(scenario, scenario_file)
            print(f" → Basic: {num_objects} objects ✅")
            
        elif scenario_type == 'cascade':
            depth = params.get('depth', 3)
            scenario = ScenarioGenerator.generate_cascade(depth)
            scenario_file = os.path.join(SCENARIOS_DIR, 'cascade_delete.json')
            ScenarioGenerator.save_scenario(scenario, scenario_file)
            print(f" → Cascade: depth {depth} ✅")
            
        elif scenario_type == 'cycle':
            num_cycles = params.get('num_cycles', 1)
            scenario = ScenarioGenerator.generate_cycle(num_cycles)
            scenario_file = os.path.join(SCENARIOS_DIR, 'cycle_leak.json')
            ScenarioGenerator.save_scenario(scenario, scenario_file)
            print(f" → Cycle: {num_cycles} cycles ✅")

        # 3️⃣ ЗАПУСТИТЬ C++ ТЕСТЕР
        if not os.path.exists(RC_TESTER):
            print(f"❌ rc_tester.exe not found: {RC_TESTER}")
            return jsonify({'error': 'rc_tester.exe not found'}), 404

        # ✅ ЗАПУСК: exe будет в cpp/, но сценарии в ../scenarios/
        cmd = [RC_TESTER, scenario_type]
        print(f"✅ Running: {' '.join(cmd)}")
        print(f"   from directory: {CPP_DIR}")

        result = subprocess.run(
            cmd,
            cwd=CPP_DIR,
            capture_output=True,
            text=True,
            timeout=30,
            encoding='utf-8',
            errors='replace'
        )

        print(f"Return code: {result.returncode}")
        if result.stdout:
            print(f"STDOUT:\n{result.stdout[:1000]}")
        if result.stderr:
            print(f"STDERR:\n{result.stderr[:1000]}")

        # 4️⃣ ПРОВЕРЯЕМ ЧТО ЛОГ СОЗДАН
        if not os.path.exists(LOGS_FILE):
            print(f"⚠️ Log file not found: {LOGS_FILE}")
            return jsonify({'error': 'Log file not created by C++ program'}), 500

        # 5️⃣ ПАРСИТЬ ЛОГИ
        events = LogParser.parse_logs(LOGS_FILE)
        summary = LogParser.get_summary(events)

        print(f"✅ Parsed {len(events)} events")
        print(f"Summary: {summary}\n")

        return jsonify({
            'success': True,
            'type': scenario_type,
            'params': params,
            'events': events,
            'summary': summary
        })

    except subprocess.TimeoutExpired:
        print("❌ Timeout!")
        return jsonify({'error': 'Timeout'}), 500

    except Exception as e:
        print(f"❌ Error: {e}")
        import traceback
        traceback.print_exc()
        return jsonify({'error': str(e)}), 500

@app.route('/api/clear-logs', methods=['POST'])
def clear_logs():
    try:
        if os.path.exists(LOGS_FILE):
            os.remove(LOGS_FILE)
        return jsonify({'success': True})
    except Exception as e:
        return jsonify({'error': str(e)}), 500

if __name__ == '__main__':
    print("📍 Запуск сервера на http://localhost:5000")
    print("Откройте браузер и перейдите по адресу выше\n")
    print("Для остановки нажмите Ctrl+C\n")
    app.run(debug=True, port=5000, host='0.0.0.0')
