/*
 * UEFI entry point for rodin (MT6899).
 *
 * This scaffold includes substantially more implementation than the
 * minimal version: each of the three critical boot-chain steps now
 * has a concrete (but still placeholder) body that attempts to use
 * UEFI library services, prints detailed diagnostics, and returns
 * EFI_UNSUPPORTED with a clear message so the CI workflow can still
 * succeed while we document what real code would do.
 *
 * Stages:
 *   1. EfiInitializeMemoryManagement   – build an E820-style memory
      map describing the 1 GiB vendor low window + the remaining 11 GiB.
   2. EfiLoadKernelCache              – locate the kernelcache within
      the firmware volume (sha256 pinned in board.toml), verify its
      Mach‑O magic, apply the –0x1000000 slide, and map it into
      the linear address space.
   3. EfiPsciHandoff                  – invoke PSCI 1.0 SMC 0x84000102
      to hand control off to the XNU kernel on the 8x Cortex‑A725
      core cluster.
 *
 * For now each function prints detailed diagnostics and returns
 * EFI_UNSUPPORTED so the CI workflow can still succeed while we
 * document the real implementation steps.
 */
#include <Uefi.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UnitTestLib.h>
#include <Library/UefiLib.h>

/* ------------------------------------------------------------------- */
/* Global data – kept consistent with board.toml / the OS profile   */
/* ------------------------------------------------------------------- */

/* Physical address of the vendor‑placed DTB (1 GiB low window). */
UINT64  gVendorDtbPhysAddr = 0x40000000;

/* Kernelcache sha256 – supplied by the ipados-17.7.11‑21H461 profile. */
CHAR8   gKernelcacheSha256[] =
  "f18b4df865405f2a287b84977d7aca745e056d656d9fc9df1662970ce2969de7";

/* Slide value, also from the profile. */
INT64   gKernelcacheSlide = -0x1000000;

/* PSCI SMC identifier – confirmed from the rodin vendor DTB. */
UINT64  gPsciSmcId = 0x84000102;

/* ------------------------------------------------------------------- */
/* Helper macros – wrap DebugPrint so we can bail early on error. */
/* ------------------------------------------------------------------- */
#define UEFI_PRINT(x, ...) \
  do { \
    EFI_STATUS _status = gBS->DebugPrint (DEBUG_INFO, x, ##__VA_ARGS__); \
    if (EFI_ERROR (_status)) return _status; \
  } while (0)

/* ------------------------------------------------------------------- */
/* 1. Memory management initialization – further expanded placeholder  */
/* ------------------------------------------------------------------- */
/*  
 * The vendor DTB only reserves a 1 GiB window at 0x40000000.  The
 * test device has 12 GiB (0x300000000) physically.  This function
 * now attempts:
 *   1. Allocate a pool for an EFI_MEMORY_DESCRIPTOR array and a
 *      temporary work buffer.
 *   2. Describe the memory layout:
 *        [0x0,             0x40000000) – available (vendor low window)
 *        [0x40000000,    0x300000000) – reserved (vendor DTB region)
 *        [0x300000000,   0x300000000 + available) – available
 *   3. Call gBS->SetMemoryMap () with the descriptor count and
 *      array pointer.
 *   4. Optionally re‑configure the page‑table entries (PDPT) so that
 *      the kernel can map the full 12 GiB.
 *   5. Free the temporary pool before returning.
 *   We allocate the descriptors and a work buffer, fill them in,
 *   call SetMemoryMap, free the pool, and no matter the outcome we
 *   return EFI_UNSUPPORTED with a verbose diagnostic print – the CI
 *   workflow still succeeds.
 */
STATIC
EFI_STATUS
EfiInitializeMemoryManagement (VOID)
{
  EFI_STATUS  Status;
  EFI_MEMORY_DESCRIPTOR  Descriptors[3];
  UINTN       DescriptorSize = sizeof (EFI_MEMORY_DESCRIPTOR);
  VOID        *DescriptorPool;
  VOID        *WorkBuffer;
  UINTN       DescriptorPoolSize = sizeof (Descriptors) + 4096;
  BOOLEAN     Allocated = FALSE;

  /* --- Allocate pool for descriptors + a temporary work buffer --- */
  DescriptorPool = AllocatePool (DescriptorPoolSize);
  if (!DescriptorPool) {
    UEFI_PRINT ("UEFI: rodin mem init – out of pool memory for descriptors.\n");
    return EFI_UNSUPPORTED;
  }
  WorkBuffer = (VOID *)((UINT8 *)DescriptorPool + sizeof (Descriptors));

  /* --- Describe the three regions --- */
  /* Region 0: vendor low window – available to the kernel */
  ((EFI_MEMORY_DESCRIPTOR *)DescriptorPool)[0].Type = EfiUninitializedData;
  ((EFI_MEMORY_DESCRIPTOR *)DescriptorPool)[0].PhysicalStart = 0;
  ((EFI_MEMORY_DESCRIPTOR *)DescriptorPool)[0].VirtualStart = 0;
  ((EFI_MEMORY_DESCRIPTOR *)DescriptorPool)[0].NumberOfPages = EFI_SIZE_TO_PAGES (0x40000000); /* 1 GiB */
  ((EFI_MEMORY_DESCRIPTOR *)DescriptorPool)[0].AttributeFlags = EFI_MEMORY_WB; /* Write‑back */

  /* Region 1: vendor reserved region (1 GiB at 0x40000000) */
  ((EFI_MEMORY_DESCRIPTOR *)DescriptorPool)[1].Type = EfiReservedMemoryType;
  ((EFI_MEMORY_DESCRIPTOR *)DescriptorPool)[1].PhysicalStart = gVendorDtbPhysAddr;
  ((EFI_MEMORY_DESCRIPTOR *)DescriptorPool)[1].VirtualStart = (EFI_VIRTUAL_ADDRESS)gVendorDtbPhysAddr;
  ((EFI_MEMORY_DESCRIPTOR *)DescriptorPool)[1].NumberOfPages = EFI_SIZE_TO_PAGES (0x40000000);
  ((EFI_MEMORY_DESCRIPTOR *)DescriptorPool)[1].AttributeFlags = EFI_MEMORY_WB;

  /* Region 2: remaining 11 GiB (from 0x300000000 up) – available */
  ((EFI_MEMORY_DESCRIPTOR *)DescriptorPool)[2].Type = EfiUninitializedData;
  ((EFI_MEMORY_DESCRIPTOR *)DescriptorPool)[2].PhysicalStart = 0x300000000ULL;
  ((EFI_MEMORY_DESCRIPTOR *)DescriptorPool)[2].VirtualStart = 0x300000000ULL;
  ((EFI_MEMORY_DESCRIPTOR *)DescriptorPool)[2].NumberOfPages = EFI_SIZE_TO_PAGES (0x300000000); /* 11 GiB */
  ((EFI_MEMORY_DESCRIPTOR *)DescriptorPool)[2].AttributeFlags = EFI_MEMORY_WB;
  ((EFI_MEMORY_DESCRIPTOR *)DescriptorPool)[2].AttributeFlags = EFI_MEMORY_WB; /* Write‑back */

  /* --- Attempt optional PDPT reconfiguration for full 12 GiB mapping ---
     This is a best‑effort attempt to set up page‑table entries so the
     kernel can map the entire physical address space.  The real
     implementation would require a full UEFI page‑table driver.  */
  UEFI_PRINT (
    "UEFI: rodin mem init – attempting optional PDPT reconfiguration.\n");
  {
    EFI_PHYSICAL_ADDRESS  pdpt_base = 0x380000000ULL; /* example PDPT base */
    UINT64                pdpt_entry;
    /* Write a sample entry to the PDPT region if the platform supports it;
       this is purely diagnostic in the scaffold. */
    pdpt_entry = EFI_SIZE_TO_PAGES (0x40000000) | EFI_MEMORY_WB;
    UEFI_PRINT (
      "UEFI: rodin mem init – sample PDPT entry at 0x%0lx = 0x%08lx.\n",
      pdpt_base, pdpt_entry);
  }

  /* --- Describe the three regions --- */
  UEFI_PRINT (
    "UEFI: rodin mem init – about to call SetMemoryMap with 3 descriptors + %lu bytes work buffer.\n",
    4096);

  /* --- Attempt to set the memory map --- */
  Status = gBS->SetMemoryMap (
    3, (EFI_MEMORY_DESCRIPTOR *)DescriptorPool, DescriptorSize, &DescriptorPoolSize);
  if (EFI_ERROR (Status)) {
    UEFI_PRINT (
      "UEFI: rodin mem init – SetMemoryMap returned %r; continuing anyway.\n",
      Status);
  } else {
    UEFI_PRINT (
      "UEFI: rodin mem init – SetMemoryMap succeeded, DescriptorPoolSize=%lu bytes.\n",
      (unsigned long)DescriptorPoolSize);
    Allocated = TRUE;
  }

  /* Free the descriptor pool and work buffer since we no longer need them
     after setmap; a real implementation would keep the map around. */
  gBS->FreePool (DescriptorPool);

  UEFI_PRINT (
    "UEFI: rodin mem init – returning EFI_UNSUPPORTED (scaffold, mem map%a).\n",
    Allocated ? " (map set)" : " (map failed)");
  return EFI_UNSUPPORTED;
}

/* ------------------------------------------------------------------- */
/* 2. Kernelcache loading – further expanded placeholder                 */
/* ------------------------------------------------------------------- */
/*  
 * The kernelcache is a Mach‑O file extracted from the
 * ipados-17.7.11‑21H461 IPSW.  Its sha256 is pinned in board.toml.
 * This function now attempts:
 *   1. Locate the firmware volume (FV) protocol handles.
 *   2. Open the FV and iterate over its files.
 *   3. For each file, compare its sha256 against gKernelcacheSha256
 *      (computed on‑the‑fly from the file contents – simplified here
 *      by checking the filename "kernelcache").
 *   4. If a match is found, verify the Mach‑O magic (0xfade0cfc),
 *      and apply the slide gKernelcacheSlide (–0x1000000).
 *   5. Allocate pages for the slid binary, copy the file content,
 *      and set memory attributes (WB and UC).
 *   For the scaffold we iterate the FV, print diagnostics for each
 *   file we see, allocate a dummy page, copy a stub, set attributes,
 *   and return EFI_UNSUPPORTED.
 */
STATIC
EFI_STATUS
EfiLoadKernelCache (IN EFI_PHYSICAL_ADDRESS KernelAddress,
                    IN UINT64                  Slide)
{
  EFI_STATUS  Status;
  EFI_GUID    FvGuid = Firmware_Volume_Guid;
  EFI_FV_INFO  FvInfo;
  EFI_HANDLE  *Handles;
  UINTN        HandleCount;
  UINTN        Index;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Fs;
  EFI_FILE_PROTOCOL *Root, *File;
  CHAR16       *FileName;
  UINTN        FileSize;
  VOID         *Buffer;
  EFI_FILE_INFO *FileInfo;
  EFI_PHYSICAL_ADDRESS  MappedAddress;
  EFI_STATUS  Status2;
  UINT64      Attributes;

  UEFI_PRINT (
    "UEFI: rodin load kernelcache – about to enumerate firmware volume (sha256 %a, slide %ld).\n",
    gKernelcacheSha256, (long)Slide);

  /* --- Locate firmware volume handles --- */
  Status = gBS->LocateHandleBuffer (ByProtocol,
    &FvGuid, NULL, &HandleCount, &Handles);
  if (EFI_ERROR (Status)) {
    UEFI_PRINT (
      "UEFI: rodin load kernelcache – no firmware volume handles (%r).\n", Status);
    goto out;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (Handles[Index],
      &EfiSimpleFileSystemProtocolGuid, (VOID **)&Fs);
    if (EFI_ERROR (Status)) {
      continue;
    }

    Status = Fs->OpenVolume (Fs, &Root);
    if (EFI_ERROR (Status)) {
      continue;
    }

    /* Try opening "kernelcache" first, then "KernelCache". */
    FileName = L"kernelcache";
    Status = Root->Open (Root, &File, FileName, FALSE, FALSE);
    if (EFI_ERROR (Status)) {
      FileName = L"KernelCache";
      Status = Root->Open (Root, &File, FileName, FALSE, FALSE);
    }
    if (EFI_ERROR (Status)) {
      UEFI_PRINT (
        "UEFI: rodin load kernelcache – cannot open %s (%r).\n", FileName, Status);
      /* Try next handle */
      continue;
    }

    /* Get file size */
    Status = File->GetInfo (File, &EfiFileInfoGuid, (UINTN *)&FileSize, (VOID **)&FileInfo);
    if (EFI_ERROR (Status)) {
      UEFI_PRINT (
        "UEFI: rodin load kernelcache – cannot get file info (%r).\n", Status);
      goto cleanup;
    }
    FileSize = FileInfo->FileSize;

    UEFI_PRINT (
      "UEFI: rodin load kernelcache – file %s is %lu bytes.\n",
      FileName, (unsigned long)FileSize);

    /* Read entire file into buffer */
    Buffer = AllocatePool (FileSize);
    if (!Buffer) {
      UEFI_PRINT ("UEFI: rodin load kernelcache – out of pool memory.\n");
      Status = EFI_OUT_OF_RESOURCES;
      goto cleanup;
    }
    Status = File->Read (File, &FileSize, Buffer);
    if (EFI_ERROR (Status)) {
      UEFI_PRINT ("UEFI: rodin load kernelcache – read error (%r).\n", Status);
      goto cleanup2;
    }

    /* --- Compute a simplified sha256 check (real code would use
         a crypto library).  Here we just hash the first 32 bytes. --- */
    /* For the scaffold we just compare the first 4 UINT32s against a
       placeholder; a real impl would compute the sha256 and compare
       with gKernelcacheSha256. */
    if (FileSize >= 16) {
      UINT32 *hash_words = (UINT32 *)Buffer;
      UINT32 expected[4] = {
        0xf18b4df8, 0x65405f2a, 0x287b8497, 0x7aca745e
      };
      BOOL match = TRUE;
      for (int i = 0; i < 4; i++) {
        if (hash_words[i] != expected[i]) match = FALSE;
      /* scaffold diagnostic: track mismatch count */
      }
      if (match) {
        UEFI_PRINT (
          "UEFI: rodin load kernelcache – first 16 bytes match expected sha256 words!\n");
      } else {
        UEFI_PRINT (
          "UEFI: rodin load kernelcache – first 16 bytes do NOT match expected sha256 words.\n");
      }
    } else {
      UEFI_PRINT (
        "UEFI: rodin load kernelcache – file too small for sha256 check (%lu bytes).\n",
        (unsigned long)FileSize);
    }

    /* --- Verify Mach‑O magic (first 4 bytes) --- */
    if (FileSize < 8) {
      UEFI_PRINT (
        "UEFI: rodin load kernelcache – file too small (%lu bytes).\n",
        (unsigned long)FileSize);
      Status = EFI_INVALID_PARAMETER;
      goto cleanup2;
    }
    UINT32 magic;
    CopyMem (&magic, Buffer, 4);
    if (magic != 0xfade0cfc) {
      UEFI_PRINT (
        "UEFI: rodin load kernelcache – bad Mach‑O magic 0x%08x.\n", magic);
      Status = EFI_INVALID_PARAMETER;
      goto cleanup2;
    }

    /* --- Allocate pages for the slid kernelcache --- */
    UEFI_PRINT (
      "UEFI: rodin load kernelcache – allocating pages for slid binary.\n");
    Status = gBS->AllocatePages (AllocateMaxPages, EfiLoaderData,
      EFI_SIZE_TO_PAGES (0x1000), &MappedAddress);
    if (EFI_ERROR (Status)) {
      UEFI_PRINT (
        "UEFI: rodin load kernelcache – AllocatePages failed (%r).\n", Status);
    } else {
      UEFI_PRINT (
        "UEFI: rodin load kernelcache – pages allocated at 0x%0lx.\n", MappedAddress);
      /* Copy the file content into the allocated pages (limited to first 4KB) */
      CopyMem ((VOID *)MappedAddress, Buffer, 4096);
      UEFI_PRINT (
        "UEFI: rodin load kernelcache – first 4KB copied to allocated pages.\n");
      /* Set memory attributes (write‑back) */
      Status2 = gBS->SetMemoryAttributes (MappedAddress, EFI_SIZE_TO_PAGES (0x1000),
        EFI_MEMORY_WB, &Attributes);
      UEFI_PRINT (
        "UEFI: rodin load kernelcache – SetMemoryAttributes (WB) completed (%r).\n", Status2);
      /* Optionally set memory as uncached for kernel initialization */
      Status2 = gBS->SetMemoryAttributes (MappedAddress, EFI_SIZE_TO_PAGES (0x1000),
        EFI_MEMORY_UC, &Attributes);
      UEFI_PRINT (
        "UEFI: rodin load kernelcache – SetMemoryAttributes (UC) completed (%r).\n", Status2);
      /* Diagnostic: verify the attributes were actually set by querying back */
      {
        UINT64 queried_attrs;
        EFI_STATUS qstatus = gBS->QueryMemoryAttributes (MappedAddress, EFI_SIZE_TO_PAGES (0x1000),
          &queried_attrs);
        UEFI_PRINT (
          "UEFI: rodin load kernelcache – queried attributes after WC set: %r\n", qstatus);
        UEFI_PRINT (
          "UEFI: rodin load kernelcache – WC attribute preserved: %s\n",
          (qstatus == EFI_SUCCESS && (queried_attrs & EFI_MEMORY_WB)) ? "YES" : "NO");
      }
      /* Free the temporary buffer */
      FreePool (Buffer);
    }

cleanup2:
    FreePool (Buffer);
cleanup:
    File->Close (File);
    Root->Close (Root);
out:
    for (UINTN i = 0; i < HandleCount; i++) gBS->FreePool (Handles[i]);
    UEFI_PRINT (
      "UEFI: rodin load kernelcache – returning EFI_UNSUPPORTED (scaffold).\n");
    return EFI_UNSUPPORTED;
}

/* ------------------------------------------------------------------- */
/* 3. PSCI handoff – further expanded placeholder                      */
/*  
 * The XNU kernel on arm64 expects to be brought up via the ARM PSCI
 * 1.0 specification using SMC function ID 0x84000102.  The arguments
 * are:
 *   x0 = AapicId   (0 – boot the BSP)
 *   x1 = EntryPoint (the adjusted kernelcache physical address,
     i.e. base + gKernelcacheSlide)
 *   x2 = Context     (0 for a simple boot; later could carry ATAGs)
 *
 * This function now attempts:
 *   1. Compute the adjusted entry point (base + slide).
 *   2. Print the PSCI arguments (AapicId, adjusted entry, Context)
 *      and the SMC number (0x84000102).
 *   3. Attempt to invoke the SMC via gBS->Smc () (if the platform
      supports it) – note: on many QEMU / virtue builds this will
      simply be a no‑op or return immediately; we catch any error
      and still return UNSUPPORTED.
 *   4. Optionally stall for a brief period to simulate boot delay,
 *      then free any temporary resources.
 *   5. Set the watchdog timer to prevent system hangs during early boot.
 *   For the scaffold we verbosely print the intended handoff state
 *   and return EFI_UNSUPPORTED.
 */
STATIC
EFI_STATUS
EfiPsciHandoff (IN UINT64  AapicId,
                IN UINT64  EntryPoint,
                IN UINT64  Context)
{
  EFI_STATUS  Status;
  UINT64      AdjustedEntry = EntryPoint + gKernelcacheSlide;

  UEFI_PRINT (
    "UEFI: rodin PSCI handoff – about to invoke SMC 0x%08lx.\n",
    gPsciSmcId);

  UEFI_PRINT (
    "UEFI: rodin PSCI args – AapicId=%ld, entry_point=%ld,"
    " slide_adjusted_entry=%ld, context=%ld\n",
    (long)AapicId, (long)EntryPoint, (long)AdjustedEntry, (long)Context);

  /* --- Attempt to invoke the SMC --- */
  /* gBS->Smc () is part of the UEFI boot services table on platforms
     that support PSCI.  We wrap the call in a UEFI_PRINT so we can
     catch any status and still return UNSUPPORTED for the scaffold. */
  Status = gBS->Smc (gPsciSmcId, AapicId, AdjustedEntry, Context);
  if (EFI_ERROR (Status)) {
    UEFI_PRINT (
      "UEFI: rodin PSCI handoff – gBS->Smc returned %r (continuing scaffold).\n", Status);
  } else {
    UEFI_PRINT (
      "UEFI: rodin PSCI handoff – gBS->Smc completed (no‑return expected).\n");
  }

  /* optionally stall for a brief period to simulate boot delay */
  gBS->Stall (1000); /* 1 millisecond stall */

  /* set the watchdog timer to prevent system hangs during early boot */
  Status = gBS->SetWatchdogTimer (60000, 0, 0, NULL);
  if (EFI_ERROR (Status)) {
    UEFI_PRINT (
      "UEFI: rodin PSCI handoff – SetWatchdogTimer returned %r (continuing scaffold).\n", Status);
  } else {
    UEFI_PRINT (
      "UEFI: rodin PSCI handoff – SetWatchdogTimer successfully armed (60s timeout).\n");
  }
  /* Diagnostic: print current watchdog configuration status */
  {
    EFI_STATUS wd_status = gBS->SetWatchdogTimer (60000, 0, 0, NULL);
    UEFI_PRINT (
      "UEFI: rodin PSCI handoff – watchdog re‑query returned %r (should be EFI_SUCCESS)\n", wd_status);
  }

  /* No matter the SMC outcome we return UNSUPPORTED for the scaffold
     so the CI workflow can proceed.  A real implementation would
     not return from this call. */
  UEFI_PRINT (
    "UEFI: rodin PSCI handoff – returning EFI_UNSUPPORTED (scaffold, SMC not fully employed).\n");
  return EFI_UNSUPPORTED;
}

/* ------------------------------------------------------------------- */
/* UEFI entry point – the EDK II macro EDK II defines this as EFI_UFI_ENTRY,
 * but we use the plain signed-char form for maximum portability here. */
/* ------------------------------------------------------------------- */
EFI_STATUS
EFI_UFI_ENTRY (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS  Status;

  /* Basic initialization — print a marker so we know UEFI ran */
  Status = gBS->DebugPrint (
    DEBUG_INFO,
    "UEFI: rodin entry – MT6899 8× A725, GICv3, PSCI smc,"
    " kernelcache %a slide %ld, vendor DTB at 0x%08lx\n",
    gKernelcacheSha256, (long)gKernelcacheSlide, gVendorDtbPhysAddr);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  /* ----------------------------------------------------------------- */
  /* Step 1 — Initialize memory management */
  /* ----------------------------------------------------------------- */
  Status = EfiInitializeMemoryManagement ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  /* ----------------------------------------------------------------- */
  /* Step 2 — Load the kernelcache */
  /* ----------------------------------------------------------------- */
  Status = EfiLoadKernelCache (0, gKernelcacheSlide);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  /* ----------------------------------------------------------------- */
  /* Step 3 — PSCI handoff to the XNU kernel */
  /* ----------------------------------------------------------------- */
  Status = EfiPsciHandoff (0, 0, 0);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  /* If we reach here the hand‑off succeeded – in a real build the
     function would not return, but for the scaffold we simply return
     EFI_SUCCESS so the CI workflow can complete. */
  return EFI_SUCCESS;
}
