#include "Common.h"
#include "Bds.h"

#include <PiDxe.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/GenericBdsLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/BlockIo.h>
#include <Protocol/LoadFile.h>

#include <Guid/FileInfo.h>

EFI_GUID mBdsAutoCreateAppleBootOptionGuid  = { 0x12d0ef4a, 0xbb4f, 0x4cb2, { 0x90, 0x17, 0x9b, 0x3a, 0x58, 0x56, 0x13, 0x00 } };

EFI_STATUS
BdsFileSystemLoadImage (
  IN     EFI_HANDLE Handle,
  IN     EFI_ALLOCATE_TYPE     Type,
  IN OUT EFI_PHYSICAL_ADDRESS* Image,
  OUT    UINTN                 *ImageSize
  )
{
  EFI_STATUS                      Status;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FsProtocol;
  EFI_FILE_PROTOCOL               *Fs;
  EFI_FILE_INFO                   *FileInfo;
  EFI_FILE_PROTOCOL               *File;
  UINTN                           Size;

  /* FilePathDevicePath = (FILEPATH_DEVICE_PATH*)RemainingDevicePath; */

  Status = gBS->HandleProtocol (Handle, &gEfiSimpleFileSystemProtocolGuid, (VOID **)&FsProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Try to Open the volume and get root directory
  Status = FsProtocol->OpenVolume (FsProtocol, &Fs);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  File = NULL;
  Status = Fs->Open (Fs, &File, EFI_CORESERVICES, EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Size = 0;
  File->GetInfo (File, &gEfiFileInfoGuid, &Size, NULL);
  FileInfo = AllocatePool (Size);
  Status = File->GetInfo (File, &gEfiFileInfoGuid, &Size, FileInfo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Get the file size
  Size = FileInfo->FileSize;
  if (ImageSize) {
    *ImageSize = Size;
  }
  FreePool (FileInfo);

  Status = gBS->AllocatePages (Type, EfiBootServicesCode, EFI_SIZE_TO_PAGES(Size), Image);
  // Try to allocate in any pages if failed to allocate memory at the defined location
  if ((Status == EFI_OUT_OF_RESOURCES) && (Type != AllocateAnyPages)) {
    Status = gBS->AllocatePages (AllocateAnyPages, EfiBootServicesCode, EFI_SIZE_TO_PAGES(Size), Image);
  }
  if (!EFI_ERROR (Status)) {
    Status = File->Read (File, &Size, (VOID*)(UINTN)(*Image));
  }

  return Status;
}


/**
  Start an EFI Application from a Device Path

  @param  ParentImageHandle     Handle of the calling image
  @param  DevicePath            Location of the EFI Application

  @retval EFI_SUCCESS           All drivers have been connected
  @retval EFI_NOT_FOUND         The Linux kernel Device Path has not been found
  @retval EFI_OUT_OF_RESOURCES  There is not enough resource memory to store the matching results.

**/
EFI_STATUS
BdsStartEfiApplication (
  IN EFI_HANDLE                  Handle,
  IN EFI_HANDLE                  ParentImageHandle,
  IN EFI_DEVICE_PATH_PROTOCOL    *DevicePath
  )
{
  EFI_STATUS                   Status;
  EFI_HANDLE                   ImageHandle;
  EFI_PHYSICAL_ADDRESS         BinaryBuffer;
  UINTN                        BinarySize;

  // Find the nearest supported file loader
  Status = BdsFileSystemLoadImage (Handle, AllocateAnyPages, &BinaryBuffer, &BinarySize);
  if (EFI_ERROR (Status)) {
    DEBUG((EFI_D_INFO, "== Bds could not load System image =="));
    return Status;
  }

  // Load the image from the Buffer with Boot Services function
  Status = gBS->LoadImage (TRUE, ParentImageHandle, DevicePath, (VOID*)(UINTN)BinaryBuffer, BinarySize, &ImageHandle);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Before calling the image, enable the Watchdog Timer for  the 5 Minute period
  gBS->SetWatchdogTimer (5 * 60, 0x0000, 0x00, NULL);
  // Start the image
  Status = gBS->StartImage (ImageHandle, NULL, NULL);
  // Clear the Watchdog Timer after the image returns
  gBS->SetWatchdogTimer (0x0000, 0x0000, 0x0000, NULL);

  return Status;
}


EFI_STATUS
BdsBootApple ()
{
  UINTN                           Index;
  UINTN                           TempSize;
  EFI_DEVICE_PATH_PROTOCOL        *TempDevicePath;
  EFI_HANDLE                      *SimpleFileSystemHandles;
  UINTN                           NumberSimpleFileSystemHandles;

  gBS->LocateHandleBuffer (
      ByProtocol,
      &gEfiSimpleFileSystemProtocolGuid,
      NULL,
      &NumberSimpleFileSystemHandles,
      &SimpleFileSystemHandles
      );
  DEBUG((EFI_D_INFO, "Number Device File System: %d\n", NumberSimpleFileSystemHandles));
  for (Index = 0; Index < NumberSimpleFileSystemHandles; Index++) {
    //
    // Get the device path size of SimpleFileSystem handle
    //
    TempDevicePath = DevicePathFromHandle (SimpleFileSystemHandles[Index]);
    TempSize = GetDevicePathSize (TempDevicePath)- sizeof (EFI_DEVICE_PATH_PROTOCOL); // minus the end node
    (void)TempSize;
    BdsStartEfiApplication (
      SimpleFileSystemHandles[Index],
      gImageHandle,
      TempDevicePath
      );
      //

  }
  return EFI_SUCCESS;
}

CHAR16* CONST BOOT_EFI_PATHS[] = {
  L"macOS Install Data\\Locked Files\\Boot Files\\boot.efi",
  L"System\\Library\\CoreServices\\boot.efi",
  L"usr\\standalone\\i386\\boot.efi",
};


static void InitBootEfiBootOption(
  EFI_BOOT_MANAGER_LOAD_OPTION* BootOption, EFI_HANDLE* ParentDevice, CHAR16* PathToFile)
{
  EFI_STATUS Status;
  EFI_DEVICE_PATH_PROTOCOL* FilePath;
  CHAR16* PathText;
  CHAR16* Description;

  FilePath = FileDevicePath(ParentDevice, PathToFile);
 
  PathText = ConvertDevicePathToText(FilePath, TRUE, TRUE);
  DEBUG((EFI_D_INFO, "RegisterFileAsBootOption: device path '%s'\n", PathText ?: L"[NULL]"));
  FreePool (PathText);

  Description = L"Boot macOS/OS X";

  Status = EfiBootManagerInitializeLoadOption (
                 BootOption,
                 LoadOptionNumberUnassigned,
                 LoadOptionTypeBoot,
                 LOAD_OPTION_ACTIVE,
                 Description,
                 FilePath,
                 NULL,
                 0
                 );
      ASSERT_EFI_ERROR (Status);
}

static BOOLEAN
BdsIsAutoCreateAppleBootOption (
  EFI_BOOT_MANAGER_LOAD_OPTION    *BootOption
  )
{
  if ((BootOption->OptionalDataSize == sizeof (EFI_GUID)) &&
      CompareGuid ((EFI_GUID *) BootOption->OptionalData, &mBdsAutoCreateAppleBootOptionGuid)
      ) {
    return TRUE;
  } else {
    return FALSE;
  }
}


EFI_STATUS
BdsRefreshMacBootOptions (
  VOID
  )
{
  UINTN                           Index;
  UINTN                                 HandleCount;
  EFI_HANDLE                            *Handles;
  EFI_DEVICE_PATH_PROTOCOL        *DriveDevicePath;
  CHAR16* PathText;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
  EFI_STATUS Status;
  EFI_BLOCK_IO_PROTOCOL* BlockIO;
  EFI_BOOT_MANAGER_LOAD_OPTION  *NvBootOptions;
  UINTN                         NvBootOptionCount;
  EFI_BOOT_MANAGER_LOAD_OPTION  *BootOptions;
  UINTN                         BootOptionCount;
  
  gBS->LocateHandleBuffer (
         ByProtocol,
         &gEfiBlockIoProtocolGuid,
         NULL,
         &HandleCount,
         &Handles
         );




  NvBootOptions = EfiBootManagerGetLoadOptions (&NvBootOptionCount, LoadOptionTypeBoot);

  BootOptionCount = 0;
  BootOptions = NULL;

  DEBUG((EFI_D_INFO, "BdsRefreshMacBootOptions: %d Block IO protocol handlers found\n", HandleCount));
  // TODO: deprioritise removable media similarly to BmEnumerateBootOptions
  for (Index = 0; Index < HandleCount; Index++) {
    DriveDevicePath = DevicePathFromHandle (Handles[Index]);
    if (DriveDevicePath == NULL) {
      DEBUG((EFI_D_INFO, "%d: NULL device path\n", Index));
    } else {
      PathText = ConvertDevicePathToText(DriveDevicePath, TRUE, TRUE);
      DEBUG((EFI_D_INFO, "%d: device path '%s'\n", Index, PathText ?: L"[NULL]"));
      FreePool (PathText);
      
      Status = gBS->HandleProtocol (
        Handles[Index],
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID *) &FileSystem
      );
      if (EFI_ERROR (Status)) {
        DEBUG((EFI_D_INFO, "%d: Not a mounted file system\n", Index));
      } else {
        DEBUG((EFI_D_INFO, "%d: File system mounted on device\n", Index));
        Status = gBS->HandleProtocol (
          Handles[Index],
          &gEfiBlockIoProtocolGuid,
          (VOID *) &BlockIO
        );
        if (EFI_ERROR (Status)) {
          DEBUG((EFI_D_INFO, "Error getting BlockIO protocol\n"));
        } else {
          UINT32 block_size = BlockIO->Media->BlockSize;
          UINT32 read_size = 512;
          UINT32 read_offset = 1024;
          UINT32 num_blocks = read_size / block_size;
          UINT32 buffer_header_offset = 0;
          if (num_blocks == 0) {
            num_blocks = 1;
            read_size = block_size;
          }
          read_offset /= block_size;
          if (read_offset == 0) {
            buffer_header_offset = 1024;
          }
          VOID* Buffer = AllocatePool (read_size);
          if (Buffer != NULL) {
            BlockIO->ReadBlocks (
               BlockIO,
               BlockIO->Media->MediaId,
               read_offset,
               read_size,
               Buffer
               );
            UINT8* volume_header = (UINT8*)Buffer + buffer_header_offset;
            UINT16 Version = ((UINT16)volume_header[2] << 8) | volume_header[3];
            DEBUG((EFI_D_INFO, "Volume header? Signature: 0x%02x%02x Version: %d\n", volume_header[0], volume_header[1], Version));
            if (volume_header[0] == 0x48 && volume_header[1] == 0x2b && (Version == 4 || Version == 5)) {
              DEBUG((EFI_D_INFO, "Seems to be an HFS+ volume!\n"));

// locate a boot.efi
  EFI_FILE_PROTOCOL               *Fs;
  EFI_FILE_PROTOCOL               *BootEfiFile;

              Status = FileSystem->OpenVolume (FileSystem, &Fs);
              if (EFI_ERROR (Status)) {
                DEBUG((EFI_D_INFO, "Error opening volume\n"));
              } else {
  UINTN PathIndex;
                for (PathIndex = 0; PathIndex < 3; ++PathIndex) {
                  Status = Fs->Open(Fs, &BootEfiFile, BOOT_EFI_PATHS[PathIndex], EFI_FILE_MODE_READ, 0);
                  if (EFI_ERROR (Status)) {
                    if (Status == EFI_NOT_FOUND) {
                      DEBUG((EFI_D_INFO, "No boot.efi found at '%s'\n", BOOT_EFI_PATHS[PathIndex]));
                    } else {
                      DEBUG((EFI_D_INFO, "Error accessing boot.efi at '%s'\n", BOOT_EFI_PATHS[PathIndex]));
                    }
                  } else {
  UINTN                           Size;
  EFI_FILE_INFO* FileInfo;
                    Size = 0;
                    BootEfiFile->GetInfo (BootEfiFile, &gEfiFileInfoGuid, &Size, NULL);
                    FileInfo = AllocatePool (Size);
                    Status = BootEfiFile->GetInfo (BootEfiFile, &gEfiFileInfoGuid, &Size, FileInfo);
                    if (EFI_ERROR (Status) || Size < sizeof(*FileInfo)) {
                      DEBUG((EFI_D_INFO, "Error for GetInfo on '%s'\n", BOOT_EFI_PATHS[PathIndex]));
                      FreePool (FileInfo);
                      BootEfiFile->Close(BootEfiFile);
                      continue;
                    }
  UINT64 BootEfiFileSize;
                    BootEfiFileSize = FileInfo->FileSize;
                    DEBUG((EFI_D_INFO, "boot.efi with %d bytes found at '%s'\n", BootEfiFileSize, BOOT_EFI_PATHS[PathIndex]));
                    FreePool (FileInfo);
                    if (BootEfiFileSize == 0) {
                      BootEfiFile->Close(BootEfiFile);
                      continue;
                    }
                    
                    BootOptions = ReallocatePool (
                      sizeof (EFI_BOOT_MANAGER_LOAD_OPTION) * (BootOptionCount),
                      sizeof (EFI_BOOT_MANAGER_LOAD_OPTION) * (BootOptionCount + 1),
                      BootOptions
                      );                    
                    InitBootEfiBootOption(&BootOptions[BootOptionCount], Handles[Index], BOOT_EFI_PATHS[PathIndex]);
                    BootOptionCount += 1;
                    break;
                  }
                }
              }
            }
            FreePool(Buffer);
          }
        }
      }
    }
  }

  for (Index = 0; Index < BootOptionCount; Index++) {
    BootOptions[Index].OptionalData     = AllocateCopyPool (sizeof (EFI_GUID), &mBdsAutoCreateAppleBootOptionGuid);
    BootOptions[Index].OptionalDataSize = sizeof (EFI_GUID);
  }

  for (Index = 0; Index < NvBootOptionCount; ++Index) {
    if (BdsIsAutoCreateAppleBootOption(&NvBootOptions[Index])) {
      if (EfiBootManagerFindLoadOption (&NvBootOptions[Index], BootOptions, BootOptionCount) == -1) {
        DEBUG ((EFI_D_INFO, "Removing boot option '%s'\n", NvBootOptions[Index].Description));
        Status = EfiBootManagerDeleteLoadOptionVariable (NvBootOptions[Index].OptionNumber, LoadOptionTypeBoot);
      }
    }
  }

  for (Index = 0; Index < BootOptionCount; Index++) {
    if (EfiBootManagerFindLoadOption (&BootOptions[Index], NvBootOptions, NvBootOptionCount) == -1) {
      DEBUG ((EFI_D_INFO, "Adding boot option '%s'\n", BootOptions[Index].Description));
      EfiBootManagerAddLoadOptionVariable (&BootOptions[Index], (UINTN) 0/*-1*/);
    }
  }

  EfiBootManagerFreeLoadOptions (BootOptions,   BootOptionCount);
  EfiBootManagerFreeLoadOptions (NvBootOptions, NvBootOptionCount);

  if (HandleCount != 0) {
    FreePool (Handles);
  }
  return EFI_SUCCESS;
}


