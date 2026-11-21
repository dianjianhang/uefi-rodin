# rodinPkg (MT6899) Porting Notes

Redmi Turbo 4 / POCO X7 Pro (rodin, MediaTek Dimensity 8400-Ultra, MT6899).

## Status: first compiled UEFI image (2026-11-21)

The platform is now **buildable** and produces a valid UEFI firmware volume. The
rodin platform was integrated into the Mu-Silicium fork `dianjianhang/uefi-sheng`
(default branch `sheng`) and built by GitHub Actions:

- CI workflow: `.github/workflows/rodin_uefi.yml`
  -> `build_uefi.py -d rodin -r DEBUG -c`.
- First green run: [44816679614](https://github.com/dianjianhang/uefi-sheng/actions/runs/44816679614)
  (commit `08f8295` on `sheng`).
- Artifacts:
  - `SILICIUM_UEFI.fd` â 0x200000 bytes, valid `_FVH` firmware-volume header
    (sha256 `7a4628ce7fbee6728d8864bf2e74e3ec0b744d0152843160f7c0eee5431dc68d`).
  - `Mu-rodin.img` â Android boot image (header v2), kernel = gzip(FD),
    `kernel_addr 0x40040000` (sha256 `7a88f515e02290a49349af47ae6d088a1c80f9c2810a02e3ac18ae1a448656fc`).

What made it link: the shared `MediaTekPkg` DXE drivers are never exercised by the
upstream CI matrix (no MediaTek device is `Active` in `Status.md`), so `MsdcDxe`
carried latent `-Wunused-but-set-variable` warnings that `-Werror` turns fatal. A
single platform-scoped `-Wno-unused-but-set-variable` was added to
`MT6899Pkg.dsc.inc` `[BuildOptions]`; no driver source is modified.

## Memory map (2026-11-21)

`Library/MemoryMapLib/MemoryMapLib.c` was extended and every region is now tied to
a concrete rodin data source (vendor DTB under `turbo4ååº/`), commit `0f54550` on
`sheng`, verified green in run [44819034364](https://github.com/dianjianhang/uefi-sheng/actions/runs/44819034364).

Sourced / confirmed:

- **DDR low window**: vendor DTB `/memory@40000000` = `0x40000000` + `0x40000000`
  (1 GiB). UEFI Stack / FD / DXE heap tile it exactly, with the DXE heap **split
  around** the bootmonitor carveout so nothing overlaps.
- **BootMonitor PMSG carveout**: vendor DTB `/soc/bootmonitor_pmsg@6ec00000`
  reg `0x6ec00000` + `0x410000`. Mapped `Reserv` so the firmware never hands the
  Xiaomi pmsg/console ring to the DXE heap or the OS.
- **AP watchdog confirmed**: vendor DTB `watchdog@1c00b000` reg `0x1c00b000` +
  `0x100`, compatible `mediatek,mt6899-wdt` (TOPRGU syscon child). `0x1C00B000` is
  now source-confirmed, not a carry-over guess.
- All register/MMIO bases re-verified against the vendor DTB (`serial@11002000`,
  `mmc@11240000`, `ufsphy@112a0000`, `ufshci@112b0000`, `xhci0@11200000`,
  `dispsys-config@14000000`, `systimer@1cc10000`, `gic500@c000000`,
  `gic-cpu@c400000`, `syscon@10000000` topckgen, `infracfg@1020e000`,
  `emicen@10219000`, `dramc@10230000`, apmixedsys `0x1000c000`, pwrap `0x1000d000`).

Memory map â **sourced from a live root dump of a booted rodin**:

- A rooted rodin (model 24129RT7CC, HyperOS 4.0 / Android 17, kernel 6.6.118,
  KernelSU) was dumped over adb:
  `/proc/device-tree/memory@40000000` = `0x40000000` + `0x300000000` (12 GiB, one bank),
  `/proc/device-tree/reserved-memory/*/reg` = the **53 LK mblocks** with their real
  `base + size`, and `/proc/iomem` = System RAM holes + MMIO ranges. The live DT
  carries the addresses LK actually programmed (the vendor boot/lk images only
  carried sizes with address fields zeroed).
- `MemoryMapLib.c` is generated from this dump as an exact, **non-overlapping**
  partition of the 12 GiB bank: UEFI stack+FD (`BsData`), all 53 mblocks (`Reserv`),
  and the remaining free DRAM (`Conv`, ~10.85 GiB). Reserving every mblock is safe:
  launched after LK those regions belong to ATF/TEE/coprocessors and must not reach
  the payload; if UEFI replaced LK they are just unused.

  Key carveouts (full 53-entry table lives in `MemoryMapLib.c`):

  | Region | Base | Size |
  |---|---|---|
  | BL31 / ATF | `0x48800000` | 2 MiB |
  | TEE secmem / reserved / shm | `0xBF800000` / `0xEE200000` / `0x7FA00000` | 5 / 227 / 4 MiB |
  | GZ (GenieZone) + gz-log | `0x325C00000` | 32 MiB (+0.3) |
  | SCP / SSPM / MCUPM / VCP | `0xB8000000` / `0x9FD70000` / `0x7FEF0000` / `0xBF000000` | 10 / 2.5 / 1 / 8 MiB |
  | modem (md_mem Ã4, ap_md_*) | `0xCC000000`â¦`0xE9780000` | ~200 MiB |
  | ADSP / APU(apusys) | `0x78000000` / `0x8FA00000` | 25 / 6 MiB |
  | framebuffer / display-dbg | `0xFCC20000` / `0x6CC00000` | 52 / 32 MiB |
  | security booker / dice | `0x327F70000` | 384 MiB |
  | wifi dma-pool / connsys EMI | `0x88B00000` / `0x87E00000` | 20 / 13 MiB |
  | bootmonitor pmsg / pstore | `0x6EC00000` | 4.06 MiB |

- MMIO corrected against the same dump: **GICD is `0x0C400000`** (not
  `0x0C000000`), **GICR `0x0C440000`** (+2 MiB), and the **PMIC interface is SPMI
  `0x1CC04000`**. UART0 `0x11002000`, topckgen `0x10000000`, infracfg-ao
  `0x10001000`, apmixedsys `0x1000C000`, dramc `0x10230000`, UFS PHY `0x112A0000`,
  UFS HCI `0x112B0000`, USB0 `0x11200000`, display `0x14000000`, watchdog
  `0x1C00B000` all confirmed. AP watchdog (`watchdog@1c00b000`, `mediatek,mt6899-wdt`)
  is present and mapped.

## Present (in the fork)

- `Platforms/Xiaomi/rodinPkg/`: real `rodin.dsc`, `rodin.fdf`, `rodin.dec` (empty, as
  for other Xiaomi platforms), `DeviceBuild.py`, `Include/{APRIORI,DXE}.inc`,
  `Library/MemoryMapLib/`, derived from the `lancelot` (MT6768) MediaTek template.
- `Silicon/MediaTek/MT6899Pkg/`: `MT6899Pkg.dsc.inc` (GIC 0x0C000000 / 0x0C400000,
  8x Cortex-A725 single cluster, UFS) + Clock/Gpio/Msdc/PmicWrapper/PlatformSec Impl
  libraries, structurally derived from `MT6789Pkg`.
- `Resources/Configs/rodin.toml` (`[uefi_fd] base 0x40040000 size 0x200000`).
- `Resources/DTBs/rodin.dtb` â rodin vendor DTB.

## Still required before hardware boot

- Re-derive the `MT6899Pkg` library bodies (esp. the ~4k-line `ClockImplLib.c` clock
  tree) from the MT6899 TRM; the current bodies are the MT6789 register maps carried
  over as scaffolding only.
- Memory map is **done**: full 12 GiB layout (UEFI + 53 LK mblocks) is sourced from a
  live root dump and encoded in `MemoryMapLib.c` (see "Memory map" above).
- Re-derive the `[boot_image]` offsets in `rodin.toml` (dtb/second/tags/ramdisk) from
  the rodin vendor boot config; those are still lancelot placeholders.
- Drop `MT6358PmicDxe` / replace with the rodin PMIC driver once the PMIC IC is known;
  panel/touch/storage/display integration; BootShim / Android boot image handoff.

## Device inputs available locally (workspace)

- Stock `boot.img`, `vendor_boot.img`, `dtbo.img`, `gz.img`, `tee.img`, `spmfw.img`,
  `sspm.img`, `scp.img`, `lk.img`, `preloader_raw.img`, `gpt.bin` (under
  `turbo4ååº/` in the workspace).
- Extracted vendor DTB/DTS (`vendor_boot` dtb0, `dtbo` dtb0, `tee` dtb0, `rodin.dts`,
  `dtbo.dts`).
- Vendor DTB/DTS cache in UniXNU-rodin (`.cache/firmware/rodin-vendor.dtb/.dts`).

## Build (via the fork)

- Fork: `dianjianhang/uefi-sheng`, branch `sheng`.
- Manual: `bash ./setup_env.sh -p apt && python3 ./build_uefi.py -d rodin -r DEBUG -c`
  (or dispatch the `Build UEFI for rodin` workflow).

Then from UniXNU:
- `unixnu build firmware --uefi <Mu-Silicium checkout>`
- `unixnu bootimg --uefi <Mu-Silicium checkout>`


## Live-device verified hardware facts (root dump, 2026-11-21)

Rodin was booted with KernelSU root and queried over adb; the values below are
read directly from the running device and supersede any vendor-image guesses.

### SoC / platform
- PMIC: **MediaTek MT6363** (`pmic-ecid` compatible `mediatek,mt6363-efuse`; also `mt6363-temp`, `mt6363-vs-voter`).
- PMIC bus: **SPMI**, `spmi@1cc04000` (`mediatek,mt6899-spmi`). Sub-windows:
  `0x1CC04000+0x8FF`, `0x1CC01000+0x100`, `0x1C020000+0x8FF`, `0x1CC01800+0x100`, `0x1C014000+0x8FF`, `0x1CC08000+0x100`.
  The legacy pwrap register map in `PmicWrapperImplLib.c` is wrong for rodin and must be replaced by an SPMI-based implementation. `TODO(MT6899)`.
- GIC: distributor `0x0C400000` (0x40000), redistributor `0x0C440000` (0x200000), from `interrupt-controller@c400000/reg` (the earlier `0x0C000000` was wrong).
- Timer `arm,armv8-timer`; AP watchdog `watchdog@1c00b000` (`mediatek,mt6899-wdt`), active (`aee_aed.poffreason=AP_WDT`).

### Memory / boot
- Bank `memory@40000000` = `0x40000000` + `0x300000000` (12 GiB, single bank). 53 LK mblocks carry real base+size (see MemoryMapLib).
- Free DRAM window `0xA7000000..0xA8000000` hosts the Silicium UEFI runtime cluster.
- Stock `boot.img` = Android boot **header v4**, kernel-only (`ramdisk_size = 0`), no second stage; ramdisk lives in `init_boot`.
- Live cmdline: `bootopt=64S3,32N2,64N2`, `ramoops.mem_address=0x6ec00000 ramoops.mem_size=0x410000`, `hwid.project_name=rodin`, `ufshcd_core.poll_queues=0`, `initcall_blacklist=ufshcd_core_init`.

### Clock / register bases
MTK clock controllers are not device-tree nodes (kernel hardcodes them): topckgen `0x10000000`,
infracfg-ao `0x10001000`, apmixedsys `0x1000C000`, dramc `0x10230000`, UART0 `0x11002000`,
USB0 `0x11200000`, UFS PHY `0x112A0000`, UFS HCI `0x112B0000`, display `0x14000000`.

## BootShim / boot-header-v4 handoff (migrated)

Rodin now uses the same Silicium layout as marble / peridot / muyu:

```
[boot_shim]  requires_kernel_header = true
[uefi_fd]    base = 0xA7100000   size = 0x300000
[boot_image] header_version = 4
```

`build_uefi.py` concatenates `BootShim.bin + SILICIUM_UEFI.fd`, gzips it and writes a v4
boot image; BootShim (built from `BootShim/BootShim.S` with `FD_BASE`/`FD_SIZE`) runs from
the LK kernel load address, relocates the FD to `0xA7100000` and jumps to it. The matching
UEFI runtime cluster (FD, stack 0xA760D000, info block, page tables, log buffer, SEC/sched
heaps, FV region, UEFI reserve) is carved out of `0xA7000000..0xA8000000` in MemoryMapLib and
`PcdCPUCoresStackBase` = `0xA760D000`.

Status: builds green and boots on hardware to the end of DXE dispatch with a readable
framebuffer console (see "On-device boot results" below); PMIC/clock/UFS driver bodies
remain MT6899 scaffolds, so booting an OS is not yet possible.

## On-device boot results (2026-11-21, flashed to `boot_a`)

Flashed with `fastboot flash boot_a Mu-rodin.img` (`fastboot boot` RAM-boot is
unsupported on rodin: returns `unknown command`). The panel is DRM-only and there is
no UART, so all feedback comes from the UEFI DEBUG console rendered on screen.

Progression, one CI image per step:

1. The framebuffer memory-map region must be named **`Display Reserved`**:
   `FrameBufferSerialPortLib` and `SimpleFbDxe` both look it up by name. With the
   region named `framebuffer` nothing was rendered (blind splash).
2. The panel reports 1220x2712 but the hardware stride is **4992** (= 1248 * 4);
   `PcdFrameBufferWidth` must be **1248**, not 1220. At 1220 the console was a skewed
   colour stripe.
3. DxeCore needs a **`DXE Heap`** region (`Failed to locate DXE Heap memory region`).
   Added 124 MiB `DXE Heap` at `0x100000000` carved out of the 3 GiB free System RAM.
4. Every MMIO region length must be **4 KiB page aligned** for the MMU setup (UFS HCI
   `0x2A00` -> `0x3000`, WatchDog `0x100` -> `0x1000`); otherwise MMU configuration
   fails with `EFI_INVALID_PARAMETER`.
5. The shared MTK DXE drivers look up MMIO by **exact region name**: `GpioDxe` ->
   `Pinctrl`, `ClockDxe` -> `AP Mixed` / `Top Ck Gen` / `Infra Cfg AO`,
   `PmicWrapperDxe` -> `PMIC Wrapper`. Our regions were renamed to match
   (`Pinctrl/GPIO` -> `Pinctrl`, `Infracfg AO` -> `Infra Cfg AO`).
6. `MsdcDxe` (SD/eMMC) requires **GPIO + Clock + PMIC protocols** in its constructor
   and `ASSERT`s if any is missing. The PMIC protocol cannot exist yet (see below), so
   it always asserted. rodin is a UFS device, so the whole SD/eMMC stack
   (`MsdcDxe`, `EmmcDxe`, `SdDxe`) was dropped from `Include/DXE.inc` and
   `Include/APRIORI.inc`.
7. `RamManagerDxe` (Silicium) calls `GetUsableMemoryRanges()`, which looks for a QCOM
   **`Emi`** memory region (absent on MTK) and then frees an **uninitialised**
   `EFI_MEMORY_RANGE *` in its `cleanup` path -> `Data abort: Translation fault` at the
   very end of DXE dispatch. Dropped from `rodin.fdf`; `MemoryMapLib` already describes
   RAM. `TODO(MT6899)`: add an `Emi` region or fix the upstream uninitialised free if
   RAM-manager behaviour is ever needed.

Result: the panel shows a readable, correctly pitched UEFI DEBUG console. `GpioDxe`,
`ClockDxe`, `SimpleFbDxe` (GOP), `ConSplitterDxe`, `GraphicsConsoleDxe`, `SmbiosDxe`,
`AcpiTableDxe` all load. The only remaining failures are the PMIC wrapper and
`MT6358PmicDxe`, which fail cleanly: rodin's PMIC is SPMI-only (`spmi@1cc04000`,
MT6363) and `PmicWrapperImplLib.c` still holds the MT6789 *pwrap* register map, which is
wrong for rodin. `TODO(MT6899)`: replace it with an SPMI-based MT6363 driver.

