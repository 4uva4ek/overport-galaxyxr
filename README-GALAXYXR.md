# overport — Galaxy XR edition

Форк [ovrport/app](https://github.com/ovrport/app) для патчинга Quest-игр под
**Samsung Galaxy XR** (Android XR).

## Как это работает (архитектура закрытого рунтайма)

Рунтайм overport (дистрибуция: `https://ovrp.crx.moe/api/v1/releases/index`,
исходники закрыты) при анализе свежей сборки `3.4.3-aa54c3f` (experimental)
содержит следующий стек OpenXR:

| Библиотека | Роль |
|---|---|
| `libOVRPlugin.so` | Замена OVRPlugin от Meta (OpenXR-композитор, OVR_* API) |
| `libopenxr_loader.so` | Диспетчер: читает `ro.product.brand`, грузит платформенный лоадер |
| `libopenxr_loader_pico.so` | Pico (bytedance): интеракционный профиль, джойстики |
| `libopenxr_loader_meta.so` | Meta Quest |
| `libopenxr_loader_yvr.so` | Yield VR |
| `libopenxr_loader_huawei.so` | Huawei |
| `libopenxr_loader_generic.so` | **Полноценная реализация Khronos OpenXR-лоадера**: сканирует манифесты в `/system/etc/openxr` и `/vendor/etc/openxr` и грузит тот runtime, что описан в них |
| `libovrplatformloader.so` | Шим OVRP_* (пользователь/лидерборды/достижения, backend ovrp.crx.moe) |
| `libpxrplatformloader.so` | Pico-вариант платформенного бэкенда |

Диспетчер выбирает лоадер по `ro.product.brand`: `bytedance` → pico и т.д.
Для неизвестных брендов (в т.ч. **samsung**) fallback — `generic`. На Galaxy XR
(Android XR) системный OpenXR-runtime — Google (`libopenxr.google.so`), его
манифест находится в `/system/etc/openxr/`, поэтому **generic-лоадер должен
подхватить его без изменений**: в отличие от PFDMR-форка (где система PFDMR
требовала собственного shim-лоадера), здесь штатный путь, вероятно, работает.

## Что сделано в этом форке

1. **`--target=galaxyxr`** в CLI-команде `patch` — пресет набора патчей под
   Galaxy XR (все recommended-патчи, метаданные — Android XR подмножество).
2. **`patch_vr_metadata` принимает аргумент цели** (`galaxyxr` / по умолчанию
   `all`): для Galaxy XR в манифест пишутся только релевантные маркеры
   (`android.software.xr.api.openxr`, XR-фичи, Khronos-права, XR-проп
   активности, `libopenxr.google.so`, `max_aspect`, `minWaveSDKVersion`),
   без Pico/YVR/Huawei/HTC вендорного мусора.
3. **`requiresOverportConfig`** флаг патча (паттерн из форка
   [OVRPort-PFDMR](https://github.com/urscaviezel/OVRPort-PFDMR)): манифестные
   патчи можно выполнять без закрытого рунтайма — в этом случае каталог
   либ и запись конфига пропускаются.

Использование:

```
overport patch --input=game.apk --target=galaxyxr
```

## Открытые вопросы (нужна проверка на железе)

- Точное значение `ro.product.brand` на Galaxy XR (ожидаемо `samsung`;
  если Samsung использует другой бренд/одиночный product-маркер, диспетчер
  всё равно уйдёт в generic).
- Есть ли на прошивке Galaxy XR манифест `/system/etc/openxr/*` у
  `libopenxr.google.so` (проверить: `ls /system/etc/openxr` в adb).
- Как ведут себя игры в режиме unmanaged (XR_ACTIVITY_START_MODE) на Galaxy XR.
- ОVRP-платформенный бэкенд (liBovrplatformloader) ориентирован на
  ovrp.crx.moe-сервис; для Galaxy XR, возможно, понадобится свой бэкенд
  (Samsung-аккаунт) — либо просто отключать.
- Стереопарадокс PFDMR (swap левый/правый eye) — проверить, есть ли он на
  Galaxy XR; если да — повторить `StereoPatch`-подход из PFDMR-форка.

## Форки как референс

- **PFDMR** ([urscaviezel/OVRPort-PFDMR](https://github.com/urscaviezel/OVRPort-PFDMR)) —
  единственный содержательный форк: собственный `forward.cpp`-шим OpenXR
  (dlopen системного `libopenxr.so`, re-export + фикс стерео), `requiresOverportConfig`.
  Изучен как шаблон; в Galaxy XR-форке собственный нативный шим пока не нужен
  (generic-лоадер рунтайма должен подхватить Google OpenXR сам).
- Остальные форки (tomcat-bj, TechnoHoschi, bitsk, xiaoyang00001) — замороженные
  зеркала, изменений нет.
