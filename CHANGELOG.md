# SOFAR Version History

## Version 0.0086 (Current - Built 2025-01-29)
- КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: Полностью устранен резкий скачок громкости при переходе от 0% к 1% дистанции
- Добавлено плавное нарастание эффектов от 0м, без порогового включения
- Исправлена физика затухания громкости - теперь полностью реалистичное поведение
- Громкость теперь всегда остается на единице для близких расстояний (0-1м)
- Убраны все резкие изменения при изменении параметров комнаты
- Оптимизирована обработка сигнала для более натурального звучания
- Улучшена стабильность и производительность

## Version 0.0085 (Built 2025-01-28)
- Fixed compilation issues
- Added missing header files (EarlyReflectionIR.h, MySofaHRIR.h)
- Optimized project structure
- Added automatic build script
- Cleaned up unnecessary files
- Improved documentation

## Version 0.0084 (Previous)
- Initial working version with full UI
- Distance parameter (0-100%)
- Panning control
- Height control
- Volume Compensation
- Room dimensions controls (Length, Width, Height)
- Air Absorption
- Temperature control
- 4 acoustic spaces: Room, Studio, Hall, Cave
- Physical sound propagation modeling
- VST3 format support

## Planned Features
- Additional acoustic spaces
- Custom impulse response loading
- Enhanced UI with visual feedback
- Parameter automation improvements
