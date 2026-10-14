# Overport Galaxy XR

Форк [ovrport/app](https://github.com/ovrport/app), поддерживаемый **4uva4ek**.
Подготавливает APK приложений Quest для Samsung Galaxy XR.

## Графическое приложение

В [Releases](https://github.com/4uva4ek/overport-galaxyxr/releases) доступны две версии:

- **Windows:** распакуйте ZIP и запустите `OverportGalaxyXR.exe`. Java уже включена.
- **Android:** установите APK и откройте **Overport Galaxy XR**.

Выберите APK игры, при необходимости измените разрешение, производительность,
запрос разрешения на трекинг рук и диспетчер OpenXR (открытый по умолчанию,
переключатель возвращает закрытый диспетчер Overport), затем нажмите
**Save patched APK**. Дополнительные патчи находятся в **Advanced patches**.
Проверенный runtime загружается автоматически. Для первой обработки нужен интернет.

## Командная строка

CLI поставляется дополнительно и требует Java 21.

```sh
java -jar overport-galaxyxr-0.2.0-cli.jar patch --input=game.apk --target=galaxyxr --output=patched
```

Для каждого приложения профиль Galaxy XR по умолчанию задаёт:

- рекомендацию **3552×3840 на глаз**; игра может применять собственный масштаб;
- запросы **BOOST для CPU/GPU** и **90 Гц**, если частота поддерживается;
- открытый диспетчер OpenXR ([openxr-dispatcher-open](https://github.com/Molier/openxr-dispatcher-open)
  1.0.0, MIT) вместо закрытого диспетчера Overport; swapchain больше 4096 пикселей
  передаются runtime без аварийного завершения (`--dispatcher=open`);
- метаданные Android XR и исправление EGL для ARM64-сборок Unity (по сигнатуре кода);
- запрос разрешения Android XR на трекинг рук при запуске, если игра объявляет
  поддержку рук Quest в манифесте (`--hands=auto`);
- восстановление скелета рук для OVRPlugin: системный runtime отдаёт неполное
  описание `XR_FB_hand_tracking_mesh`, адаптер достраивает его (`--hand-mesh=fix`).

Запросы производительности не гарантируют 90 FPS: частотами и температурными
ограничениями управляет системный runtime. На SM-I610 подтверждены 60/72/90 Гц.
Раскладка контроллеров соответствует Quest. Переназначения AB/XY нет;
в Beat Saber паузу открывает отдельная кнопка Menu левого контроллера.

## Настройки

```sh
# Снизить нагрузку на GPU:
java -jar overport-galaxyxr-0.2.0-cli.jar patch --input=game.apk --target=galaxyxr --texture-size=2320x2700

# Вернуть рекомендации runtime и управление производительностью приложению:
java -jar overport-galaxyxr-0.2.0-cli.jar patch --input=game.apk --target=galaxyxr --texture-size=native --performance=app

# Трекинг рук: on запрашивает разрешение всегда, off никогда (по умолчанию auto):
java -jar overport-galaxyxr-0.2.0-cli.jar patch --input=game.apk --target=galaxyxr --hands=off

# Вернуть закрытый диспетчер Overport 3.4.3 с исправлением ограничения swapchain:
java -jar overport-galaxyxr-0.2.0-cli.jar patch --input=game.apk --target=galaxyxr --dispatcher=overport
```

Трекинг рук Android XR требует runtime-разрешения `android.permission.HAND_TRACKING`.
Для игр, у которых оно нужно, обработанный APK показывает системный диалог
перед первым запуском; при отказе игра запускается без рук. Само отслеживание
выполняет runtime гарнитуры, живые суставы адаптер не меняет.

Суставы `XR_EXT_hand_tracking` runtime Galaxy XR отдаёт по спецификации OpenXR,
но описание скелета `XR_FB_hand_tracking_mesh` (родители, радиусы, bind-позы
кончиков и пястных костей) неполное, из-за чего OVRPlugin скручивает руки.
Адаптер при `xrGetHandMeshFB` достраивает родителей, радиусы и недостающие
суставы по валидным суставам и живым пропорциям и переводит bind-позу в корневую
систему, которую ожидает OVRPlugin (как у runtime Meta); вершины меша только
поворачиваются вместе со скелетом.
`--hand-mesh=runtime` оставляет описание runtime без изменений.
Проверено на Job Simulator 1.4.0.6515 и SM-I610.

`native` означает рекомендацию OpenXR runtime (в проверенной конфигурации
1856×2160), а не разрешение панели. `--verbose-runtime` включает подробные
журналы внутри APK; для обычной игры он не нужен. Явный список `--patches`
заменяет профиль. Параметры разрешения и производительности применяются после него.

## Совместимость и откат

Профиль использует проверенный ARM64 runtime **3.4.3-aa54c3f**: из него берутся
generic-лоадер и платформенные библиотеки. Сам диспетчер `libopenxr_loader.so`
по умолчанию открытый: он собирается из закреплённого коммита upstream
(`native/galaxyxr/build_dispatcher.sh`), читает те же ключи `liboverport.config.so`,
грузит адаптер по имени `libopenxr_loader_generic.so` и пишет журнал с тегом
`OpenXRDispatch` (`adb logcat -s OpenXRDispatch`; строки `unsupported:` перечисляют,
чего runtime не дал игре). `--dispatcher=overport` возвращает закрытый диспетчер;
только для него применяется исправление ограничения swapchain, которое проверяет
полный SHA-256 и отклоняет неизвестные бинарные версии.
EGL-исправление находит в `libunity.so` единственную
последовательность инструкций, задающую `EGL_CONTEXT_OPENGL_NO_ERROR_KHR = 1`, и меняет
значение на 0; проверено на Unity 2022.3.33f1 и 2022.3.67f1. Если совпадений нет
или их несколько, библиотека не изменяется и выводится `NOT APPLIED`.
Совместимость со всеми играми Quest не подтверждена.

Все изменения находятся в новом APK. Прошивка, системный OpenXR, KernelSU
и настройки устройства не меняются. Принудительная остановка приложения
прекращает работу адаптера. Для отката переустановите сохранённый предыдущий APK
с той же подписью; перезагрузка не отменяет обновление приложения.
Сохраняйте каталог `--workspace`: в нём находится ключ подписи.

Не очищайте данные Android-приложения: там хранится ключ подписи обработанных игр.
Игровые APK и закрытые сторонние библиотеки не распространяются. Открытый диспетчер
и входящий в него cJSON распространяются по лицензии MIT; тексты уведомлений
включены в сборки (`galaxyxr/LICENSE-openxr-dispatcher-open.txt`, `galaxyxr/LICENSE-cJSON.txt`).
Один [workflow](docs/RELEASING.md) собирает и публикует Windows и Android вместе.

[Проверка на Beat Saber](docs/BEAT-SABER-GALAXYXR-2026-10-13.md) |
[Открытый диспетчер](docs/OPEN-DISPATCHER-GALAXYXR-2026-10-14.md) |
[Исходники и сборка адаптера](native/galaxyxr/README.md)
