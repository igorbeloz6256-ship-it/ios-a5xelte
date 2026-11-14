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
