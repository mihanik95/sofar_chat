#!/bin/bash
# SOFAR Build Script
# Автоматическая сборка плагина через Projucer и Xcode

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

echo "=== SOFAR Build Script ==="
echo "Версия: 1.0.1"
echo ""

# Проверка наличия Projucer
if [ ! -f "JUCE/extras/Projucer/Builds/MacOSX/build/Debug/Projucer.app/Contents/MacOS/Projucer" ]; then
    echo "❌ Projucer не найден. Сборка Projucer..."
    cd JUCE/extras/Projucer/Builds/MacOSX/
    xcodebuild -configuration Debug
    cd "$SCRIPT_DIR"
    echo "✅ Projucer собран"
fi

# Открытие проекта в Projucer и генерация Xcode проекта
echo "📋 Генерация Xcode проекта..."
./JUCE/extras/Projucer/Builds/MacOSX/build/Debug/Projucer.app/Contents/MacOS/Projucer --resave SOFAR.jucer

# Сборка через Xcode
echo "🔨 Сборка Release версии..."
cd Builds/MacOSX/
xcodebuild -project SOFAR.xcodeproj -configuration Release -target "SOFAR - VST3"

echo ""
echo "✅ Сборка завершена!"
echo "📍 Плагин установлен: ~/Library/Audio/Plug-Ins/VST3/SOFAR.vst3"

# Обновление версии в CHANGELOG
cd "$SCRIPT_DIR"
VERSION=$(grep 'version=' SOFAR.jucer | sed 's/.*version="\([^"]*\)".*/\1/')
echo ""
echo "📝 Текущая версия: $VERSION"

# Показать размер плагина
if [ -d ~/Library/Audio/Plug-Ins/VST3/SOFAR.vst3 ]; then
    SIZE=$(du -sh ~/Library/Audio/Plug-Ins/VST3/SOFAR.vst3 | cut -f1)
    echo "💾 Размер плагина: $SIZE"
fi

echo ""
echo "Для тестирования откройте вашу DAW и найдите плагин SOFAR в списке VST3 эффектов."
