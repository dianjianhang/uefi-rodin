# rodinPkg (MT6899) Porting Notes

Scaffold status for Redmi Turbo 4 / POCO X7 Pro (rodin, MediaTek Dimensity 8400-Ultra).

## Present
- `rodin.dsc`, `rodin.fdf`, `rodin.dec`, `DeviceBuild.py` minimal stubs.
- Include lists (`APRIORI.inc`, `DXE.inc`, `RAW.inc`) are placeholders.
- Basic SMBIOS/framebuffer placeholders; memory map is a conservative stub.

## Required for a buildable Mu-Silicium platform
- MT6899 silicon package support (clocks, GPIO, PMIC, storage/UFS, USB, display, etc.).
  Current Mu-Silicium/uefi-sheng tree has limited MediaTek silicon packages (MT6768/MT6789/MediaTekPkg);
  MT6899 bring-up is not present.
- Verified memory map (bootloader/DRAM regions, reserved areas) and stack/heap layout.
- Platform SEC/PEI/DXE libraries and drivers (MemoryMapLib, ConfigurationMapLib, PlatformSecLib, etc.).
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
