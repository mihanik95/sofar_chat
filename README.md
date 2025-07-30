# SOFAR - Spatial Distance Effect VST3 Plugin

SOFAR - это VST3-плагин для имитации реалистичного отдаления звука в пространстве.

## Возможности

- **Физически реалистичное затухание громкости** (-6 дБ каждые 2x расстояния)
- **Поглощение высоких частот воздухом** (low-pass фильтрация)
- **Реверберация помещения** через конволюцию с импульсными откликами
- **4 типа акустических пространств**: Room, Studio, Hall, Cave

## Установка

1. Скачайте последнюю версию из релизов
2. Скопируйте `SOFAR.vst3` в папку VST3 плагинов:
   - macOS: `~/Library/Audio/Plug-Ins/VST3/`
   - Windows: `C:\Program Files\Common Files\VST3\`
3. Перезапустите DAW

## Сборка из исходников

См. [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md)

## Документация

- [Инструкция по сборке](BUILD_INSTRUCTIONS.md)
- [Техническая документация](docs/TECHNICAL_DOCUMENTATION.md)
- [История версий](CHANGELOG.md)

## Лицензия

Проект использует JUCE Framework.

---
Michael Afanasyev © 2024
