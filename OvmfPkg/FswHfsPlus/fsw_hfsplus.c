/** @file
  HFS+ file system driver.

  Copyright (C) 2017, Gabriel L. Somlo <gsomlo@gmail.com>

  This program and the accompanying materials are licensed and made
  available under the terms and conditions of the BSD License which
  accompanies this distribution.   The full text of the license may
  be found at http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS"
  BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER
  EXPRESS OR IMPLIED.
**/

#include "fsw_hfsplus.h"


/* Given dnode 'd' and offset 'pos' into the data file it represents,
 * retrieve 'len' bytes of data into buffer 'buf';
 * Return FSW_SUCCESS or error code.
 */
static fsw_status_t
fsw_hfsplus_read(struct fsw_hfsplus_dnode *d, fsw_u64 pos,
                 fsw_u32 len, void *buf)
{
    struct fsw_shandle sh;
    fsw_u32            buflen;
    fsw_status_t       status;

    status = fsw_shandle_open(d, &sh);
    if (status)
        return status;

    sh.pos = pos;
    buflen = len;
    status = fsw_shandle_read(&sh, &buflen, buf);
    if (!status && buflen != len)
        status = FSW_IO_ERROR;

    fsw_shandle_close(&sh);
    return status;
}

/* Given the volume being mounted ('v'), and the ID & fork data of the B-Tree
 * file being set up ('dn_id' and 'f', respectively), populate a cached,
 * in-memory record of the B-Tree file at the location pointed to by 'btp';
 * Return FSW_SUCCESS or error code.
 */
static fsw_status_t
fsw_hfsplus_btf_setup(struct fsw_hfsplus_volume *v,
                      fsw_u32 dn_id, HFSPlusForkData *f,
                      struct fsw_hfsplus_dnode **btp)
{
    BTHeaderRec  hdr_rec;
    fsw_status_t status;

    status = fsw_dnode_create_root(v, dn_id, btp);
    if (status)
        return status;

    (*btp)->g.size = fsw_u64_be_swap(f->logicalSize);
    fsw_memcpy((*btp)->extents, f->extents, sizeof(HFSPlusExtentRecord));

    // read header record (from node 0, immediately past the node descriptor)
    status = fsw_hfsplus_read(*btp, sizeof(BTNodeDescriptor),
                              sizeof(BTHeaderRec), &hdr_rec);
    if (status)
        return status;

    // grab root node index and node size from header record
    (*btp)->bt_root = fsw_u32_be_swap(hdr_rec.rootNode);
    (*btp)->bt_ndsz = fsw_u16_be_swap(hdr_rec.nodeSize);

    return FSW_SUCCESS;
}

/* Mount an HFS+ volume. Read volume header (equivalent of superblock),
 * and set up dnodes for the root folder and B-Tree file(s).
 * Return FSW_SUCCESS or error code.
 */
static fsw_status_t
fsw_hfsplus_vol_mount(struct fsw_hfsplus_volume *v)
{
    void         *buf;
    fsw_u32      bs;
    fsw_status_t status;

    // allocate memory for vol. header
    status = fsw_alloc(sizeof(HFSPlusVolumeHeader), &v->vh);
    if (status)
        return status;

    // read vol. header into buffer
    fsw_set_blocksize(v, kHFSBlockSize, kHFSBlockSize);
    status = fsw_block_get(v, kMasterDirectoryBlock, 0, &buf);
    if (status)
        return status;
    fsw_memcpy(v->vh, buf, sizeof(HFSPlusVolumeHeader));
    fsw_block_release(v, kMasterDirectoryBlock, buf);

    // check vol. header
    if (fsw_u16_be_swap(v->vh->signature) != kHFSPlusSigWord)
        return FSW_UNSUPPORTED;

    // use block size specified by vol. header
    bs = fsw_u32_be_swap(v->vh->blockSize);
    fsw_set_blocksize(v, bs, bs);

    // set up catalog B-Tree file:
    status = fsw_hfsplus_btf_setup(v, kHFSCatalogFileID, &v->vh->catalogFile,
                                   &v->catf);
    if (status)
        return status;

    // set up root folder:
    status = fsw_dnode_create_root(v, kHFSRootFolderID, &v->g.root);
    if (status)
        return status;

    return FSW_SUCCESS;
}

/* Free the volume data structure. Called by the core after an unmount or
 * unsuccessful mount, to release the memory used by the file system type
 * specific part of the volume structure.
 */
static void
fsw_hfsplus_vol_free(struct fsw_hfsplus_volume *v)
{
    if (v->vh)
        fsw_free(v->vh);
    if (v->catf)
        fsw_dnode_release((struct fsw_dnode *)v->catf);
}

/* Get in-depth information on a volume.
 */
static fsw_status_t
fsw_hfsplus_vol_stat(struct fsw_hfsplus_volume *v, struct fsw_volume_stat *s)
{
    // FIXME: not yet supported!
    return FSW_UNSUPPORTED;
}

/* Get full information on a dnode from disk. This function is called by
 * the core whenever it needs to access fields in the dnode structure that
 * may not be filled immediately upon creation of the dnode.
 */
static fsw_status_t
fsw_hfsplus_dno_fill(struct fsw_hfsplus_volume *v, struct fsw_hfsplus_dnode *d)
{
    // NOTE: not applicable to HFS+ dnodes!
    return FSW_SUCCESS;
}

/* Free the dnode data structure. Called by the core when deallocating a dnode
 * structure to release the memory used by the file system type specific part
 * of the dnode structure.
 */
static void
fsw_hfsplus_dno_free(struct fsw_hfsplus_volume *v, struct fsw_hfsplus_dnode *d)
{
    // NOTE: not applicable to HFS+ dnodes!
    return;
}

/* Get in-depth dnode information. The core ensures fsw_hfsplus_dno_fill()
 * has been called on the dnode before this function is called. Note that some
 * data is not directly stored into the structure, but passed to a host-specific
 * callback that converts it to the host-specific format.
 */
static fsw_status_t
fsw_hfsplus_dno_stat(struct fsw_hfsplus_volume *v, struct fsw_hfsplus_dnode  *d,
                     struct fsw_dnode_stat *s)
{
    // FIXME: not yet supported!
    return FSW_SUCCESS;
}

/* Given a B-Tree node pointed to by 'btnode', with node size 'size',
 * locate the record given by its record number 'rnum';
 * Return a pointer to the B-Tree key at the beginning of the record.
 */
static HFSPlusBTKey *
fsw_hfsplus_bt_get_rec(BTNodeDescriptor* btnode, fsw_u16 size, fsw_u16 rnum)
{
    fsw_u16 *off = (fsw_u16 *)((void *)btnode + size) - 1 - rnum;
    return (HFSPlusBTKey *)((void *)btnode + fsw_u16_be_swap(*off));
}

/* Given a B-Tree record pointer 'k', return a pointer to the data
 * immediately following the key record; IOW, skip the key record which
 * prefixes the record data payload.
 */
static void *
fsw_hfsplus_bt_rec_skip_key(HFSPlusBTKey *k)
{
    return (void *)k + sizeof(k->keyLength) + fsw_u16_be_swap(k->keyLength);
}

/* Return the child node number immediately following the key record 'k' of
 * an index node
 */
static fsw_u32
fsw_hfsplus_bt_idx_get_child(HFSPlusBTKey *k)
{
    fsw_u32 *child;
    child = (fsw_u32 *)fsw_hfsplus_bt_rec_skip_key(k);
    return fsw_u32_be_swap(*child);
}

/* key comparison procedure type */
typedef int (*k_cmp_t)(HFSPlusBTKey*, HFSPlusBTKey*);

/* Search an HFS+ special file's B-Tree (given by 'bt'), for a search key
 * matching 'sk', using comparison procedure 'k_cmp' to determine when a key
 * match occurs; Fill a caller-provided B-Tree node buffer ('btnode'), and
 * return a pointer to the matching record data inside 'btnode' via 'data_ptr';
 * On error, set fsw_status_t return code acoordingly.
 *
 * NOTE: A HFS+ volume has a few "special" files, linked directly from the
 *       volume header. For the purpose of this driver, we mainly care about
 *       two of them: the "catalog" and "extents" files. All of these files
 *       are organized as B-Tree structures. This means that, overlaid on
 *       top of the linear span of each file there is an array of nodes of
 *       a given size (node_size), internally cross-linked with "pointers"
 *       to parent/child/sibling nodes, which are essentially the "index"
 *       (or 'node-number') of the target node in this overlaid array.
 *       Ultimately, (node-number * node-size) is a byte offset into the
 *       file, to the location where the referenced node's data begins.
 *       Each B-Tree file's "dnode" information is available in the HFS+
 *       volume header. The node at the very beginning of each file (at
 *       index or node-number == 0) contains a "header node", which provides
 *       the 'node-number' of the B-Tree's "root" node, as well as the
 *       'node-size' of all nodes in that B-Tree file.
 */
static fsw_status_t
fsw_hfsplus_bt_search(struct fsw_hfsplus_dnode *bt,
                      HFSPlusBTKey *sk, k_cmp_t k_cmp,
                      BTNodeDescriptor *btnode, void **data_ptr)
{
    fsw_u32      node;
    fsw_u16      rec, lo, hi;
    HFSPlusBTKey *tk;    // trial key
    int          cmp;
    fsw_status_t status;

    // start searching from the B-Tree root node:
    node = bt->bt_root;

    for (;;) {
        // load data for current node into caller-provided buffer 'btnode'
        status = fsw_hfsplus_read(bt, (fsw_u64)node * bt->bt_ndsz,
                                  bt->bt_ndsz, btnode);
        if (status)
            return status;

        // sanity check: record 0 located immediately after node descriptor
        if ((void *)btnode + sizeof(BTNodeDescriptor) !=
            (void *)fsw_hfsplus_bt_get_rec(btnode, bt->bt_ndsz, 0))
            return FSW_VOLUME_CORRUPTED;

        // search records within current node
        lo = 0;
        hi = fsw_u16_be_swap(btnode->numRecords) - 1;
        while (lo <= hi) {
             // access record data, then compare to search key 'sk'
             rec = (lo + hi) >> 1;
             tk = fsw_hfsplus_bt_get_rec(btnode, bt->bt_ndsz, rec);
             cmp = k_cmp(tk, sk);

             if (cmp < 0)      // (tk < sk)
                 lo = rec + 1;
             else if (cmp > 0) // (tk > sk)
                 hi = rec - 1;
             else {            // (tk == sk)
                 if (btnode->kind != kBTLeafNode) {
                     hi = rec;
                     break;
                 }
                 // success: return pointer to data immediately past trial key
                 *data_ptr = fsw_hfsplus_bt_rec_skip_key(tk);
                 return FSW_SUCCESS;
             }
        }

        // NOTE: following the binary search, 'hi' now points at the
        //       record with the largest 'tk' for which (tk <= sk)

        if (btnode->kind != kBTIndexNode)
            break;

        // on an index node, so descend to child
        tk = fsw_hfsplus_bt_get_rec(btnode, bt->bt_ndsz, hi);
        node = fsw_hfsplus_bt_idx_get_child(tk);
    }

    // search key 'sk' not found
    return FSW_NOT_FOUND;
}


/* Compare unsigned integers 'a' and 'b';
 * Return -1/0/1 if 'a' is less/equal/greater than 'b'.
 */
static int
fsw_hfsplus_int_cmp(fsw_u32 a, fsw_u32 b)
{
    return (a < b) ? -1 : (a > b) ? 1 : 0;
}

/* Basic latin unicode lowercase
 */
static fsw_u16
fsw_hfsplus_ucblatin_tolower(fsw_u16 c)
{
    if (c == 0)
        return 0xFFFF;
    if (c == 0x00C6 || c == 0x00D0 || c == 0x00D8 || c == 0x00DE ||
        (c >= 0x0041 && c <= 0x005A))
        return c + 0x0020;
    return c;
    // FIXME: does edk2 have its own built-in function we could use here?
}

/* Compare an on-disk catalog B-Tree trial key ('tk') with an in-memory
 * search key ('sk'). Precedence is parentID, nodeName (keyLength does not
 * factor into the comparison).
 * Return -1/0/1 if 'tk'is smaller/equal/larger than 'sk', respectively.
 * NOTE: all 'tk' fields are stored as big-endian values and must be
 *       converted to CPU endianness before any comparison to corresponding
 *       fields in 'sk'.
 */
static int
fsw_hfsplus_cat_cmp(HFSPlusBTKey *tk, HFSPlusBTKey *sk)
{
    fsw_u16 *t_str, *s_str;
    fsw_u16 t_len, s_len;
    fsw_u16 t_char, s_char;
    int ret;

    // compare parent IDs: if unequal, we're done!
    ret = fsw_hfsplus_int_cmp(fsw_u32_be_swap(tk->catKey.parentID),
                              sk->catKey.parentID);
    if (ret)
        return ret;

    // unicode string pointers and lengths:
    t_len = fsw_u16_be_swap(tk->catKey.nodeName.length);
    t_str = tk->catKey.nodeName.unicode;
    s_len = sk->catKey.nodeName.length;
    s_str = sk->catKey.nodeName.unicode;

    for (;;) {
        // start by assuming strings are empty:
        t_char = s_char = 0;

        // find next valid char from on-disk key string:
        while (t_char == 0 && t_len > 0) {
            t_char = fsw_hfsplus_ucblatin_tolower(fsw_u16_be_swap(*t_str));
            t_len--;
            t_str++;
        }

        // find next valid char from memory key string:
        while (s_char == 0 && s_len > 0) {
            s_char = fsw_hfsplus_ucblatin_tolower(*s_str);
            s_len--;
            s_str++;
        }

        // stop if difference or both strings exhausted:
        ret = fsw_hfsplus_int_cmp(t_char, s_char);
        if (ret || s_char == 0)
            break;
    }

    return ret;
}

/* Retrieve file data mapping information. This function is called by
 * the core when fsw_shandle_read needs to know where on the disk the
 * required piece of the file's data can be found. The core makes sure
 * that fsw_hfsplus_dno_fill has been called on the dnode before.
 * Our task here is to get the physical disk block number for the
 * requested logical block number.
 * NOTE: logical and physical block sizes are the same (see mount method).
 */
static fsw_status_t
fsw_hfsplus_get_ext(struct fsw_hfsplus_volume *v, struct fsw_hfsplus_dnode *d,
                    struct fsw_extent *e)
{
    fsw_u32             off, bc;
    HFSPlusExtentRecord *er;
    int                 i;

    // set initial offset to provided starting logical block number:
    off = e->log_start;

    // start with dnode's initial extent record:
    er = &d->extents;

    // search extent record:
    for (i = 0; i < kHFSPlusExtentDensity; i++) {
        // get block count for current extent descriptor:
        bc = fsw_u32_be_swap((*er)[i].blockCount);

        // have we exhausted all available extents?
        if (bc == 0)
            return FSW_NOT_FOUND;

        // offset is relative to current extent's physical startBlock:
        if (off < bc) {
            e->type = FSW_EXTENT_TYPE_PHYSBLOCK;
            e->phys_start = fsw_u32_be_swap((*er)[i].startBlock) + off;
            e->log_count = bc - off;
            return FSW_SUCCESS;
        }

        // update offset to NEXT extent descriptor:
        off -= bc;
    }

    // FIXME: more than 8 fragments not yet supported!
    return FSW_UNSUPPORTED;
}


/* Lookup a directory's child dnode by name. This function is called on a
 * directory to retrieve the directory entry with the given name. A dnode
 * is constructed for this entry and returned. The core makes sure that
 * fsw_hfsplus_dno_fill has been called and the dnode is actually a directory.
 */
static fsw_status_t
fsw_hfsplus_dir_get(struct fsw_hfsplus_volume *v, struct fsw_hfsplus_dnode *d,
                    struct fsw_string *name, struct fsw_hfsplus_dnode **d_out)
{
    BTNodeDescriptor     *btnode;
    fsw_s16              child_rec_type;
    fsw_u32              child_dno_id, child_dno_type;
    HFSPlusCatalogKey    k;
    HFSPlusCatalogRecord *rec;
    fsw_status_t         status;

    // we only support FSW_STRING_TYPE_UTF16 names:
    if (name->type != FSW_STRING_TYPE_UTF16 ||
        name->size > sizeof(k.nodeName.unicode))
        return FSW_UNSUPPORTED;

    // pre-allocate bt-node buffer for use by search function:
    status = fsw_alloc(v->catf->bt_ndsz, &btnode);
    if (status)
        return status;

    // search catalog file for child named by 'name':
    k.parentID = d->g.dnode_id;
    k.nodeName.length = name->len;
    fsw_memcpy(k.nodeName.unicode, name->data, name->size);
    // NOTE: keyLength not used in search, setting only for completeness:
    k.keyLength = sizeof(k.parentID) + sizeof(k.nodeName.length) + name->size;
    status = fsw_hfsplus_bt_search(v->catf,
                                   (HFSPlusBTKey *)&k, fsw_hfsplus_cat_cmp,
                                   btnode, (void **)&rec);
    if (status)
        goto done;

    // child record immediately follows the record key data:
    child_rec_type = fsw_u16_be_swap(rec->recordType);
    if (child_rec_type == kHFSPlusFolderRecord) {
        child_dno_id = fsw_u32_be_swap(rec->folderRecord.folderID);
        child_dno_type = FSW_DNODE_TYPE_DIR;
    } else if (child_rec_type == kHFSPlusFileRecord) {
        child_dno_id = fsw_u32_be_swap(rec->fileRecord.fileID);
        child_dno_type = FSW_DNODE_TYPE_FILE;
    } else {
        child_dno_id = 0;
        child_dno_type = FSW_DNODE_TYPE_UNKNOWN;
    }
    status = fsw_dnode_create(d, child_dno_id, child_dno_type, name, d_out);
    if (status)
        goto done;

    // if child node is a file, set size and initial extents:
    if (child_rec_type == kHFSPlusFileRecord) {
        (*d_out)->g.size =
            fsw_u64_be_swap(rec->fileRecord.dataFork.logicalSize);
        fsw_memcpy((*d_out)->extents, &rec->fileRecord.dataFork.extents,
                   sizeof(HFSPlusExtentRecord));
    }

done:
    fsw_free(btnode);
    return status;
}

/* Get the next directory entry when reading a directory. This function is
 * called during directory iteration to retrieve the next directory entry.
 * A dnode is constructed for the entry and returned. The core makes sure
 * that fsw_hfsplus_dno_fill has been called and the dnode is actually a
 * directory. The shandle provided by the caller is used to record the
 * position in the directory between calls.
 */
static fsw_status_t
fsw_hfsplus_dir_read(struct fsw_hfsplus_volume *v, struct fsw_hfsplus_dnode *d,
                     struct fsw_shandle *sh, struct fsw_hfsplus_dnode **d_out)
{
    // FIXME: not yet supported!
    return FSW_UNSUPPORTED;
}

/* Get the target path of a symbolic link. This function is called when a
 * symbolic link needs to be resolved. The core makes sure that the
 * fsw_hfsplus_dno_fill has been called on the dnode and that it really is a
 * symlink.
 */
static fsw_status_t
fsw_hfsplus_readlink(struct fsw_hfsplus_volume *v, struct fsw_hfsplus_dnode *d,
                     struct fsw_string *lnk_tgt)
{
    // FIXME: not yet supported!
    return FSW_UNSUPPORTED;
}


/* HFS+ FSW Method Dispatch Table
 */
struct fsw_fstype_table FSW_FSTYPE_TABLE_NAME(hfsplus) = {
    { FSW_STRING_TYPE_ISO88591, 4, 4, "hfsplus" },
    sizeof(struct fsw_hfsplus_volume), sizeof(struct fsw_hfsplus_dnode),
    fsw_hfsplus_vol_mount, fsw_hfsplus_vol_free, fsw_hfsplus_vol_stat,
    fsw_hfsplus_dno_fill, fsw_hfsplus_dno_free, fsw_hfsplus_dno_stat,
    fsw_hfsplus_get_ext, fsw_hfsplus_dir_get, fsw_hfsplus_dir_read,
    fsw_hfsplus_readlink,
};
