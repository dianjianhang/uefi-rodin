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
  - `SILICIUM_UEFI.fd` — 0x200000 bytes, valid `_FVH` firmware-volume header
    (sha256 `7a4628ce7fbee6728d8864bf2e74e3ec0b744d0152843160f7c0eee5431dc68d`).
  - `Mu-rodin.img` — Android boot image (header v2), kernel = gzip(FD),
    `kernel_addr 0x40040000` (sha256 `7a88f515e02290a49349af47ae6d088a1c80f9c2810a02e3ac18ae1a448656fc`).

What made it link: the shared `MediaTekPkg` DXE drivers are never exercised by the
upstream CI matrix (no MediaTek device is `Active` in `Status.md`), so `MsdcDxe`
carried latent `-Wunused-but-set-variable` warnings that `-Werror` turns fatal. A
single platform-scoped `-Wno-unused-but-set-variable` was added to
`MT6899Pkg.dsc.inc` `[BuildOptions]`; no driver source is modified.

## Memory map (2026-11-21)

`Library/MemoryMapLib/MemoryMapLib.c` was extended and every region is now tied to
a concrete rodin data source (vendor DTB under `turbo4分区/`), commit `0f54550` on
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

High DRAM / firmware carveouts — **blocked on real data**:

- board.toml declares `dram_size = 0x300000000` (12 GiB) at base `0x40000000`, i.e.
  a contiguous `0x40000000..0x340000000` span. Only the low 1 GiB is described by
  any DTB we have.
- The vendor DTBs for `vendor_boot`, `dtbo` and `tee` all carry an **empty
  `/memreserve/`** table, and `gz.img` is the GenieZone hypervisor which computes
  DRAM size at runtime (`platform_memsize`, `total dram size`). There is **no
  static DRAM/TEE/ATF/SPM table** in the partition dumps.
- Therefore the remaining 11 GiB is mapped as a single `Reserv` placeholder
  (`High DRAM Reserve`, `0x80000000` + `0x2C0000000`). It is deliberately **not**
  handed to the OS: feeding unsourced DRAM to the payload would risk corrupting
  TEE/ATF. Replace it with the real per-region carveouts (and promote the leftover
  to `Conv`) once the bootloader **LK "reserved mblock"** table — or `/proc/iomem`
  from a booted rodin — is available.

## Present (in the fork)

- `Platforms/Xiaomi/rodinPkg/`: real `rodin.dsc`, `rodin.fdf`, `rodin.dec` (empty, as
  for other Xiaomi platforms), `DeviceBuild.py`, `Include/{APRIORI,DXE}.inc`,
  `Library/MemoryMapLib/`, derived from the `lancelot` (MT6768) MediaTek template.
- `Silicon/MediaTek/MT6899Pkg/`: `MT6899Pkg.dsc.inc` (GIC 0x0C000000 / 0x0C400000,
  8x Cortex-A725 single cluster, UFS) + Clock/Gpio/Msdc/PmicWrapper/PlatformSec Impl
  libraries, structurally derived from `MT6789Pkg`.
- `Resources/Configs/rodin.toml` (`[uefi_fd] base 0x40040000 size 0x200000`).
- `Resources/DTBs/rodin.dtb` — rodin vendor DTB.

## Still required before hardware boot

- Re-derive the `MT6899Pkg` library bodies (esp. the ~4k-line `ClockImplLib.c` clock
  tree) from the MT6899 TRM; the current bodies are the MT6789 register maps carried
  over as scaffolding only.
- Finish the memory map: the high 11 GiB bank and the TEE/ATF/SPM/SSPM/SCP/display
  carveouts need the bootloader (LK) DRAM table; the low window, bootmonitor carveout
  and AP watchdog are already sourced (see "Memory map" above).
- Re-derive the `[boot_image]` offsets in `rodin.toml` (dtb/second/tags/ramdisk) from
  the rodin vendor boot config; those are still lancelot placeholders.
- Drop `MT6358PmicDxe` / replace with the rodin PMIC driver once the PMIC IC is known;
  panel/touch/storage/display integration; BootShim / Android boot image handoff.

## Device inputs available locally (workspace)

- Stock `boot.img`, `vendor_boot.img`, `dtbo.img`, `gz.img`, `tee.img`, `spmfw.img`,
  `sspm.img`, `scp.img`, `gpt.bin` (under `turbo4分区/` in the workspace).
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
