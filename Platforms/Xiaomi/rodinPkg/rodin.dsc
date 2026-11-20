##
#  Copyright (c) 2024-2026, UniXNU contributors.
#  SPDX-License-Identifier: BSD-2-Clause-Patent
##

################################################################################
# Rodin (Redmi Turbo 4 / POCO X7 Pro, MT6899) - Mu-Silicium platform scaffold
# Status: WIP. MT6899 silicon support, memory map, clocks, display, storage,
# and boot chain integration require bring-up and verification on hardware.
################################################################################

[Defines]
  PLATFORM_NAME                  = rodin
  PLATFORM_GUID                  = 9c3f7a21-6d4e-4b8a-a2c1-0e5f6d7c8b9a
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/rodinPkg
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = rodinPkg/rodin.fdf
  USE_CUSTOM_DISPLAY_DRIVER      = 0

  DEFINE RODIN_VENDOR_HW         = 0

  # SoC/board identifiers (bring-up placeholders)
  SOC_TYPE                       = MT6899

!include SiliciumPkg/SiliciumPkg.dsc.inc
# TODO: include MT6899-specific silicon package once available
# !include MT6899Pkg/MT6899Pkg.dsc.inc
# !include MediaTekPkg/MediaTekPkg.dsc.inc

[PcdsFixedAtBuild]
  #
  # DDR Memory (placeholder - must match bootloader map)
  # Vendor DTB reserves 1 GiB at 0x40000000; full 12 GiB map is unverified.
  #
  gArmTokenSpaceGuid.PcdSystemMemoryBase|0x40000000
  gArmTokenSpaceGuid.PcdSystemMemorySize|0x40000000

  #
  # UEFI Stack (placeholder)
  #
  gArmPlatformTokenSpaceGuid.PcdCPUCoresStackBase|0xA760D000
  gArmPlatformTokenSpaceGuid.PcdCPUCorePrimaryStackSize|0x40000

  #
  # SMBIOS / device identity
  #
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemManufacturer|"Xiaomi"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemModel|"rodin"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemRetailModel|"24129RT7CC"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemRetailSku|"rodin"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemBoardModel|"rodin"

  #
  # Framebuffer (bring-up placeholder; panel params TBD from vendor LCM)
  #
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferWidth|1220
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferHeight|2712
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferColorDepth|32
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferFontScale|2
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferDelay|0

[LibraryClasses]
  #
  # Memory map / platform SEC (stubs - to be implemented)
  #
  # MemoryMapLib|rodinPkg/Library/MemoryMapLib/MemoryMapLib.inf
  # ConfigurationMapLib|rodinPkg/Library/ConfigurationMapLib/ConfigurationMapLib.inf
  # PlatformSecLib|rodinPkg/Library/PlatformSecLib/PlatformSecLib.inf

[Components]
  #
  # TODO: Add SEC/PEI/DXE components for MT6899 bring-up
  #
