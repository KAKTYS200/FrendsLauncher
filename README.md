<p align="center">
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="/program_info/org.frends.FrendsLauncher.logo-darkmode.svg">
  <source media="(prefers-color-scheme: light)" srcset="/program_info/org.frends.FrendsLauncher.logo.svg">
  <img alt="Frends Launcher" src="/program_info/org.frends.FrendsLauncher.logo.svg" width="40%">
</picture>
</p>

<h1 align="center">Frends Launcher</h1>

<p align="center">
  Лаунчер Minecraft с офлайн-аккаунтами, своими скинами и сборками в пару кликов.<br />
  Без навязчивого софта, без рекламы, без регистрации.
</p>

<p align="center">
  <b>Версия 2.2.0</b> · Windows x64 · GPL-3.0-only
</p>

## Скачать

**[FrendsLauncher-portable.zip](https://github.com/KAKTYS200/FrendsLauncher/releases/latest/download/FrendsLauncher-portable.zip)** — последняя версия, 41 МБ.

1. Скачайте архив и распакуйте в любую папку (установка не нужна).
2. Запустите `frendslauncher.exe`.
3. Нужна **Java 17** — если её нет, лаунчер предложит скачать.

Все версии и список изменений — в разделе [Releases](https://github.com/KAKTYS200/FrendsLauncher/releases).

## Возможности

- **Офлайн-аккаунты** — добавьте любой ник без лицензии Microsoft и играйте.
- **Свои скины** — скин ставится прямо в лаунчере и работает в ванильном Minecraft 1.20.1 без Forge, Fabric и OptiFine. Лаунчер публикует скин на localhost и передаёт его клиенту через свойство `textures`, а кеш игры заполняется заранее — скин виден сразу, без обращения к сети.
- **Сборки в пару кликов** — выбор версии, загрузчика (Vanilla / Forge / NeoForge / Fabric / Quilt), автоподстановка имени.
- **Импорт из TLauncher** — укажите папку `.minecraft/versions`, и лаунчер сам разберёт сборки: определит версию игры и загрузчик, пересоздаст сборку на официальных метаданных и перенесёт моды, конфиги, сохранения и настройки.
- **Список серверов** с живым пингом и запуском игры прямо на нужный сервер.
- **Свой фон** — картинка на всю область списка сборок, палитра и масштаб интерфейса настраиваются.
- **Обновление в один клик** — кнопка со стрелкой вниз в нижней панели. Лаунчер сам спрашивает у GitHub последний релиз, скачивает архив и перезапускается уже новой версией.
- **Приватность** — лаунчер не отправляет статистику и не собирает идентификаторы.

## Что внутри

Это форк [Prism Launcher](https://github.com/PrismLauncher/PrismLauncher) 11.1.1 (который, в свою очередь, форк MultiMC). Интерфейс переписан под Frends: процедурный фон, своя боковая панель и нижняя панель с аккаунтом, сборкой и кнопкой «Войти в игру». Подробности по каждому изменению — [ниже](#изменения-в-этой-сборке).

Frends Launcher **не связан** с Prism Launcher, MultiMC и Mojang и не поддерживается ими.

## Поддержка

Нашли баг или хотите предложить идею — создайте [issue](https://github.com/KAKTYS200/FrendsLauncher/issues).

## Изменения в этой сборке

Frends Launcher — это переработанный Prism Launcher с оформлением Frends и поддержкой офлайн-аккаунтов.

### Оформление

- Главное окно (`launcher/ui/MainWindow.ui`, `launcher/ui/MainWindow.cpp`) собрано заново: вместо стандартных тулбаров и меню используются:
  - фоновая сцена, нарисованная процедурно (`launcher/ui/frends/FrendsBackdrop.*`);
  - правая синяя панель с логотипом FRENDS LAUNCHER и навигацией (`launcher/ui/frends/FrendsSidebar.*`);
  - нижняя зелёная панель с выбором аккаунта и сборки, галочкой «Обновить клиент», жёлтой кнопкой «Войти в игру» и кнопками обновления, папки и настроек (`launcher/ui/frends/FrendsBottomBar.*`).
- Старые тулбары, меню и статусбар скрыты, но их действия по-прежнему работают: навигация и кнопки нижней панели вызывают те же слоты `MainWindow` (настройки, добавление сборки, папка сборки, логи, «О лаунчере», новости).
- Кнопка «Войти в игру» запускает выбранную сборку (или останавливает её, если она уже запущена). Если сборка не выбрана, выбирается первая; если сборок нет — открывается диалог создания.

### Меню создания сборки

- `launcher/ui/pages/modplatform/CustomPage.ui` и `.cpp` — диалог «Создание сборки» переработан: список версий Minecraft теперь всегда на виду (раньше он был спрятан за кнопкой «Списки версий», из-за чего казалось, что версий нет), добавлены поиск, фильтр «Только релизы» и быстрый выбор последних 15 релизов.
- Название сборки подставляется автоматически по выбранной версии, поэтому кнопка «Создать сборку» активна сразу, а не после ручного ввода имени.
- Типы версий переведены на русский (релиз / снапшот / старая бета / альфа), оформление приведено к палитре Frends Launcher.
- Загрузчик (Vanilla / Forge / NeoForge / Fabric / Quilt) выбирается справа; при выборе загрузчика под основным списком появляется список его версий.

### Обновление лаунчера

- Кнопка со стрелкой вниз в нижней панели — обновление лаунчера в один клик.
- Источник обновлений задаётся ключом `FrendsLauncherUpdateUrl` в `frendslauncher.cfg` (обязателен в секции `[General]`): это JSON-манифест вида `{"version": "2.0", "url": "https://ваш-сайт/frendslauncher.exe"}`.
- По нажатию лаунчер скачивает манифест, сравнивает версию, скачивает новый exe с прогрессом на полосе внизу и после подтверждения подменяет файл (старый сохраняется как `frendslauncher.exe.old`) и перезапускается.
- Если источник не настроен, кнопка показывает подсказку с форматом манифеста.

### Масштабирование интерфейса

- `launcher/ui/frends/FrendsScale.*` — общий пересчёт метрик: коэффициент складывается из пользовательской настройки и высоты окна, поэтому панели подстраиваются и под размер окна, и под системный DPI.
- Ползунок «Масштаб интерфейса» (80–150 %) находится в настройках, страница «Общие», и применяется на лету через сигнал `SettingsObject::SettingChanged`.

### Страница «Серверы»

- `launcher/ui/frends/FrendsServersPage.*` — список публичных серверов с живым пингом через `McResolver`/`McClient`.
- Адреса показываются открыто: в оригинальном TLauncher партнёрские серверы идут с `hideAddress: true`, здесь такого нет.
- Двойной клик по строке запускает выбранную сборку с `--quickPlayMultiplayer` / `--server` (`MinecraftTarget`).

### Приватность

- `launcher/ui/frends/FrendsDiagnostics.*` — локальный отчёт об окружении (ОС, ядро, архитектура, ядра CPU, ОЗУ, экран, Java, идентификатор установки) и постоянный `FrendsInstallId`.
- Оригинальный лаунчер отправляет те же данные вместе с двумя стабильными идентификаторами на свои статистические домены; Frends Launcher не отправляет ничего.
- Раздел «Приватность» в настройках показывает отчёт, позволяет скопировать его и сбросить идентификатор, а также включает необязательный локальный счётчик запусков.

### Офлайн-аккаунты

- `AccountList::anyAccountIsValid()` теперь считает валидным любой добавленный аккаунт, а не только тот, что владеет Minecraft.
- В менеджере аккаунтов (`launcher/ui/pages/global/AccountListPage.cpp`) офлайн-аккаунт можно добавить без предварительного лицензионного аккаунта.
- `LaunchController::decideLaunchMode()` запускает офлайн-аккаунт сразу в офлайн-режиме, не требуя права владения игрой.
- Проверки при скачивании версии (`VersionPage`) и стартовые сообщения больше не требуют именно Microsoft-аккаунта.

### Импорт версий из TLauncher

- `launcher/TLauncherImportTask.*` — разбирает папку версии и определяет, что в ней лежит. TLauncher хранит каждую версию одним плоским JSON без `inheritsFrom`, поэтому базовая версия и загрузчик восстанавливаются по содержимому:
  - NeoForge — по аргументам запуска `--fml.neoForgeVersion` и `--fml.mcVersion` (самый точный источник, библиотеки дают только транзитивные вспомогательные артефакты);
  - Forge — по библиотекам `net.minecraftforge:forge:<версия>-<загрузчик>` и `fmlloader`;
  - Fabric — по `net.fabricmc:intermediary` (версия игры) и `fabric-loader` (версия загрузчика);
  - OptiFine — по имени артефакта `optifine:OptiFine:<версия>_HD_U_…`;
  - если ничего не нашлось — по номеру версии в названии папки или в поле `id`, с проверкой по официальному манифесту.
- Сборка не копируется как набор jar-файлов: создаётся обычный инстанс из официальных метаданных, поэтому дальше он обновляется и запускается как любой другой.
- Переносится содержимое: `mods`, `config`, `defaultconfigs`, `resourcepacks`, `shaderpacks`, `saves`, `options.txt`, `optionsof.txt`, `optionsshaders.txt`, `servers.dat`, `usercache.json`. Сами библиотеки и jar не копируются — они скачиваются заново.
- `launcher/ui/pages/modplatform/TLauncherPage.*` — страница выбора: путь к папке `versions` (по умолчанию `%APPDATA%\.minecraft\versions`), список найденных версий с определённой версией игры и загрузчиком, неимпортируемые помечены. Хостом страницы служит `InstanceCreationSink`, поэтому она работает и в модальном диалоге, и внутри окна.
- На странице «Сборки» добавлена карточка «Из TLauncher».

## Сборка

Инструкции по сборке Prism Launcher: [build instructions](https://prismlauncher.org/wiki/development/build-instructions). Окружение — MSYS2 UCRT64 + Qt6.

### Сборка с ограничением нагрузки на ЦП

В `CMakePresets.json` у build-пресетов `linux`, `windows_mingw` и `windows_msvc` задано `"jobs": 4`, поэтому
`cmake --build --preset windows_msvc` по умолчанию использует только 4 параллельных задачи.

Ручной вариант из командной строки:

```powershell
cmake --build build --parallel 4      # или -j4
```

Скрипт-обёртка `scripts/build-lowcpu.ps1` настраивает проект (если нужно) и собирает с ограничением потоков:

```powershell
# 4 потока по умолчанию
.\scripts\build-lowcpu.ps1

# 2 потока, конкретный пресет и конфигурация
.\scripts\build-lowcpu.ps1 -Jobs 2 -Preset windows_msvc -Config Release

# дополнительно добавить папку сборки в исключения Windows Defender
# (нужны права администратора; снижает накладные расходы антивируса примерно на 10-20%)
.\scripts\build-lowcpu.ps1 -Jobs 4 -AddDefenderExclusion
```

Без скрипта исключение Defender добавляется вручную (PowerShell от администратора):

```powershell
Add-MpPreference -ExclusionPath "$PWD\build"
```

## Благодарности

Проект стоит на плечах [Prism Launcher](https://github.com/PrismLauncher/PrismLauncher) и [MultiMC](https://github.com/MultiMC/Launcher), распространяемых под GPL-3.0. Спасибо их командам и контрибьюторам.

## Forking/Redistributing/Custom builds policy

You are free to fork, redistribute and provide custom builds as long as you follow the terms of the [license](LICENSE) (this is a legal responsibility), and if you made code changes rather than just packaging a custom build, please do the following as a basic courtesy:

- Make it clear that your fork is not Prism Launcher and is not endorsed by or affiliated with the Prism Launcher project (<https://prismlauncher.org>).
- Go through [CMakeLists.txt](CMakeLists.txt) and change Prism Launcher's API keys to your own or set them to empty strings (`""`) to disable them (this way the program will still compile but the functionality requiring those keys will be disabled).

If you have any questions or want any clarification on the above conditions please make an issue and ask us.

If you are just building Prism Launcher for your distribution, please make sure to set the `Launcher_BUILD_PLATFORM` to a slug representing your distribution. Examples are `archlinux`, `fedora` and `nixpkgs`.

Note that if you build this software without removing the provided API keys in [CMakeLists.txt](CMakeLists.txt) you are accepting the following terms and conditions:

- [Microsoft Identity Platform Terms of Use](https://docs.microsoft.com/en-us/legal/microsoft-identity-platform/terms-of-use)
- [CurseForge 3rd Party API Terms and Conditions](https://support.curseforge.com/en/support/solutions/articles/9000207405-curse-forge-3rd-party-api-terms-and-conditions)

If you do not agree with these terms and conditions, then remove the associated API keys from the [CMakeLists.txt](CMakeLists.txt) file by setting them to an empty string (`""`).

## Лицензия

All launcher code is available under the GPL-3.0-only license.

The logo and related assets are under the CC BY-SA 4.0 license.
