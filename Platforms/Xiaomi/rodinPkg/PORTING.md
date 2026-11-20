# rodinPkg (MT6899) Porting Notes

Scaffold status for Redmi Turbo 4 / POCO X7 Pro (rodin, MediaTek Dimensity 8400-Ultra).

## Present
- `rodin.dsc`, `rodin.fdf`, `rodin.dec`, `DeviceBuild.py` minimal stubs.
- Include lists (`APRIORI.inc`, `DXE.inc`, `RAW.inc`) are placeholders.
- Basic SMBIOS/framebuffer placeholders.
- **`Silicon/MediaTek/MT6899Pkg/`** - MediaTek MT6899 SoC package skeleton (Clock/Gpio/
  Msdc/Pmic/PlatformSec Impl libraries + `MT6899Pkg.dsc.inc`), structurally derived from
  `MT6789Pkg`. `MT6899Pkg.dsc.inc` PCDs are sourced from the vendor DTB (GIC 0x0C000000 /
  0x0C400000, 8x A725 single cluster, UFS). The library bodies (esp. the 4k-line
  `ClockImplLib.c` clock tree) are still MT6789 register maps and MUST be re-derived from
  the MT6899 TRM before any hardware use.
- **`Platforms/Xiaomi/rodinPkg/Library/MemoryMapLib/`** - device memory map derived from
  the rodin vendor DTB: low 1 GiB DDR window (`/memory@40000000`) plus real MT6899 register
  regions (GIC, topckgen, pinctrl, pwrap, dramc, UART0, MSDC-0, UFS PHY/HCI, USB xHCI,
  display, systimer, watchdog). Wired into `rodin.dsc` via `MemoryMapLib|...`.

## Required for a buildable Mu-Silicium platform
- Re-derive `MT6899Pkg` library bodies (Clock/Gpio/Msdc/Pmic) from the MT6899 TRM; the
  current bodies are the MT6789 register maps carried over as scaffolding only.
- Complete the memory map: add the high ~11 GiB DRAM bank (board.toml `dram_size`) and all
  TEE/ATF/SPM/display carveouts from the bootloader (LK) DRAM table; confirm AP WDT register.
- Add `ConfigurationMapLib`, fill `rodin.fdf` / `APRIORI.inc` / `DXE.inc` / `RAW.inc`, and
  `DeviceBuild.py` components (SEC/PEI/DXE) once the MT6899 silicon package is real.
- Panel/touch/storage/display driver integration and device-specific configuration.
- BootShim / Android boot image handoff integration as needed for this device.

## Device inputs available locally (workspace)
- Stock `boot.img`, `vendor_boot.img`, `gpt.bin` (under `turbo4分区/` in workspace).
- Vendor DTB/DTS cache in UniXNU-rodin (`.cache/firmware/rodin-vendor.dtb/.dts`).

## Build (target, once platform is complete)
From a Mu-Silicium checkout with this platform integrated:
- `stuart_setup -c Platforms/Xiaomi/rodinPkg/DeviceBuild.py`
- `stuart_update -c Platforms/Xiaomi/rodinPkg/DeviceBuild.py`
- `stuart_build -c Platforms/Xiaomi/rodinPkg/DeviceBuild.py`

Then from UniXNU:
- `unixnu build firmware --uefi <Mu-Silicium checkout>`
- `unixnu bootimg --uefi <Mu-Silicium checkout>`
