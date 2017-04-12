/** @file
  HFS+ file system driver header.

  Copyright (C) 2017, Gabriel L. Somlo <gsomlo@gmail.com>

  This program and the accompanying materials are licensed and made
  available under the terms and conditions of the BSD License which
  accompanies this distribution.   The full text of the license may
  be found at http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS"
  BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER
  EXPRESS OR IMPLIED.
**/

#ifndef _FSW_HFSPLUS_H_
#define _FSW_HFSPLUS_H_

#define VOLSTRUCTNAME fsw_hfsplus_volume
#define DNODESTRUCTNAME fsw_hfsplus_dnode
#include "fsw_core.h"

/*============= HFS+ constants and data types from Apple TN1150 =============*/
#pragma pack(1)

#define kHFSBlockSize            512 // Minimum block size to transfer Vol.Hdr.
#define kMasterDirectoryBlock      2 // Vol.Hdr. disk offset (x kHFSBlockSize)

#define kHFSPlusSigWord       0x482B // HFS+ volume signature (ASCII for 'H+')

#define kHFSPlusMaxFileNameChars 255 // Max. length of HFS+ folder or filename

#define kHFSPlusExtentDensity      8 // Number of extent descriptors per record

#define kHFSPlusDataFork        0x00 // data fork type
#define kHFSPlusResourceFork    0xFF // resource fork type

#define kHFSRootFolderID           2 // ID of the root folder
#define kHFSExtentsFileID          3 // ID of the extent overflow file
#define kHFSCatalogFileID          4 // ID of the catalog file

#define kHFSPlusFolderRecord       1 // catalog folder record type
#define kHFSPlusFileRecord         2 // catalog file record type

#define kBTLeafNode               -1 // B-Tree leaf node type
#define kBTIndexNode               0 // B-Tree index node type
#define kBTHeaderNode              1 // B-Tree header node type

// file/folder name unicode string type
typedef struct {
    fsw_u16 length;                            // character count
    fsw_u16 unicode[kHFSPlusMaxFileNameChars]; // character string
} HFSUniStr255;

// extent descriptor type
typedef struct {
    fsw_u32 startBlock; // first allocation block in the extent
    fsw_u32 blockCount; // extent length, in allocation blocks
} HFSPlusExtentDescriptor;

// extent record type
typedef HFSPlusExtentDescriptor HFSPlusExtentRecord[kHFSPlusExtentDensity];

// fork-data information type
typedef struct {
    fsw_u64             logicalSize; // size of valid fork data, in bytes
    fsw_u32             clumpSize;   // fork-specific clump size
    fsw_u32             totalBlocks; // total blocks across all extents in fork
    HFSPlusExtentRecord extents;     // initial extents for the fork data
} HFSPlusForkData;

// volume header type
typedef struct {
    fsw_u16 signature;          // should be kHFSPlusSigWord
    fsw_u16 version;            // should be kHFSPlusVersion
    fsw_u32 attributes;         // volume attributes
    fsw_u32 lastMountedVersion; // unique ID of software that last wrote volume
    fsw_u32 journalInfoBlock;   // block no. of journal info
    fsw_u32 createDate;         // volume creation timestamp (date and time)
    fsw_u32 modifyDate;         // volume last modification timestamp
    fsw_u32 backupDate;         // timestamp of last backup
    fsw_u32 checkedDate;        // timestamp of last consistency check
    fsw_u32 fileCount;          // total number of files on volume
    fsw_u32 folderCount;        // total number of folders on volume
    fsw_u32 blockSize;          // allocation block size, in bytes
    fsw_u32 totalBlocks;        // total number of allocation blocks
    fsw_u32 freeBlocks;         // total number of unused allocation blocks
    fsw_u32 nextAllocation;     // block number to start next allocation search
    fsw_u32 rsrcClumpSize;      // dflt. resource fork clump size (in bytes)
    fsw_u32 dataClumpSize;      // dflt. data fork clump size (in bytes)
    fsw_u32 nextCatalogID;      // next unused catalog ID
    fsw_u32 writeCount;         // incr. each time volume is write-mounted
    fsw_u64 encodingsBitmap;    // keep track of text encodings used in names
    fsw_u8  finderInfo[32];     // information used by the OS X Finder
    HFSPlusForkData allocationFile; // info re. size & location of alloc. file
    HFSPlusForkData extentsFile;    // info re. size & location of extents file
    HFSPlusForkData catalogFile;    // info re. size & location of catalog file
    HFSPlusForkData attributesFile; // info re. size & location of attr. file
    HFSPlusForkData startupFile;    // info re. size & location of startup file
} HFSPlusVolumeHeader;

// file permissions type
typedef struct {
    fsw_u32 ownerID;    // owner UID (or hard-link previous-link)
    fsw_u32 groupID;    // owner GID (or hard-link next-link)
    fsw_u8  adminFlags; // flags changeable by root only
    fsw_u8  ownerFlags; // flags changeable by owner
    fsw_u16 fileMode;   // BSD file type and mode bits
    union {
        fsw_u32 iNodeNum;  // link reference number, if hard-link
        fsw_u32 linkCount; // ref-count of hard-links, if indirect node file
        fsw_u32 rawDevice; // device number, if block/char special dev. file
    } special;          // reserved for directories and most files
} HFSPlusBSDInfo;

// finder info types
typedef struct {
    fsw_u8 opaque[16];
} FndrFileInfo, FndrDirInfo, FndrOpaqueInfo;

// catalog folder record type
typedef struct {
    fsw_s16 recordType;         // should be kHFSPlusFolderRecord
    fsw_u16 flags;              // bit flags about the folder (reserved)
    fsw_u32 valence;            // items directly contained by this folder
    fsw_u32 folderID;           // CNID of this folder
    fsw_u32 createDate;         // folder creation timestamp (date and time)
    fsw_u32 contentModDate;     // folder content last modification timestamp
    fsw_u32 attributeModDate;   // ctime (last change to a catalog record field)
    fsw_u32 accessDate;         // atime (last access timestamp)
    fsw_u32 backupDate;         // timestamp of last backup
    HFSPlusBSDInfo permissions; // folder permissions
    FndrDirInfo userInfo;       // information used by the OS X Finder
    FndrOpaqueInfo finderInfo;  // additional information for the OS X Finder
    fsw_u32 textEncoding;       // hint re. folder name text encoding
    fsw_u32 reserved;           // reserved field
} HFSPlusCatalogFolder;

// catalog file record type
typedef struct {
    fsw_s16 recordType;         // should be kHFSPlusFileRecord
    fsw_u16 flags;              // bit flags about the file (reserved)
    fsw_u32 reserved1;          // reserved field
    fsw_u32 fileID;             // CNID of this file
    fsw_u32 createDate;         // file creation timestamp (date and time)
    fsw_u32 contentModDate;     // file content last modification timestamp
    fsw_u32 attributeModDate;   // ctime (last change to a catalog record field)
    fsw_u32 accessDate;         // atime (last access timestamp)
    fsw_u32 backupDate;         // timestamp of last backup
    HFSPlusBSDInfo permissions; // file permissions
    FndrFileInfo userInfo;      // information used by the OS X Finder
    FndrOpaqueInfo finderInfo;  // additional information for the OS X Finder
    fsw_u32 textEncoding;       // hint re. file name text encoding
    fsw_u32 reserved2;          // reserved field
    HFSPlusForkData dataFork;     // info re. size & location of data fork
    HFSPlusForkData resourceFork; // info re. size & location of resource fork
} HFSPlusCatalogFile;

// generic (union) catalog record type
typedef union {
    fsw_s16              recordType;   // set to kHFSPlus[Folder|File]Record
    HFSPlusCatalogFolder folderRecord; // catalog folder record fields
    HFSPlusCatalogFile   fileRecord;   // catalog file record fields
} HFSPlusCatalogRecord;

// B-Tree node descriptor found at the start of each node
typedef struct {
    fsw_u32 fLink;      // number of next node of this type (0 if we're last)
    fsw_u32 bLink;      // number of prev. node of this type (0 if we're first)
    fsw_s8  kind;       // node type (leaf, index, header, map)
    fsw_u8  height;     // node depth in B-Tree hierarchy (0 for header)
    fsw_u16 numRecords; // number of records contained in this node
    fsw_u16 reserved;   // reserved field
} BTNodeDescriptor;

// B-Tree header record type (first record of a B-Tree header node)
typedef struct {
    fsw_u16 treeDepth;      // current B-Tree depth (always == rootNode.height)
    fsw_u32 rootNode;       // node number of B-Tree root node
    fsw_u32 leafRecords;    // total number of records across all leaf nodes
    fsw_u32 firstLeafNode;  // node number of first leaf node
    fsw_u32 lastLeafNode;   // node number of last leaf node
    fsw_u16 nodeSize;       // node size (in bytes)
    fsw_u16 maxKeyLength;   // max. length of a key in index/leaf node
    fsw_u32 totalNodes;     // total number of nodes in the B-Tree
    fsw_u32 freeNodes;      // number of unused nodes in the B-Tree
    fsw_u16 reserved1;      // reserved field
    fsw_u32 clumpSize;      // reserved field (deprecated)
    fsw_u8  btreeType;      // reserved (0 for catalog, extents, attrib. file)
    fsw_u8  keyCompareType; // case-sensitive string comparison (HFSX only)
    fsw_u32 attributes;     // B-Tree attributes
    fsw_u32 reserved3[16];  // reserved field
} BTHeaderRec;

// extent overflow file key type
typedef struct {
    fsw_u16 keyLength;  // key length (excluding this field)
    fsw_u8  forkType;   // data or resource fork
    fsw_u8  pad;        // ensure 32-bit alignment for subsequent fields
    fsw_u32 fileID;     // CNID of file to which this extent record applies
    fsw_u32 startBlock; // start block of first extent described by this record
} HFSPlusExtentKey;

// catalog file key type
typedef struct {
    fsw_u16      keyLength; // key length (excluding this field)
    fsw_u32      parentID;  // ID of parent folder (or CNID if thread record)
    HFSUniStr255 nodeName;  // basename of file or folder
} HFSPlusCatalogKey;

// generic (union) B-Tree record key type
typedef union {
    fsw_u16           keyLength; // key length (excluding this field)
    HFSPlusExtentKey  extKey;    // extent key fields
    HFSPlusCatalogKey catKey;    // catalog key fields
} HFSPlusBTKey;

#pragma pack()
/*========= end HFS+ constants and data types from Apple TN1150 =============*/


// FSW: HFS+ specific dnode
struct fsw_hfsplus_dnode {
    struct fsw_dnode g;             // Generic (parent) dnode structure
    HFSPlusExtentRecord extents;    // HFS+ initial extent record
    fsw_u32 bt_root;                // root node index (if B-Tree file)
    fsw_u16 bt_ndsz;                // node size (if B-Tree file)
};


// FSW: HFS+ specific volume
struct fsw_hfsplus_volume {
    struct fsw_volume g;            // Generic (parent) volume structure
    HFSPlusVolumeHeader *vh;        // Raw HFS+ Volume Header
    struct fsw_hfsplus_dnode *catf; // Catalog file dnode
};

#endif // _FSW_HFSPLUS_H_
