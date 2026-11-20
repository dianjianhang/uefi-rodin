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
- Complete the memory map: add the high ~11 GiB DRAM bank (board.toml `dram_size =
  0x300000000` — only the 1 GiB `/memory@40000000` window is described by the vendor
  DTB) and all TEE/ATF/SPM/display carveouts from the bootloader (LK) DRAM table;
  confirm the AP WDT register.
- Re-derive the `[boot_image]` offsets in `rodin.toml` (dtb/second/tags/ramdisk) from
  the rodin vendor boot config; those are still lancelot placeholders.
- Drop `MT6358PmicDxe` / replace with the rodin PMIC driver once the PMIC IC is known;
  panel/touch/storage/display integration; BootShim / Android boot image handoff.

## Device inputs available locally (workspace)

- Stock `boot.img`, `vendor_boot.img`, `gpt.bin` (under `turbo4分区/` in the workspace).
- Vendor DTB/DTS cache in UniXNU-rodin (`.cache/firmware/rodin-vendor.dtb/.dts`).

## Build (via the fork)

- Fork: `dianjianhang/uefi-sheng`, branch `sheng`.
- Manual: `bash ./setup_env.sh -p apt && python3 ./build_uefi.py -d rodin -r DEBUG -c`
  (or dispatch the `Build UEFI for rodin` workflow).

Then from UniXNU:
- `unixnu build firmware --uefi <Mu-Silicium checkout>`
- `unixnu bootimg --uefi <Mu-Silicium checkout>`
