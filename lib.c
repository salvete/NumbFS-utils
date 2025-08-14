// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025, Hongzhen Luo
 */

#include "internal.h"
#include "utils.h"
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int numbfs_read_block(struct numbfs_superblock_info *sbi,
                      char buf[BYTES_PER_BLOCK], int blkno)
{
        int ret;

        ret = pread(sbi->fd, buf, BYTES_PER_BLOCK, blkno * BYTES_PER_BLOCK);
        if (ret != BYTES_PER_BLOCK) {
                fprintf(stderr, "failed to read block@%d\n", blkno);
                return -EIO;
        }
        return 0;
}

int numbfs_write_block(struct numbfs_superblock_info *sbi,
                       char buf[BYTES_PER_BLOCK], int blkno)
{
        int ret;

        ret = pwrite(sbi->fd, buf, BYTES_PER_BLOCK, blkno * BYTES_PER_BLOCK);
        if (ret != BYTES_PER_BLOCK) {
                fprintf(stderr, "failed to write block@%d\n", blkno);
                return -EIO;
        }
        return 0;
}

/* get the superblock info from device@fd */
int numbfs_get_superblock(struct numbfs_superblock_info *sbi, int fd)
{
        struct numbfs_super_block *sb;
        char buf[BYTES_PER_BLOCK];
        int err;

        sbi->fd = fd;

        err = numbfs_read_block(sbi, buf, NUMBFS_SUPER_OFFSET / BYTES_PER_BLOCK);
        if (err)
                return err;

        sb = (struct numbfs_super_block*)buf;
        if (le32_to_cpu(sb->s_magic) != NUMBFS_MAGIC) {
                fprintf(stderr, "[corrupted] invalid superblock, magic: %X\n", le32_to_cpu(sb->s_magic));
                return -EINVAL;
        }

        sbi->ibitmap_start      = le32_to_cpu(sb->s_ibitmap_start);
        sbi->inode_start        = le32_to_cpu(sb->s_inode_start);
        sbi->bbitmap_start      = le32_to_cpu(sb->s_bbitmap_start);
        sbi->data_start         = le32_to_cpu(sb->s_data_start);
        sbi->num_inodes         = le32_to_cpu(sb->s_num_inodes);
        sbi->nfree_blocks       = le32_to_cpu(sb->s_nfree_blocks);
        sbi->feature            = le32_to_cpu(sb->s_feature);
        return 0;
}

/* alloc a free data block */
int numbfs_alloc_block(struct numbfs_superblock_info *sbi)
{
        int ret, i, err, byte, bit;
        char buf[BYTES_PER_BLOCK];

        ret = -1;
        for (i = 0; i < sbi->nfree_blocks; i++) {
                /* read a new block */
                if (i % NUMBFS_BLOCKS_PER_BLOCK == 0) {
                        err = numbfs_read_block(sbi, buf, numbfs_bmap_blk(sbi, i));
                        if (err)
                                return err;
                }

                byte = numbfs_bmap_byte(i);
                bit = numbfs_bmap_bit(i);
                if (!(buf[byte] & (1 << bit))) {
                        ret = i;
                        /* set this bit to 1 */
                        buf[byte] |= (1 << bit);
                        err = numbfs_write_block(sbi, buf, numbfs_bmap_blk(sbi, i));
                        if (err)
                                return err;
                        break;
                }

        }
        return ret + sbi->data_start;
}

/* free a data block */
int numbfs_free_block(struct numbfs_superblock_info *sbi, int blkno)
{
        char buf[BYTES_PER_BLOCK];
        int err, byte, bit;

        blkno -= sbi->data_start;
        err = numbfs_read_block(sbi, buf, numbfs_bmap_blk(sbi, blkno));
        if (err)
                return err;

        byte = numbfs_bmap_byte(blkno);
        bit = numbfs_bmap_bit(blkno);
        BUG_ON(!(buf[byte] & (1 << bit)));
        buf[byte] &= ~(1 << bit);

        err = numbfs_write_block(sbi, buf, numbfs_bmap_blk(sbi, blkno));
        return err;
}

/* get the inode info at @inode_i->nid */
int numbfs_get_inode(struct numbfs_superblock_info *sbi,
                     struct numbfs_inode_info *inode_i)
{
        struct numbfs_inode *inode;
        char buf[BYTES_PER_BLOCK];
        int err, i;

        err = numbfs_read_block(sbi, buf, numbfs_inode_blk(sbi, inode_i->nid));
        if (err)
                return err;

        inode = ((struct numbfs_inode*)buf) + (inode_i->nid % NUMBFS_NODES_PER_BLOCK);
        inode_i->sbi = sbi;
        inode_i->mode   = le16_to_cpu(inode->i_mode);
        inode_i->nlink  = le16_to_cpu(inode->i_nlink);
        inode_i->uid    = le16_to_cpu(inode->i_uid);
        inode_i->gid    = le16_to_cpu(inode->i_gid);
        inode_i->size   = le32_to_cpu(inode->i_size);
        for (i = 0; i < NUMBFS_NUM_DATA_ENTRY; i++)
                inode_i->data[i] = le32_to_cpu(inode->i_data[i]);

        return 0;
}

/*
 * get the block that contains pos-th byte in the address space;
 * if there is a hole, then alloc a block
 */
int numbfs_inode_blkaddr(struct numbfs_inode_info *inode, int pos, bool alloc, bool extent)
{
        int blkno;

        if (extent) {
                fprintf(stderr, "error: extent feature currently is not supported!\n");
                return -ENOTSUP;
        }

        if ((pos / BYTES_PER_BLOCK) >= NUMBFS_NUM_DATA_ENTRY) {
                fprintf(stderr, "error: pos@%d is out of range!\n", pos);
                return -E2BIG;
        }

        if (alloc && inode->data[pos / BYTES_PER_BLOCK] == NUMBFS_HOLE) {
                blkno = numbfs_alloc_block(inode->sbi);
                if (blkno < 0) {
                        fprintf(stderr, "failed to alloc data block\n");
                        return blkno;
                }
                inode->data[pos / BYTES_PER_BLOCK] = blkno;
        }

        return inode->data[pos / BYTES_PER_BLOCK];
}

static int numbfs_dump_inode(struct numbfs_inode_info *inode_i)
{
        struct numbfs_superblock_info *sbi = inode_i->sbi;
        struct numbfs_inode *inode;
        char meta[BYTES_PER_BLOCK];
        int nid = inode_i->nid;
        int i, err;

        err = numbfs_read_block(sbi, meta, numbfs_inode_blk(sbi, nid));
        if (err)
                return err;

        inode = ((struct numbfs_inode*)meta) + (nid % NUMBFS_NODES_PER_BLOCK);
        inode->i_ino    = cpu_to_le16(inode_i->nid);
        inode->i_mode   = cpu_to_le16(inode_i->mode);
        inode->i_nlink  = cpu_to_le16(inode_i->nlink);
        inode->i_uid    = cpu_to_le16(inode_i->uid);
        inode->i_gid    = cpu_to_le16(inode_i->gid);
        inode->i_size   = cpu_to_le32(inode_i->size);
        for (i = 0; i < NUMBFS_NUM_DATA_ENTRY; i++)
                inode->i_data[i] = cpu_to_le32(inode_i->data[i]);

        err = numbfs_write_block(sbi, meta, numbfs_inode_blk(sbi, nid));
        if (err) {
                fprintf(stderr, "error: failed to dump inode@%d\n", nid);
                return -EIO;
        }

#ifdef HAVE_NUMBFS_DEBUG
        err = numbfs_read_block(sbi, meta, numbfs_inode_blk(sbi, nid));
        if (err) {
                fprintf(stderr, "error: failed to read inode meta block@%d\n",
                        numbfs_inode_blk(sbi, nid));
                return -EIO;
        }

        inode = ((struct numbfs_inode*)meta) + (nid % NUMBFS_NODES_PER_BLOCK);
        assert(inode->i_nlink == cpu_to_le16(inode_i->nlink));
        assert(inode->i_size == cpu_to_le32(inode_i->size));
        for (i = 0; i < NUMBFS_NUM_DATA_ENTRY; i++)
                assert(inode->i_data[i] == cpu_to_le32(inode_i->data[i]));
#endif

        return 0;
}

/* write the buffer to the blkaddr-th block in the address space */
int numbfs_pwrite_inode(struct numbfs_inode_info *inode_i,
                        char buf[BYTES_PER_BLOCK], int blkaddr)
{
        int target, err;

        /* extend the inode size with holes */
        inode_i->size = max(inode_i->size, (blkaddr + 1) * BYTES_PER_BLOCK);

        target = numbfs_inode_blkaddr(inode_i, blkaddr * BYTES_PER_BLOCK,
                                      true, false);
        if (target < 0)
                return target;

        err = numbfs_write_block(inode_i->sbi, buf, target);
        if (err)
                return err;

        return numbfs_dump_inode(inode_i);
}

/* read the blkaddr-th block in the address space */
int numbfs_pread_inode(struct numbfs_inode_info *inode_i,
                       char buf[BYTES_PER_BLOCK], int blkaddr)
{
        int target;

        target = numbfs_inode_blkaddr(inode_i, blkaddr * BYTES_PER_BLOCK,
                                      false, false);
        if (target < 0 && target != NUMBFS_HOLE)
                return target;

        /* read a hole */
        if (round_up(inode_i->size, BYTES_PER_BLOCK) < blkaddr * BYTES_PER_BLOCK ||
            target == NUMBFS_HOLE) {
                memset(buf, 0, BYTES_PER_BLOCK);
                return 0;
        }

        return numbfs_read_block(inode_i->sbi, buf, target);
}

/* make a empty dir */
int numbfs_empty_dir(struct numbfs_superblock_info *sbi,
                     int pnid, int nid)
{
        char buf[BYTES_PER_BLOCK];
        struct numbfs_inode_info *inode_i;
        struct numbfs_dirent *dir;
        int err, i;

        inode_i = malloc(sizeof(*inode_i));
        if (!inode_i)
                return -ENOMEM;

        inode_i->nid = nid;
        inode_i->sbi = sbi;
        err = numbfs_get_inode(sbi, inode_i);
        if (err)
                goto exit;

        /* sanity check */
        for (i = 0; i < NUMBFS_NUM_DATA_ENTRY; i++)
                BUG_ON(inode_i->data[i] != NUMBFS_HOLE);

        /* write data block */
        memset(buf, 0, BYTES_PER_BLOCK);
        dir = (struct numbfs_dirent*)buf;
        memcpy(dir->name, "..", 2);
        dir->name[2] = '\0';
        dir->ino = cpu_to_le16(pnid);
        dir++;
        memcpy(dir->name, ".", 1);
        dir->name[1] = '\0';
        dir->ino = cpu_to_le16(nid);
        err = numbfs_pwrite_inode(inode_i, buf, 0);
        if (err)
                goto exit;

        /* update metadata */
        inode_i->mode = S_IFDIR;
        inode_i->nlink = 2;
        inode_i->uid = inode_i->gid = 0;
        inode_i->size = sizeof(struct numbfs_dirent) * 2;
        err = numbfs_dump_inode(inode_i);
exit:
        free(inode_i);
        return err;
}
