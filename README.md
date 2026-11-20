# UEFI Firmware Scaffold for rodin (MT6899)

This repository contains firmware bring-up scaffolding for Redmi Turbo 4 / POCO X7 Pro
(codename: `rodin`, SoC: MediaTek Dimensity 8400-Ultra / MT6899).

## Status

- **SoC/device context**: UniXNU-mt6899-common and UniXNU-rodin define the board/SoC configuration.
- **Minimal scaffold**: `src/main.c`, `board.toml`, `dts/rodin.dts` provide early boot-chain placeholders.
- **Mu-Silicium platform scaffold**: `Platforms/Xiaomi/rodinPkg/` adds initial `rodin.dsc/.fdf/.dec`,
  `DeviceBuild.py`, and include stubs as a starting point for integration into a Mu-Silicium checkout.
- **Build readiness**: not yet buildable for MT6899; silicon/platform drivers, memory map, and boot
  integration are still required (see `Platforms/Xiaomi/rodinPkg/PORTING.md`).

## Device inputs (available in workspace)

- Stock `boot.img`, `vendor_boot.img`, `gpt.bin` (workspace `turbo4分区/`)
- Vendor DTB/DTS cached in UniXNU-rodin (`.cache/firmware/rodin-vendor.dtb/.dts`)

## Next steps

1. Integrate `Platforms/Xiaomi/rodinPkg` into a Mu-Silicium/UniXNU firmware tree.
2. Implement MT6899 silicon/platform support (memory map, clocks, GPIO/PMIC, storage, display, USB, etc.).
3. Build firmware image: `unixnu build firmware --uefi <Mu-Silicium checkout>`.
4. Assemble boot image: `unixnu bootimg --uefi <Mu-Silicium checkout>` with required device inputs.
