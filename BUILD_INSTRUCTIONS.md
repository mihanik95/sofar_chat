# Инструкция по сборке SOFAR VST3 плагина

## Предварительные требования

### 1. Установка JUCE Framework

**Скачать JUCE:**
- Перейдите на https://juce.com/get-juce/download
- Скачайте последнюю версию JUCE (рекомендуется 7.0.5+)
- Распакуйте в папку, например: `/Applications/JUCE` (macOS)

**JUCE уже установлен в проекте!**
JUCE Framework версии 7.0.12 уже включен в проект в папке `JUCE/`. 

Если по какой-то причине папка JUCE пуста, выполните:
```bash
cd /путь/к/проекту/sofar_cursor
rm -rf JUCE
git clone --depth 1 --branch 7.0.12 https://github.com/juce-framework/JUCE.git JUCE
```

### 2. Установка Xcode (для macOS)
- Установите Xcode из App Store
- Установите Command Line Tools: `xcode-select --install`

## Пошаговая сборка через Projucer

### Шаг 1: Сборка и запуск Projucer

1. **Соберите Projucer** (только в первый раз):
   ```bash
   cd JUCE/extras/Projucer/Builds/MacOSX/
   open Projucer.xcodeproj
   # В Xcode нажмите Cmd+B для сборки Projucer
   ```

2. **Запустите Projucer**:
   - Найдите собранный Projucer.app в папке build
   - Или запустите из Xcode (Cmd+R)

3. **Откройте проект**:
   - В Projucer: File → Open → выберите `SOFAR.jucer` из корня проекта

### Шаг 2: Проверка настроек проекта

1. В **Project Settings** убедитесь что:
   - **Project Type**: Plugin
   - **Plugin Format**: VST3 включен
   - **Plugin Characteristics**: 
     - Is a Synth: No
     - Wants MIDI input: No
     - Wants MIDI output: No

2. Проверьте **Module Paths** в настройках экспорта:
   - Все пути должны указывать на `JUCE/modules`

### Шаг 3: Генерация проекта Xcode

1. Нажмите кнопку **"Save Project and Open in IDE"** (иконка Xcode)
2. Или используйте **File → Save Project**
3. Projucer создаст папку `Builds/MacOSX/` с файлом `SOFAR.xcodeproj`

### Шаг 4: Сборка в Xcode

1. **Xcode откроется автоматически** с проектом SOFAR
2. Выберите схему **"SOFAR - VST3"** в верхней части Xcode
3. Выберите целевую архитектуру:
   - **Debug**: для разработки и тестирования
   - **Release**: для финальной версии

4. **Нажмите Cmd+B** для сборки

### Шаг 5: Результат сборки

После успешной сборки плагин будет установлен в:
- **VST3**: `~/Library/Audio/Plug-Ins/VST3/SOFAR.vst3/`
- **AudioUnit** (если включен): `~/Library/Audio/Plug-Ins/Components/SOFAR.component`

## Автоматическая установка

Projucer настроен с параметром `COPY_PLUGIN_AFTER_BUILD=TRUE`, поэтому плагин автоматически копируется в системную папку плагинов после сборки.

## Тестирование

1. Откройте вашу DAW (Logic Pro, Cubase, Reaper, etc.)
2. Создайте аудиотрек
3. Добавьте плагин **SOFAR** в цепь эффектов
4. Проверьте работу:
   - Слайдер **Distance** (0-100%)
   - Кнопки выбора помещения (A/B/C/D)

## Решение проблем

### JUCE модули не найдены
```
Solution: Проверьте пути к JUCE модулям в Projucer:
- Откройте Exporters → Xcode (macOS) → Module Paths
- Убедитесь что все пути указывают на "JUCE/modules"
```

### Ошибки компиляции BinaryData
```
Solution: Убедитесь что аудиофайлы в Resources/ корректны:
- room.wav, studio.wav, hall.wav, cave.wav
- Файлы должны быть в формате WAV, 44.1kHz
```

### Плагин не появляется в DAW
```
Solution 1: Проверьте путь установки:
~/Library/Audio/Plug-Ins/VST3/SOFAR.vst3/

Solution 2: Пересканируйте плагины в DAW
Solution 3: Проверьте архитектуру (arm64 vs x86_64)
```

### Проблемы с зависимостями
```
Solution: Установите дополнительные framework'и:
- CoreAudio.framework
- CoreMIDI.framework
- AudioUnit.framework
- AudioToolbox.framework
(обычно добавляются автоматически)
```

## Опции сборки

### Debug сборка (для разработки)
- Включены символы отладки
- Отключены оптимизации
- Размер файла больше

### Release сборка (для распространения)
- Включены оптимизации (-O2)
- Link Time Optimization (LTO)
- Stripped symbols
- Минимальный размер файла

## Дополнительные форматы плагинов

Для добавления других форматов в `.jucer` файле:
- **AudioUnit**: для Logic Pro, GarageBand
- **AAX**: для Pro Tools (требует Avid SDK)
- **Standalone**: отдельное приложение

## Минимальные системные требования

- **macOS**: 10.13 High Sierra или выше
- **Архитектура**: Intel x86_64 или Apple Silicon arm64
- **Xcode**: 12.0 или выше
- **JUCE**: 6.1.0 или выше (рекомендуется 7.0.5+)

## Готово!

После выполнения всех шагов у вас будет рабочий VST3 плагин SOFAR, готовый к использованию в любой совместимой DAW. 