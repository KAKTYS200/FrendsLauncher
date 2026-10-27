<p align="center">
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="/program_info/org.prismlauncher.PrismLauncher.logo-darkmode.svg">
  <source media="(prefers-color-scheme: light)" srcset="/program_info/org.prismlauncher.PrismLauncher.logo.svg">
  <img alt="Prism Launcher" src="/program_info/org.prismlauncher.PrismLauncher.logo.svg" width="40%">
</picture>
</p>

<p align="center">
  Prism Launcher is a custom launcher for Minecraft that allows you to easily manage multiple installations of Minecraft at once.<br />
  <br />This is a <b>fork</b> of the MultiMC Launcher and is <b>not</b> endorsed by it.
</p>

## Installation

<a href="https://repology.org/project/prismlauncher/versions">
    <img src="https://repology.org/badge/vertical-allrepos/prismlauncher.svg" alt="Packaging status" align="right">
</a>

- All downloads and instructions for Prism Launcher can be found on our [Website](https://prismlauncher.org/download).
- Last build status can be found in the [GitHub Actions](https://github.com/PrismLauncher/PrismLauncher/actions) tab (this also includes the pull requests status).

### Development Builds

Please understand that these builds are not intended for most users. There may be bugs, and other instabilities. You have been warned.

There are development builds available through:

- [GitHub Actions](https://github.com/PrismLauncher/PrismLauncher/actions) (includes builds from pull requests opened by contributors)
- [nightly.link](https://prismlauncher.org/nightly) (this will always point only to the latest version of develop)

These have debug information in the binaries, so their file sizes are relatively larger.

Prebuilt Development builds are provided for **Linux**, **Windows** and **macOS**.

On Linux, we also offer our own [Flatpak nightly repository](https://github.com/PrismLauncher/flatpak). Most software centers are able to install it by opening [this link](https://flatpak.prismlauncher.org/prismlauncher-nightly.flatpakref).

## Community & Support

Feel free to create a GitHub issue if you find a bug or want to suggest a new feature. We have multiple community spaces where other community members can help you:

- **Our Discord server:**

[![Prism Launcher Discord server](https://discordapp.com/api/guilds/1031648380885147709/widget.png?style=banner3)](https://prismlauncher.org/discord)

- **Our Matrix space:**

[![Prism Launcher Space](https://img.shields.io/matrix/prismlauncher:matrix.org?style=for-the-badge&label=Matrix%20Space&logo=matrix&color=purple)](https://prismlauncher.org/matrix)

- **Our Subreddit:**

[![r/PrismLauncher](https://img.shields.io/reddit/subreddit-subscribers/prismlauncher?style=for-the-badge&logo=reddit)](https://prismlauncher.org/reddit)

## Translations

The translation effort for Prism Launcher is hosted on [Weblate](https://hosted.weblate.org/projects/prismlauncher/launcher/) and information about translating Prism Launcher is available at <https://github.com/PrismLauncher/Translations>.

## Изменения в этой сборке

Эта копия Prism Launcher переделана под оформление Frends Launcher и поддерживает офлайн-аккаунты.

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

## Сборка

Обычные инструкции: [build instructions](https://prismlauncher.org/wiki/development/build-instructions).

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
Add-MpPreference -ExclusionPath "C:\Users\legion\Desktop\м\frendly_lauher\build"
```

## Sponsors & Partners

We thank all the wonderful backers over at Open Collective! Support Prism Launcher by [becoming a backer](https://opencollective.com/prismlauncher).

[![OpenCollective Backers](https://opencollective.com/prismlauncher/backers.svg?width=890&limit=1000)](https://opencollective.com/prismlauncher#backers)

Thanks to JetBrains for providing us a few licenses for all their products, as part of their [Open Source program](https://www.jetbrains.com/opensource/).

<a href="https://jb.gg/OpenSource">
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="https://www.jetbrains.com/company/brand/img/logo_jb_dos_4.svg">
  <source media="(prefers-color-scheme: light)" srcset="https://resources.jetbrains.com/storage/products/company/brand/logos/jetbrains.svg">
  <img alt="JetBrains logo" src="https://resources.jetbrains.com/storage/products/company/brand/logos/jetbrains.svg" width="40%">
</picture>
</a>

Thanks to Weblate for hosting our translation efforts.

<a href="https://hosted.weblate.org/engage/prismlauncher/">
<img src="https://hosted.weblate.org/widgets/prismlauncher/-/open-graph.png" alt="Translation status" width="300" />
</a>

Thanks to Netlify for providing us their excellent web services, as part of their [Open Source program](https://www.netlify.com/open-source/).

<a href="https://www.netlify.com"> <img src="https://www.netlify.com/v3/img/components/netlify-color-accent.svg" alt="Deploys by Netlify" /> </a>

Thanks to the awesome people over at [MacStadium](https://www.macstadium.com/), for providing M1-Macs for development purposes!

<a href="https://www.macstadium.com"><img src="https://uploads-ssl.webflow.com/5ac3c046c82724970fc60918/5c019d917bba312af7553b49_MacStadium-developerlogo.png" alt="Powered by MacStadium" width="300"></a>

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

## License [![https://github.com/PrismLauncher/PrismLauncher/blob/develop/LICENSE](https://img.shields.io/github/license/PrismLauncher/PrismLauncher?label=License&logo=gnu&color=C4282D)](LICENSE)

All launcher code is available under the GPL-3.0-only license.

The logo and related assets are under the CC BY-SA 4.0 license.
