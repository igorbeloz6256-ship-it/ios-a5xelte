# A50 progress (SM-A505FN/DS, Exynos 9610)

Факты с телефона записываются сюда, отдельно от данных из исходников
(`docs/a50-bringup.md`). Сборка, вывод на экран и поведение iOS — разные
утверждения.

## 2026-11-12

- Код: `shim/a50/` (probe: экран из регистров DPU DMA, разбор FDT,
  перезагрузка в recovery через PMU A50), `tools/a50-build-shim.py`,
  `tools/a50-pack-boot.py` (заголовки boot v0/v1/v2, старый формат Samsung).
- `a50-pack-boot.py`: тест на синтетических образах v0/v1/v2 проходит
  (секции на месте, `recovery_dtbo_offset` и SHA-1 id пересчитаны, AVB-футер
  убран). На настоящем BOOT от A50 ещё не запускался.
- Probe собран Arm GNU Toolchain 15.2.Rel1 (`aarch64-none-elf`, SHA-256 архива
  сверен с сайтом Arm): `shim/a50/out/probe.Image`, 14016 байт, без
  предупреждений при `-Werror`, без ABS64-релокаций; в дизассемблере вызовы
  только своих функций, нет hvc/smc/tlbi/записи в SCTLR. На телефоне и в
  эмуляторе не запускался.

- `tools/a50-odin-tar.py`: пустой vbmeta с flags=2 и ustar-архив для слота AP
  Odin (+ `.tar.md5`), отказывается паковать что-то кроме
  recovery/boot/vbmeta. Тесты формата проходят. Сгенерирован
  `E:\iOS-A50\flash\vbmeta.img` (4096 байт, sha256 `d2e19797…b442`).
- Инструкция: `docs/a50-unlock-twrp.md` (TWRP 3.7.0_11-0 a505f с dl.twrp.me).
- Пользователь разблокировал загрузчик; в Download: `KG STATE: Checking`.
- TWRP `twrp-3.7.0_11-0-a505f.img` скачан с dl.twrp.me, sha256 `352bb21e…2e02`
  совпал с опубликованным. `a50-pack-boot.py info` на нём (первая проверка на
  настоящем образе семейства A50): header v2, page 2048, kernel raw arm64 Image
  (text_offset 0x80000, flags 0xa), kernel_addr 0x10008000, recovery_dtbo
  654424 байт, dtb 134468 байт, разбор доходит точно до конца файла.
- Собран `E:\iOS-A50\flash\twrp-vbmeta.tar.md5` (recovery.img + vbmeta.img).
- Пользователь прошил его Odin (AP, Auto Reboot выкл.) → PASS.

## 2026-11-13

- После прошивки: цикл «логотип → предупреждение → логотип с красным “not
  running Samsung's official software”» и без кнопок, и с Vol+ + Power. Вход в
  TWRP не удаётся. Монитор USB (опрос раз в 0,7 с, ~10 мин) не увидел ни
  одного устройства Samsung за это время (кабель, по словам пользователя,
  подключён) — вероятно, загрузчик не запускает ни ядро Android, ни TWRP.
- Причина по XDA (findroid2000, 2023-08): новый патч безопасности Samsung не
  даёт A505FN грузить своё рекавери. Решение: полная прошивка `A505FNXXS9CVJ2`
  через Odin, затем снова TWRP (см. `docs/a50-unlock-twrp.md`).
- Пользователь скачал `A505FNXXS9CVJ2` (samfw, CSC `OMC_MET`, CP
  `A505FXXU9CVG1`) в `E:\iOS-A50\proshivka\SAMFW.COM_SM-A505FN_MET_A505FNXXS9CVJ2_fac`.
  MD5-трейлеры всех пяти `.tar.md5` сходятся. AP: boot, recovery, dt, dtbo,
  system, vendor, userdata, vbmeta; BL: sboot, param, up_param, cm,
  keystorage, uh, vbmeta; CSC содержит PIT `A50_SWA_OPEN.pit`.
  CL24323734 совпадает с номером в версии ядра на телефоне до прошивки
  (`4.14.113-24323734`).
- После отката на `A505FNXXS9CVJ2` и повторной прошивки `twrp-vbmeta.tar.md5`
  пользователь сообщил: TWRP загрузился (экран «Keep System Read only?»).
  Значит, цикл был из-за прошивки `PUU9CWB4`, а не из-за образа TWRP или vbmeta.
  На этом экране ПК телефон по USB не видел (ни adb, ни устройств Samsung).
- В главном меню TWRP (MTP включён, кабель переткнут) за ~10 мин на USB не
  появилось ни одного нового устройства, даже с ошибкой. Тот же кабель и
  порт в режиме Download: `SAMSUNG Mobile USB CDC Composite Device` + `Modem`
  появились сразу, Odin видит. Значит, кабель и драйверы в порядке, а USB не
  поднимает TWRP 3.7.0_11-0 a505f.
- Почему (getprop в терминале TWRP + разбор рамдиска образа): сборка
  `3.7.0_11-GiovanYCringe`, ядро `4.14.282-Bocchi-Kernel` (MUIC/CCIC SM5713 —
  как в DTS A50). `sys.usb.configfs=0`, `sys.usb.config=mtp,adb`,
  `sys.usb.state=mtp,adb`, `init.svc.adbd=running`, `sys.usb.ffs.mtp.ready=0`.
  В рамдиске для `configfs=0` есть правила только для `adb`/`sideload`/
  `fastboot`/`none` (пишут `/sys/class/android_usb/android0/enable`), а для
  `mtp,adb` — только `start adbd`. TWRP при включении MTP ставит `none` →
  `mtp,adb`, и USB остаётся выключенным. Ожидаемый обход: MTP выключить
  (`sys.usb.config=adb`).
- `setprop sys.usb.config adb` в терминале TWRP USB не поднял (15 мин
  наблюдения — пусто); короткие команды терминал выводит без результата.
  Ядро (IKCONFIG): `USB_CONFIGFS=y`, `USB_CONFIGFS_F_FS=y`, UDC `13200000.dwc3`.
- Собран `E:\iOS-A50\flash\twrp-usbfix.tar.md5` (только recovery.img, sha256
  `23394d2d…8c62`): в рамдиске `sys.usb.configfs 1` и правило, которое для
  `mtp,adb` и `adb` привязывает к UDC только `ffs.adb`. Ядро, dtb,
  recovery_dtbo побайтно как у оригинала; cpio-писатель проверен обратной
  сборкой исходного архива.
- Пользователь прошил `twrp-usbfix.tar.md5` (Odin, AP) и вошёл в TWRP: ПК
  сразу увидел `Samsung Galaxy A50`, `adb devices` → `recovery`, shell root
  (`u:r:su:s0`), `sys.usb.configfs=1`. **Исправление USB работает** (только
  adb; MTP в этом TWRP по-прежнему нет).
- Бэкап (dd в `/tmp` TWRP → `adb pull`), `E:\iOS-A50\backup\2026-11-13\`
  (вне git, содержит IMEI — никуда не выкладывать): efs, sec_efs, cpefs
  (`sdd1`), param, up_param, persistent, steady, keystorage, boot, dtb, dtbo,
  uhcfg — 12 образов, 148,9 МБ, SHA-256 на телефоне и на ПК совпали у всех
  (`SHA256SUMS` рядом). efs — ext4 (магия `53ef`). Копии в `/tmp` телефона
  удалены.
- Стоковый BOOT `A505FNXXS9CVJ2` (раздел 57671680 байт): header **v1**, page
  2048, kernel raw arm64 Image 30535692 байт (text_offset 0x80000,
  image_size 0x2976000, flags 0xa), **ramdisk пустой**, cmdline пустой,
  kernel_addr 0x10008000, tags 0x10000100; SEANDROIDENFORCE и AVB-футер есть.
  `a50-pack-boot.py pack` теперь разрешает пустой ramdisk, если его нет и в
  стоковом образе.
- Probe пересобран (14016 байт) и упакован в заголовок стокового BOOT:
  `E:\iOS-A50\flash\probe1\boot.img`, 16400 байт, sha256 `c90be3b8…4c32`,
  header v1, arm64 header text_offset 0x80000 / image_size 0x14000 / flags
  0xa, SEANDROIDENFORCE в конце. Остаток раздела (старое ядро, AVB-футер) не
  трогается — так же, как Odin оставил хвост в RECOVERY под TWRP.
- `tools/a50-adb-boot.ps1`: пишет образ только в BOOT из TWRP (push →
  sha256 на телефоне → dd → чтение обратно `head -c N | sha256sum`), не
  перезагружает. Проверка формата и чтения проверена без записи: `head -c
  16400` стокового BOOT на телефоне = те же байты бэкапа на ПК.

### Probe 1 на телефоне (15:28–15:31)

- `a50-adb-boot.ps1` записал `probe1\boot.img` в BOOT, чтение обратно
  совпало; `adb reboot` в 15:28:48. **Probe запустился**: текст поверх
  экрана S-Boot (красное предупреждение и «Powered by android» остались
  под ним), отсчёт `TWRP IN 88 S` на фото. В 15:31:02 телефон сам вернулся
  в TWRP по adb (`/proc/reset_reason` = `RPON`) — перезагрузка в рекавери
  через PMU INFORM2/INFORM3 + SWRESET работает. BOOT после этого всё ещё
  probe (sha256 первых 16400 байт = `c90be3b8…`).
- Значения с фото (цифры мелкие, но читаются):

  | Строка | Значение | Смысл |
  |---|---|---|
  | EL | 1 | ядро стартует в EL1 (EL2 занят uH) |
  | SCTLR | `0x30C5083A` | MMU выкл., кэши выкл., **A=1 (проверка выравнивания включена)** |
  | FDT | `0x8A000000` | DTB от S-Boot в x0 |
  | IMAGE | `0x80240000` | куда S-Boot положил Image (по фото) |
  | MIDR | `0x410FD034` | загрузочное ядро — Cortex-A53 r0p4 |
  | MPIDR | `0x80000000` | CPU0, кластер 0 |
  | MMFR0 | `0x1122` | PA 40 бит, ASID 16, **TGran16 = 0 → 16K страниц нет**, 4K и 64K есть |
  | PFR0 | `0x2222` | EL0–EL3 AArch64+AArch32, нет системного GICv3 (GIC-400) |
  | ISAR0 | `0x11120` | AES+PMULL, SHA1, SHA2, CRC32, без LSE |
  | CNTFRQ | `0x18CBA80` | 26 МГц |
  | TRIG OLD | `0x3010` | DECON_F HW_SW_TRIG_CONTROL до записи 0x1281 |
  | FB | `0xCA000000`, 1080×2340, pitch 0x438 | найден **по IDMA-каналу 2** («FROM DMA») |
  | DMA2 | base `0xCA000000`, src/img `0x0924_0438`, enable `0x4` | DMA0/1/3 нули |
  | INITRD | `0x89000000`–`0x89000000` | пустой (ramdisk нет) |
  | MEM00–03 | `0x80000000+0x3AB00000`, `0xC0000000+0x20000000`, `0xE1900000+0x1E700000`, `0x880000000+0x80000000` | ≈3,9 ГБ, как и предполагалось по gta4xl |
  | RSV | — | /memreserve/ нет |

  `/reserved-memory` (RM00–RM10): SECLOG_MEM `0xC0000000+0x80000`,
  ABOX_RMEM `0xE9400000+0x2800000`, SEC_DEBUG_MA(GIC) `0x80000000+0x1000`,
  MODEM_IF `0xED000000+0x8A00000`, CP_RAM_LOGGI(NG) `0xF5A00000+0x1E00000`,
  CHUB_RMEM `0xFD000000+0x80000`, GNSS_IF `0xFB000000+0x800000`,
  VIPX_FW_RMEM `0xB8000000+0x500000`, **EL2_CODE `0x87000000+0x200000`**,
  **EL2_EARLYMEM `0x8FE600000+0x1A00000`**, KASLR `0x80001000+0x1000`.
- Выводы для XNU: нужна сборка с **4K страницами** (16K на A53 нет); весь
  код до MMU — с `-mstrict-align` (SCTLR.A=1); регионы uH (EL2_CODE,
  EL2_EARLYMEM) и модема исключить из памяти XNU; экран — готовый буфер
  `0xCA000000` 1080×2340 на IDMA2, вывод текста в него работает без
  перенастройки DECON.
- Загрузчик `shim/a50/xnu_boot.*` (на базе herolte) и тестовая нагрузка
  `shim/a50/paytest.*` собраны без предупреждений (`xnu_boot.Image` 15808
  байт, `paytest.Image` 12096 байт), без ABS64; в дизассемблере из
  системных инструкций только намеренный `msr sctlr_el1` перед прыжком.
  `tools/a50-test-payload.py` → `E:\iOS-A50\flash\loadertest1\payload.bin`
  (13072 байта), `a50-pack-boot.py pack … --append` →
  `loadertest1\boot.img` (32784 байта, sha256 `3deca9d4…cd1b`). Хост-скрипт
  повторил все проверки `xnu_boot.c` на этом образе с картой памяти из
  probe 1: нагрузка `0x80244000..0x80258000`, вход `0x90004000`, ADT
  `0x90008000`, boot_args `0x90009000`. На телефоне ещё не запускался.
- `a50.h`: `A50_DRAM_HIGH_END` исправлен на `0x900000000` (probe 1).

### Тест загрузчика на телефоне (15:45)

- `loadertest1\boot.img` записан в BOOT (`a50-adb-boot.ps1`, чтение обратно
  совпало), `adb reboot` в 15:45:02. Фото экрана: загрузчик `A50 XNU
  LOADER 1` → `XNU RAM 0090000000 00000200 MIB` → `ENTRY
  0000000090004000` → `BOOTARGS 0000000090009000` → `JUMP TO XNU`; затем
  paytest: EL 1, SCTLR `0x30C5083A`, PC `0x90004000`, REV/VER 2/2,
  VIRTBASE `0xFFFFFFF006000000`, PHYS/MEM `0x90000000`/512 МиБ, TOPKDATA
  `0x9000A000`, ADT `0x90008000`+0x100; REVISION / PC / MMU OFF / ADT /
  CMDLINE / TOPKDATA — все OK, **`ALL CHECKS PASSED`**, отсчёт `TWRP IN 59 S`.
  Все адреса совпали с хост-прогоном `simulate_loader.py`.
- Значит, на телефоне подтверждено: нагрузка, приклеенная к Image, доходит
  до загрузчика целиком; окно `0x90000000+512 МиБ` и страница `0x8ffff000`
  не пересекаются ни с одним регионом FDT телефона (проверка в
  `check_ram` прошла); EL1 под uH может писать в это окно и исполнять из
  него код с выключенным MMU; передача `boot_args`/ADT/командной строки — в
  формате, который ждёт XNU.
- Работа переехала в форк `E:\iOS-A50\github` (ветка `a50` от `herolte`
  `ff2c55e`). `a50-build-shim.py` в форке собирает `xnu_boot.Image`
  побайтно таким же, как проверенный на телефоне. Сейчас в BOOT телефона
  стоит тест загрузчика (`loadertest1`), а не probe.
- Ядро: локально собрано дерево `xnu-8796.141.3` + патчи форка (LF, как в
  облаке): 254 из 256 ложатся (`0031`, `0041` ждут HFS от `0026-stage-hfs.sh`).
  В патчах S7 больше 50 фиксированных адресов Exynos 8890 (CMU, PMU, GPIO,
  HSI2C, DECON, Mali, аудио, PCIe Wi-Fi, TMU). Все вызовы драйверов S7 из
  общего кода: `IOKitBSDInit.cpp` (кнопки, тач, GPU, UFS), личности
  `SweetBattery`/`HeroGPU`, `bsd_init.c` (drop zone, Wi-Fi),
  `SweetFramebuffer.cpp` (DECON vsync, Mali present — только при
  `hfb_state`/boot-args). Потоки драйверов стартуют только из этих мест.
- `xnu/patches/0400-a50-board-target.patch`: `MACHINE_CONFIG=A50` (HEROLTE +
  `ARM64_BOARD_CONFIG_A50`), эти вызовы отключены для A50, константы экрана
  и MPIDR A50; `0019`/`0020`/`0026` и `xnu.yml` знают A50 (пуш в `a50` →
  `kernel.development.a50`). На LF-дереве накладывается чисто.
- Коммит `0f10725` отправлен, Actions `xnu` (run 43437740240,
  macos-15-intel) запустился в 13:54 UTC.
- Run 43437740240 — **success** (13:54–14:08 UTC): все шаги зелёные, в т.ч.
  проверка на LSE (она идёт только по существующему
  `kernel.development.a50`). Артефакты: `xnu-kernel-macos-15-intel`
  (62,7 МБ zip), `xnu-buildtree-macos-15-intel`. Значит, патч `0400`
  компилируется с первого раза.
- Пользователь скачал артефакт; `kernel.development.a50` 17402752 байта,
  SHA256SUMS из артефакта совпал. Mach-O arm64 (cputype 0x100000c, не
  arm64e), без chained fixups, вход `0xfffffff00743d488`, `__TEXT` с
  `0xfffffff007004000`, конец ядра `0xfffffff008528000`.
- `E:\iOS-A50\flash\xnu1\`: `a50.adt` (4264 байта, без `/product`),
  `payload.bin` (16769056 байт, cmdline `debug=0xc serial=3 cpus=1
  keepsyms=1 -enable_kprintf_spam -noprogress`), `boot.img` (16789520 байт,
  sha256 `08964e35…aa48`). Хост-прогон проверок загрузчика: нагрузка
  `0x80244000..0x81253000` (ниже EL2_CODE), 10 сегментов в
  `0x91004000..0x92528000`, вход `0x9143d488`, ADT `0x92528000`, boot_args
  `0x9252a000` — всё проходит.

### Первый запуск XNU на A50 (19:15)

- `xnu1\boot.img` записан в BOOT (чтение обратно совпало), `adb reboot` в
  19:15:26. **XNU загрузился до ожидания корневого раздела.** Цветных
  блоков пользователь не заметил (их сразу перекрывает консоль), но ранняя
  консоль ядра на экране A50 работает: два фото, строки (с фото):
  - `SWEET FB: AppleCLCD 1080x2340 stride 4320 at 0xca000000, default
    surface 1, presented as 1125x2436`; `A5XELTE FB: requested 30 Hz,
    effective 30 Hz`; `parallel copy with 3 helper threads`;
  - `zone_init: map GEN3 … DATA … metadata …`, `VM bootstrap done`,
    `IOKit IOMD setownership ENABLED`, `Scheduler: Default of clutch`,
    `Scheduler: SWEET fast cores 0xf0`, `IPC_PTHREAD_PRIORITY subsystem`;
  - `debug_log_init: Error!! gPanicBase is still not initialized` (ожидаемо:
    PLATFORM_PANIC_LOG_DISABLED), `Initializing serial KDP`, `iBoot version:
    a50-entry`;
  - `SWEET PRNG: HMAC-SHA256 registered`, `A5XELTE pthread:
    libpthread-514.100.2 registered`, `SWEET AMFI`, `vnguard`, `SWEET:
    trust caches unavailable`, `A5XELTE: starting device-tree platform
    expert`, `SWEET AKS`, `SWEET WDT`, `SWEET SURF`, `SWEET HID: buttons /
    touch registered`;
  - `ml_processor_register … cluster_id 0 cpu_number 0 is type 0`,
    `A5XELTE CPU: requested=1 selected=1 (limit=1)`, `cpu=0 entered initCPU
    mpidr=0x0 EL1 IRQs=1`, `cpu=0 mpidr=0x0 came alive; SGI1 OK`, `1/1 CPUs
    initialized, SGI1 verified`, `timer test cpu=0 woke on 0 after 21070 us`
    — **GIC-400 по 0x12301000/0x12302000 и виртуальный таймер (INTID 27)
    работают под uH**;
  - `MAC Framework successfully initialized`, `SWEET HFS: hfs-627.100.6
    registered`, `mcache: 1 CPU(s)`, `mbinit: done (64 MB total pool size)`,
    `DLIL: running SUM16 self-tests … PASSED`, `Skywalk: running
    self-tests` — все `PASSED`, `bsd_autoconf: calling kminit`,
    `ifnet_attach` lo0/gif0/stf0;
  - последняя строка: `Waiting on <dict ID="0"><key>IOProviderClass</key>
    <string ID="1">IOMedia</string><key>Content</key><string
    ID="2">Apple_HFS</string></dict>` — ядро ждёт корневой раздел HFS
    (драйвера UFS для A50 нет, и так и задумано).
- Пользователь перезагрузил телефон кнопками; в 19:18:54 TWRP снова по
  adb. USB под XNU не появлялся (драйвера нет). `/dev/mem` в TWRP нет —
  страницу меток `0x8ffff000` прочитать нельзя.
- Следующее: драйвер UFS для Exynos 9610 (`0x13520000`, по образцу
  `bsd/dev/herolte_ufs.c`), затем корень iOS в USERDATA.

### UFS для A50 (патч 0401)

- Отличия 9610 от 8890 по DTS и исходникам ядра A50 (FreshROMs,
  `ufs-exynos.h`, `ufshci.h`, `ufs-exynos.c`): вендорский блок HCI — reg 1
  `0x13521100` (`+0x1100`, у 8890 `+0x100`), смещения в нём те же (TXPRDT
  0x00, RXPRDT 0x04, UTRL_NEXUS_TYPE 0x40); квирки `PRDT_BYTE_GRAN`,
  `BROKEN_REQ_LIST_CLR`; с `CONFIG_SCSI_UFS_EXYNOS_FMP` запись PRDT —
  128 байт (des4..des31 — IV и ключи FMP), без него — 16. Как загрузчик
  оставил FMP, заранее неизвестно.
- `xnu/patches/0401-a50-ufs.patch`: `herolte_ufs.c` для A50 — узел ADT
  `/a50-ufs` (`0x13520000`, карта 0x2000), смещение вендорских регистров
  `0x1100`, раздел по умолчанию `userdata`, шаг PRDT 16/128. Шаг
  измеряется одним чтением LBA 1–2: вторая запись лежит и на байте 16
  (→ буфер +4 КиБ), и на байте 128 (→ +8 КиБ), остальные слоты → +12 КиБ,
  длина 256 байт — DMA при любом шаге остаётся в своём буфере, по
  заполненному слоту видно шаг. Без успешного измерения дальше не читает.
  Для S7 поведение то же (шаг 16, раскладка DMA прежняя). `IOKitBSDInit`
  для A50 вызывает только UFS (`ufsprobe=1` — только чтение; `rd=ufsroot0`).
- `mkadt_a50.py`: узел `a50-ufs` reg `0x13520000`+`0x2000`.
- На LF-дереве `0400`+`0401` накладываются чисто. Коммит `8079f96`, run
  43465565315 — **success** (16:43–17:17 UTC, проверка LSE пройдена),
  артефакт `xnu-kernel-macos-15-intel` 62,7 МБ.
- `E:\iOS-A50\flash\xnu2\`: ядро 17402880 байт (SHA256SUMS OK, строки
  `PRDT stride probe` и `/a50-ufs` внутри), `a50.adt` 4368 байт (с
  `a50-ufs`), `payload.bin` с cmdline `… -noprogress ufsprobe=1`,
  `boot.img` 16789520 байт, sha256 `b2a33f4b…e508`; хост-прогон загрузчика
  проходит (вход `0x9143d488`, ADT `0x92528000`).

### Проверка UFS на телефоне (20:58, `ufsprobe=1`, только чтение)

- Записан `xnu2\boot.img` (чтение обратно совпало), `adb reboot` 20:58:37.
  XNU загрузился как в первый раз (пользователь заметил мелькнувший синий
  блок — метку MMU), затем блок драйвера UFS (с фото):
  - `probe: cap 0x1303ff0f ver 0x210 hcs 0x10f hce 0x1 is 0x0 utrlba
    0x0:…ae40 dbr 0x0 rsr 0x1` — UFSHCI 2.1, хост включён, линк и списки
    готовы (S-Boot оставил хост рабочим);
  - `txprdt 0xc rxprdt 0xc nexus 0xffff` — PRDT_SET_SIZE(12), без prefetch;
  - W-LUN 208 TEST UNIT READY: `ocs 0x7 … status 0x2 sense key 0x2 asc
    0x4 ascq 0x2` (нужна команда инициализации — ожидаемо), затем `START
    STOP UNIT (ACTIVE) 0`; TEST UNIT READY LUN 0–3 — все 0;
  - **`PRDT stride probe: LBA1 EFI PART, 16-byte slot filled, 128-byte slot
    empty, spare empty` → `host walks 16-byte PRDT entries`** (S-Boot
    оставил обычные 16-байтные записи);
  - `READ(10) LBA1 0, signature EFI PART`, `GPT entries 128 x 128 at LBA 2`
    (4 блока одной командой — многоблочное чтение работает);
  - GPT LUN 0, 32 раздела (в блоках по 4 КиБ): p3 efs 4096+5120 (20 МиБ),
    p4 param, p5 up_param, p6 keystorage, p7 sec_efs, p9 steady, p10 uh,
    p11 uhcfg, p12 dtb, p13 dtbo, **p14 boot 27264+14080 (55 МиБ — ровно
    57671680 байт)**, p15 recovery 41344+16512 (64 МиБ), p16 radio, …,
    p23 vbmeta 83584+16, p25 system 83968+1356800 (5300 МиБ), p26 vendor,
    p27 product, p28 cache, p29 omr, p30 cp_debug, p31 nad_fw,
    **p32 userdata 1869568+13745152 (53692 МиБ)** — модель на 64 ГБ; размеры
    совпадают с `blockdev` из TWRP;
  - `probe done (0)`, затем, как и ожидалось, `Waiting on … IOMedia …
    Apple_HFS`.
- Пользователь вернул телефон в TWRP кнопками. Записей на UFS не было.

### Корень iOS (2026-11-14)

- `ios-root.yml`: `runs-on: macos-14` больше не работает ("The configuration
  'macos-14-arm64-us-xl' is not supported") → `macos-15-intel`. Пользователь
  импортировал форк в приватный репозиторий, поправил строку там, сборка
  `ios-root` прошла, релиз скачан. Частей четыре (`part00..03`), все
  SHA-256 из релиза совпали, `d22ap.adt` 186084 байта — тоже.
- Образ: HFSX (`HX`, версия 5), блок 4096, 5242880 блоков = 20480 МиБ,
  занято 11430 МиБ, 188517 файлов, 134240 папок.
- `tools/herolte-flash-root.py`: новый `--dev` (для A50
  `/dev/block/by-name/userdata`, по умолчанию прежний путь S7).
- В TWRP: `userdata` → `sda32`, 56300142592 байта (= 53692 МиБ, как в GPT
  под XNU); **TWRP держит его смонтированным как ext4** на `/data` и
  `/sdcard` — перед записью размонтировать.
- `E:\iOS-A50\flash\xnu3\`: `a50.adt` 7056 байт (с `/product` iPhone X:
  product-name "iPhone X", artwork-device-subtype 2436, scale 3, sRGB),
  `boot.img` 16791568 байт, sha256 `eba8628a…6041`, cmdline `… rd=ufsroot0
  ufsroot=userdata cs_enforcement_disable=1 … wdt=-1 -wdt_no_pmu …` —
  **корень только на чтение** (без `ufsrw`/`rootrw`) для первой проверки.
- Пользователь подтвердил стирание userdata. `/sdcard` и `/data`
  отмонтированы. Первый запуск `herolte-flash-root.py --dev
  /dev/block/by-name/userdata`: кусок 0 записан и сверен, затем обнуление
  `dd … seek=64 count=640 conv=fsync` упало — toybox dd в этом TWRP делает
  `ftruncate` на блочном устройстве: `dd: ftruncate: Invalid argument`.
  Исправлено `conv=notrunc,fsync` (проверено на 1 МиБ: rc=0, sha256 нулей
  совпал); запись продолжена с файла состояния, ~41–57 МиБ/с.
- Готово: `done: 20480 MiB image, 192 chunks written now, 1 from a previous
  run, 127 zero chunks cleared` (320 кусков по 64 МиБ, каждый ненулевой
  сверен чтением обратно); sha256 распакованного образа
  `27f97e71…0c61`. На телефоне по смещению 1024 `userdata` — `HX\0\5`.
- `xnu3\boot.img` записан в BOOT (чтение обратно совпало), `sync`, `adb
  reboot` 14:14:25.

### Первая загрузка iOS userland (14:14, корень только на чтение)

- С фото (экран повёрнут): `"userdata" … read-only, DMA at 0x9357f000`,
  `root: ufsroot0`, `A5XELTE: root volume is not authenticated (error …)
  continuing un…`, `load_init_program: attempting to load /sbin/launchd`,
  **`Darwin Bootstrapper Version 7.0.0 … libignition_core`** — таблица
  libignition: program launchd, rd ufsroot0, hardware target d22ap,
  boot spec local, stage count 7; `hello from launchd.1`; preboot /
  cryptex1 / graft пропущены (`ignition level insufficient: actual = 1`);
  `dyld_cache: opened shared cache directory:
  /System/Library/Caches/com.apple.dyld`; `goodbye: ignition sequence
  complete`.
- Потом `com.apple.xpc.launchd[1]` выполняет boot-task'и: `fsck: Skipping
  fsck on OS const…`, `exec mount_hfs`, **`GetMasterBlock: Error 16 opening
  /dev/ufsroot0`, `mount_hfs: error on mount(): error = -1`, `mount_hfs:
  Read-only file system`**, `mount: / failed with 1`, `ERROR: BindFS …
  /private/preboot/<hash>/usr/standalone/firmware`, `pid 3 exited … exit 1`,
  **`panic(cpu 0 caller …): mount(3) exited -- no exit reason available --
  (signal 0, exit status 1)`**. Ожидаемо: корень без `ufsrw=1 rootrw=1`
  (у herolte в рабочей строке они есть), а iOS перемонтирует `/` на запись.
- Пользователь вернул телефон в TWRP.
- `E:\iOS-A50\flash\xnu4\boot.img` (sha256 `9fbbc272…98c4`): то же ядро и
  ADT, cmdline + `ufsrw=1 rootrw=1` (запись только внутри раздела
  `userdata`, проверка границ в `ufs_write_blocks`; с записью включается и
  herolog — кольцо консоли в последних 4 МиБ раздела, за томом HFSX).

### Корень на запись (14:23, `ufsrw=1 rootrw=1`)

- `xnu4\boot.img` записан (чтение обратно совпало), reboot 14:23:44.
  **Паники нет, iOS userland работает больше 20 минут.** На 20 фото
  консоль: `launchd` запускает и перезапускает процессы — `xpcproxy`,
  `fairplayd.H2`, `bluetoothd` (циклом выходит: Bluetooth нет), виджеты
  (`WeatherPoster`, `FitnessWidget`, `WorldClockWidget`,
  `ScreenTimeWidget`, `RemindersWidget`, `LockScreenPeople`,
  `ExtragalacticPoster` …) падают с `extension check-in timeout`, `Too many
  corpses being created`; сэмплер (`sweet_sample.c`) печатает стеки
  `SpringBoard t12 pc …` и `backboardd t4 pc …` — **SpringBoard и
  backboardd живы**; строки `HEROLTE LOG: alive N s, MB free … active …
  inactive …` — **журнал на UFS пишется**; `HEROLTE FB: frames=… ` —
  AppleCLCD получает кадры. Экран занят только текстом консоли.
- `tools/herolte-log-pull.py`: необязательный третий аргумент DEV (A50:
  `/dev/block/by-name/userdata`).
- Журнал из TWRP (`E:\iOS-A50\flash\logs\boot-xnu4.txt`, вне git): flush
  #151, 281952 байта, 6172 строки, ~25 мин работы. Итог:
  - **backboardd рисует и отдаёт кадры**: `SWEET FB: display on`, IOSurface
    1080×2340 и 1125×2436, `SWEET FB: swap 3 … swap 49 … surface 3
    1125x2436`; `A5XELTE FB: frames=… avg_copy_us≈5900` (кадр 10,1 МБ на
    панель за ~6 мс, 0,1–0,9 кадра/с). На экране их перекрывает консоль
    ядра (без `a5fb_noconsole=1` она не глушится);
  - SpringBoard жив (250 строк сэмплера, потоки в ожидании);
  - циклы: `xpcproxy` 695 раз, `bluetoothd` 145 (нет Bluetooth),
    `fairplayd.H2` 141; ~25 виджетов «Too many corpses»; `SPAWN FAILED` у
    синтезаторов речи (Kona/MacinTalk/SiriTTS), `HorizonSetup`,
    `/private/var/herolte/autorun` (drop zone на A50 выключен — ожидаемо);
  - память: `MB free 21–41` при 512 МиБ — почти вся занята.
- `E:\iOS-A50\flash\xnu5\boot.img` (sha256 `11600c85…0248`): то же ядро,
  ADT `--bank2` — **XNU RAM `0x880000000`+2022 МиБ** (второй банк до
  EL2_EARLYMEM), cmdline + `a5fb_keep_on=1 a5fb_noconsole=1
  vm_compression_limit=98304 vm_compressor=2 heronoexec=vmd,bluetoothd,…`
  (как у herolte, без GPU-аргументов). Хост-прогон: вход `0x88143d488`,
  ADT `0x882528000`, boot_args `0x88252a000` — проходит.

### 2 ГБ RAM (14:54, xnu5) и дальше

- xnu5 записан (чтение обратно совпало), reboot 14:54:01. Пользователь:
  **чёрный экран** (консоль заглушена после первого кадра, как и задумано).
  Журнал `logs\boot-xnu5.txt` (flush #19, ~3 мин, 2645 строк): RAM
  больше (`MB free 445 active 701` на 60 с), `SWEET FB: display on`,
  IOSurface 1080×2340 и 1125×2436, `swap 3 … ` (16 записей) поверхности 3
  1125×2436; `heronoexec` работает (`SPAWN FAILED … bluetoothd`, `homed`,
  `cloudd`, … — так и задумано).
- Причина чёрного — по `HEROLTE_PROGRESS.md` (строки 137–179): (1) без
  `sweetfb_native=1` презентуется 1125×2436, а поверхность по умолчанию —
  размер панели, кадры с содержимым не приходят; у S7 с `sweetfb_native=1`
  появились логотип Apple и экран «Привет»; (2) «с одним A53 первый кадр не
  заканчивается» (рендер CA на CPU) — у S7 помогли 4 ядра.
- `xnu6\boot.img` (sha256 `7a3bcdf7…4b71`): xnu5 + `sweetfb_native=1`,
  cpus=1 — проверка (1) без пересборки.
- DTS A50: A53 — `reg` 0x0..0x3 (cpu@100..103 в DTS, Linux cpu0..3), A73 —
  0x100..0x103; PSCI `method = "smc"`, `cpu_on = <0xC4000003>` — как у S7.
- `xnu/patches/0402-a50-smp.patch`: для A50 `SWEET_SMP_MAX_CPUS 8`,
  `A5XELTE_FAST_MPIDR 0x100`, имя «Cortex-A73» (у S7 0x0 и «Mongoose»);
  `SweetCPU.cpp` берёт второй кластер из `A5XELTE_FAST_MPIDR`. Трамплин
  SMP `0x8fffe000` (свободен в FDT A50). На LF-дереве ложится поверх 0401.
- `mkadt_a50.py`: 8 узлов CPU (A73 — с `arm,cortex-a73` и
  `arm,cortex-a53`). Только с ядром 0402; `cpus=1` ограничивает топологию.

### 🍎 Экран блокировки iOS на A50 (15:05, xnu6, `sweetfb_native=1`, 1 CPU)

- xnu6 записан (чтение обратно совпало), reboot 15:05:45. Пользователь:
  **сначала логотип Apple, потом экран блокировки iOS** (фото): «Wednesday,
  December 31», «4:03», кнопка камеры, полоска «домой», строка состояния;
  фон оранжево-красный (возможно, перепутаны каналы R/B — проверить).
  Даты нет (RTC не подключён).
- Журнал `logs\boot-xnu6.txt` (flush #40, ~6 мин): `SWEET FB: AppleCLCD
  1080x2340 … presented as 1080x2340`, `display on`, 18 свопов поверхностей
  2/3 1080×2340 `BGRA`, копия кадра на панель ~5,2 мс, 0,1–0,5 кадра/с;
  `MB free 24–31 active ~915 inactive ~600` на 360 с.
- Коммит `c2ce8e9` (0402 SMP, инструменты A50) отправлен, сборка xnu
  43594040830 — success (12:17–12:34 UTC).
- `xnu7`: ядро 17386496 байт (SHA256SUMS OK, строка `Cortex-A73` внутри),
  ADT `--bank2` с 8 CPU (12412 байт), cmdline как xnu6, но `cpus=4` (4 A53,
  как первый SMP-шаг S7). `boot.img` sha256 `8f14a903…67e1`, хост-прогон
  проходит (boot_args `0x88252c000`).
- xnu7 записан, reboot 16:41:50. Пользователь: **экран блокировки за ~1,5
  мин**. Журнал `logs\boot-xnu7.txt`: `requested=4 selected=4 (limit=8)`;
  для cpu1..3 `entry 0x88143f000 via trampoline 0x8fffe000`, `PSCI CPU_ON
  rc=0`, `mpidr=0x1..0x3 came alive; SGI1 OK`, `midr=0x410fd034`,
  `vct_lag≈17500`; **`4/4 CPUs initialized, SGI1 verified`** — PSCI под
  uH и трамплин ниже 4 ГиБ работают. Кадры до 1,5/с (копия 2–3,5 мс,
  параллельно), `MB free 35–60`. Паник нет.
- `xnu8`: то же, `cpus=8` (A73 MPIDR 0x100..0x103). Записан, reboot
  17:09:05. Пользователь: **экран блокировки чуть больше чем за минуту**.
  Журнал `logs\boot-xnu8.txt`: `requested=8 selected=8 (limit=8)`, cpu4..7
  `PSCI CPU_ON rc=0`, `mpidr=0x100..0x103 came alive; SGI1 OK`,
  **`midr=0x410fd092` (Cortex-A73 r0p2)**, `vct_lag≈16000–16600`;
  **`8/8 CPUs initialized, SGI1 verified`**; timer test на каждом из 8 ядер
  ~20 мс. Кадры до 4,1/с (21 своп за 5 с), копия 4–12 мс. Паник нет.

### Ввод: кнопки (патч 0403) и разведка тача

- DTS A50 `gpio_keys`: все три кнопки на **GPA1** (pinctrl_0 ALIVE
  `0x11850000`, банк `+0x20`): Power — pin 7, Vol+ — 5, Vol− — 6, функция
  0xf (EINT), активный низкий; кнопки «Домой» нет.
- `xnu/patches/0403-a50-keys.patch`: `a5xelte_keys.c` с таблицей A50
  (только чтение `GPA1_DAT` = `0x11850024` раз в 10 мс, антидребезг, 3
  кнопки → `sweet_hid_key` 0/1/2), в журнал — `GPA1 con/dat/pud` (для
  проверки: у pin 5–7 функция 0xf, `con & 0xfff00000 == 0xfff00000`);
  `IOKitBSDInit` для A50 вызывает `a5xelte_keys_init()` (`a5keys=0`
  выключает). На LF-дереве ложится поверх 0402.
- Тач (DTS): Melfas **MSS100** (`melfas,mms_ts`, I2C-адрес 0x48, прошивка
  `mss100_a50.fw`, события формата 8 по 12 байт, 4095×4095) на
  **`i2c_4` = `i2c@13870000`** (обычный I2C Samsung, не HSI2C — как
  `0x13850000` у A5/7580 в `a5xelte_touch.c`); питание — `tsp_ldo_en`,
  regulator-fixed 3,0 В по **GPIO gpg3-1** (active high); IRQ — gpa0-0;
  SCL/SDA — gpp1-1/gpp1-0. Нужно: адреса банков gpg3/gpp1/gpa0, тактирование
  I2C-4, протокол MSS100 (вероятно MIP4).
- Коммит `6b8e264` (0403), сборка 43604170767 — success. `xnu9`: ядро
  17386496 байт (SHA256SUMS OK, строки `A50 GPA1 con` и `Cortex-A73`), ADT
  с 8 CPU, cmdline как xnu8 (`cpus=8`); `boot.img` sha256 `eae6bc2b…ac9d`,
  хост-прогон проходит.
- xnu9 на телефоне (20:12): **кнопки не работают**. Журнал:
  `A5XELTE KEYS: A50 GPA1 con=0 dat=0x4f pud=0x111111`, ни одного
  `key=`. В TWRP (debugfs, только чтение) Linux показывает банки ALIVE:
  **etc0 (6 выводов), gpa0, gpa1, gpa2, gpq0**; у gpa1-5/6/7 `CON(0xf)
  DAT(0x1) PUD(0x0)` (отпущены = 1). Исходник ядра A50
  (`pinctrl-exynos-arm64.c`, `exynos9610_pin_banks0`): etc0 `0x000`, gpa0
  `0x020`, **gpa1 `0x040`**, gpa2 `0x060`, gpq0 `0x080`; поля CON/PUD по 4
  бита, DAT по 1. Мой драйвер читал gpa0. Исправлено в `0403` (GPA1 =
  `0x40/0x44/0x48`), патч пересобран и ложится поверх 0402.
- Попутно для тача: gpa0-0 (IRQ тача по DTS) в Linux `CON(0xf) DAT(0x1)
  PUD(0x0)`.
- Коммит `ae4e313`, сборка 43621047845 — success. `xnu10`: ядро 17402880
  байт (SHA256SUMS OK, отличается от xnu9), ADT и cmdline как xnu9;
  `boot.img` sha256 `19b03bf4…4309`, хост-прогон проходит.
- xnu10 на телефоне (20:45): **кнопка питания работает полностью**
  (коротко — экран, средне — Siri, долго — меню выключения). Журнал:
  `A50 GPA1 con=0 dat=0xe2 pud=0x11111`; `key=0 down=1 gpio=0x62` →
  `SWEET HID: key 0x30`; громкость: `key=2 down=1 gpio=0xa2` → `SWEET HID:
  key 0xea down` — **события громкости доходят до iOS**, но индикатора нет:
  в порте нет аудиоустройства (у S7 звук — отдельные патчи).

### Тач (патч 0404)

- TWRP (debugfs, только чтение): `gpp1-0/1` и `gpg3-1` в pinctrl
  **`0x139b0000`** (TOP): `gpp1-0/1 CON(0x2) DAT(0x1) PUD(0x0)`, `gpg3-1
  CON(0x1) DAT(0x1)`; I2C-4 — `s3c2410-i2c` на `13870000.i2c`, на нём
  `4-0048` `mms_ts`. Исходник ядра A50: `exynos9610_pin_banks4` (TOP) gpp1
  `0x020`, gpg3 `0x120`, bank_type_6 (CON/DAT/PUD `+0/+4/+8`); i2c_4 —
  `samsung,s3c2440-i2c`, `GATE_I2C_4_QCH`, i2c4_bus gpp1-1/0 функция 2.
  Протокол `melfas/mss100` = **MIP4**: PACKET_INFO `02 10` (размер в битах
  6:0, alert в 7), PACKET_DATA `02 11`; событие 12 байт: id `(p0&0xf)-1`,
  тип `p0>>4` (1 = экран), state `p1&1`, x `(p2&0xf)<<8|p3`, y
  `(p2>>4)<<8|p4`; включение питания — 90 мс.
- `xnu/patches/0404-a50-touch.patch`: новый `bsd/dev/a50_touch.c` (только
  A50): лог исходного состояния, `gpp1-0/1` → функция 2 без подтяжки,
  `gpg3-1` → выход «1» (RMW только этих полей), 90 мс, чтение ready/
  product/resolution/versions по MIP4 (S3C2440 I2C как в
  `a5xelte_touch.c`, запись адреса регистра + STOP + чтение), затем опрос
  событий каждые 10 мс → `sweet_hid_touch` в пикселях 1080×2340
  (`a50touch=0` выключает). `SweetHID.cpp`: для A50 панель 1080×2340,
  68×148 мм. На LF-дереве ложится поверх 0403.
- Коммит `6de2d64`, сборка 43624194287 — **success, без ошибок компиляции**.
  `xnu11`: ядро 17337760 байт (SHA256SUMS OK, строка `A50 TOUCH: polling`
  внутри), ADT и cmdline как xnu10; `boot.img` sha256 `21c4db72…6962`,
  хост-прогон проходит.
- xnu11 на телефоне (21:24): **тач работает «практически идеально»**
  (пользователь: разблокировка свайпом, рабочий стол, иконки). Журнал
  `boot-xnu11.txt`: `before: gpg3 con=0 dat=0x1c` (питание тача после
  S-Boot выключено), `gpp1 con=0`, `i2c con=0xa`; `power switched on; after:
  gpg3 con=0x10 dat=0x1e gpp1 con=0x22`; `ready status read 0x5`, **`product
  "2019-A50"`**, `resolution 4095 x 4095`, `versions … core 0301 custom 1130
  param 2702`; 32 касания (`finger 0 down/up at x,y`), 1 ошибка I2C.
- Строка состояния на экране блокировки была оранжево-красной полосой: это
  отладочная полоса UIKit rdar:45025538 («No status bar visual provider»),
  как у S7 при не-айфонном размере. 1080×2340 @3x = 360×780 точек = экран
  **iPhone 12 mini** (artwork-device-subtype 2340). `mkadt_a50.py`: в
  `/product` `artwork-device-subtype` 2340 (опция `--subtype`), scale 3.
  `xnu12\boot.img` (ядро xnu11, sha256 `8e487650…507d`) — проверка;
  записан, reboot 21:59:53.
- Обои (как у S7): `libherosb.dylib` (gpu/eagl/src/herosb.c, артефакт
  workflow `mesa-ios`) с boot-arg `herosb=1` встраивается в SpringBoard
  (`herolte_gl.c`) и ставит `/private/var/herolte/wallpaper.png` в окна обоев и
  за рабочий стол; файлы приходят через drop zone (`herolte_drop.c`, 128 МиБ
  под журналом в конце userdata, `tools/herolte-drop.py`). herosb ещё и
  подменяет `_UIStatusBarVisualProvider_Fallback` на `_Phone` в SpringBoard
  (запасной способ против красной полосы).
- `xnu/patches/0405-a50-drop-zone.patch`: `herolte_drop_apply()` и для A50
  (Wi-Fi early — нет). `tools/herolte-drop.py`: `--dev`, `conv=notrunc,fsync`.
- xnu12 на телефоне: **рабочий стол iOS** (фото: виджеты Weather/Calendar,
  иконки, док; тач работает), но вверху всё ещё красная полоса
  `rdar:45025538` с вырезом. Причина точнее: iPhone 12 mini рисует в 375×812
  точек и уменьшает до 1080×2340, а мы рисуем прямо в 360×780 точек —
  такого экрана нет ни у одного iPhone, subtype 2340 не помогает.
- Путь S7: `libherosb` в SpringBoard подменяет
  `_UIStatusBarVisualProvider_Fallback`. `herosb.c`: класс-замена теперь
  читается из `/private/var/herolte/statusbar.provider` (иначе, как было,
  `_Phone`); для A50 — `_UIStatusBarVisualProvider_Split58` (часы слева,
  середина свободна под вырез камеры). `mesa-ios.yml` собирается и на `a50`.
- `E:\iOS-A50\flash\drop\` (вне git): `wallpaper.png` 1080×2340 (растянута из
  присланных 376×816), `statusbar.provider`.
- Коммит `78f5607`: сборки xnu 43631270076 и mesa-ios 43631270223 — success.
  `xnu13`: ядро (SHA256SUMS OK, `HERODRP1` внутри); `mesa-ios`: все 9 файлов
  по SHA256SUMS OK (пути с префиксом `out/`), в `libherosb.dylib` есть
  `statusbar.provider`. `libdrm.2.dylib` без конструкторов, к HeroGPU — только
  из своих функций. `xnu13\boot.img` (ADT xnu12, cmdline + `herosb=1`),
  sha256 `fe0f8fe6…4b49`, хост-прогон проходит.
- Пакет drop zone записан (`herolte-drop.py --dev /dev/block/by-name/userdata`):
  `lib/libherosb.dylib`, `lib/libdrm.2.dylib`, `wallpaper.png`,
  `statusbar.provider`; package 1794685348, 1388544 байта, блок 13711359
  (= 13745152 − 1025 − 32768, далеко за 5242880 блоками HFSX), **verified**.
  xnu13 записан (чтение обратно совпало), reboot 22:42:39.
- Камера: не делаем — сенсоры A50 идут через ISP Exynos с закрытой прошивкой,
  а камера iOS говорит с драйвером ISP Apple по закрытому протоколу; у S7
  камер тоже нет.
- **xnu13 + пакет: ни шторки, ни обоев.** В логе: `HEROLTE DROP: package
  1794685348: 5 entries written`, `HEROLTE GL: SpringBoard launch 1 (pid
  27): libherosb inserted`, `SWEET AMFI: dyld policy for pid 27: input 0x0
  -> 0x23`, но ни одной строки `herosb:`. Причина слепоты: `herodrm_log`
  пишет только в user client HeroGPU (Mali S7), а на A50 его нет; копия в
  stderr SpringBoard никуда не попадает. Загрузилась ли библиотека и
  сработали ли хуки, не видно. Зависимость `@rpath/libdrm.2.dylib` с
  rpath `/private/var/herolte/lib` есть, и AMFI разрешает @-пути (0x23).
- **0406 (журнал из userland):** на A50 `log_data` (syscall 533) с tag
  `'HER0'` (0x48455230) печатает строку в журнал ядра как `HEROLTE USER pid
  имя: ...`, не больше 4000 строк за загрузку. `herodrm_log` без HeroGPU
  вызывает его напрямую (`svc #0x80`); ядро S7 отвечает EPERM. herosb
  пишет первой строкой `herosb: in pid N, ...`.
- **Шторка, второй путь: экран iPhone XR.** 360×780 pt при @3x не совпадает
  ни с одним iPhone, поэтому UIKit берёт провайдер Fallback с любым subtype.
  `mkadt_a50.py --bank2 --xr`: 828×1792 @2x (414×896 pt, subtype 1792,
  `chosen/display-scale` 2), как `--scale2` у S7. 19.5:9 совпадает с панелью,
  поэтому без полос. Ядро уже умеет это делать (`sweetfb_scale=1
  sweetfb_lw=828 sweetfb_lh=1792`). CoreAnimation рисует в свои IOSurface (2/3
  в логе xnu13, кэшируемая память), `scaledCopy` растягивает кадр ×1.304
  ближайшим соседом на всю панель. Кадр по умолчанию лежит в `0xa0000000`, в
  нижнем окне, которое при `--bank2` свободно. `sweetfb_native=1` остаётся:
  с ним SweetHID нормализует тач по всей панели. На CPU рисуется на 41%
  меньше пикселей. Хуки шторки herosb выключаются файлом `gestures.off`
  (жесты и так родные, home-button-type 2 из D22AP), обои остаются.
- Коммит `c8b6e8e`: xnu 43635906385 и mesa-ios 43635906389 — success,
  SHA256SUMS OK. В ядре есть `HEROLTE USER %d %s: %s`; в `libdrm.2.dylib`
  есть `mov w0,#0x48455230` / `mov w16,#533` / `svc #0x80`. Рецепт сборки
  BOOT проверен: xnu13, пересобранный из своих входов, совпал байт в байт.
  `xnu14\boot.img` (ADT `--bank2 --xr`, cmdline xnu13 + `sweetfb_scale=1
  sweetfb_lw=828 sweetfb_lh=1792`, 494 символа), sha256 `b8cbd5f6…8f92`,
  хост-прогон загрузчика проходит (сегменты в 0x881004000…, в нижнем окне
  ничего).
- Пакет 1794688563 (`lib/libherosb.dylib`, `lib/libdrm.2.dylib`,
  `wallpaper.png` 828×1792, `gestures.off`), 892928 байт, блок 13711359,
  **verified**. xnu14 записан в BOOT (чтение обратно совпало), reboot 23:36:21.
- **xnu14 на телефоне: шторка нормальная** (пользователь: «Шторка работает»),
  экран XR масштабируется на всю панель, тач попадает. **Обоев нет.** В логе
  (`boot-xnu14.txt`) по-прежнему `libherosb inserted` и политика 0x23, но
  ни одной строки `HEROLTE USER`: конструктор herosb не выполнялся.
- Причина найдена чтением HFSX из TWRP (`herolte-hfs-read.py`, теперь с
  `HEROLTE_DEV=/dev/block/by-name/userdata`; `stat` показывает режим и uid
  папок): `/private/var/herolte` — **0744 root:wheel**. Её создал launchd при
  первом запуске iOS для `autorun.out` (StandardOutPath задания autorun;
  cnid 326205/326206, раньше любого пакета). `hdrop_mkdir` существующую
  папку не трогал. SpringBoard работает как mobile и не может войти в папку →
  dyld не открывает `lib/libherosb.dylib` и с флагом
  ALLOW_FAILED_LIBRARY_INSERTION молча её пропускает. Файлы на месте и
  совпадают с пакетом (sha256), `lib/` 0755. Шелла на корне нет
  (`/bin`: только `df`, `ps`), поэтому chmod из autorun невозможен.
- **0407:** `hdrop_mkdir` возвращает существующей папке drop zone её режим
  (0755) и пишет `HEROLTE DROP: … mode 744 -> 755 (0)`. Это делается при каждой
  загрузке с пакетом, до проверки id пакета, а значит до SpringBoard.
- **Батарея:** на A50 SweetBattery выключен (0400: он для MAX77854 у S7),
  поэтому iOS показывает не настоящий заряд. В TWRP:
  `sm5713-fuelgauge`/`sm5713-charger`, MFD `sm5713mfd` на i2c-1 по адресу 0x49,
  dummy 0x25 и 0x71; `battery`: 100%, 4312000 мкВ, Full (на USB). i2c-1 —
  `s3c2410-i2c`, как у тача.
- Коммит `ca36f14`: xnu 43641718200 — success, SHA256SUMS OK, строка 0407 в
  ядре. `xnu15\boot.img` (ADT и cmdline xnu14), sha256 `0b725a53…24da`,
  хост-прогон проходит. Пакет не переписывался (1794688563 уже установлен,
  а исправление режима работает до проверки id). xnu15 записан в BOOT (чтение
  обратно совпало), reboot 2026-11-15 14:30:54.
- **xnu15 на телефоне: обои работают** (пользователь: «обои работают
  хорошо»). В логе `boot-xnu15.txt`: `HEROLTE DROP: /private/var/herolte
  mode 744 -> 755 (0)`, `package 1794688563 already installed`, и строки herosb
  через 0406: `HEROLTE USER 27 SpringBoard: herosb: in pid 27, status bar and
  gestures off, wallpaper on`, `round 1, 14 windows, picture 1`, `picture in
  SBHomeScreenWindow, 414 x 896`, `picture in _SBWallpaperSecureWindow, 414 x
  896`; ограничения частоты кадров подняты до 60 (fluid/animation settings yes).
  Шторка нормальная при выключенных хуках herosb: её рисует сам UIKit для
  экрана XR.
- **Батарея, исходники** (ядро A50 universal9610, ветка A505FDDU1ASC6:
  `drivers/battery_v2/sm5713_fuelgauge.c`, `sm5713_charger.c`,
  `include/linux/mfd/sm5713-private.h`; DTS: `sm5713@49` на `i2c_1`
  `0x13840000`, S3C2440, выводы gpp0-2 SDA / gpp0-3 SCL, функция 2).
  Топливомер `0xE2>>1 = 0x71`, 16-битные регистры (SMBus word, младший байт
  первым): SOC `0x05` = биты 14:8 целые %, 7:0 — 1/256 % (бит 15 → 0 %);
  VBAT `0x07` = `(v&0x3fff)*1000>>11` мВ; CURRENT `0x08` в том же масштабе в
  мА, бит 15 = минус. Зарядник `0x92>>1 = 0x49`, байтовые: STATUS1 `0x0D`
  бит 0 VBUS_POK; STATUS2 `0x0E` бит 3 идёт заряд, бит 5 top-off (полный).
- **Батарея, TWRP** (на USB, Android: 100 %, 4309–4313 мВ, Full, CHARGE_FULL
  4000000 мкА·ч): pinmux gpp0-2/3 = `13840000.i2c` i2c1-bus; toybox `i2cget -f
  -y 1` (только байты): `0x71`: 0x00→0x01, 0x05→0x00, 0x07→**0x78** (младший
  байт 4309·2048/1000 = 0x2278 — формула сходится); `0x49`: STATUS1 **0x01**,
  STATUS2 **0x20** (VBUS есть, полный).
- **0408 (батарея A50):** `bsd/dev/a50_battery.c` даёт SweetBattery
  `herolte_fg_init/read` (SM5713) и `a50_charger_read`. Только чтение: SM5713
  получает лишь адреса регистров. I2C-1 как у тача, но чтение с повторным
  START, как SMBus в Linux; если не выйдет — STOP между адресом и данными, и в
  лог пишется, какой способ сработал. SweetBattery включён для A50 (HeroGPU
  нет), 4000 мА·ч, заряд/USB/полный — из статуса зарядника. `herolte_battery.c`
  (MAX77854 S7) на A50 больше не собирается. `a50fg=0` оставляет статичные
  100 %. Процент — сырой SOC топливомера; Android ещё масштабирует его
  (capacity_max), поэтому числа могут расходиться на несколько процентов.
- Коммит `783db1a`: xnu 43714850920 — success, SHA256SUMS OK, строки
  `A50 BATTERY` и `SM5713 fuel gauge` в ядре. `xnu16\boot.img` (ADT и cmdline
  xnu15), sha256 `9a732c6b…aecb`, хост-прогон проходит. Записан в BOOT (чтение
  обратно совпало), reboot 2026-11-15 17:10:28.
- **xnu16 на телефоне: батарея правильная** (пользователь: «батарея работает
  корректно», но кабель отображается с задержкой 40–60 с). Лог
  `boot-xnu16.txt`: `before: gpp0 con=0x1111 pud=0x11110000 i2c con=0xa`
  (S-Boot оставил gpp0-2/3 как GPIO-выходы), `I2C1 reads with a repeated
  START`, `SM5713 id 0x1, SOC 0x6400 = 100.0%, VBAT 0x229f = 4327 mV, current
  0xc5 = 96 mA; charger read vbus=1 charging=1`, потом без кабеля `99% 4188mV
  -380mA vbus=0`. TWRP после этого: 100 %, Full, 4343000 мкВ.
- **0409:** на A50 SweetBattery каждые 2 с читает два байта статуса
  зарядника, а полный опрос делает при их изменении или раз в 30 с. Строка
  в логе: `SWEET BATTERY: charger now vbus=… charging=… done=…`.
  Коммит `b7184a6`: xnu 43724541481 — success, SHA256SUMS OK. `xnu17\boot.img`
  (ADT и cmdline xnu16), sha256 `5dbada40…3cdc`, хост-прогон проходит.
  Записан в BOOT (чтение обратно совпало), reboot 2026-11-15 17:53:04.
- **xnu17 на телефоне: значок кабеля реагирует сразу** (пользователь: «всё
  работает отлично»). Лог `boot-xnu17.txt`: после старта `100% 4328mV 104mA
  vbus=1 charging=1`, затем `charger now vbus=1 charging=0 done=0` и 0 мА
  (заряд остановился при 100 %, но бит top-off не стоит), потом шесть строк
  `charger now vbus=0/1` — подключения и отключения кабеля.
  **Не проверено:** заряжается ли батарея под iOS, если она ниже 100 %. Без
  драйвера Android никто не сбрасывает сторожевой таймер зарядника SM5713
  (бит 7 STATUS2 в Linux), и пока неясно, не он ли остановил заряд. Проверить
  при заряде ~80–90 %: `charging=1` и ток > 0 в строках SWEET BATTERY.

### Фонарик и яркость (0410, 2026-11-15)

- **Как iOS это делает** (поиск строк по распакованному образу корня, без
  вывода файлов Apple): CoreBrightness (`CBDisplayContaineriOS`) и
  `PLDisplayAgent` ждут службу **`AppleARMBacklight`** с
  `{"backlight-control" = true}`. Они читают и пишут её `IODisplayParameters` →
  `brightness` / `BrightnessMilliNits` (`{min,max,value}`), а CoreBrightness
  ещё задаёт `IOMFBBrightnessLevel` (`_SetIOMFBBrightness`,
  `SetBLDriverNitsValue`). В порте такой службы не было. Фонарик — это
  `AVFlashlight` (`+hasFlashlight`, `available`, `overheated`,
  `flashlightLevel`, `setFlashlightLevel:withError:`,
  `turnPowerOnWithError:`, `turnPowerOff`). Без камеры `hasFlashlight` = NO
  («AVFlashlight cannot be instantiated on this platform…»).
- **Панель:** EA8076 (TWRP: `ea8076_read_id: 904003`, `lcdtype=9453571`),
  командный режим MIPI, DSIM `0x148E0000`. Яркость в ядре Samsung
  (`ea8076_a50_lcd_ctrl.c`): `F0 5A 5A`, `51 hi lo` (10 бит), `F0 A5 A5`,
  каждая — DCS long write 0x39. `brightness_table` в TWRP: UI 0..255 → 3..1023
  почти линейно, выше 255 — HBM. Регистры DSIM (`cal_9610/regs-dsim.h`):
  PKTHDR 0x58, PAYLOAD 0x5C, FIFOCTRL 0x68 (бит 10 — FIFO заголовков пуст),
  INTSRC 0x50.
- **Фонарик, TWRP** (`echo 1/0 > /sys/class/camera/flash/rear_flash`,
  `i2cget -f -y 1 0x49`): `0x41` 0x1c → **0x1f** → 0x1c (биты 1:0 — режим),
  `0x42` 0x28 → **0x18** (биты 6:4 — ток, 50 + 25·код мА); `0x14` 0x05 и `0x23`
  0x31 не менялись (на USB). Выводы при горящем светодиоде: `gpa1-3`
  (torch-gpio) CON 1 DAT **1**, `gpg0-6` (flash-gpio) CON 1 DAT 0
  (`fimc-is-flash-gpio@0` в живом DT). `sm5713_charger_oper.c`: без VBUS
  фонарик = op mode **8** (FLASH_BOOST) в `0x14`[3:0], BSTOUT 4500 мВ (1) и OTG
  900 мА (1) в `0x23` [3:0]/[7:6]; без событий и с VBUS — 5 (CHG_ON_VBUS),
  4500 мВ, 500 мА. Банк gpg0 в TOP — `0x0c0` (gpp0…gpc2 перед ним, шаг 0x20,
  gpg3 = 0x120 сходится с тачем).
- **0410:** `bsd/dev/a50_torch.c` — sysctl `kern.a50_torch` 0..100 (любой
  пользователь), уровень → код 1..4 (75–150 мА), режим зарядника по VBUS,
  перепроверка при смене кабеля (из 2-секундной проверки SweetBattery).
  `a50_battery.c`: блокировка I2C-1 и запись байта. `bsd/dev/a50_panel.c` —
  три DCS-команды через DSIM; после трёх неудач подряд панель не трогается.
  `iokit/Kernel/arm/A50Backlight.cpp` — служба `AppleARMBacklight`: свойства
  `/backlight` iPhone X (ADT), `backlight-control`, `IODisplayParameters`;
  `setProperties` (у неё и у AppleCLCD) пишет в лог всё, что приходит, и
  переводит `BrightnessMilliNits` / `brightness` / `IOMFBBrightnessLevel` в
  уровень панели (500 нит = 1023, предположение). Селекторы IOMFB 50/66/67/68
  пишутся в лог со значениями. При старте службы яркость ставится на 60 %.
- `mkadt_a50.py`: в ADT добавляется `/backlight` из D22AP (без phandle и
  `function-*`). `herosb.c`: в SpringBoard методы `AVFlashlight` отвечают сами
  и передают уровень в `kern.a50_torch` — только если этот sysctl есть;
  `flashlight.off` выключает.
- Коммит `d2f95cc`: xnu 43732536281 и mesa-ios 43732536267 — success,
  SHA256SUMS OK. `xnu18\boot.img` (ADT с `/backlight`), sha256
  `e79f37d8…d2b7`, хост-прогон проходит. Пакет 1794760779 **verified**, xnu18
  в BOOT, reboot 2026-11-15 19:39:47.
- **xnu18 на телефоне: яркость работает** (пользователь: «яркость регулируется
  нормально»). **Обоев нет, значка фонарика нет.** Лог `boot-xnu18.txt`:
  `A50 PANEL: DSIM fifoctrl=0x5507 intsrc=0x1000000`, `brightness 60.0% ->
  WRDISBV 615 (boot)`, `AppleARMBacklight up, 38 properties`. CoreBrightness
  пишет в AppleARMBacklight `brightness` и `backlight-level` (одно и то же, из
  65536: 0x8000 → 50 %, 0x8f19 → 55.8 %), `DisplayBrightness.{Nits,
  NitsPhysical,Brightness}` = 0, `DisplayBrightnessSliderPosition`, `FadePeriod`,
  `CompensationTweak.{cap,scaler}`, `brightness-nits(-cap)`. Один раз приходит
  `IOMFBBrightnessLevel = 0` на AppleCLCD (кратко 0 % до следующего
  `brightness`). Единицы, значит, — доля от 65536.
- **Причина пропажи обоев:** `CRASH SpringBoard[27] sig 5 … sub 0x21a530f18`, затем
  то же с pid 162; третий SpringBoard запущен без libherosb (лимит 2), ни одной
  строки `HEROLTE USER`. `objc_addLoadImageFunc` вызывает функцию для уже
  загруженных образов, держа runtimeLock, а `look_for_flashlight` внутри
  вызывала objc_getClass/method_setImplementation — повторный захват
  os_unfair_lock = SIGTRAP. Вторая ошибка: `method_setImplementation` по ответу
  `class_getInstanceMethod` поменял бы унаследованные `-init`/`-dealloc` у всех
  классов. Исправлено в herosb: перехват сразу в конструкторе (фреймворк
  AVFlashlight уже загружен у SpringBoard), `class_replaceMethod` только в
  AVFlashlight (и его метаклассе), init/dealloc идут в суперкласс; функция для
  objc_addLoadImageFunc только ставит попытку в main queue.
- Коммит `ce15096`: mesa-ios 43744715669 — success, SHA256SUMS OK (9).
  Пакет 1794766374 (новая `libherosb.dylib` 71072 байта, `libdrm`, обои,
  `gestures.off`), блок 13711359, **verified**; BOOT остался xnu18, reboot
  2026-11-15 21:12:55.
- **На телефоне: обои вернулись, фонарик работает и от кабеля, и без него**
  (пользователь). Яркость — как в xnu18. Лог этой загрузки не снимался.
- **Зарядка под iOS ниже 100 % работает** (пользователь проверил). Значит,
  остановка заряда при 100 % в `boot-xnu17.txt` — обычное окончание, а не
  сторожевой таймер зарядника.

### Звук и вибрация: разведка (2026-11-15)

- **Вибромотор:** `sec_vib` в живом DT, `sec_vib,ldo_en` = phandle 0xad
  (**gpa2**, ALIVE `0x11850000` + 0x060) вывод **1**, `ldo_type` 1,
  `max_timeout` 10000 мс. TWRP: `echo 500 > /sys/class/timed_output/vibrator/enable`
  — `gpa2-1 CON(0x1) DAT(0x1)` во время вибрации, DAT 0 после. Мотор включается
  одним выводом.
- **Как iOS 16 вибрирует** (строки корня): нет AppleVibrator/IOHIDVibrator;
  клиенты идут в `com.apple.audio.SystemSoundServer-iOS`
  (`kAudioServicesPlaySystemSoundOptionVibrationPatternKey`) и
  `com.apple.audio.hapticd`; у iPhone X Taptic Engine питается через аудиотракт
  (`HapticAudioUnit`, `HapticsAmpTurnOn`, `ConfigureTDMControls`,
  `AppleHapticsSupport`, `audio-haptic` в ADT). На телефоне запущен
  `mediaserverd` (audiomxd нет), в нём и живут эти службы.
- **Звук, iOS:** на S7 уже работает модуль `gpu/hakios-audio`
  (AudioServerPlugIn в mediaserverd → `/dev/hakios-audio`, синк ядра, громкость).
- **Звук, железо A50:** ABOX `0x14A50000` (DSP «calliope» со своей прошивкой,
  в TWRP `calliope_version` пустой; RDMA ×8, WDMA ×5, UAIF ×4, IPC), карта
  `Exynos9610-Audio`; динамик через усилитель `tfa98xx` (I2C-7, 0x34, ему нужна
  своя конфигурация), кодек `rt5665` (I2C-6, 0x1b). Это несравнимо больше, чем
  I2S на S7.
- **Звук, vendor** (TWRP, `mount -o ro …/vendor`; файлы скопированы локально в
  `E:\iOS-A50\flash\audio\vendor`, в git их нет): `firmware/calliope_sram.bin`
  (32540), `calliope_dram.bin` (2061736), `calliope_iva.bin` (57728) — прошивка
  ABOX; усилитель **TFA9872**: `firmware/Tfa9872.cnt` (7645), `tfadsp.bin`
  (76972 — защита динамика, она работает на ABOX); `etc/mixer_paths.xml`.
  `media-speaker` = `dev-speaker` + `erap-amp-reference` (DSM On) +
  `route-sifs0-to-uaif0` + обратная связь `uaif2→nsrc3→wdma4`, `uaif4→nsrc2→wdma3`
  + **`route-rdma7-to-sifs1` (SPUS OUT7 = SIFS1, SIFS1 = SPUS OUT7) +
  `route-sifs1-to-uaif2` (UAIF2 SPK = SIFS1)**. Значит, усилитель на UAIF2 и
  получает RDMA7 (у Android — поток после DSP).
- **TWRP как стенд:** `tools/a50-alsa/a50alsa.c` — свой ALSA-инструмент
  (статический arm64 без libc, сборка Arm GNU `aarch64-none-elf`; list/get/set/
  tone). В TWRP 348 контролов; TFA-контролов нет. `firmware_class.path` =
  `/vendor/firmware`, но TWRP подключает vendor уже после загрузки драйверов.
  Маршрут RDMA7→SIFS1→UAIF2 ставится, но `hw_params` = -22:
  `tfa98xx: invalid sample rate 48000` (контейнер не загружен, нет профилей) и
  `samsung-abox: Invalid calliope state: 0` (прошивка ABOX не запущена; ABOX
  остаётся runtime-active и не перезапускается). У `abox_enable()` прошивка
  грузится только при «reset»-пробуждении (`calliope_sram.bin` → `dram` → `iva`).
  `/dev/mem` в TWRP нет: опыты с регистрами ABOX — только из нашего ядра.
  У `7-0034` (tfa98xx) есть отладочные `reg`/`rw` в sysfs.
- **Регистры ABOX** (`sound/soc/samsung/abox/abox.h`, SFR `0x14A50000`–`0x14A5FFFF`):
  IP_INDEX 0x0, VERSION 0x4, SYSPOWER_CTRL 0x10, SYSTEM_CONFIG0 0x20, ROUTE_CTRL0–2
  0x40–0x48 (UAIF_SPK(x) по 4 бита, SPUSM 19:16, DSIF 23:20), SPUS_CTRL0–3
  0x200–0x20C, SIFS_CNT 0x280/0x284; UAIF(x) = 0x500 + 0x10·x: CTRL0 (SPK_ENABLE
  0, MIC_ENABLE 1, MODE 2, IRQ_MODE 3, DATA_MODE 4, START_FIFO_DIFF 27:24/31:28),
  CTRL1 (VALID_END 5:0, VALID_STR 11:6, SBIT_MAX 17:12, SLOT_MAX 20:18, WS_POLAR
  21, WS_MODE 22, BCLK_POLARITY 23, FORMAT 28:24), STATUS +0xC; RDMA(x) = 0x1000
  + 0x100·x: CTRL0 (ENABLE 0, PROGRESS 31), CTRL1, BUF_STR 0x08, BUF_END 0x0C,
  BUF_OFFSET 0x10, STR_POINT 0x14, VOL_FACTOR 0x18, VOL_CHANGE 0x1C, SBACK_LIMIT
  0x20, STATUS 0x30.
- **Что ещё нужно для звука без Linux:** аудио-PLL (в TWRP «pll clock:
  1179648040» при игре, 26 МГц после) и делители CMU для UAIF; SysMMU ABOX
  (адреса буферов RDMA — это IOVA); сам ABOX может требовать прошивку calliope
  для запуска DMA (у Linux старт RDMA идёт через IPC); TFA9872 на I2C-7 — это
  HSI2C/USI (`11d10000.hsi2c`), другой контроллер, чем у тача и батареи, плюс
  его настройка из `Tfa9872.cnt`; затем синк `/dev/hakios-audio` и установка
  `HakiosAudio.driver` в mediaserverd.
- **План:** (1) раунд только чтения: ядро логирует CMU/PLL, питание и SysMMU
  ABOX, регистры ABOX (только если тактирование включено), ID TFA9872 по
  HSI2C; (2) попытка звука без прошивки: PLL, UAIF2 мастером 48 кГц/16 бит,
  RDMA7 с тоном, TFA9872 без DSP на малой громкости; (3) если DMA без прошивки
  не идёт — загрузка calliope и IPC, как у Linux; (4) синк и модуль для iOS.
- **Адреса для звука** (cmucal-sfr.c 9610, DTS, живой DT): CMU_TOP `0x12100000`
  (CLKCMU_DISPAUD_AUD: mux 0x1030, div 0x1838, gate 0x2038); **CMU_DISPAUD
  `0x14980000`**: PLL_AUD CON0 0x160 (ENABLE 31, STABLE 29, M 25:16, P 13:8, S
  2:0), CON3 0x16c (K), locktime 0x0; MUX_AUD_UAIF2 0x1018, DIV AUDIF 0x1800,
  CPU 0x180c, CPU_ACLK 0x1810, UAIF2 0x182c; гейты ABOX ACLK 0x2020, BCLK_UAIF2
  0x2008, UAIF2 rstnsync 0x2018, CCLK_CA7 0x2030, SMMU_ABOX 0x20b0 (CG_VAL 21,
  MANUAL 20); QCH ABOX S_ACLK 0x3014, S_BCLK2 0x3020, SMMU_ABOX 0x3060. **SysMMU
  ABOX `0x14920000`** (port-name «ABox»; DPU — `0x14820000`). Тактирование ABOX
  в DTS: PLL_OUT_AUD, GATE_ABOX_QCH_CPU, DOUT_CLK_AUD_AUDIF, DOUT_CLK_AUD_ACLK;
  UAIF2: DOUT_CLK_AUD_UAIF2 + GATE_ABOX_QCH_S_BCLK2; sysreg ABOX `0x14810000`,
  SRAM `0x14B00000`, GIC ABOX `0x14AF1000`.
- **TFA9872:** I2C-7 = **hsi2c_3 `0x11D10000`** (USI_0_CMGP_I2C, USI v2:
  USI_CON 0xC4 = 0), выводы gpm2-0 SCL / gpm3-0 SDA, функция 3 (pinctrl CMGP
  `0x11C20000`); CMU_CMGP `0x11C00000`: MUX_CMGP_I2C 0x1004 (RCO или шина
  CMGP), DIV 0x1804, гейты 0x2018/0x201c/0x2044, QCH 0x3014. TWRP: `i2cget -f
  -y 7 0x34 0x03` = **0x3b** (старший байт ревизии, TFA9872 rev 0x3B72).
- **0412 (раунд «только чтение»):** `bsd/dev/a50_audio.c`, по boot-arg
  `a50audio=1` через 30 с после старта: CMU_TOP/DISPAUD/CMGP, PLL_AUD, гейты и
  QCH; ABOX и SysMMU ABOX читаются только если их гейты не выключены вручную;
  TFA9872 — ревизия и регистры 0x00–0x1F по hsi2c_3 (максимальный делитель).
  Пишутся только поля выводов, контроллер HSI2C и адреса регистров TFA.
- **Вибрация, где ловить** (имена методов из образа корня): ToneLibrary —
  `+[TLAlertSystemSoundController _vibrationPatternForAlert:withSound:]` (узор
  оповещения), `-[TLAlertController stopPlayingAlerts:withOptions:
  playbackCompletionType:]`, `-[TLAlert stop]`, `-[TLAlert stopWithOptions:]`;
  UIKit — `-[UIFeedbackGenerator _playFeedback:withMinimumIntervalPassed:since:
  prefersRegularPace:]`, `-[_UIFeedback play]`; на сервере — `AVHapticServerInstance`,
  `AVHapticPlayer`, `CHHapticEngine`.
- **0411 (вибромотор):** `bsd/dev/a50_vibrator.c` — sysctl `kern.a50_vibrate` =
  N мс (до 10000, `max_timeout`), 0 — стоп; новый запрос заменяет старый,
  выключение — thread_call с поколением (старое выключение не гасит новое).
- **herovibe** (в `libherosb`, SpringBoard): `herovibe_tramp.S` — один вход
  для всех наблюдаемых методов. Он сохраняет x0–x8 и q0–q7, вызывает
  `herovibe_note(self, _cmd, arg2, arg3)` и прыгает в собственную реализацию
  Apple, поэтому сигнатуры знать не нужно. Узор `VibePattern` (вкл/выкл, мс…)
  играется очередью по 20 мс, стоп по методам остановки; тактильный отклик
  UIKit — 25 мс, не чаще раза в 60 мс. Логируются типы методов и первые
  вызовы. Только если есть `kern.a50_vibrate`; `vibration.off` выключает.
- Коммит `a76e12e`: xnu 43756706415 и mesa-ios 43756706393 — success,
  SHA256SUMS OK; в `libherosb.dylib` трамплин на месте (stp/…/`bl`/`mov x16, x0`/
  восстановление/`br x16`). `xnu19\boot.img` (ADT и cmdline xnu18), sha256
  `aa483575…1032`, хост-прогон проходит. Пакет 1794774359 **verified**, xnu19 в
  BOOT (чтение обратно совпало), reboot 2026-11-15 23:26:07.
- **xnu19 на телефоне: вибрация работает** (пользователь: «вибрация работает и
  все элементы тоже»): обои, фонарик и яркость на месте. Лог этой загрузки не
  снимался.
- **Bluetooth: не делаем.** Wi-Fi и BT у A50 — встроенный блок Samsung WLBT
  (`scsc_wifibt@119c0000`: mailbox AP↔WLBT, BAAW, remap; BT-звук идёт через
  ABOX, «scsc bt»). Он работает только с закрытой прошивкой Samsung (Maxwell)
  и драйверами scsc_mx/scsc_bt (общая память и mailbox). А bluetoothd/BlueTool в
  iOS ждут контроллер Broadcom/Apple с его vendor-командами. Пришлось бы
  перенести в XNU стек Samsung и подделать для iOS чип Broadcom. У S7 BT
  тоже нет; bluetoothd выключен через `heronoexec`.

- `tools/mkadt_a50.py`: ADT без файлов Apple (без `/product`) собирается,
  проверено чтением обратно: GIC `0x12301000`/`0x12302000`, fbcon
  `0xca000000`+`0x9a3f80`, RAM `0x90000000`+512 МиБ, один CPU с MPIDR 0.
- По решению пользователя probe оставлен в BOOT: Android не грузится,
  каждое включение — экран probe → TWRP. Стоковый BOOT для возврата:
  `E:\iOS-A50\backup\2026-11-13\boot.img` через `tools/a50-adb-boot.ps1`.
- Загрузчик после отката: `ro.boot.bootloader=A505FNXXS9CVJ2`,
  `verifiedbootstate=orange`, `carrierid` остался `SER`.

### С телефона (adb, без root, только чтение)

- SM-A505FN, `a50xser`, прошивка `A505FNPUU9CWB4` (загрузчик ревизии **9**),
  Android 11, патч 2023-03-01, ядро 4.14.113.
- Заблокирован: `verifiedbootstate=green`, `flash.locked=1`,
  `oem_unlock_supported=1`, `sys.oem_unlock_allowed=0` (переключатель
  «Заводская разблокировка» выключен), шифрование FBE.
- RAM: MemTotal 3690784 kB → модель на 4 ГБ.
- CPU: 4× Cortex-A53 (part 0xd03, r0p4) + 4× Cortex-A73 (0xd09, r0p2);
  features `fp asimd evtstrm aes pmull sha1 sha2 crc32 cpuid` — без LSE, как
  ожидалось.
- Разделы: boot `sda14`, recovery `sda15`, dtb `sda12`, dtbo `sda13`,
  vbmeta `sda23`, efs `sda3`, sec_efs `sda7`, param `sda4`, **uh `sda10`,
  uhcfg `sda11`** (гипервизор Samsung), userdata `sda32`. Путь:
  `/dev/block/by-name/<имя>` (имена строчные).
- Узлы живого DT (значения без root не читаются, только имена):
  `memory@80000000`, `memory@C0000000`, `memory@E1900000`, `memory@880000000`
  — S-Boot режет нижний банк с дырами (как у gta4xl: `0x80000000+0x3AB00000`,
  `0xC0000000+0x20000000`, `0xE1900000+0x1E700000`).
  В `/reserved-memory` S-Boot добавляет **`el2_code`, `el2_earlymem`** (uH на
  EL2) и `kaslr`, которых нет в исходниках. Probe покажет их адреса.
- I2C: устройство `4-0048` — тач Melfas на i2c-4, как в DTS.
- Платформенные устройства: `13520000.ufs`, `148b0000.decon_f`,
  `148e0000.dsim`, `11500000.mali`, `11a10000.speedy`, `119c0000.scsc_wifibt`
  — адреса совпадают с DTS.
