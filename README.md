# Ассистент для настольных ролевых игр dnd/pathfinder

## как собрать
Для анализа голосового ввода требуется vosk.
Необходимо указать путь к libvosk.so в VOSK_LIB, сейчас задается в extern/CMakeLists.txt.
Без vosk останутся доступными только графический и консольный интерфейсы.

```console
git clone --recursive https://github.com/stasana666/ttrpg_assistant.git
mkdir build
cd build
cmake .. -DGGML_CUDA=ON
make
```

Для использования голосового ввода необходим vosk, в extern/CMakeLists.txt необходимо в VOSK_LIB указать путь к libvosk.so.
Взять его например можно из соответствующего python3 модуля или собрать самостоятельно.

Для использования голосового ввода необходимо наличие моделей для vosk и llama-cpp.
путь к моделям передается в соответствующих аргументах командной строки.
* Модели для vosk можно найти тут: https://alphacephei.com/vosk/models
* Модели для llama-cpp можно найти тут: https://huggingface.co/models?apps=llama.cpp


Использование CUDA не обязательно, но без него голосовой ввод будет выполняться на CPU.
Скорость инференса на CPU настолько низкая, что использование инструмента теряет практический смысл.

## Структура проекта

* pf2e_engine/src/ и pf2e_engine/include/ - все исходники.
* pf2e_engine/main/ - точка входа приложения.
* pf2e_engine/tests - тесты.
* pf2e_engine/data - описание всех игровых сущностей.
* pf2e_engien/schemas - содержит JSON schema для валидации описаний игровых сущностей.
* tools/data_validation.py - скрипт для валидации без запуска основного приложения.
