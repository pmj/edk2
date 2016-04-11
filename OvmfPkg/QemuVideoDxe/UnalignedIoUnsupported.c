/** @file
  Unaligned port I/O dummy implementation for platforms which do not support it.

  Copyright (c) 2017, Phil Dennis-Jordan.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/


#include "UnalignedIoInternal.h"

/**
  Reads 32-bit word from the specified, possibly unaligned I/O-type address.

  If 32-bit unaligned I/O port operations are not supported, then ASSERT().

  @param[in]  Port  I/O port from which to read.

  @return The value read from the specified location.
**/
UINT32
UnalignedIoRead32 (
  IN      UINTN                     Port
  )
{
  ASSERT (FALSE);
}

/**
  Performs a 32-bit write to the specified, possibly unaligned I/O-type address.

  If 32-bit unaligned I/O port operations are not supported, then ASSERT().

  @param[in]  Port   I/O port address
  @param[in]  Value  32-bit word to write

  @return The value written to the I/O port.

**/
UINT32
UnalignedIoWrite32 (
  IN      UINTN                     Port,
  IN      UINT32                    Value
  )
{
  ASSERT (FALSE);
}
