## Task 3 - Drama Troll

In this task, we need to create a fuse script that can do three things when it is mounted:
1. Display different text according to the user who uses the "concatenate" command on a certain file
2. Trigger a certain action is performed
3. Have a persistent ASCII art of the text displayed in the files when the trigger is performed

# Code
```
##include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <unistd.h>
```
These are the headers, amongst which we've included the fuse header so that this script may be mounted, string so that it can perform string functions like compare to register the trigger action, fncntl to mask the files for access mode, and pwd.h to grab user password.
```
#define FILE1_PATH "/very_spicy_info.txt"
#define FILE2_PATH "/upload.txt"
#define TROLL_FLAG_PATH "/tmp/.trollflag"
```
These are the three relevant paths used in this fuse script, two files and one path for the trap flag. 
```
int is_trap_triggered() {
    return access(TROLL_FLAG_PATH, F_OK) == 0;
}

void set_trap_flag() {
    FILE *f = fopen(TROLL_FLAG_PATH, "w");
    if (f) {
        fputs("TRIGGERED", f);
        fclose(f);
    }
}
```
This part is to heck whether the flag is triggered or not, and to activate the flag persistently.
```
static int fs_getattr(const char *path, struct stat *st, struct fuse_file_info *fi) {
    (void) fi;
    memset(st, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;
    } else if (strcmp(path, FILE1_PATH) == 0 || strcmp(path, FILE2_PATH) == 0) {
        st->st_mode = S_IFREG | 0444;
        st->st_nlink = 1;
        if (is_trap_triggered()) {
            st->st_size = strlen(" _  _         _  _   _   ___ ___ \n|_ |_ |  |   |_ | | |_|   |   |\n|  |_ |_ |_  |  |_| |\\   _|_  |\n       _   _   _  ___\n      |_| | _ |_|  |  |\\ |\n   0   | | |_| | | _|_ | \\|\n     _   _        _   _   _\n    |_| |_ \\ | / |_| |_| | \\\n    |\\  |_  V V  | | |\\  |_/");
        } else if (strcmp(path, FILE1_PATH) == 0) {
            st->st_size = 100;
        } else {
            st->st_size = 0;
        }
    } else {
        return -ENOENT;
    }

    return 0;
}
```
This is what to do when the flag is triggered. Essentially, it replaces the content of said file into the ascii art when the "concatenate" function is used on the file.
```
static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;

    if (strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    filler(buf, "very_spicy_info.txt", NULL, 0, 0);
    filler(buf, "upload.txt", NULL, 0, 0);

    return 0;
}
```
These are buffers to store string data.
```
static int fs_open(const char *path, struct fuse_file_info *fi) {
    if (strcmp(path, FILE1_PATH) != 0 && strcmp(path, FILE2_PATH) != 0)
        return -ENOENT;

    if ((fi->flags & O_ACCMODE) != O_RDONLY && strcmp(path, FILE2_PATH) != 0)
        return -EACCES;

    return 0;
}

static int fs_read(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
    (void) fi;

    const char *msg = "";

    if (is_trap_triggered()) {
        msg = " _  _         _  _   _   ___ ___ \n|_ |_ |  |   |_ | | |_|   |   |\n|  |_ |_ |_  |  |_| |\\   _|_  |\n       _   _   _  ___\n      |_| | _ |_|  |  |\\ |\n      | | |_| | | _|_ | \\|\n     _   _        _   _   _\n    |_| |_ \\ | / |_| |_| | \\\n    |\\  |_  V V  | | |\\  |_/";
    } else if (strcmp(path, FILE1_PATH) == 0) {
        struct fuse_context *ctx = fuse_get_context();
        struct passwd *pw = getpwuid(ctx->uid);
        const char *username = pw ? pw->pw_name : "unknown";
        msg = strcmp(username, "DainTontas") == 0 ? "Very spicy internal development information: leaked roadmap.docx" : "DainTontas' personal secret!!.txt";
    } else if (strcmp(path, FILE2_PATH) == 0) {
        msg = "";
    } else {
        return -ENOENT;
    }

    size_t len = strlen(msg);
    if (offset >= len) return 0;
    if (offset + size > len) size = len - offset;
    memcpy(buf, msg + offset, size);
    return size;
}
```
This replaces the content of the file based on who is accessing it. If user DainTontas accesses the file using "concatenate", it would show a different output to if other users were to open this file. This also replaces the content of th file with the ASCII art when the flag is triggered.
```
static int fs_write(const char *path, const char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi) {
    (void) offset;
    (void) fi;

    if (strcmp(path, FILE2_PATH) == 0) {
        if (size >= 6 && strncmp(buf, "upload", 6) == 0) {
            set_trap_flag();
        }
        return size;
    }

    return -EACCES;
}
```
This is the trigger, the trigger turns on when the string "upload" is echoed into the file "upload.txt"
```
static const struct fuse_operations fs_oper = {
    .getattr = fs_getattr,
    .readdir = fs_readdir,
    .open    = fs_open,
    .read    = fs_read,
    .write   = fs_write,
};

int main(int argc, char *argv[]) {
    return fuse_main(argc, argv, &fs_oper, NULL);
}
```
And these are all the operations to be called when running the fuse script.
