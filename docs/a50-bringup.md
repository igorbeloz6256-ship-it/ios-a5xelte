# A50 (SM-A505FN, Exynos 9610): порт XNU / iOS 16

Основа — ветка `herolte` (Galaxy S7). Она уже умеет: ядро XNU с 4 КиБ
страницами, Cortex-A53 без LSE, GICv2, 39-битные виртуальные адреса,
загрузку через Samsung S-Boot и вывод на экран копированием процессором.
У A50 загрузочное ядро тоже Cortex-A53, поэтому всё это переиспользуется.

Здесь **данные из исходников** отделены от **проверки на телефоне**.
Проверок на телефоне пока нет.

## Карта железа (из исходников, на телефоне не проверено)

Источники:
- ядро Samsung A50: `FreshROMs/android_kernel_samsung_exynos9610_mint`, ветка
  `android-14.0` (`exynos9610.dts`, `exynos9610-rmem.dtsi`,
  `samsung/exynos9610-a50_common.dtsi`, `display-lcd_a50_common.dtsi`,
  `drivers/samsung/sec_reboot.c`, `dpu20/cal_9610/regs-{decon,dpp}.h`);
- `ivoszbg/uniLoader`: `soc/exynos/exynos9610.c`, `board-m31.c`, `board-gta4xl.c`;
- черновик mainline DTS для Exynos 9610 (gta4xl, LKML, сентябрь 2025).

| Что | A50 | S7 herolte (для сравнения) |
| --- | --- | --- |
| Процессор | 4× Cortex-A53 (MPIDR 0..3) + 4× Cortex-A73 (0x100..0x103), оба ARMv8.0 без LSE | A53 0x100..0x103 + Mongoose 0..3 |
| SMP | PSCI через SMC, `cpu_on = 0xC4000003` | то же |
| GIC | GIC-400: distributor `0x12301000`, CPU `0x12302000` | `0x11001000` / `0x11002000` |
| Таймер | 26 МГц, виртуальный PPI 11 → **INTID 27** | то же |
| RAM (4 ГБ) | `0x80000000` 2 ГиБ + `0x880000000` 2 ГиБ | то же расположение |
| Резервы (DTS) | sec_debug `0x80000000` (4 КиБ), initrd по DTS `0x84000000`, ECT `0xA0000000` (0x3A000), VIPX `0xB8000000`, ABOX `0xE9400000`, модем `0xED000000`+0x8A00000, CP-лог `0xF5A00000`, GNSS `0xFB000000`, CHUB `0xFD000000`; плюс DSS/seclog (адреса в заголовках, неизвестны) | ESS `0x86000000`, ECT `0xA0000000`, ION `0xE0000000..` |
| Экран | EA8076, 1080×2340, **MIPI command mode**, аппаратный триггер по TE | 1440×2560 |
| Буфер S-Boot | **предположительно `0xca000000`**, ARGB8888, шаг 4320 (так у M31/gta4xl на 9610/9611). Probe читает настоящий адрес из `IDMA_IN_BASE_ADDR_Y` | `0xe2a00000` |
| DECON | DECON_F `0x148B0000`; записать `0x1281` в `+0x70` (HW_SW_TRIG_CONTROL), иначе панель не обновляется | `0x13960000`, `0x1010` |
| DPU DMA | GF `0x14884000`, VG0 `0x14883000`, G0 `0x14881000`, G1 `0x14882000`; адрес буфера `+0x40` | — |
| Хранилище | **UFS** (1 линия, gear 3): HCI `0x13520000`, vendor `0x13521100`, UniPro `0x13510000`, protector `0x13530000`, PHY `0x13524000`, IRQ SPI 157 | UFS 2.0 `0x155a0000` |
| PMU / перезагрузка | `0x11860000`: INFORM2 `+0x808` (байт 23:16 = `0x4E`), INFORM3 `+0x80C` = `0x12345674` (recovery), SWRESET `+0x400` = 1 | `0x105c0000`, 0x12345678/0x12345674 |
| Кнопки | Power GPA1-7, Vol+ GPA1-5, Vol− GPA1-6 (pinctrl alive `0x11850000`) | GPA2-4, GPA0-3/4, Home GPA0-5 |
| Тачскрин | **Melfas MSS100**, `0x48` на I2C_4 (`0x13870000`, старый s3c2440-I2C), IRQ GPA0-0, питание GPG3-1 | Samsung `sec_ts`, HSI2C |
| PMIC | **S2MPU09 по шине SPEEDY** `0x11a10000` | S2MPS16 по HSI2C |
| GPU | **Mali-G72 MP3 (Bifrost)** `0x11500000` | Mali-T880 (Midgard) |
| Wi-Fi | **Samsung SCSC (Maxwell)**, не Broadcom | Broadcom PCIe |
| USB | DWC3 `0x13200000` | — |

### Что из S7 переносится, а что нет

- **Переносится почти без изменений:** загрузчик XNU (`xnu_boot`), формат
  payload, 4 КиБ XNU, GICv2, таймер, PSCI, HFS-корень, AMFI/pthread/crypto
  патчи, раскладка «XNU в верхнем банке RAM».
- **Нужна адаптация (адреса и мелочи):** драйвер UFS (патч `0204` для Exynos
  UFS — хорошая база), кнопки (другие GPIO), DECON-мост для IOSurface,
  постоянный лог, перезагрузка.
- **Писать заново:** тачскрин Melfas MSS100 (I2C другого типа), PMIC через
  SPEEDY (для батареи и питания тача), Wi-Fi SCSC (очень долго), GPU Bifrost
  (работы S7 по Midgard напрямую не подходят).

## Этапы

1. **Probe** (`shim/a50/probe.*`), без XNU. Показывает на экране EL, адрес
   FDT, адрес загрузки, MIDR/MPIDR, поддержку 16 КиБ страниц, CNTFRQ,
   регистры DPU DMA (настоящий адрес экрана), initrd, банки памяти,
   `/memreserve/` и `/reserved-memory`. Через 90 с перезагружает в TWRP.
2. **Загрузчик XNU** (`shim/a50/xnu_boot.*`, на базе herolte): окно RAM
   выбирается по результатам этапа 1, вывод на экран. Проверяется сначала
   тестовой нагрузкой `paytest` вместо XNU (см. «Этап 2»).
3. **Конфигурация `A50` для XNU**: копия HEROLTE с константами A50 (экран
   `0xca000000` 1080×2340, шаг 4320, 5 блоков по 2 МиБ, ~403 ppi, GIC
   `0x12301000/0x12302000`), сначала `cpus=1`, потом 4 ядра A53.
4. **Корень iOS**: UFS только на чтение, потом запись только в USERDATA.
5. **Экран iOS**, кнопки, тачскрин Melfas.
6. Потом: 8 ядер, батарея (S2MPU09), звук, Wi-Fi, GPU.

## Этап 1: как проверить

Собрать (нужен любой aarch64 GCC, например Arm GNU Toolchain `aarch64-none-elf`):

```sh
python tools/a50-build-shim.py --cross <путь>/aarch64-none-elf-
```

В TWRP (терминал или `adb shell`) сохранить стоковый BOOT и его размер:

```sh
dd if=/dev/block/by-name/boot of=/sdcard/boot-stock.img
blockdev --getsize64 /dev/block/by-name/boot
```

Скопировать `boot-stock.img` на ПК (MTP в TWRP) и упаковать:

```sh
python tools/a50-pack-boot.py info boot-stock.img
python tools/a50-pack-boot.py pack boot-stock.img shim/a50/out/probe.Image probe-boot.img --partition-size <размер>
```

Прошить `probe-boot.img` из TWRP (Установка → Установить образ → Boot) и
перезагрузиться. Ожидается: белый текст на чёрных полосах поверх логотипа и
обратный отсчёт, потом TWRP. **Сфотографировать экран.** Вернуть Android:
прошить `boot-stock.img` в Boot.

Если завис без текста: Vol− + Power ~7 с (принудительная перезагрузка), сразу
Vol+ + Power — TWRP. S-Boot и режим Download не затрагиваются.

## Этап 2: загрузчик `xnu_boot` и тестовая нагрузка

Что выбрано по probe 1 (`A50_PROGRESS.md`, 2026-11-13), константы — в
`shim/a50/a50.h`:

- RAM для XNU: окно `0x90000000..0xb0000000` (как у herolte; дальше в
  `0xb8000000` прошивка VIPX) или второй банк `0x880000000..0x8fe600000`
  (выше — `EL2_EARLYMEM` гипервизора uH). Страница статуса — `0x8ffff000`
  (XNU пишет метки в `+0x100`). Перед записью загрузчик сам сверяет окно
  и страницу с `/memory`, `/reserved-memory` и `/memreserve/` из FDT
  телефона и при пересечении останавливается с именем региона на экране.
- Нагрузка SWEETXNU (`tools/mkpayload.py`, формат v1) приклеивается к концу
  `xnu_boot.Image` (`a50-pack-boot.py pack … --append`). S-Boot кладёт Image
  в `0x80240000`, поэтому Image с нагрузкой должен кончаться ниже
  `EL2_CODE` (`0x87000000`) — упаковщик это проверяет (≈ 109 МиБ).
- Вход в XNU как у herolte: MMU выключен, `x0` = физический адрес
  `boot_args`, сразу за ядром ADT и `boot_args` (Revision/Version 2).
- При ошибке загрузчик 30 с показывает причину и перезагружает в TWRP
  (`reboot.c`, та же последовательность PMU, что у probe).

Проверка без XNU: `paytest` (`shim/a50/paytest.*`) — «поддельное ядро»,
которое загрузчик копирует и запускает ровно как XNU. Оно сверяет на экране
PC, MMU, `boot_args`, ADT и командную строку и пишет `ALL CHECKS PASSED`,
потом через 60 с — TWRP.

```sh
python tools/a50-build-shim.py --cross <путь>/aarch64-none-elf-
python tools/a50-test-payload.py shim/a50/out/paytest.Image paytest-payload.bin
python tools/a50-pack-boot.py pack boot-stock.img shim/a50/out/xnu_boot.Image \
    loadertest-boot.img --append paytest-payload.bin --partition-size 57671680
powershell -File tools/a50-adb-boot.ps1 -Image loadertest-boot.img   # из TWRP
```

Ожидается сверху `A50 XNU LOADER 1`, `XNU RAM 0090000000 00000200 MIB`,
`ENTRY 0000000090004000`, `BOOTARGS 0000000090009000`, `JUMP TO XNU`, ниже
блок `PAYLOAD TEST 1: RUNNING` … `ALL CHECKS PASSED` и отсчёт.

## Правила безопасности

- Пишем **только BOOT**. EFS (IMEI), RECOVERY, загрузчики (BOTA/SBOOT),
  модем, PARAM не трогаем. Любую другую запись сначала согласовать.
- Стоковый BOOT, бэкап EFS из TWRP и полная прошивка для Odin хранятся вне
  телефона.
- Дампы, серийные номера и файлы Apple в git не кладём.
