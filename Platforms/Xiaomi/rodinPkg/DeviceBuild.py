##
# Copyright (c) 2024-2026, UniXNU contributors.
# SPDX-License-Identifier: BSD-2-Clause-Patent
##

import datetime
import logging
import os

from edk2toolext.environment import shell_environment
from edk2toolext.environment.uefi_build import UefiBuilder
from edk2toolext.invocables.edk2_platform_build import BuildSettingsManager
from edk2toolext.invocables.edk2_pr_eval import PrEvalSettingsManager
from edk2toolext.invocables.edk2_setup import RequiredSubmodule, SetupSettingsManager
from edk2toolext.invocables.edk2_update import UpdateSettingsManager
from edk2toolext.invocables.edk2_parse import ParseSettingsManager

# ####################################################################################### #
#                                Common Configuration                                     #
# ####################################################################################### #
class CommonPlatform():
    PackagesSupported = ("rodinPkg",)
    ArchSupported = ("AARCH64",)
    TargetsSupported = ("DEBUG", "RELEASE")
    Scopes = ('rodin', 'gcc_aarch64_linux', 'edk2-build')
    WorkspaceRoot = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    PackagesPath = (
        "Platforms/Xiaomi",
        "Common/Mu",
        "Common/Mu_OEM_Sample",
        "Mu_Basecore",
        "Silicon/MediaTek",
        "Silicon/Silicium",
        "Silicium-ACPI",
    )

# ####################################################################################### #
#                         Configuration for Update & Setup                                #
# ####################################################################################### #
class SettingsManager(UpdateSettingsManager, SetupSettingsManager, PrEvalSettingsManager, ParseSettingsManager):

    def GetPackagesSupported(self):
        return CommonPlatform.PackagesSupported

    def GetArchitecturesSupported(self):
        return CommonPlatform.ArchSupported

    def GetTargetsSupported(self):
        return CommonPlatform.TargetsSupported

    def GetRequiredSubmodules(self):
        return [
            RequiredSubmodule("Binaries", True),
            RequiredSubmodule("Common/Mu", True),
            RequiredSubmodule("Common/Mu_OEM_Sample", True),
            RequiredSubmodule("Mu_Basecore", True),
            RequiredSubmodule("Silicium-ACPI", True),
        ]

    def SetArchitectures(self, list_of_requested_architectures):
        unsupported = set(list_of_requested_architectures) - set(self.GetArchitecturesSupported())
        if len(unsupported) > 0:
            raise Exception("Unsupported Architecture Requested: " + " ".join(unsupported))
        self.ActualArchitectures = list_of_requested_architectures

    def GetWorkspaceRoot(self):
        return CommonPlatform.WorkspaceRoot

    def GetActiveScopes(self):
        return CommonPlatform.Scopes

    def FilterPackagesToTest(self, changedFilesList: list, potentialPackagesList: list) -> list:
        return []

    def GetPlatformDscAndConfig(self) -> tuple:
        return ("rodinPkg/rodin.dsc", {})

    def GetName(self):
        return "rodin"

    def GetPackagesPath(self):
        return CommonPlatform.PackagesPath

# ####################################################################################### #
#                         Actual Configuration for Platform Build                         #
# ####################################################################################### #
class PlatformBuilder(UefiBuilder, BuildSettingsManager):
    def __init__(self):
        UefiBuilder.__init__(self)

    def AddCommandLineOptions(self, parserObj):
        parserObj.add_argument('-a', "--arch", dest="build_arch", type=str, default="AARCH64",
                               help="Optional - CSV of architecture to build. AARCH64 is default.")

    def RetrieveCommandLineOptions(self, args):
        if args.build_arch.upper() != "AARCH64":
            raise Exception("Invalid Arch Specified.  Please see comments in DeviceBuild.py::PlatformBuilder::AddCommandLineOptions")

    def GetWorkspaceRoot(self):
        return CommonPlatform.WorkspaceRoot

    def GetPackagesPath(self):
        result = list(CommonPlatform.PackagesPath)
        unixnu_root = shell_environment.GetBuildVars().GetValue("UNIXNU_ROOT", None)
        if unixnu_root:
            result.insert(0, os.path.join(unixnu_root, "package"))
        return tuple(result)

    def GetActiveScopes(self):
        return CommonPlatform.Scopes

    def GetName(self):
        return "rodin"

    def GetLoggingLevel(self, loggerType):
        return logging.INFO

    def SetPlatformEnv(self):
        logging.debug("PlatformBuilder SetPlatformEnv")
        self.env.SetValue("PRODUCT_NAME", "rodin", "Platform Hardcoded")
        self.env.SetValue("ACTIVE_PLATFORM", "rodinPkg/rodin.dsc", "Platform Hardcoded")
        self.env.SetValue("TARGET_ARCH", "AARCH64", "Platform Hardcoded")
        self.env.SetValue("EMPTY_DRIVE", "FALSE", "Default to false")
        self.env.SetValue("RUN_TESTS", "FALSE", "Default to false")
        self.env.SetValue("SHUTDOWN_AFTER_RUN", "FALSE", "Default to false")
        self.env.SetValue("BLD_*_BUILDID_STRING", "Unknown", "Default")
        self.env.SetValue("BUILDREPORTING", "TRUE", "Enabling build report")
        self.env.SetValue("BUILDREPORT_TYPES", "PCD DEPEX FLASH BUILD_FLAGS LIBRARY FIXED_ADDRESS HASH", "Setting build report types")
        self.env.SetValue("BLD_*_MEMORY_PROTECTION", "TRUE", "Default")
        return 0

    def PlatformPreBuild(self):
        return 0

    def PlatformPostBuild(self):
        return 0
