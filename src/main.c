#include <Uefi.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiBootServicesTableLib.h>

/*
 * Minimal UEFI entry point for rodin (MT6899).
 * Prints a debug string and returns EFI_SUCCESS.
 * The real hand-off to the XNU kernelcache will be filled in
 * by the UniXNU payload generator once the ADT/identity is sorted.
 */
EFI_STATUS
EFI_UFI_ENTRY(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS Status;

    /* Basic initialization — print a marker so we know UEFI ran */
    Status = gBS->DebugPrint(
        DEBUG_INFO,
        "UEFI: rodin entry point — MT6899 A725 8-core, GICv3, PSCI smc\n"
    );
    if (EFI_ERROR(Status)) {
        return Status;
    }

    /* TODO: later steps will:
     *  - Set up the arm64 page tables from the vendor DTB
     *  - Load the kernelcache (sha256 above) at its entry address
     *  - Configure PSCI handoff to the XNU kernel
     *  - Return to the kernel with a proper exit context
     */

    return EFI_SUCCESS;
}
