// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025, Hongzhen Luo
 */

#include "internal.h"
#include "disk.h"
#include <getopt.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

static struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"inodes", no_argument, NULL, 'i'},
        {"blocks", no_argument, NULL, 'b'},
        {"nid", required_argument, NULL, 'n'},
        {0, 0, 0, 0}
};

struct numbfs_fsck_cfg {
        bool show_inodes;
        bool show_blocks;
        int nid;
        char *dev;
};

static void numbfs_fsck_help(void)
{
        printf(
                "Usage: [OPTIONS] TARGET\n"
                "Get disk statistics.\n"
                "\n"
                "Gerneral options:\n"
                " --help                display this help information and exit\n"
                " --inodes|-i           display inode usage\n"
                " --blocks|-b           display block usage\n"
                " --nid=X               display the inode information of inode@nid\n"
        );
}

static void numbfs_fsck_parse_args(int argc, char **argv, struct numbfs_fsck_cfg *cfg)
{
        int opt;

        while ((opt = getopt_long(argc, argv, "n:hib", long_options, NULL)) != -1) {
                switch(opt) {
                        case 'h':
                                numbfs_fsck_help();
                                exit(0);
                        case 'i':
                                cfg->show_inodes = true;
                                break;
                        case 'b':
                                cfg->show_blocks = true;
                                break;
                        case 'n':
                                cfg->nid = atoi(optarg);
                                break;
                        default:
                                fprintf(stderr, "Unknown option: %s\n\n", argv[optind - 1]);
                                numbfs_fsck_help();
                                exit(1);
                }
        }

        if (optind >= argc) {
                fprintf(stderr, "missing block device!\n");
                exit(1);
        }

        cfg->dev = strdup(argv[optind++]);
        if (!cfg->dev) {
                fprintf(stderr, "failed to get block device path\n");
                exit(1);
        }
}

static int count_bit(__u8 byte)
{
        int ret = 0, i;

        for (i = 0; i < 8; i ++) {
                ret += (byte & 1);
                byte >>= 1;
        }
        return ret;
}

static int numbfs_fsck_used(char *buf)
{
        int i, ret = 0;

        for (i = 0; i < BYTES_PER_BLOCK; i++)
                ret += count_bit(buf[i]);
        return ret;
}

static inline char *numbfs_dir_type(int type)
{
        if (type == DT_DIR)
                return "DIR    ";
        else if (type == DT_LNK)
                return "SYMLINK";
        else
                return "REGULAR";
}

/* show the inode information at @nid */
static int numbfs_fsck_show_inode(struct numbfs_superblock_info *sbi,
                                  int nid)
{
        struct numbfs_inode_info *inode_i;
        struct numbfs_dirent *dir;
        char buf[BYTES_PER_BLOCK];
        int err, i;


        inode_i = malloc(sizeof(*inode_i));
        if (!inode_i)
                return -ENOMEM;

        inode_i->nid = nid;
        inode_i->sbi = sbi;
        err = numbfs_get_inode(sbi, inode_i);
        if (err) {
                fprintf(stderr, "error: failed to get inode information\n");
                goto exit;
        }

        printf("================================\n");
        printf("Inode Information\n");
        printf("    inode number:               %d\n", nid);
        if (S_ISDIR(inode_i->mode))
                printf("    inode type:                 DIR\n");
        else if (S_ISLNK(inode_i->mode))
                printf("    inode type:                 SYMLINK\n");
        else
                printf("    inode type:                 REGULAR FILE\n");
        printf("    link count:                 %d\n", inode_i->nlink);
        printf("    inode uid:                  %d\n", inode_i->uid);
        printf("    inode gid:                  %d\n", inode_i->gid);
        printf("    inode size:                 %d\n\n", inode_i->size);

        for (i = 0; i < NUMBFS_NUM_DATA_ENTRY; i++) {
                if (inode_i->data[i] != NUMBFS_HOLE)
                        printf("blk@%d: %d\n", i, numbfs_data_blk(sbi, inode_i->data[i]));
                else
                        printf("blk@%d: HOLE\n", i);
        }

        if (S_ISDIR(inode_i->mode)) {
                printf("    DIR CONTENT\n");
                for (i = 0; i < inode_i->size; i += sizeof(struct numbfs_dirent)) {
                        if (i % BYTES_PER_BLOCK == 0) {
                                err = numbfs_pread_inode(inode_i, buf, i, BYTES_PER_BLOCK);
                                if (err) {
                                        fprintf(stderr, "error: failed to read block@%d of inode@%d\n",
                                                i / BYTES_PER_BLOCK, nid);
                                        goto exit;
                                }
                        }
                        dir = (struct numbfs_dirent*)&buf[i % BYTES_PER_BLOCK];
                        printf("       INODE: %05d, TYPE: %s, NAMELEN: %02d NAME: %s\n",
                                le16_to_cpu(dir->ino), numbfs_dir_type(dir->type),dir->name_len, dir->name);
                }
        }

exit:
        free(inode_i);
        return err;
}

static int numbfs_fsck(int argc, char **argv)
{
        struct numbfs_fsck_cfg cfg = {
                .show_inodes = 0,
                .show_blocks = 0,
                .nid = -1,
                .dev = NULL
        };
        struct numbfs_superblock_info sbi;
        char buf[BYTES_PER_BLOCK];
        int fd, err, cnt, i;

        numbfs_fsck_parse_args(argc, argv, &cfg);

        fd = open(cfg.dev, O_RDWR, 0644);
        if (fd < 0)
                return -errno;

        err = numbfs_get_superblock(&sbi, fd);
        if (err) {
                fprintf(stderr, "failed to read superblock\n");
                goto exit;
        }

        printf("Superblock Information\n");
        printf("    inode bitmap start:         %d\n", sbi.ibitmap_start);
        printf("    inode zone start:           %d\n", sbi.inode_start);
        printf("    block bitmap start:         %d\n", sbi.bbitmap_start);
        printf("    data zone start:            %d\n", sbi.data_start);
        printf("    free inodes:                %d\n", sbi.free_inodes);
        printf("    total inodes:               %d\n", sbi.total_inodes);
        printf("    total free blocks:          %d\n", sbi.free_blocks);
        printf("    total data blocks:          %d\n", sbi.data_blocks);

        if (cfg.show_inodes) {
                cnt = 0;
                for (i = sbi.ibitmap_start; i < sbi.inode_start; i++) {
                        err = pread(fd, buf, BYTES_PER_BLOCK, i * BYTES_PER_BLOCK);
                        if (err != BYTES_PER_BLOCK) {
                                fprintf(stderr, "failed to read block@%d\n", i);
                                err = -EIO;
                                goto exit;
                        }

                        cnt += numbfs_fsck_used(buf);
                }
                BUG_ON(cnt != sbi.total_inodes - sbi.free_inodes);
                printf("    inodes usage:               %.2f%%\n", 100.0 * cnt / sbi.total_inodes);
        }

        if (cfg.show_blocks) {
                cnt = 0;
                for (i = sbi.bbitmap_start; i < sbi.data_start; i++) {
                        err = pread(fd, buf, BYTES_PER_BLOCK, i * BYTES_PER_BLOCK);
                        if (err != BYTES_PER_BLOCK) {
                                fprintf(stderr, "failed to read block@%d\n", i);
                                err = -EIO;
                                goto exit;
                        }

                        cnt += numbfs_fsck_used(buf);
                }
                BUG_ON(cnt != sbi.data_blocks - sbi.free_blocks);
                printf("    blocks usage:               %.2f%%\n", 100.0 * cnt / sbi.data_blocks);
        }

        if (cfg.nid >= 0) {
                err = numbfs_fsck_show_inode(&sbi, cfg.nid);
                if (err) {
                        fprintf(stderr, "error: failed to show inode information\n");
                        goto exit;
                }
        }

        err = 0;
exit:
        close(fd);
        free(cfg.dev);
        return err;
}

int main(int argc, char **argv)
{
        int err;

        err = numbfs_fsck(argc, argv);
        if (err) {
                fprintf(stderr, "Error occured in fsck, err: %d\n", err);
                exit(1);
        }
        return 0;
}
