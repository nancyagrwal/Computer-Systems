/*
 * file:        homework.c
 * description: skeleton file for CS 5600 file system
 *
 * CS 5600, Computer Systems, Northeastern CCIS
 * Peter Desnoyers, November 2018
 */

#define FUSE_USE_VERSION 27
#define _FILE_OFFSET_BITS 64

#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fuse.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>
#include <assert.h>
#include "fsx600.h"

// Make GCC happy and not emit warnings
#define UNUSED __attribute__((unused))

/* disk access. All access is in terms of 1024-byte blocks; read and
 * write functions return 0 (success) or -EIO.
 */
//extern int block_read(void* buf, int lba, int nblks);

// Only slightly evil way to avoid extremely repetitive error handling
#define CALL_OR_RET_ERROR(funname, varname)\
int varname = funname;\
if(varname < 0) {\
    return varname; \
}

static void split_path(const char* _path, char** parent_path, char** file_name);

extern int block_read(void* buf, int lba, int nblks);

extern int block_write(void* buf, int lba, int nblks);

static int get_blk_number(int inodenum, int num, int alloc_on_fail);

static int read_inode(int index, char* buf, size_t len, off_t offset);

static int write_inode(int index, const char* buf, size_t len, off_t offset);

int locate_entry(const char* file_name, const struct fs_dirent* directory_entries, int num_entries);

unsigned char* inode_map, * block_map;
int inode_m_base;
int block_m_base;
struct fs_inode* inodes;
int no_of_inodes;
int inode_base;
int no_of_blocks;
int root_inode;
struct fs_super* super_block;

/* bitmap functions
 */
void bit_set(unsigned char* map, int i) {
    map[i / 8] |= (1 << (i % 8));
}

void bit_clear(unsigned char* map, int i) {
    map[i / 8] &= ~(1 << (i % 8));
}

int bit_test(const unsigned char* map, int i) {
    return map[i / 8] & (1 << (i % 8));
}

int flush_inodes_bitmaps() {
    CALL_OR_RET_ERROR(block_write(inode_map, inode_m_base, super_block->inode_map_sz), write1);

    CALL_OR_RET_ERROR (block_write(block_map, block_m_base, super_block->block_map_sz), write2);

    CALL_OR_RET_ERROR (block_write(inodes, inode_base, super_block->inode_region_sz), write3);

    return 0;
}

/* init - this is called once by the FUSE framework at startup. Ignore
 * the 'conn' argument.
 * recommended actions:
 *   - read superblock
 *   - allocate memory, read bitmaps and inodes
 */
void* fs_init(__attribute__((unused)) struct fuse_conn_info* conn) {

    // Clear previous init in case init is called multiple times (like during testing)
    if (inode_map) free(inode_map);
    if (block_map) free(block_map);
    if (inodes) free(inodes);
    if (super_block) free(super_block);

    struct fs_super sb;
    if (block_read(&sb, 0, 1) < 0)
        exit(1);

    /* your code here */

    /* Write the inode and block map after the superblock */
    inode_m_base = 1;
    inode_map = malloc(sb.inode_map_sz * FS_BLOCK_SIZE);
    if (block_read(inode_map, inode_m_base, sb.inode_map_sz) < 0)
        exit(1);

    block_m_base = inode_m_base + sb.inode_map_sz;
    block_map = malloc(sb.block_map_sz * FS_BLOCK_SIZE);
    if (block_read(block_map, block_m_base, sb.block_map_sz) < 0)
        exit(1);

    /* Next write the inode data next to the set of blocks */
    inode_base = block_m_base + sb.block_map_sz;

    //calculate the number of inodes:
    no_of_inodes = sb.inode_region_sz * INODES_PER_BLK;
    inodes = malloc(sb.inode_region_sz * FS_BLOCK_SIZE);
    if (block_read(inodes, inode_base, sb.inode_region_sz) < 0)
        exit(1);

    no_of_blocks = sb.num_blocks;
    root_inode = sb.root_inode;
    super_block = malloc(sizeof(struct fs_super));
    memcpy(super_block, &sb, sizeof(struct fs_super));

    return NULL;
}

static int block_allocation() {
    int c;
    for (c = 0; c < no_of_blocks; c++) {
        if (!bit_test(block_map, c)) {
            bit_set(block_map, c);
            return c;
        }
    }
    return -ENOSPC;
}

/* Note on path translation errors:
 * In addition to the method-specific errors listed below, almost
 * every method can return one of the following errors if it fails to
 * locate a file or directory corresponding to a specified path.
 *
 * ENOENT - a component of the path doesn't exist
 * ENOTDIR - an intermediate component of the path (e.g. 'b' in
 *           /a/b/c) is not a directory
 */


/* note on splitting the 'path' variable:
 * the value passed in by the FUSE framework is declared as 'const',
 * which means you can't modify it. The standard mechanisms for
 * splitting strings in C (strtok, strsep) modify the string in place,
 * so you have to copy the string and then free the copy when you're
 * done. One way of doing this:
 *
 *    char *_path = strdup(path);
 *    int inum = translate(_path);
 *    free(_path);
 */
static int translate(const char* path) {

    char* _path = strdup(path);

    //root inode = 1:
    if (strcmp(_path, "/") == 0) {
        free(_path);
        return root_inode;
    }

    int index = 1;

    // split the path and traverse:
    char* token = strtok(_path, "/");
    while (token != NULL) {
        struct fs_inode current_inode = inodes[index];

        // check if a directory:
        if (!S_ISDIR(current_inode.mode)) {
            free(_path);
            return -ENOTDIR;
        }

        int numentries = current_inode.size / sizeof(struct fs_dirent);
        struct fs_dirent* dir_entries = malloc((size_t) current_inode.size);

        int read = read_inode(index, (char*) dir_entries, (size_t) current_inode.size, 0);

        if (read < 0) {
            free(dir_entries);
            free(_path);
            return read;
        }

        int entry = locate_entry(token, dir_entries, numentries);

        if (entry < 0) {
            free(dir_entries);
            free(_path);
            return entry;
        }

        index = dir_entries[entry].inode;

        free(dir_entries);

        token = strtok(NULL, "/");
    }
    free(_path);
    return index;
}

static void inode_getattr(int inodenum, struct stat* sb) {
    struct fs_inode i = inodes[inodenum];

    // set the static fields here:
    memset(sb, 0, sizeof(*sb));
    sb->st_uid = i.uid;
    sb->st_gid = i.gid;
    sb->st_mode = i.mode;
    sb->st_ctime = (time_t) i.ctime;
    sb->st_atime = (time_t) i.mtime;
    sb->st_mtime = (time_t) i.mtime;
    sb->st_size = i.size;
    sb->st_nlink = 1;
    sb->st_blksize = FS_BLOCK_SIZE;
    sb->st_blocks = (i.size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;
}

/* getattr - get file or directory attributes. For a description of
 *  the fields in 'struct stat', see 'man lstat'.
 *
 * Note - fields not provided in fsx600 are:
 *    st_nlink - always set to 1
 *    st_atime - set to same value as st_mtime
 *
 * errors - path translation, ENOENT
 */
static int fs_getattr(const char* path, struct stat* sb) {

    // search for inode in the path:
    int index = translate(path);

    if (index < 0)
        return index;

    inode_getattr(index, sb);

    return 0;
}

/* readdir - get directory contents.
 *
 * for each entry in the directory, invoke the 'filler' function,
 * which is passed as a function pointer, as follows:
 *     filler(buf, <name>, <statbuf>, 0)
 * where <statbuf> is a pointer to struct stat, just like in getattr.
 *
 * Errors - path resolution, ENOTDIR, ENOENT
 */
static int fs_readdir(const char* path, void* ptr, fuse_fill_dir_t filler,
                      UNUSED off_t offset, UNUSED struct fuse_file_info* fi) {

    int index = translate(path);
    if (index < 0)
        return index;

    struct fs_inode* i = &inodes[index];

    if (!S_ISDIR(i->mode))
        return -ENOTDIR;

    struct fs_dirent* directory_entries = malloc((size_t) i->size);
    int num_entries = i->size / sizeof(struct fs_dirent);

    read_inode(index, (char*) directory_entries, (size_t) i->size, 0);

    struct stat sb;

    for (int j = 0; j < num_entries; j++) {
        // if the directory entries are valid:
        if (directory_entries[j].valid) {

            inode_getattr(directory_entries[j].inode, &sb);

            /* insert entry into the buffer */
            filler(ptr, directory_entries[j].name, &sb, 0);
        }
    }

    free(directory_entries);
    return 0;
}

static int add_to_directory(uint32_t dirnode, uint32_t childnode, const char* name) {
    struct fs_inode* parent_dir = &inodes[dirnode];

    if (!S_ISDIR(parent_dir->mode))
        return -ENOTDIR;

    // Find an opening in the directory
    int num_entries = parent_dir->size / sizeof(struct fs_dirent);

    struct fs_dirent* entries = malloc(num_entries * sizeof(struct fs_dirent));

    CALL_OR_RET_ERROR(read_inode(dirnode, (char*) entries, (size_t) parent_dir->size, 0), read_err);

    // Make sure it doesn't exist already
    int entry = locate_entry(name, entries, num_entries);
    if(entry != -ENOENT){
        return -EEXIST;
    }

    // Where the entry should go
    // Default to appending at end of existing entries
    int slot = num_entries;

    for (int i = 0; i < num_entries; i++) {
        if (!entries[i].valid) {
            slot = i;
            break;
        }
    }

    free(entries);

    struct fs_dirent newentry;

    newentry.valid = 1;
    newentry.isDir = (uint32_t) S_ISDIR(inodes[childnode].mode);
    newentry.inode = childnode;
    strncpy((char*) &newentry.name, name, 27);
    newentry.name[27] = 0;

    CALL_OR_RET_ERROR(
            write_inode(dirnode, (char*) &newentry, sizeof(struct fs_dirent), slot * sizeof(struct fs_dirent)),
            write_err);

    return slot;
}

int locate_entry(const char* file_name, const struct fs_dirent* directory_entries, int num_entries) {
    int matching_entry = -ENOENT;

    for (int c = 0; c < num_entries; c++) {
        if (directory_entries[c].valid == 1 && strncmp(directory_entries[c].name, file_name, 27) == 0) {
            matching_entry = c;
            break;
        }
    }
    return matching_entry;
}

/**
 * Returns the inode number of the removed entry
 */
static int remove_from_directory(uint32_t dirnode, const char* file_name) {
    struct fs_inode* parent_dir = &inodes[dirnode];

    if (!S_ISDIR(parent_dir->mode))
        return -ENOTDIR;

    int num_entries = parent_dir->size / sizeof(struct fs_dirent);

    struct fs_dirent* directory_entries = malloc(num_entries * sizeof(struct fs_dirent));

    // Read all directory entries
    CALL_OR_RET_ERROR(read_inode(dirnode, (char*) directory_entries, (size_t) parent_dir->size, 0), read_err);

    CALL_OR_RET_ERROR(locate_entry(file_name, directory_entries, num_entries), matching_entry);

    directory_entries[matching_entry].valid = 0;
    int node = directory_entries[matching_entry].inode;

    write_inode(dirnode,
                (char*) &directory_entries[matching_entry],
                sizeof(struct fs_dirent),
                matching_entry * sizeof(struct fs_dirent));

    free(directory_entries);

    return node;
}

static int first_available_inode_idx() {
    for (int c = 0; c < no_of_inodes; c++) {
        if (!bit_test(inode_map, c)) {
            return c;
        }
    }

    return -ENOSPC;
}

static int allocate_inode(uint32_t mode) {
    CALL_OR_RET_ERROR(first_available_inode_idx(), alloc_idx);

    bit_set(inode_map, alloc_idx);

    struct fs_inode* node = &inodes[alloc_idx];

    node->mode = mode;
    node->size = 0;
    for (int i = 0; i < N_DIRECT; i++) {
        node->direct[i] = 0;
    }
    node->indir_1 = 0;
    node->indir_2 = 0;
    node->ctime = (uint32_t) time(NULL);
    node->mtime = (uint32_t) time(NULL);
    node->uid = (uint16_t) getuid();
    node->gid = (uint16_t) getgid();

    return alloc_idx;
}

static int do_fs_create(const char* base, const char* file, mode_t mode) {
    CALL_OR_RET_ERROR(translate(base), basenode);

    if(!S_ISDIR(inodes[basenode].mode)){
        return -ENOTDIR;
    }

    CALL_OR_RET_ERROR(allocate_inode(mode), inode_num);

    return add_to_directory((uint32_t) basenode, (uint32_t) inode_num, file);
}

/* create - create a new file with specified permissions
 *
 * Errors - path resolution, EEXIST
 *          in particular, for create("/a/b/c") to succeed,
 *          "/a/b" must exist, and "/a/b/c" must not.
 *
 * Note that 'mode' will already have the S_IFREG bit set, so you can
 * just use it directly.
 *
 * If a file or directory of this name already exists, return -EEXIST.
 * If there are already 32 entries in the directory (i.e. it's filled an
 * entire block), you are free to return -ENOSPC instead of expanding it.
 */
static int fs_create(const char* path, mode_t mode, UNUSED struct fuse_file_info* fi) {
    int exists = translate(path);
    if (exists >= 0) {
        return -EEXIST;
    }

    char* base;
    char* file;
    split_path(path, &base, &file);

    int ret = do_fs_create(base, file, mode | S_IFREG);

    free(base);
    free(file);
    CALL_OR_RET_ERROR(flush_inodes_bitmaps(), flush);

    if (ret < 0) return ret;

    return 0;
}

static void split_path(const char* _path, char** parent_dir, char** file_name) {

    size_t last_delim = (strrchr(_path, '/') - _path) + 1; // length of after the last "/"

    char* pre_path = (char*) calloc(last_delim + 1, sizeof(char));

    size_t post_path_len = strlen(_path) - last_delim;
    char* post_path = (char*) calloc(post_path_len + 1, sizeof(char)); // Add 1 to leave room for null terminator

    memcpy(pre_path, _path, last_delim);
    memcpy(post_path, _path + last_delim, post_path_len);

    *parent_dir = pre_path;
    *file_name = post_path;
}

/* mkdir - create a directory with the given mode.
 * Errors - path resolution, EEXIST
 * Conditions for EEXIST are the same as for create. 
 *
 * If this would result in >32 entries in a directory, return -ENOSPC
 * Note that (unlike the 'create' case) 'mode' needs to be OR-ed with S_IFDIR.
 *
 * Note that you may want to combine the logic of fs_create and
 * fs_mkdir. 
 */
static int fs_mkdir(const char* path, mode_t mode) {

    int index = translate(path);

    // check if directory already exists:
    if (index > 0)
        return -EEXIST;

    char* file_name;
    char* parent_dir;
    split_path(path, &parent_dir, &file_name);

    // get inode index of the parent directory
    index = translate(parent_dir);
    if (index < 0) {
        free(file_name);
        free(parent_dir);
        return -ENOENT;
    }

    // find parent directory inode and check:
    struct fs_inode p_inode_directory = inodes[index];
    if (!S_ISDIR(p_inode_directory.mode)) {
        free(file_name);
        free(parent_dir);
        return -ENOTDIR;
    }

    CALL_OR_RET_ERROR(allocate_inode(mode | S_IFDIR), new_inode);

    CALL_OR_RET_ERROR(get_blk_number(new_inode, 0, 1), blk_number); // Allocate a block for the directory

    add_to_directory((uint32_t) index, (uint32_t) new_inode, file_name);

    free(file_name);
    free(parent_dir);

    return flush_inodes_bitmaps();
}

int N_INDIRECT = FS_BLOCK_SIZE / sizeof(uint32_t);

static int get_or_alloc_on_fail(uint32_t* place_of_node, int alloc_on_fail) {
    if (!(*place_of_node)) {
        if (alloc_on_fail) {
            CALL_OR_RET_ERROR(block_allocation(), newblk);

            static const void* zeroes[FS_BLOCK_SIZE] = {0};

            // Zero out old block
            CALL_OR_RET_ERROR(block_write(zeroes, newblk, 1), write1);

            *place_of_node = (uint32_t) newblk;
        } else {
            return -1;
        }
    }

    return *place_of_node;
}

#define N_BEFORE_INDIR_2 (N_DIRECT + N_INDIRECT)

static int get_blk_number(int nodenum, int num, int alloc_on_fail) {

    struct fs_inode* inode = &inodes[nodenum];

    uint32_t indir_ptr[N_INDIRECT];

    if (num < N_DIRECT) {

        return get_or_alloc_on_fail(&inode->direct[num], alloc_on_fail);

    } else if (num < N_DIRECT + N_INDIRECT) {
        // Read indirect block
        CALL_OR_RET_ERROR(get_or_alloc_on_fail(&inode->indir_1, alloc_on_fail), loc);
        CALL_OR_RET_ERROR(block_read(indir_ptr, loc, 1), read);

        int indir_idx = num - N_DIRECT;

        // Get the actual block
        CALL_OR_RET_ERROR(get_or_alloc_on_fail(&indir_ptr[indir_idx], alloc_on_fail), blknum);
        // Write back indirect block
        CALL_OR_RET_ERROR(block_write(indir_ptr, loc, 1), err);

        return blknum;

    } else if (num < N_BEFORE_INDIR_2 + (N_INDIRECT * N_INDIRECT)) {

        // Read first-level indirect 2
        CALL_OR_RET_ERROR(get_or_alloc_on_fail(&inode->indir_2, alloc_on_fail), loc_lvl_1);
        CALL_OR_RET_ERROR(block_read(indir_ptr, loc_lvl_1, 1), read);

        int indir_idx = num - N_BEFORE_INDIR_2;
        int idx_lvl_1 = indir_idx / N_INDIRECT;
        int idx_lvl_2 = indir_idx % N_INDIRECT;

        // Read second-level indirect 2
        CALL_OR_RET_ERROR(get_or_alloc_on_fail(&indir_ptr[idx_lvl_1], alloc_on_fail), loc_lvl_2);
        // Write back to first-level indirect
        CALL_OR_RET_ERROR(block_write(indir_ptr, loc_lvl_1, 1), err);

        CALL_OR_RET_ERROR(block_read(indir_ptr, loc_lvl_2, 1), read2);


        // Allocate second-level indirect
        CALL_OR_RET_ERROR(get_or_alloc_on_fail(&indir_ptr[idx_lvl_2], alloc_on_fail), blknum);
        // Write second-level indirect
        CALL_OR_RET_ERROR(block_write(indir_ptr, loc_lvl_2, 1), write1);

        return blknum;
    }

    return -EINVAL;
}

static int read_from_inode_block(void* read_buf, int block_number, int inode) {
    int blknum = get_blk_number(inode, block_number, 0);
    if (blknum < 0) return blknum;

    int read = block_read(read_buf, blknum, 1);
    return read;
}

static int read_inode(int index, char* buf, size_t len, off_t offset) {
    struct fs_inode* i = &inodes[index];

    if (offset >= i->size)
        return 0;

    if (len + offset > i->size)
        len = (size_t) (i->size - offset);

    int j = 0;

    char temp_read_buff[FS_BLOCK_SIZE] = {0};

    while (j < len) {
        int block_number = (int) offset / FS_BLOCK_SIZE;
        int block_offset = (int) offset % FS_BLOCK_SIZE;

        int err = read_from_inode_block(temp_read_buff, block_number, index);
        if (err < 0) {
            return err;
        }

        // calculate the length to read:
        size_t read_length;
        if ((FS_BLOCK_SIZE - block_offset) > (len - j))
            read_length = (len - j);
        else
            read_length = (size_t) (FS_BLOCK_SIZE - block_offset);

        //read and increase buffer.
        memcpy(buf + j, temp_read_buff + block_offset, read_length);
        j = j + read_length;
        offset = offset + read_length;
    }

    return j;
}


/* read - read data from an open file.
 * should return exactly the number of bytes requested, except:
 *   - if offset >= file len, return 0
 *   - if offset+len > file len, return bytes from offset to EOF
 *   - on error, return <0
 * Errors - path resolution, ENOENT, EISDIR
 */
static int fs_read(const char* path, char* buf, size_t len, off_t offset,
                   UNUSED struct fuse_file_info* fi) {
    int index = translate(path);

    if (index < 0)
        return index;

    if (S_ISDIR(inodes[index].mode)) {
        return -EISDIR;
    }

    return read_inode(index, buf, len, offset);
}


/* chmod - change file permissions
 * utime - change access and modification times
 *         (for definition of 'struct utimebuf', see 'man utime')
 *
 * Errors - path resolution, ENOENT.
 */
static int fs_chmod(const char* path, mode_t mode) {
    CALL_OR_RET_ERROR(translate(path), inodenum);

    int isDir = inodes[inodenum].mode & S_IFDIR;
    int isReg = inodes[inodenum].mode & S_IFREG;
    inodes[inodenum].mode = mode | isDir | isReg;

    return flush_inodes_bitmaps();
}

int fs_utime(const char* path, struct utimbuf* ut) {
    CALL_OR_RET_ERROR(translate(path), inodenum);

    time_t ctime = (ut->modtime == 0) ? time(NULL) : ut->modtime;
    inodes[inodenum].mtime = (uint32_t) ctime;

    return flush_inodes_bitmaps();
}

void dealloc(int blknum) {
    assert(blknum > 0);
    if (bit_test(block_map, blknum)) bit_clear(block_map, blknum);
}

static int truncate_inode(int inode_idx) {

    struct fs_inode* node = &inodes[inode_idx];

    // Mark blocks as free
    for (int blkidx = 0; blkidx < DIV_ROUND_UP(node->size, 1024); blkidx++) {
        int blknum = get_blk_number(inode_idx, blkidx, 0);
        if (blknum < 0) return blknum;

        dealloc(blknum);
    }

    // Get rid of extra blocks that may have been allocated after end of file
    for (int blkidx = DIV_ROUND_UP(node->size, 1024);
         blkidx < (N_DIRECT + N_INDIRECT + (N_INDIRECT * N_INDIRECT)); blkidx++) {

        int blknum = get_blk_number(inode_idx, blkidx, 0);
        if (blknum < 0) break;

        dealloc(blknum);
    }



    // Delete pointers to blocks
    int block = 0;
    for (; block < N_DIRECT; block++) {
        node->direct[block] = 0;
    }

    // delete all indir1 data
    if (node->indir_1) {
        dealloc(node->indir_1);
        node->indir_1 = 0;
    }

    if (node->indir_2) {

        uint32_t ptr_for_indr2_l1[N_INDIRECT];

        block_read(&ptr_for_indr2_l1, node->indir_2, 1);

        for (int indiridx = 0; indiridx < N_INDIRECT; indiridx++) {
            if (ptr_for_indr2_l1[indiridx]) {
                dealloc(ptr_for_indr2_l1[indiridx]);
            }
        }

        dealloc(node->indir_2);
        node->indir_2 = 0;
    }

    node->size = 0;
    node->mtime = (uint32_t) time(NULL);

    return 0;
}

/* truncate - truncate file to exactly 'len' bytes
 * Errors - path resolution, ENOENT, EISDIR, EINVAL
 *    return EINVAL if len > 0.
 */
static int fs_truncate(const char* path, off_t len) {
    /* you can cheat by only implementing this for the case of len==0,
     * and an error otherwise.
     */
    if (len != 0)
        return -EINVAL;        /* invalid argument */

    int index = translate(path);
    if (index < 0)
        return index;

    struct fs_inode* node = &inodes[index];

    if (S_ISDIR(node->mode))
        return -EISDIR;

    CALL_OR_RET_ERROR(truncate_inode(index), trunc_err);

    return flush_inodes_bitmaps();
}

/* unlink - delete a file
 *  Errors - path resolution, ENOENT, EISDIR
 * Note that you have to delete (i.e. truncate) all the data.
 */
static int fs_unlink(const char* path) {

    int index = translate(path);
    if (index < 0)
        return index;

    struct fs_inode* node = &inodes[index];

    if (S_ISDIR(node->mode))
        return -EISDIR;

    //truncate all data and check:
    int truncate_check = truncate_inode(index);
    if (truncate_check < 0)
        return truncate_check;

    // get file name:
    char* file_name;
    char* parent_directory;
    split_path(path, &parent_directory, &file_name);
    int i_dir = translate(parent_directory);

    int remove_err = remove_from_directory((uint32_t) i_dir, file_name);

    free(file_name);
    free(parent_directory);

    if (remove_err < 0) {
        return remove_err;
    }

    inodes[i_dir].mtime = (uint32_t) time(NULL);
    bit_clear(inode_map, index);

    return flush_inodes_bitmaps();
}

/* rmdir - remove a directory
 *  Errors - path resolution, ENOENT, ENOTDIR, ENOTEMPTY
 */
static int fs_rmdir(const char* path) {
    CALL_OR_RET_ERROR(translate(path), index)

    struct fs_inode* node = &inodes[index];

    if (!S_ISDIR(node->mode)) return -ENOTDIR;

    struct fs_dirent* entries = malloc((size_t) node->size);

    int read_err = read_inode(index, (char*) entries, (size_t) node->size, 0);

    if (read_err < 0) {
        free(entries);
        return read_err;
    }

    int num_entries = node->size / sizeof(*entries);

    int num_valid = 0;

    for (int i = 0; i < num_entries; i++) {
        if (entries[i].valid) {
            num_valid++;
        }
    }

    free(entries);

    if (num_valid > 0) {
        return -ENOTEMPTY;
    }

    char* parent;
    char* child;
    split_path(path, &parent, &child);

    int parentidx = translate(parent);

    int err = remove_from_directory((uint32_t) parentidx, child);
    bit_clear(inode_map, index);

    free(parent);
    free(child);

    CALL_OR_RET_ERROR(truncate_inode(index), trunc_error);

    CALL_OR_RET_ERROR(flush_inodes_bitmaps(), flush);

    if (err < 0) return err;

    return 0;
}

// Helper method to avoid messy resource cleaning in fs_rename
static int move_from_to(const char* src_dir, const char* src_file,
                        const char* dst_dir, const char* dst_file) {

    CALL_OR_RET_ERROR(translate(src_dir), src_inode);
    if (!S_ISDIR(inodes[src_inode].mode)) return -ENOTDIR;

    CALL_OR_RET_ERROR(translate(dst_dir), dst_inode);
    if (!S_ISDIR(inodes[dst_inode].mode)) return -ENOTDIR;

    CALL_OR_RET_ERROR(remove_from_directory((uint32_t) src_inode, src_file), file_inode);
    return add_to_directory((uint32_t) dst_inode, (uint32_t) file_inode, dst_file);
}

/* rename - rename a file or directory
 * Errors - path resolution, ENOENT, EINVAL, EEXIST
 *
 * ENOENT - source does not exist
 * EEXIST - destination already exists
 * EINVAL - source and destination are not in the same directory
 *
 * Note that this is a simplified version of the UNIX rename
 * functionality - see 'man 2 rename' for full semantics. In
 * particular, the full version can move across directories, replace a
 * destination file, and replace an empty directory with a full one.
 */
static int fs_rename(const char* src_path, const char* dst_path) {
    CALL_OR_RET_ERROR(translate(src_path), idx_from);

    int idx_to = translate(dst_path);
    if (idx_to >= 0) return -EEXIST;

    char* source_dir;
    char* source_file;

    char* dest_dir;
    char* dest_file;

    split_path(src_path, &source_dir, &source_file);
    split_path(dst_path, &dest_dir, &dest_file);

    int ret = move_from_to(source_dir, source_file, dest_dir, dest_file);

    free(source_dir);
    free(source_file);
    free(dest_dir);
    free(dest_file);

    CALL_OR_RET_ERROR(flush_inodes_bitmaps(), flush);

    if(ret < 0) return ret;

    return 0;
}

static int write_inode(int index, const char* buf, size_t len, off_t offset) {
    struct fs_inode* i = &inodes[index];

    if (offset > i->size) {
        return -EINVAL;
    }

    int first_blk_idx = (int) offset / FS_BLOCK_SIZE;
    int first_blk_offset = (int) offset % FS_BLOCK_SIZE;

    off_t end = len + offset;

    int last_blk_idx = (int) end / FS_BLOCK_SIZE;
    int last_blk_len = (int) end % FS_BLOCK_SIZE;

    int write_idx = 0;

    char tmp_buffer[FS_BLOCK_SIZE];

    for (int current_blk = first_blk_idx; current_blk <= last_blk_idx; current_blk++) {
        int blk_no_allocate = get_blk_number(index, current_blk, 0);
        int blk_actual_idx = get_blk_number(index, current_blk, 1);

        if (blk_actual_idx < 0) {
            return blk_actual_idx;
        }

        int data_already_exists = (blk_no_allocate == blk_actual_idx);

        int is_first_block = (current_blk == first_blk_idx);
        int is_last_block = (current_blk == last_blk_idx);

        int start_write = is_first_block ? first_blk_offset : 0;
        int end_write = is_last_block ? last_blk_len : FS_BLOCK_SIZE;

        size_t write_len = (size_t) end_write - start_write;
        if (write_len == 0) continue;

        if (data_already_exists && write_len < FS_BLOCK_SIZE) {
            int err = block_read(tmp_buffer, blk_actual_idx, 1);
            if (err < 0) {
                return err;
            }
        } else {
            memset(tmp_buffer, 0, FS_BLOCK_SIZE);
        }

        memcpy(tmp_buffer + start_write, buf + write_idx, write_len);
        block_write(tmp_buffer, blk_actual_idx, 1);

        write_idx += write_len;
    }

    if (end > i->size) {
        i->size = (int32_t) end;
    }

    return len;
}

/* write - write data to a file
 * It should return exactly the number of bytes requested, except on
 * error.
 * Errors - path resolution, ENOENT, EISDIR
 *  return EINVAL if 'offset' is greater than current file length.
 *  (POSIX semantics support the creation of files with "holes" in them, 
 *   but we don't)
 */
static int fs_write(const char* path, const char* buf, size_t len,
                    off_t offset, UNUSED struct fuse_file_info* fi) {
    int index = translate(path);
    if (index < 0)
        return index;

    if (S_ISDIR(inodes[index].mode))
        return -EISDIR;

    int ret = write_inode(index, buf, len, offset);

    CALL_OR_RET_ERROR(flush_inodes_bitmaps(), flush);

    return ret;
}

/* statfs - get file system statistics
 * see 'man 2 statfs' for description of 'struct statvfs'.
 * Errors - none. 
 */
static int fs_statfs(UNUSED const char* path, struct statvfs* st) {
    /* needs to return the following fields (set others to zero):
     *   f_bsize = BLOCK_SIZE
     *   f_blocks = total image - metadata
     *   f_bfree = f_blocks - blocks used
     *   f_bavail = f_bfree
     *   f_namelen = <whatever your max namelength is>
     *
     * this should work fine, but you may want to add code to
     * calculate the correct values later.
     */
    st->f_bsize = FS_BLOCK_SIZE;
    st->f_blocks = (__fsblkcnt_t) no_of_blocks;

    size_t no_of_blocks_used = 0;
    for (int i = 0; i < no_of_blocks; i++)
        if (bit_test(block_map, i))
            no_of_blocks_used++;

    st->f_bfree = no_of_blocks - no_of_blocks_used;            /* change these */
    st->f_bavail = st->f_bfree;           /* values */
    st->f_namemax = 27;

    return 0;
}


/* operations vector. Please don't rename it, as the code in
 * misc.c and libhw3.c assumes it is named 'fs_ops'.
 */
struct fuse_operations fs_ops = {
        .init = fs_init,
        .getattr = fs_getattr,
        .readdir = fs_readdir,
        .create = fs_create,
        .mkdir = fs_mkdir,
        .unlink = fs_unlink,
        .rmdir = fs_rmdir,
        .rename = fs_rename,
        .chmod = fs_chmod,
        .utime = fs_utime,
        .truncate = fs_truncate,
        .read = fs_read,
        .write = fs_write,
        .statfs = fs_statfs,
};

