# FUSecure

Yuadi, a skilled developer, is frustrated because his friend Irwandi repeatedly plagiarizes his operating system practicum answers despite warnings and reprimands from teaching assistants. To protect his work and integrity, Yuadi leverages his data security expertise to create FUSecure, a FUSE-based file system that enforces read-only access and restricts file visibility based on user permissions.

### a. Directory Setup and User Creation

1. Create a "source directory" on your system (e.g., `/home/shared_files`). This will serve as the main storage location for all files.

```
sudo mkdir -p /home/shared_files/public
sudo mkdir -p /home/shared_files/private_yuadi
sudo mkdir -p /home/shared_files/private_irwandi
```

2. Inside this source directory, create 3 subdirectories: `public`, `private_yuadi`, `private_irwandi`. Create 2 Linux users: `yuadi` and `irwandi`. You can choose their passwords.
   | User | Private Folder |
   | ------- | --------------- |
   | yuadi | private_yuadi |
   | irwandi | private_irwandi |

```
sudo adduser yuadi
sudo adduser irwandi
```

Yuadi wisely designed this structure: a `public` folder for sharing course materials and references accessible to anyone, while each person has a private folder to store their respective practicum answers.

```
sudo chown yuadi:yuadi /home/shared_files/private_yuadi
sudo chown irwandi:irwandi /home/shared_files/private_irwandi

sudo chmod 755 /home/shared_files/public

sudo chmod 700 /home/shared_files/private_*
```

### b. Mount Point Access

Next, Yuadi wants to ensure his file system is easily accessible yet controlled.

Your FUSE mount point (e.g., `/mnt/secure_fs`) should display the contents of the `source directory` directly. So, if you `ls /mnt/secure_fs`, you should see `public/`, `private_yuadi/`, and `private_irwandi/`.

```
#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <stdlib.h>

static const char *root_path = "/home/shared_files";

static int is_user(uid_t uid, const char *username) {
    struct passwd *pw = getpwnam(username);
    if (!pw) return 0;
    return uid == pw->pw_uid;
}

static int xmp_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    int res;
    char fullpath[1024];

    uid_t uid = fuse_get_context()->uid;

    // Map / to root_path
    if (strcmp(path, "/") == 0) {
        res = lstat(root_path, stbuf);
        if (res == -1)
            return -errno;
        return 0;
    }

    // Build full real path
    snprintf(fullpath, sizeof(fullpath), "%s%s", root_path, path);

    // Access control for private dirs
    if (strncmp(path, "/private_yuadi", 14) == 0 && !is_user(uid, "yuadi"))
        return -EACCES;
    if (strncmp(path, "/private_irwandi", 16) == 0 && !is_user(uid, "irwandi"))
        return -EACCES;

    res = lstat(fullpath, stbuf);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi,
                       enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;

    DIR *dp;
    struct dirent *de;
    char fullpath[1024];
    uid_t uid = fuse_get_context()->uid;

    // Map / to root_path
    if (strcmp(path, "/") == 0) {
        dp = opendir(root_path);
        if (dp == NULL)
            return -errno;

        while ((de = readdir(dp)) != NULL) {
            // Always list all entries at root (including private dirs)
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
                continue;
            filler(buf, de->d_name, NULL, 0, 0);
        }
        closedir(dp);
        return 0;
    }

    // Access control inside private dirs
    if (strncmp(path, "/private_yuadi", 14) == 0 && !is_user(uid, "yuadi"))
        return -EACCES;
    if (strncmp(path, "/private_irwandi", 16) == 0 && !is_user(uid, "irwandi"))
        return -EACCES;

    // Build full real path
    snprintf(fullpath, sizeof(fullpath), "%s%s", root_path, path);

    dp = opendir(fullpath);
    if (dp == NULL)
        return -errno;

    while ((de = readdir(dp)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        filler(buf, de->d_name, NULL, 0, 0);
    }
    closedir(dp);
    return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi) {
    int res;
    char fullpath[1024];
    uid_t uid = fuse_get_context()->uid;

    // Access control for private dirs
    if (strncmp(path, "/private_yuadi", 14) == 0 && !is_user(uid, "yuadi"))
        return -EACCES;
    if (strncmp(path, "/private_irwandi", 16) == 0 && !is_user(uid, "irwandi"))
        return -EACCES;

    snprintf(fullpath, sizeof(fullpath), "%s%s", root_path, path);

    res = open(fullpath, fi->flags);
    if (res == -1)
        return -errno;

    close(res);
    return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi) {
    int fd;
    int res;
    char fullpath[1024];

    (void) fi;

    snprintf(fullpath, sizeof(fullpath), "%s%s", root_path, path);

    fd = open(fullpath, O_RDONLY);
    if (fd == -1)
        return -errno;

    res = pread(fd, buf, size, offset);
    if (res == -1)
        res = -errno;

    close(fd);
    return res;
}

// Deny all write operations (read-only)
static int deny_write() {
    return -EROFS;
}

static struct fuse_operations xmp_oper = {
    .getattr    = xmp_getattr,
    .readdir    = xmp_readdir,
    .open       = xmp_open,
    .read       = xmp_read,

    // Deny all write ops
    .write      = deny_write,
    .mkdir      = deny_write,
    .rmdir      = deny_write,
    .mknod      = deny_write,
    .unlink     = deny_write,
    .rename     = deny_write,
    .create     = deny_write,
    .chmod      = deny_write,
    .chown      = deny_write,
    .truncate   = deny_write,
    .utimens    = deny_write,
    .symlink    = deny_write,
    .link       = deny_write,
    .setxattr   = deny_write,
    .removexattr= deny_write,
    .release    = deny_write,
    .fsync      = deny_write,
    .getxattr   = deny_write,
    .listxattr  = deny_write,
    .readlink   = deny_write,
    .statfs     = deny_write,
};

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <mountpoint>\n", argv[0]);
        return 1;
    }
    umask(0);
    return fuse_main(argc, argv, &xmp_oper, NULL);
}
```

1. Includes and Definitions

```
#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <stdlib.h>
```
These are standard headers for:
- FUSE support
- File and directory access
- User identity and error handling

2. Set Source Directory
```
static const char *root_path = "/home/shared_files";
```
This is the real directory you are mirroring with FUSE. It contains:
- /public
- /private_yuadi
- /private_irwandi

3. Check if UID matches a specific username
```
static int is_user(uid_t uid, const char *username) {
    struct passwd *pw = getpwnam(username);
    if (!pw) return 0;
    return uid == pw->pw_uid;
}
```
This function checks if the calling user matches the given username (used for access control).

4. xmp_getattr – Handle file metadata requests
```
static int xmp_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
```
- Runs every time a file or folder is accessed (e.g. ls, cat)
- Builds the real file path
- Blocks access to private_yuadi if user ≠ yuadi
- Blocks access to private_irwandi if user ≠ irwandi

5. xmp_readdir – Handle directory listings
```
static int xmp_readdir(const char *path, void *buf, ...)
```
Lists directory contents (ls)

Always shows all folders at /
- When reading inside /private_yuadi, checks if user is yuadi
- When reading inside /private_irwandi, checks if user is irwandi
- Denies listing if unauthorized

6. xmp_open – Open file (for reading only)
```
static int xmp_open(const char *path, struct fuse_file_info *fi)
```
- Builds real file path
- Blocks access to private files if the user is not authorized
- Allows only reading (open), no write or exec

7. xmp_read – Read file content
```
static int xmp_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
```
- Opens the real file at root_path + path
- Reads up to size bytes at offset into buffer
- Used by commands like cat, less, etc.

8. deny_write() – Used to block all write operations
```
static int deny_write() {
    return -EROFS;  // Error: Read-only file system
}
```
9. fuse_operations structure – Register file system behavior
```
static struct fuse_operations xmp_oper = {
    .getattr    = xmp_getattr,
    .readdir    = xmp_readdir,
    .open       = xmp_open,
    .read       = xmp_read,
    // Everything else is denied
    .write      = deny_write,
    .mkdir      = deny_write,
    ...
};
```
This tells FUSE which function to call for each file operation:
- What to do when reading a file → call xmp_read
- What to do when trying to create a file → call deny_write

10. main() – Entry point
```
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <mountpoint>\n", argv[0]);
        return 1;
    }
    umask(0);
    return fuse_main(argc, argv, &xmp_oper, NULL);
}
```
Mounts the file system at the specified path

### c. Read-Only for All Users

Yuadi was very annoyed with Irwandi's habit of modifying or even deleting answer files after copying them to erase traces of plagiarism. To prevent this, he decided to make the entire system read-only.

1. Make the entire FUSE mount point **read-only for all users**.
2. This means no user (including `root`) can create, modify, or delete any files or folders within `/mnt/secure_fs`. Commands like `mkdir`, `rmdir`, `touch`, `rm`, `cp`, `mv` should all fail.

```
sudo ./FUSecure -o allow_other /mnt/secure_fs
```

<img width="1470" alt="Image" src="https://github.com/user-attachments/assets/6c8d2ce2-0099-475b-be76-8f628f572f0d" />


### d. Public Folder Access

Despite wanting to protect his practicum answers, Yuadi still wanted to share course materials and references with Irwandi and other friends.

Any user (including `yuadi`, `irwandi`, or others) should be able to **read** the contents of any file inside the `public` folder. For example, `cat /mnt/secure_fs/public/course_material.txt` should work for `yuadi` and `irwandi`.

<img width="1470" alt="Image" src="https://github.com/user-attachments/assets/bcca2550-320e-4942-a40a-6f4fe4ef1856" />

### e. Restricted Private Folder Access

This is the most important part of Yuadi's plan - ensuring practicum answers are truly protected from plagiarism.

1. Files inside `private_yuadi` can **only be read by the user `yuadi`**. If `irwandi` tries to read practicum answer files in `private_yuadi`, it should fail (e.g., permission denied).
2. Similarly, files inside `private_irwandi` can **only be read by the user `irwandi`**. If `yuadi` tries to read a file in `private_irwandi`, it should fail.

<img width="1470" alt="Image" src="https://github.com/user-attachments/assets/e5ed2cdd-091d-4646-bbf3-477c54ef9adc" />

## Example Scenario

After the system is complete, this is how FUSecure works in daily academic life:

- **User `yuadi` logs in:**

  - `cat /mnt/secure_fs/public/algorithm_material.txt` (Succeeds - course materials can be read)
  - `cat /mnt/secure_fs/private_yuadi/practicum1_answer.c` (Succeeds - his own practicum answer)
  - `cat /mnt/secure_fs/private_irwandi/sisop_assignment.c` (Fails with "Permission denied" - cannot read Irwandi's practicum answers)
  - `touch /mnt/secure_fs/public/new_file.txt` (Fails with "Permission denied" - read-only system)

- **User `irwandi` logs in:**
  - `cat /mnt/secure_fs/public/algorithm_material.txt` (Succeeds - course materials can be read)
  - `cat /mnt/secure_fs/private_irwandi/sisop_assignment.c` (Succeeds - his own practicum answer)
  - `cat /mnt/secure_fs/private_yuadi/practicum1_answer.c` (Fails with "Permission denied" - cannot read Yuadi's practicum answers)
  - `mkdir /mnt/secure_fs/new_folder` (Fails with "Permission denied" - read-only system)
 
<img width="1470" alt="Image" src="https://github.com/user-attachments/assets/0e4bb4dc-5233-475a-b971-6a3322640da4" />
