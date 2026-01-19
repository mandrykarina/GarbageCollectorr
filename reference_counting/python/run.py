#!/usr/bin/env python3
"""
Reference Counting GC Visualizer - Entry Point
Запуск Flask приложения
"""

import os
import sys

# Добавить текущую директорию в PYTHONPATH
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from app import app

if __name__ == '__main__':
    print("=" * 60)
    print("🗑️  Reference Counting GC Visualizer")
    print("=" * 60)
    print("\n📍 Запуск сервера на http://localhost:5000")
    print("Откройте браузер и перейдите по адресу выше\n")
    print("Для остановки нажмите Ctrl+C\n")
    
    app.run(debug=True, port=5000, host='0.0.0.0')