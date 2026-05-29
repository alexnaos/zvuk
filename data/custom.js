// Ждем полной загрузки DOM-дерева страницы
document.addEventListener("DOMContentLoaded", () => {
    // 1. Создаем элемент для CSS-стилей
    const style = document.createElement("style");
    
    // 2. Описываем геометрию и цвета iOS Clean Style
    style.textContent = `
        /* Системный фон Apple и шрифты */
        body, button, input, select {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif !important;
            background-color: #F2F2F7 !important;
            color: #000000 !important;
        }
        
        /* Превращаем группы в аккуратные карточки */
        .sets-group {
            background: #FFFFFF !important;
            border-radius: 12px !important;
            box-shadow: 0 4px 12px rgba(0, 0, 0, 0.05) !important;
            border: 1px solid rgba(0, 0, 0, 0.04) !important;
            margin: 15px 0 !important;
        }
        
        /* Фирменные синие кнопки-капсулы */
        button {
            background-color: #007AFF !important;
            color: #FFFFFF !important;
            border-radius: 20px !important;
            font-weight: 500 !important;
            font-size: 15px !important;
            box-shadow: 0 2px 4px rgba(0, 122, 255, 0.15) !important;
            transition: all 0.2s ease !important;
        }
        
        /* Эффект нажатия Apple */
        button:active {
            opacity: 0.7 !important;
            transform: scale(0.98) !important;
        }
        
        /* Меню переключения вкладок */
        .sets-tab-bar {
            background: #E5E5EA !important;
            border-radius: 9px !important;
            padding: 2px !important;
        }
        
        .sets-tab {
            color: #000000 !important;
            border-radius: 7px !important;
            border-bottom: none !important;
            font-weight: 600 !important;
        }
        
        .sets-tab-active {
            background: #FFFFFF !important;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1) !important;
        }
    `;
    
    // 3. Внедряем стили в заголовок страницы
    document.head.appendChild(style);
});
