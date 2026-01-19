// Reference Counting GC Visualizer - ИНТЕРАКТИВНАЯ С ШАГАМИ (FIXED)
const API_BASE = '/api';
let cy = null;
let currentData = null;
let allEvents = [];
let currentStepIndex = 0;
let isAnimating = false;
let objectMap = new Map();
let edges = new Map();

// ✅ window.load
window.addEventListener('load', function() {
    console.log('✅ Загружено, инициализируем...');
    initializeUI();
    initializeCytoscape();
});

function initializeUI() {
    console.log('🔧 Инициализация UI...');
    
    const scenarioSelect = document.getElementById('scenarioType');
    const paramSlider = document.getElementById('paramSlider');
    const paramValue = document.getElementById('paramValue');
    const runBtn = document.getElementById('runTest');
    const clearBtn = document.getElementById('clearLogs');
    const nextStepBtn = document.getElementById('nextStep');
    const prevStepBtn = document.getElementById('prevStep');
    const autoPlayBtn = document.getElementById('autoPlay');
    const paramLabel = document.getElementById('paramLabel');

    if (!scenarioSelect || !paramSlider || !runBtn) {
        console.error('❌ UI элементы не найдены!');
        return;
    }

    scenarioSelect.addEventListener('change', function() {
        console.log('🔄 Сценарий изменен на:', this.value);
        const labels = {
            'basic': 'Объектов:',
            'cascade': 'Глубина:',
            'cycle': 'Циклов:'
        };
        paramLabel.textContent = labels[this.value] || 'Параметр:';
        
        const ranges = {
            'basic': { min: 1, max: 10, value: 3 },
            'cascade': { min: 2, max: 8, value: 4 },
            'cycle': { min: 1, max: 5, value: 2 }
        };
        const range = ranges[this.value] || { min: 1, max: 10, value: 3 };
        paramSlider.min = range.min;
        paramSlider.max = range.max;
        paramSlider.value = range.value;
        paramValue.textContent = range.value;
    });

    paramSlider.addEventListener('input', function() {
        paramValue.textContent = this.value;
    });

    runBtn.addEventListener('click', runTest);
    clearBtn.addEventListener('click', clearLogs);
    if (nextStepBtn) nextStepBtn.addEventListener('click', nextStep);
    if (prevStepBtn) prevStepBtn.addEventListener('click', prevStep);
    if (autoPlayBtn) autoPlayBtn.addEventListener('click', autoPlay);

    scenarioSelect.dispatchEvent(new Event('change'));
    console.log('✅ UI инициализирован');
}

function initializeCytoscape() {
    console.log('🔧 Инициализация Cytoscape...');
    
    if (typeof cytoscape === 'undefined') {
        console.error('❌ Cytoscape не загружен');
        setTimeout(initializeCytoscape, 500);
        return;
    }

    const container = document.getElementById('graph-container');
    if (!container) {
        console.error('❌ Контейнер не найден');
        return;
    }

    try {
        cy = cytoscape({
            container: container,
            style: [
                {
                    selector: 'node',
                    style: {
                        'content': 'data(label)',
                        'text-valign': 'center',
                        'text-halign': 'center',
                        'background-color': function(ele) {
                            const status = ele.data('status');
                            if (status === 'alive') return '#4CAF50';
                            if (status === 'deleted') return '#9E9E9E';
                            if (status === 'leak') return '#FF5252';
                            return '#666666';
                        },
                        'border-width': 3,
                        'border-color': function(ele) {
                            if (ele.data('isRoot')) return '#FFD700';
                            return '#333';
                        },
                        'width': 80,
                        'height': 80,
                        'font-size': 14,
                        'color': '#fff',
                        'font-weight': 'bold',
                        'transition-property': 'background-color',
                        'transition-duration': '300ms'
                    }
                },
                {
                    selector: 'edge',
                    style: {
                        'target-arrow-shape': 'triangle',
                        'line-color': '#888888',
                        'target-arrow-color': '#888888',
                        'width': 2.5,
                        'curve-style': 'bezier',
                        'label': 'data(label)'
                    }
                }
            ],
            layout: {
                name: 'cose',
                directed: true,
                padding: 30,
                animate: true,
                animationDuration: 400
            }
        });

        console.log('✅ Cytoscape инициализирован');
    } catch (error) {
        console.error('❌ Ошибка:', error);
    }
}

async function runTest() {
    console.log('▶️ Запуск теста...');
    
    if (cy) {
        console.log('🗑️ Очистка старого графа...');
        cy.elements().remove();
    }
    
    objectMap.clear();
    edges.clear();
    currentStepIndex = 0;
    allEvents = [];
    isAnimating = false;
    
    const scenarioType = document.getElementById('scenarioType').value;
    const paramValue = parseInt(document.getElementById('paramSlider').value);
    
    let params = {};
    if (scenarioType === 'basic') {
        params.num_objects = paramValue;
    } else if (scenarioType === 'cascade') {
        params.depth = paramValue;
    } else if (scenarioType === 'cycle') {
        params.num_cycles = paramValue;
    }

    console.log(`📊 Запуск: тип=${scenarioType}, параметры=${JSON.stringify(params)}`);

    try {
        updateStatus('⏳ Загрузка данных...');

        const response = await fetch(`${API_BASE}/run-test`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ type: scenarioType, params: params })
        });

        const data = await response.json();
        console.log('✅ Ответ получен:', data);

        if (data.success) {
            currentData = data;
            allEvents = data.events || [];
            currentStepIndex = 0;
            
            updateStatistics(data.summary);
            updateEventsTable(data.events);
            updateStepIndicator();
            updateStatus(`✅ Готово. Всего ${allEvents.length} событий. Нажми "Следующий шаг"`);
            
            console.log(`✅ Загружено ${allEvents.length} событий`);
        } else {
            updateStatus('❌ ' + (data.error || 'Ошибка'));
            console.error('❌ Ошибка:', data.error);
        }
    } catch (error) {
        console.error('❌ Ошибка:', error);
        updateStatus('❌ ' + error.message);
    }
}

function nextStep() {
    if (allEvents.length === 0) {
        updateStatus('⚠️ Сначала запусти тест');
        return;
    }
    
    if (currentStepIndex >= allEvents.length) {
        updateStatus('✅ Все события обработаны');
        return;
    }
    
    const event = allEvents[currentStepIndex];
    console.log(`📍 Шаг ${currentStepIndex + 1}/${allEvents.length}: ${event.type}`);
    
    processEvent(event);
    currentStepIndex++;
    
    updateStatus(`📍 Шаг ${currentStepIndex}/${allEvents.length}: ${getEventName(event.type)}`);
    updateStepIndicator();
}

function prevStep() {
    if (currentStepIndex <= 0) {
        updateStatus('⚠️ Это первый шаг');
        return;
    }
    
    if (cy) cy.elements().remove();
    objectMap.clear();
    edges.clear();
    currentStepIndex = 0;
    
    while (currentStepIndex < allEvents.length - 1) {
        const event = allEvents[currentStepIndex];
        processEventSilent(event);
        currentStepIndex++;
    }
    
    updateStatus(`📍 Вернулся на шаг ${currentStepIndex}/${allEvents.length}`);
    updateStepIndicator();
}

function processEvent(event) {
    const eventType = event.type || '';
    const objId = event.object_id;
    const fromId = event.from_id;
    const toId = event.to_id;

    switch (eventType) {
        case 'allocate':
            if (objId) {
                // ✅ FIXED: Проверяем есть ли уже узел
                const existingNode = cy.getElementById(`obj${objId}`);
                if (existingNode && existingNode.length > 0) {
                    console.log(`  ℹ️ Объект ${objId} уже существует, пропускаем`);
                    break;
                }
                
                objectMap.set(objId, { status: 'alive', rc: 0 });
                cy.add({
                    data: {
                        id: `obj${objId}`,
                        label: `Объект ${objId}`,
                        status: 'alive',
                        isRoot: false
                    }
                });
                console.log(`  ✨ Объект ${objId} создан`);
                
                const layout = cy.layout({ name: 'cose', directed: true, animate: true, animationDuration: 400 });
                layout.run();
            }
            break;

        case 'add_ref':
            if (fromId === 0 || fromId === undefined) {
                if (toId && objectMap.has(toId)) {
                    objectMap.get(toId).isRoot = true;
                    const node = cy.getElementById(`obj${toId}`);
                    if (node && node.length > 0) {
                        node.data('isRoot', true);
                        console.log(`  🟡 Объект ${toId} стал ROOT`);
                    }
                }
            } else if (fromId > 0 && toId > 0 && fromId !== toId) {
                const edgeId = `${fromId}-${toId}`;
                if (!edges.has(edgeId)) {
                    edges.set(edgeId, true);
                    cy.add({
                        data: {
                            id: edgeId,
                            source: `obj${fromId}`,
                            target: `obj${toId}`,
                            label: '→'
                        }
                    });
                    console.log(`  ➡️ Связь: ${fromId} → ${toId}`);
                    
                    const layout = cy.layout({ name: 'cose', directed: true, animate: true, animationDuration: 400 });
                    layout.run();
                }
            }
            break;

        case 'remove_ref':
            if (fromId === 0 || fromId === undefined) {
                if (toId && objectMap.has(toId)) {
                    objectMap.get(toId).isRoot = false;
                    const node = cy.getElementById(`obj${toId}`);
                    if (node && node.length > 0) {
                        node.data('isRoot', false);
                        console.log(`  ⬅️ Объект ${toId} потерял ROOT`);
                    }
                }
            }
            break;

        case 'delete':
            if (objId && objectMap.has(objId)) {
                objectMap.get(objId).status = 'deleted';
                const node = cy.getElementById(`obj${objId}`);
                if (node && node.length > 0) {
                    node.data('status', 'deleted');
                    console.log(`  ⚫ Объект ${objId} удален`);
                }
            }
            break;

        case 'leak':
            if (objId && objectMap.has(objId)) {
                objectMap.get(objId).status = 'leak';
                const node = cy.getElementById(`obj${objId}`);
                if (node && node.length > 0) {
                    node.data('status', 'leak');
                    console.log(`  🔴 УТЕЧКА: Объект ${objId}`);
                }
            }
            break;
    }
}

function processEventSilent(event) {
    const eventType = event.type || '';
    const objId = event.object_id;
    const fromId = event.from_id;
    const toId = event.to_id;

    switch (eventType) {
        case 'allocate':
            if (objId) {
                // ✅ FIXED: Проверяем есть ли уже узел
                const existingNode = cy.getElementById(`obj${objId}`);
                if (existingNode && existingNode.length > 0) {
                    break;
                }
                
                objectMap.set(objId, { status: 'alive', rc: 0 });
                cy.add({
                    data: {
                        id: `obj${objId}`,
                        label: `Объект ${objId}`,
                        status: 'alive',
                        isRoot: false
                    }
                });
            }
            break;

        case 'add_ref':
            if (fromId === 0 || fromId === undefined) {
                if (toId && objectMap.has(toId)) {
                    objectMap.get(toId).isRoot = true;
                    const node = cy.getElementById(`obj${toId}`);
                    if (node && node.length > 0) {
                        node.data('isRoot', true);
                    }
                }
            } else if (fromId > 0 && toId > 0 && fromId !== toId) {
                const edgeId = `${fromId}-${toId}`;
                if (!edges.has(edgeId)) {
                    edges.set(edgeId, true);
                    cy.add({
                        data: {
                            id: edgeId,
                            source: `obj${fromId}`,
                            target: `obj${toId}`,
                            label: '→'
                        }
                    });
                }
            }
            break;

        case 'remove_ref':
            if (fromId === 0 || fromId === undefined) {
                if (toId && objectMap.has(toId)) {
                    objectMap.get(toId).isRoot = false;
                    const node = cy.getElementById(`obj${toId}`);
                    if (node && node.length > 0) {
                        node.data('isRoot', false);
                    }
                }
            }
            break;

        case 'delete':
            if (objId && objectMap.has(objId)) {
                objectMap.get(objId).status = 'deleted';
                const node = cy.getElementById(`obj${objId}`);
                if (node && node.length > 0) {
                    node.data('status', 'deleted');
                }
            }
            break;

        case 'leak':
            if (objId && objectMap.has(objId)) {
                objectMap.get(objId).status = 'leak';
                const node = cy.getElementById(`obj${objId}`);
                if (node && node.length > 0) {
                    node.data('status', 'leak');
                }
            }
            break;
    }
}

async function autoPlay() {
    if (isAnimating) {
        isAnimating = false;
        updateStatus('⏸️ Пауза');
        return;
    }
    
    isAnimating = true;
    updateStatus('▶️ Автопроигрывание...');
    
    while (currentStepIndex < allEvents.length && isAnimating) {
        nextStep();
        await sleep(600);
    }
    
    isAnimating = false;
    if (currentStepIndex >= allEvents.length) {
        updateStatus('✅ Все события обработаны');
    }
}

function updateStepIndicator() {
    const indicator = document.getElementById('step-indicator');
    if (indicator) {
        indicator.textContent = `${currentStepIndex}/${allEvents.length}`;
    }
}

function getEventName(type) {
    const names = {
        'allocate': 'Создание объекта',
        'add_ref': 'Добавление ссылки',
        'remove_ref': 'Удаление ссылки',
        'delete': 'Удаление объекта',
        'leak': 'УТЕЧКА ПАМЯТИ'
    };
    return names[type] || type;
}

function updateStatistics(summary) {
    console.log('📈 Обновление статистики...');
    
    if (!summary) return;

    const stats = {
        'allocated': summary.allocated || 0,
        'deleted': summary.deleted || 0,
        'leaks': summary.leaks || 0,
    };

    for (const [id, value] of Object.entries(stats)) {
        const el = document.getElementById(id);
        if (el) {
            el.textContent = value;
        }
    }
}

function updateEventsTable(events) {
    console.log('📋 Обновление таблицы событий...');
    
    const tbody = document.querySelector('#events-table tbody');
    if (!tbody) {
        console.warn('⚠️ Таблица не найдена');
        return;
    }

    tbody.innerHTML = '';

    if (!events || events.length === 0) {
        tbody.innerHTML = '<tr><td colspan="3" style="text-align: center; padding: 20px; color: #999;">Нет событий</td></tr>';
        return;
    }

    const translations = {
        'allocate': { icon: '✨', name: 'Создание', color: '#4CAF50' },
        'add_ref': { icon: '➡️', name: 'Добавить ссылку', color: '#2196F3' },
        'remove_ref': { icon: '⬅️', name: 'Удалить ссылку', color: '#FF9800' },
        'delete': { icon: '⚫', name: 'Удаление', color: '#9E9E9E' },
        'leak': { icon: '🔴', name: 'УТЕЧКА', color: '#F44336' }
    };

    for (let i = 0; i < Math.min(events.length, 50); i++) {
        const event = events[i];
        const row = tbody.insertRow();
        const trans = translations[event.type] || { icon: '?', name: event.type, color: '#999' };

        const indexCell = row.insertCell(0);
        const typeCell = row.insertCell(1);
        const descCell = row.insertCell(2);

        indexCell.textContent = i + 1;
        indexCell.style.fontWeight = 'bold';
        indexCell.style.width = '40px';
        indexCell.style.textAlign = 'center';

        typeCell.innerHTML = `<span style="font-size: 16px; margin-right: 5px;">${trans.icon}</span>${trans.name}`;
        typeCell.style.backgroundColor = trans.color;
        typeCell.style.color = '#fff';
        typeCell.style.padding = '8px 12px';
        typeCell.style.borderRadius = '4px';
        typeCell.style.fontWeight = 'bold';
        typeCell.style.minWidth = '120px';
        typeCell.style.textAlign = 'center';

        descCell.textContent = getEventDescription(event);
        descCell.style.padding = '8px';
        descCell.style.color = '#333';

        if (event.type === 'leak') {
            row.style.backgroundColor = '#FFEBEE';
            row.style.borderLeft = '4px solid #F44336';
        }
    }

    console.log(`✅ Таблица обновлена: ${Math.min(events.length, 50)} событий`);
}

function getEventDescription(event) {
    const objId = event.object_id;
    const fromId = event.from_id;
    const toId = event.to_id;

    switch (event.type) {
        case 'allocate':
            return `Выделена память для объекта #${objId}`;
        case 'add_ref':
            if (fromId === 0 || fromId === undefined) {
                return `Объект #${toId} получил ROOT ссылку`;
            }
            return `Объект #${fromId} → Объект #${toId}`;
        case 'remove_ref':
            if (fromId === 0 || fromId === undefined) {
                return `Объект #${toId} потерял ROOT ссылку`;
            }
            return `Объект #${fromId} ← Объект #${toId}`;
        case 'delete':
            return `Объект #${objId} успешно удален из памяти`;
        case 'leak':
            return `⚠️ УТЕЧКА ПАМЯТИ: Объект #${objId}!`;
        default:
            return event.description || 'Неизвестное событие';
    }
}

async function clearLogs() {
    console.log('🗑️ Очистка логов...');
    try {
        const response = await fetch(`${API_BASE}/clear-logs`, { method: 'POST' });
        if (response.ok) {
            if (cy) cy.elements().remove();
            objectMap.clear();
            edges.clear();
            currentStepIndex = 0;
            allEvents = [];
            isAnimating = false;
            updateStatistics({});
            updateEventsTable([]);
            updateStepIndicator();
            updateStatus('✅ Очищено');
        }
    } catch (error) {
        console.error('❌ Ошибка:', error);
    }
}

function updateStatus(message) {
    const statusEl = document.getElementById('status');
    if (statusEl) {
        statusEl.textContent = message;
    }
}

function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

console.log('✅ visualization.js загружен - ИНТЕРАКТИВНАЯ ВЕРСИЯ (FIXED)');