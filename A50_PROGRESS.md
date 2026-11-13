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
  `kernel.development.a50`). На LF-дереве накладывается чисто. Компиляция
  ещё не проверена.
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
