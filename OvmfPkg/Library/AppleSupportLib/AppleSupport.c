/** @file
*
*  Copyright (c) 2014, Reza Jelveh. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
**/

#include "Console.h"
#include "Datahub.h"

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <FrameworkDxe.h>
#include <Guid/DataHubRecords.h>
#include <Protocol/DataHub.h>
#include <Library/UefiRuntimeServicesTableLib.h>


EFI_GUID gAppleFirmwareVariableGuid = {
  0x4D1EDE05, 0x38C7, 0x4A6A, {0x9C, 0xC6, 0x4B, 0xCC, 0xA8, 0xB3, 0x8C, 0x14 }
};

/** The EFI variable GUID for the 'boot-args' variable and others.
 * Also known as AppleNVRAMVariableGuid in other sources. */
EFI_GUID gEfiAppleBootGuid = {
    0x7C436110, 0xAB2A, 0x4BBB, {0xA8, 0x80, 0xFE, 0x41, 0x99, 0x5C, 0x9F, 0x82}
};

/**
  Register Handler for the specified interrupt source.

  @param This     Instance pointer for this protocol
  @param Source   Hardware source of the interrupt
  @param Handler  Callback for interrupt. NULL to unregister

  @retval  EFI_SUCCESS            The firmware has successfully stored the variable and its data as
  defined by the Attributes.
  @retval  EFI_INVALID_PARAMETER  An invalid combination of attribute bits was supplied, or the
  DataSize exceeds the maximum allowed.
  @retval  EFI_INVALID_PARAMETER  VariableName is an empty Unicode string.
  @retval  EFI_OUT_OF_RESOURCES   Not enough storage is available to hold the variable and its data.
  @retval  EFI_DEVICE_ERROR       The variable could not be saved due to a hardware failure.
  @retval  EFI_WRITE_PROTECTED    The variable in question is read-only.
  @retval  EFI_WRITE_PROTECTED    The variable in question cannot be deleted.
  @retval  EFI_SECURITY_VIOLATION The variable could not be written due to EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS
  set but the AuthInfo does NOT pass the validation check carried
  out by the firmware.

**/
EFI_STATUS
InitializeFirmware ()
{
  EFI_STATUS          Status;

  UINT32              BackgroundClear = 0x00000000;
  UINT32              FwFeatures      = 0x80000015;
  UINT32              FwFeaturesMask  = 0x800003ff;
  /* UINT32              FwFeaturesMask  = 0x37ff03e8; */
  /* static const CHAR8   DefBootArgs[]     = "debug=0x8 usb=0x800 keepsyms=1 -v dart=0"; */
   /* static const CHAR8   DefBootArgs[]     = "debug=0x10 usb=0x800 keepsyms=1 -v dart=0 acpi=off"; */
  /* static const CHAR8  DefBootArgs[]      = "debug=0x148 usb=0x800 keepsyms=1 -v -serial=0x1 dart=0"; */
  /* CHAR8               DefBootArgs[]   = "debug=0x148 usb=0x800 keepsyms=1 -v -x -serial=0x1  -topo -cpuid"; */
  /* CHAR8               DefBootArgs[]   = "debug=0x15e usb=0x800 keepsyms=1 -v -x -serial=0x1 dart=0 nv_disable=1 -topo"; */
  /* CHAR8               DefBootArgs[]   = "kext-dev-mode=1 -s debug=0x15e";  */
  /* CHAR8               DefBootArgs[]   = "kext-dev-mode=1 -s"; */
  /* UINT32              PerfRecord         = 0x00000001; */
  CHAR8               PerfRecord         = 1;

  /* Status = gRT->SetVariable(L"boot-args", */
  /*                           &gEfiAppleBootGuid, */
  /*                           EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS, */
  /*                           sizeof(DefBootArgs), &DefBootArgs); */

  Status = gRT->SetVariable(L"efiboot-perf-record",
                            &gEfiAppleBootGuid,
                            EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                            sizeof(PerfRecord), &PerfRecord);

  Status = gRT->SetVariable(L"BackgroundClear",
                            &gAppleFirmwareVariableGuid,
                            EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                            sizeof(BackgroundClear), &BackgroundClear);

  Status = gRT->SetVariable(L"FirmwareFeatures",
                            &gAppleFirmwareVariableGuid,
                            EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                            sizeof(FwFeatures), &FwFeatures);

  Status = gRT->SetVariable(L"FirmwareFeaturesMask",
                            &gAppleFirmwareVariableGuid,
                            EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                            sizeof(FwFeaturesMask), &FwFeaturesMask);

  return Status;
}

/**
  Register driver.

  @param  ImageHandle   of the loaded driver
  @param  SystemTable   Pointer to the System Table

  @retval EFI_SUCCESS   Driver registered

**/
EFI_STATUS
EFIAPI
InitializeAppleSupport (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS                         Status;

  Status = InitializeConsoleControl(ImageHandle);
  ASSERT_EFI_ERROR (Status);

  Status = InitializeDatahub(ImageHandle);
  ASSERT_EFI_ERROR(Status);

  Status = InitializeFirmware();
  ASSERT_EFI_ERROR(Status);

  return Status;
}
