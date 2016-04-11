/** @file

  Macro and enum definitions of a subset of port numbers, register identifiers
  and values required for driving the VMWare SVGA2 virtual display adapter,
  also implemented by Qemu.

  This file's contents was extracted from file lib/vmware/svga_reg.h in commit
  329dd537456f93a806841ec8a8213aed11395def of VMWare's vmware-svga repository:
  git://git.code.sf.net/p/vmware-svga/git


  Copyright 1998-2009 VMware, Inc.  All rights reserved.
  Portions Copyright 2017 Phil Dennis-Jordan <phil@philjordan.eu>

  Permission is hereby granted, free of charge, to any person
  obtaining a copy of this software and associated documentation
  files (the "Software"), to deal in the Software without
  restriction, including without limitation the rights to use, copy,
  modify, merge, publish, distribute, sublicense, and/or sell copies
  of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
  ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

**/

#ifndef _VMWARE_SVGA2_H_
#define _VMWARE_SVGA2_H_

//
// IDs for recognising the device
//
#define PCI_VENDOR_ID_VMWARE            0x15AD
#define PCI_DEVICE_ID_VMWARE_SVGA2      0x0405

//
// I/O port BAR offsets for register selection and read/write.
//
// The register index is written to the 32-bit index port, followed by a 32-bit
// read or write on the value port to read or set that register's contents.
//
#define SVGA_INDEX_PORT         0x0
#define SVGA_VALUE_PORT         0x1

//
// Some of the device's register indices for basic framebuffer functionality.
//
enum {
  SVGA_REG_ID = 0,
  SVGA_REG_ENABLE = 1,
  SVGA_REG_WIDTH = 2,
  SVGA_REG_HEIGHT = 3,
  SVGA_REG_MAX_WIDTH = 4,
  SVGA_REG_MAX_HEIGHT = 5,

  SVGA_REG_BITS_PER_PIXEL = 7,

  SVGA_REG_RED_MASK = 9,
  SVGA_REG_GREEN_MASK = 10,
  SVGA_REG_BLUE_MASK = 11,
  SVGA_REG_BYTES_PER_LINE = 12,

  SVGA_REG_FB_OFFSET = 14,

  SVGA_REG_FB_SIZE = 16,
  SVGA_REG_CAPABILITIES = 17,

  SVGA_REG_HOST_BITS_PER_PIXEL = 28,
};

//
// Values used with SVGA_REG_ID for sanity-checking the device and getting
// its version.
//
#define SVGA_MAGIC         0x900000UL
#define SVGA_MAKE_ID(ver)  (SVGA_MAGIC << 8 | (ver))

#define SVGA_VERSION_2     2
#define SVGA_ID_2          SVGA_MAKE_ID(SVGA_VERSION_2)

#define SVGA_VERSION_1     1
#define SVGA_ID_1          SVGA_MAKE_ID(SVGA_VERSION_1)

#define SVGA_VERSION_0     0
#define SVGA_ID_0          SVGA_MAKE_ID(SVGA_VERSION_0)

//
// One of the capability bits advertised by SVGA_REG_CAPABILITIES.
//
#define SVGA_CAP_8BIT_EMULATION     0x00000100

#endif
