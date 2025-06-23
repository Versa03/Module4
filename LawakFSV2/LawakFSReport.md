# LawakFS++ - A Cursed Filesystem with Censorship and Strict Access Policies

```c
#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>

static char **filter_words = NULL;
static int   filter_count  = 0;
static char  secret_basename[256];
static int   access_start, access_end;
static char  source_path[PATH_MAX];

#define LOG_PATH "/var/log/lawakfs.log"
static void log_action(const char *action, const char *path) {
    FILE *lg = fopen(LOG_PATH, "a");
    if (!lg) return;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    fprintf(lg,
      "[%04d-%02d-%02d %02d:%02d:%02d] [%d] [%s] %s\n",
      tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
      tm->tm_hour, tm->tm_min, tm->tm_sec,
      fuse_get_context()->uid, action, path);
    fclose(lg);
}

static int is_outside_time() {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    int h = tm->tm_hour;
    return (h < access_start || h >= access_end);
}

static int is_secret(const char *path) {
    const char *base = path + 1;
    char tmp[256];
    snprintf(tmp, sizeof tmp, "%s", base);
    char *dot = strrchr(tmp, '.');
    if (dot) *dot = 0;
    return strcmp(tmp, secret_basename) == 0;
}

static size_t replace_filters(const char *in, size_t len, char *out, size_t osz) {
    size_t i = 0, j = 0;
    while (i < len && j + 5 < osz) {
        int matched = 0;
        for (int w = 0; w < filter_count; w++) {
            int L = strlen(filter_words[w]);
            if (i + L <= len
             && strncasecmp(in + i, filter_words[w], L) == 0
             && (i == 0 || !isalnum(in[i-1]))
             && (i + L == len || !isalnum(in[i+L]))) {
                memcpy(out + j, "lawak", 5);
                j += 5;
                i += L;
                matched = 1;
                break;
            }
        }
        if (!matched)
            out[j++] = in[i++];
    }
    return j;
}

static const char b64[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static char *b64_encode(const unsigned char *d, size_t len, size_t *olen) {
    size_t o = ((len + 2) / 3) * 4;
    char *buf = malloc(o + 1), *p = buf;
    for (size_t i = 0; i < len; i += 3) {
        int v = d[i] << 16;
        if (i + 1 < len) v |= d[i+1] << 8;
        if (i + 2 < len) v |= d[i+2];
        *p++ = b64[(v >> 18) & 0x3F];
        *p++ = b64[(v >> 12) & 0x3F];
        *p++ = (i + 1 < len ? b64[(v >> 6) & 0x3F] : '=');
        *p++ = (i + 2 < len ? b64[v & 0x3F] : '=');
    }
    *p = 0;
    if (olen) *olen = p - buf;
    return buf;
}

static void build_real_path(const char *path, char *fpath) {
    snprintf(fpath, PATH_MAX, "%s%s", source_path, path);
    if (access(fpath, F_OK) == 0) return;

    char tmp[PATH_MAX];
    snprintf(tmp, PATH_MAX, "%s%s.txt", source_path, path);
    if (access(tmp, F_OK) == 0) {
        strncpy(fpath, tmp, PATH_MAX);
        return;
    }

    DIR *d = opendir(source_path);
    if (d) {
        struct dirent *de;
        const char *base = path + 1;
        while ((de = readdir(d))) {
            char *dot = strrchr(de->d_name, '.');
            if (!dot) continue;
            size_t namelen = dot - de->d_name;
            if (namelen == strlen(base)
             && strncmp(de->d_name, base, namelen) == 0) {
                snprintf(fpath, PATH_MAX, "%s/%s",
                         source_path, de->d_name);
                closedir(d);
                return;
            }
        }
        closedir(d);
    }

    snprintf(fpath, PATH_MAX, "%s%s", source_path, path);
}

static void parse_config() {
    FILE *cf = fopen("lawak.conf", "r");
    if (!cf) {
        fprintf(stderr, "[DEBUG] Gagal membuka lawak.conf\n");
        return;
    }
    char line[512];
    while (fgets(line, sizeof line, cf)) {
        if (line[0] == '#' || !strchr(line, '=')) continue;
        char *k = strtok(line, "=\n"), *v = strtok(NULL, "\n");
        if (!strcmp(k, "FILTER_WORDS")) {
            char *p = strtok(v, ",");
            while (p) {
                filter_words = realloc(filter_words,
                    sizeof *filter_words * (filter_count + 1));
                filter_words[filter_count++] = strdup(p);
                p = strtok(NULL, ",");
            }
        } else if (!strcmp(k, "SECRET_FILE_BASENAME")) {
            strncpy(secret_basename, v,
                    sizeof secret_basename - 1);
        } else if (!strcmp(k, "ACCESS_START")) {
            sscanf(v, "%d", &access_start);
        } else if (!strcmp(k, "ACCESS_END")) {
            sscanf(v, "%d", &access_end);
        }
    }
    fclose(cf);
    fprintf(stderr,
      "[DEBUG] lawak.conf loaded (%d filters) secret=\"%s\" hours=%02d-%02d\n",
      filter_count, secret_basename, access_start, access_end);
}

static int lawak_getattr(const char *path, struct stat *stbuf,
                         struct fuse_file_info *fi) {
    if (is_secret(path) && is_outside_time())
        return -ENOENT;
    char fpath[PATH_MAX];
    build_real_path(path, fpath);
    int res = lstat(fpath, stbuf);
    return res == -1 ? -errno : 0;
}

static int lawak_readdir(const char *path, void *buf,
    fuse_fill_dir_t filler, off_t offset,
    struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    DIR *dp = opendir(source_path);
    if (!dp) return -errno;
    struct dirent *de;
    while ((de = readdir(dp))) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
        char name[PATH_MAX];
        strcpy(name, de->d_name);
        char *dot = strrchr(name, '.');
        if (dot) *dot = 0;
        struct stat st; memset(&st, 0, sizeof st);
        st.st_mode = de->d_type << 12;
        filler(buf, name, &st, 0, 0);
    }
    closedir(dp);
    return 0;
}

static int lawak_open(const char *path, struct fuse_file_info *fi) {
    if (is_secret(path) && is_outside_time()) return -ENOENT;
    char fpath[PATH_MAX]; build_real_path(path, fpath);
    int fd = open(fpath, O_RDONLY);
    if (fd < 0) return -errno;
    close(fd);
    return 0;
}

static int lawak_access(const char *path, int mask) {
    if (is_secret(path) && is_outside_time())
        return -ENOENT;
    char fpath[PATH_MAX];
    build_real_path(path, fpath);
    if (access(fpath, mask) == -1)
        return -errno;
    log_action("ACCESS", path);
    return 0;
}

static int lawak_read(const char *path, char *buf, size_t size,
                      off_t offset, struct fuse_file_info *fi) {
    if (is_secret(path) && is_outside_time())
        return -ENOENT;
    char fpath[PATH_MAX]; build_real_path(path, fpath);
    FILE *fp = fopen(fpath, "rb");
    if (!fp) return -errno;
    if (fseeko(fp, offset, SEEK_SET) < 0) { fclose(fp); return -errno; }
    unsigned char *raw = malloc(size);
    if (!raw) { fclose(fp); return -ENOMEM; }
    size_t nread = fread(raw, 1, size, fp);
    fclose(fp);
    const char *ext = strrchr(fpath, '.');
    if (ext && !strcasecmp(ext, ".txt")) {
        char *tmp = malloc(nread * 2 + 1);
        if (!tmp) { free(raw); return -ENOMEM; }
        size_t out_len = replace_filters((char*)raw, nread, tmp, nread * 2);
        size_t to_copy = out_len > size ? size : out_len;
        memcpy(buf, tmp, to_copy);
        nread = to_copy;
        free(tmp);
    } else {
        size_t b64_len;
        char *b64 = b64_encode(raw, nread, &b64_len);
        if (!b64) { free(raw); return -ENOMEM; }
        size_t to_copy = b64_len > size ? size : b64_len;
        memcpy(buf, b64, to_copy);
        nread = to_copy;
        free(b64);
    }
    free(raw);
    log_action("READ", path);
    return nread;
}

static struct fuse_operations ops = {
    .getattr = lawak_getattr,
    .readdir = lawak_readdir,
    .open    = lawak_open,
    .access  = lawak_access,
    .read    = lawak_read,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "testing", argv[0]);
        return 1;
    }

    realpath(argv[1], source_path);

    parse_config();  

    return fuse_main(argc - 1, argv + 1, &ops, NULL);
}
```

```c
static char **filter_words = NULL;
static int   filter_count  = 0;
static char  secret_basename[256];
static int   access_start, access_end;
static char  source_path[PATH_MAX];
#define LOG_PATH "/var/log/lawakfs.log"
```

What this does: Sets up the filesystem's configuration and state

- `filter_words` + `filter_count`: Together they form a dynamic array system. These store words that get replaced with "lawak" in text files.
```
filter_words → ["bad", "evil", "hate"] (array of 3 strings)
filter_count = 3
```

- `secret_basename`: A filename pattern (like "secret" or "confidential")
Files matching this pattern get time-based restrictions.

- `access_start + access_end`: Hour range (0-23) when secret files are accessible
Example: start=9, end=17 means 9 AM to 5 PM only.

- `source_path`: The real directory path that this virtual filesystem represents
Example: /home/user/docs - all virtual operations map to files here.

- `LOG_PATH`: Where to write the activity log file.

Purpose: This establishes the "configuration" of the filesystem - what words to filter, which files to restrict, when to allow access, and where everything is located.


```c
static void log_action(const char *action, const char *path) {
    FILE *lg = fopen(LOG_PATH, "a");
    if (!lg) return;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    fprintf(lg,
      "[%04d-%02d-%02d %02d:%02d:%02d] [%d] [%s] %s\n",
      tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
      tm->tm_hour, tm->tm_min, tm->tm_sec,
      fuse_get_context()->uid, action, path);
    fclose(lg);
}
```
Records filesystem activity to a log file. Step-by-step process:

1. Open log file in append mode (adds to end without overwriting)
2. Get current time and convert to human-readable format
3. Write formatted log entry with:

- Timestamp: [2025-06-23 14:30:45]
- User ID: [1000] (who performed the action)
- Action type: [READ] or [ACCESS]
- File path: `/documents/report.txt`

### a. Hidden File Extensions

After using regular filesystems for days, Teja realized that file extensions always made it too easy for people to identify file types. "This is too predictable!" he thought. He wanted to create a more mysterious system where people would have to actually open files to discover their contents.

All files displayed within the FUSE mountpoint must have their **extensions hidden**.

- **Example:** If the original file is `document.pdf`, an `ls` command inside the FUSE directory should only show `document`.
- **Behavior:** Despite the hidden extension, accessing a file (e.g., `cat /mnt/your_mountpoint/document`) must correctly map to its original path and name (e.g., `source_dir/document.pdf`).

## Directory Listing - Hiding Extensions

```c
static int lawak_readdir(const char *path, void *buf,
    fuse_fill_dir_t filler, off_t offset,
    struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    DIR *dp = opendir(source_path);
    if (!dp) return -errno;
    struct dirent *de;
    while ((de = readdir(dp))) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
        char name[PATH_MAX];
        strcpy(name, de->d_name);
        char *dot = strrchr(name, '.');
        if (dot) *dot = 0;  // ← THIS LINE HIDES EXTENSIONS
        struct stat st; memset(&st, 0, sizeof st);
        st.st_mode = de->d_type << 12;
        filler(buf, name, &st, 0, 0);
    }
    closedir(dp);
    return 0;
}
```
**Hides file extensions when listing directory contents**

1. **Open the source directory**
   ```c
   DIR *dp = opendir(source_path);
   ```
   Opens the real directory that contains the actual files.

2. **Read each directory entry**
   ```c
   while ((de = readdir(dp))) {
   ```
   Loops through all files and folders in the directory.

3. **Skip special entries**
   ```c
   if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
   ```
   Ignores current directory (`.`) and parent directory (`..`).

4. **Copy filename to modifiable buffer**
   ```c
   char name[PATH_MAX];
   strcpy(name, de->d_name);
   ```
   Creates a copy since we need to modify it.

5. **🔑 KEY STEP: Remove extension**
   ```c
   char *dot = strrchr(name, '.');  // Find last dot
   if (dot) *dot = 0;               // Cut string at dot
   ```
   **This is where extensions get hidden!**
   - `document.pdf` → `document`
   - `image.jpg` → `image`
   - `script.sh` → `script`

6. **Return the modified filename**
   ```c
   filler(buf, name, &st, 0, 0);
   ```
   Shows the extension-less name to the user.

---

## File Access - Mapping Back to Real Files

```c
static void build_real_path(const char *path, char *fpath) {
    snprintf(fpath, PATH_MAX, "%s%s", source_path, path);
    if (access(fpath, F_OK) == 0) return;

    char tmp[PATH_MAX];
    snprintf(tmp, PATH_MAX, "%s%s.txt", source_path, path);
    if (access(tmp, F_OK) == 0) {
        strncpy(fpath, tmp, PATH_MAX);
        return;
    }

    DIR *d = opendir(source_path);
    if (d) {
        struct dirent *de;
        const char *base = path + 1;
        while ((de = readdir(d))) {
            char *dot = strrchr(de->d_name, '.');
            if (!dot) continue;
            size_t namelen = dot - de->d_name;
            if (namelen == strlen(base)
             && strncmp(de->d_name, base, namelen) == 0) {
                snprintf(fpath, PATH_MAX, "%s/%s",
                         source_path, de->d_name);
                closedir(d);
                return;
            }
        }
        closedir(d);
    }

    snprintf(fpath, PATH_MAX, "%s%s", source_path, path);
}
```
**Maps extension-less virtual paths back to real files with extensions**

1. **Try direct path first**
   ```c
   snprintf(fpath, PATH_MAX, "%s%s", source_path, path);
   if (access(fpath, F_OK) == 0) return;
   ```
   Checks if file exists without extension (for extensionless files).

2. **Try adding .txt extension**
   ```c
   char tmp[PATH_MAX];
   snprintf(tmp, PATH_MAX, "%s%s.txt", source_path, path);
   if (access(tmp, F_OK) == 0) {
       strncpy(fpath, tmp, PATH_MAX);
       return;
   }
   ```
   Common case: assumes text files if no extension found.

3. **KEY STEP: Search for matching basename**
   ```c
   const char *base = path + 1;  // Remove leading /
   while ((de = readdir(d))) {
       char *dot = strrchr(de->d_name, '.');
       if (!dot) continue;
       size_t namelen = dot - de->d_name;  // Length before extension
       if (namelen == strlen(base)
        && strncmp(de->d_name, base, namelen) == 0) {
           // Found matching file!
           snprintf(fpath, PATH_MAX, "%s/%s", source_path, de->d_name);
           return;
       }
   }
   ```
   **This is the magic mappin!**
   - User requests: `/document`
   - Function finds: `document.pdf`
   - Maps to: `/source_dir/document.pdf`







### b. Time-Based Access for Secret Files

One day, Teja discovered his collection of embarrassing high school photos stored in a folder named "secret". He didn't want others to access these files at any time, especially when he was sleeping or away from home. "Secret files should only be accessible during working hours!" he decided firmly.

Files whose base name is **`secret`** (e.g., `secret.txt`, `secret.zip`) can only be accessed **between 08:00 (8 AM) and 18:00 (6 PM) system time**.

- **Restriction:** Outside this specified time range, any attempt to open, read, or even list the `secret` file must result in an `ENOENT` (No such file or directory) error.
- **Hint:** You will need to implement this time-based access control within your `access()` and/or `getattr()` FUSE operations.

This solution implements a FUSE filesystem that restricts access to files with a specific basename ("secret") outside a configured time window (e.g., 08:00–18:00). 

## `is_outside_time()`

```c
static int is_outside_time() {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    int h = tm->tm_hour;
    return (h < access_start || h >= access_end);
}
```

Returns `1` (true) if the current time is outside the access window (e.g., 08:00–18:00).

1. **Get system time**
   - `time_t t = time(NULL);` retrieves the current time.
2. **Convert to local time**
   - `struct tm *tm = localtime(&t);` converts to a `struct tm` for hour extraction.
3. **Extract current hour**
   - `int h = tm->tm_hour;` gets the hour (0–23).
4. **Check time range**
   - Returns `1` if the hour is outside the range defined by `access_start` and `access_end`.


## `is_secret()`

```c
static int is_secret(const char *path) {
    const char *base = path + 1;
    char tmp[256];
    snprintf(tmp, sizeof tmp, "%s", base);
    char *dot = strrchr(tmp, '.');
    if (dot) *dot = 0;
    return strcmp(tmp, secret_basename) == 0;
}
```

Returns `1` if the file’s basename (before extension) matches `"secret"` (e.g., `secret.txt`, `secret.jpeg`).

1. **Skip leading slash**
   - `const char *base = path + 1;` removes the leading `/` from the FUSE path.
2. **Copy basename**
   - `snprintf(tmp, sizeof tmp, "%s", base);` copies the path to a modifiable buffer.
3. **Remove extension**
   - `char *dot = strrchr(tmp, '.'); if (dot) *dot = 0;` strips the extension (e.g., `secret.txt` → `secret`).
4. **Compare with secret basename**
   - `strcmp(tmp, secret_basename) == 0` checks if the basename matches the configured `secret_basename`.



## `lawak_getattr()` — Deny stat on secret outside working hours

```c
static int lawak_getattr(const char *path, struct stat *stbuf,
                         struct fuse_file_info *fi) {
    if (is_secret(path) && is_outside_time())
        return -ENOENT;
    char fpath[PATH_MAX];
    build_real_path(path, fpath);
    int res = lstat(fpath, stbuf);
    return res == -1 ? -errno : 0;
}
```

Denies `stat` calls on "secret" files outside the allowed time, returning `-ENOENT` (file not found).

1. **Check secret and time**
   - If `is_secret(path)` and `is_outside_time()` are true, return `-ENOENT`.
2. **Build real path**
   - `build_real_path(path, fpath);` maps the virtual path to the real file path.
3. **Perform stat**
   - `lstat(fpath, stbuf);` retrieves file metadata.
4. **Return result**
   - Returns `0` on success or `-errno` on failure.

---

## `lawak_access()` — Deny even checking existence

```c
static int lawak_access(const char *path, int mask) {
    if (is_secret(path) && is_outside_time())
        return -ENOENT;
    char fpath[PATH_MAX];
    build_real_path(path, fpath);
    if (access(fpath, mask) == -1)
        return -errno;
    log_action("ACCESS", path);
    return 0;
}
```

Prevents checking the existence of secret files outside allowed hours, making commands like `ls secret` or `test -f secret` fail.

1. **Check secret and time**
   - If `is_secret(path)` and `is_outside_time()` are true, return `-ENOENT`.
2. **Build real path**
   - `build_real_path(path, fpath);` maps the virtual path to the real file path.
3. **Check access**
   - `access(fpath, mask);` verifies permissions.
4. **Log and return**
   - Logs the action with `log_action("ACCESS", path);` and returns `0` on success or `-errno` on failure.


## `lawak_open()` — Deny opening the file

```c
static int lawak_open(const char *path, struct fuse_file_info *fi) {
    if (is_secret(path) && is_outside_time())
        return -ENOENT;
    char fpath[PATH_MAX];
    build_real_path(path, fpath);
    int fd = open(fpath, O_RDONLY);
    if (fd < 0) return -errno;
    close(fd);
    return 0;
}
```

Prevents opening secret files outside allowed hours, returning `-ENOENT`.

1. **Check secret and time**
   - If `is_secret(path)` and `is_outside_time()` are true, return `-ENOENT`.
2. **Build real path**
   - `build_real_path(path, fpath);` maps the virtual path to the real file path.
3. **Open file**
   - `int fd = open(fpath, O_RDONLY);` attempts to open the file in read-only mode.
4. **Return result**
   - Closes the file descriptor and returns `0` on success or `-errno` on failure.



## `lawak_read()` — Deny reading content

```c
static int lawak_read(const char *path, char *buf, size_t size,
                      off_t offset, struct fuse_file_info *fi) {
    if (is_secret(path) && is_outside_time())
        return -ENOENT;
    ...
    log_action("READ", path);
    return nread;
}
```

Prevents reading the content of secret files outside allowed hours, returning `-ENOENT`.


1. **Check secret and time**
   - If `is_secret(path)` and `is_outside_time()` are true, return `-ENOENT`.
2. **Read content**
   - (Ellipsed code presumably handles reading the file content into `buf`.)
3. **Log and return**
   - Logs the action with `log_action("READ", path);` and returns the number of bytes read (`nread`).


## Configuration Support

The system relies on configuration values defined in `lawak.conf`:

```ini
# lawak.conf
FILTER_WORDS=mu,ferrari,sisop,onic,prx
SECRET_FILE_BASENAME=secret
ACCESS_START=08:00
ACCESS_END=23:00
```

### How it’s loaded

The configuration is parsed by:

```c
static void parse_config();
```

### Explanation

- **FILTER_WORDS**: A comma-separated list of words to filter (e.g., mu, ferrari, sisop, onic, prx).
- **SECRET_FILE_BASENAME**: The basename for restricted files (e.g., secret).
- **ACCESS_START**: The start time of the allowed access window in 24-hour format (e.g., 08:00 for 8:00 AM).
- **ACCESS_END**: The end time of the allowed access window in 24-hour format (e.g., 23:00 for 11:00 PM).
- The `parse_config()` function loads these values into variables like `secret_basename`, `access_start`, and `access_end` for use in the program.



### c. Dynamic Content Filtering

Teja's frustration with "lawak" things reached its peak when he read online articles full of words that annoyed him. Not only that, but the images he viewed often didn't meet his expectations either. "All content entering my system must be filtered first!" he exclaimed while clenching his fist.

When a file is opened and read, its content must be **dynamically filtered or transformed** based on its perceived type:

| File Type        | Behavior                                                                                      |
| :--------------- | :-------------------------------------------------------------------------------------------- |
| **Text Files**   | All words considered lawak (case-insensitive) must be replaced with the word `"lawak"`.       |
| **Binary Files** | The raw binary content must be presented in **Base64 encoding** instead of its original form. |

> **Note:** The list of "lawak words" for text file filtering will be defined externally, as specified in requirement **e. Configuration**.


All the process happens in:

```c
static int lawak_read(const char *path, char *buf, size_t size,
                      off_t offset, struct fuse_file_info *fi)
```

**Early check for time-based restriction**

```c
if (is_secret(path) && is_outside_time())
    return -ENOENT;
```

Ensures that secret file restrictions (Task b) are still enforced during reads.

**Open and read the real file**

```c
char fpath[PATH_MAX]; 
build_real_path(path, fpath);
FILE *fp = fopen(fpath, "rb");
if (!fp) return -errno;

if (fseeko(fp, offset, SEEK_SET) < 0) {
    fclose(fp); 
    return -errno;
}
```

Resolves the real filename and opens it for reading in binary mode. Also seeks to the correct offset for partial reads.

**Read raw content**

```c
unsigned char *raw = malloc(size);
if (!raw) { fclose(fp); return -ENOMEM; }
size_t nread = fread(raw, 1, size, fp);
fclose(fp);
```

Reads up to size bytes from file into buffer raw. This works for both text and binary files.

**Determine file type based on extension**

```c
const char *ext = strrchr(fpath, '.');
```

This checks the file extension:  
- If the file ends with .txt, it is treated as text  
- Otherwise, treated as binary  

**Text file → Replace lawak words**

```c
if (ext && !strcasecmp(ext, ".txt")) {
    char *tmp = malloc(nread * 2 + 1);
    if (!tmp) { free(raw); return -ENOMEM; }

    size_t out_len = replace_filters((char*)raw, nread, tmp, nread * 2);
    size_t to_copy = out_len > size ? size : out_len;
    memcpy(buf, tmp, to_copy);
    nread = to_copy;
    free(tmp);
}
```

- Allocates a new buffer tmp  
- Calls replace_filters() to search for lawak words and replace them  
- Copies filtered result into buf  

`replace_filters()` does this:

```c
for (int w = 0; w < filter_count; w++) {
    int L = strlen(filter_words[w]);
    if (strncasecmp(in + i, filter_words[w], L) == 0
        && (i == 0 || !isalnum(in[i-1]))
        && (i + L == len || !isalnum(in[i+L]))) {
        memcpy(out + j, "lawak", 5);
        ...
    }
}
```

Ensures case-insensitive matching, and matches only whole words  
(e.g., LawAk, lawak, LaWaKIng → will match only if it’s a whole word)

**inary file → Encode with Base64**

```c
else {
    size_t b64_len;
    char *b64 = b64_encode(raw, nread, &b64_len);
    if (!b64) { free(raw); return -ENOMEM; }

    size_t to_copy = b64_len > size ? size : b64_len;
    memcpy(buf, b64, to_copy);
    nread = to_copy;
    free(b64);
}
```

- Calls b64_encode() to convert binary data to Base64 format  
- Ensures readable format for binary data (like images or PDFs)  
- Returns encoded result  

**Clean up and return**

```c
free(raw);
log_action("READ", path);
return nread;
```

Frees raw memory  
Logs read operation to /var/log/lawakfs.log  
Returns number of bytes read into FUSE buffer

### d. Access Logging

As a paranoid individual, Teja felt the need to record every activity happening in his filesystem. "Who knows if someone tries to access my important files without permission," he muttered while setting up the logging system. He wanted every movement recorded in detail, complete with timestamps and the perpetrator's identity.

All file access operations performed within LawakFS++ must be **logged** to a file located at **`/var/log/lawakfs.log`**.

Each log entry must strictly adhere to the following format:

```
[YYYY-MM-DD HH:MM:SS] [UID] [ACTION] [PATH]
```

Where:

- **`YYYY-MM-DD HH:MM:SS`**: The timestamp of the operation.
- **`UID`**: The User ID of the user performing the action.
- **`ACTION`**: The type of FUSE operation (e.g., `READ`, `ACCESS`, `GETATTR`, `OPEN`, `READDIR`).
- **`PATH`**: The logical path to the file or directory within the FUSE mountpoint (e.g., `/secret`, `/images/photo.jpg`).

> **Requirement:** You are **only required** to log successful `read` and `access` operations. Logging of other operations (e.g., failed writes) is optional.

### e. Configuration

After using his filesystem for several weeks, Teja realized that his needs kept changing. Sometimes he wanted to add new words to the filter list, sometimes he wanted to change the secret file access hours, or even change the name of the secret file itself. "I don't want the hassle of recompiling every time I want to change settings!" he complained. Finally, he decided to create a flexible external configuration system.

To ensure flexibility, the following parameters **must not be hardcoded** within your `lawak.c` source code. Instead, they must be configurable via an external configuration file (e.g., `lawak.conf`):

- The **base filename** of the 'secret' file (e.g., `secret`).
- The **start time** for accessing the 'secret' file.
- The **end time** for accessing the 'secret' file.
- The comma-separated **list of words to be filtered** from text files.

**Example `lawak.conf` content:**

```
FILTER_WORDS=ducati,ferrari,mu,chelsea,prx,onic,sisop
SECRET_FILE_BASENAME=secret
ACCESS_START=08:00
ACCESS_END=18:00
```

Your FUSE should read and parse this configuration file upon initialization.

### Summary of Expected Behaviors

To ensure clarity, here's a consolidated table of the expected behavior for specific scenarios:

| Scenario                                                    | Expected Behavior                                                                           |
| :---------------------------------------------------------- | :------------------------------------------------------------------------------------------ |
| Accessing a file outside allowed time (e.g., `secret` file) | Return `ENOENT` (No such file or directory)                                                 |
| Reading a binary file                                       | Content must be outputted in **Base64 encoding**                                            |
| Reading a text file                                         | Filtered words must be replaced with `"lawak"`                                              |
| Listing files in any directory                              | All file extensions must be hidden                                                          |
| Attempting to write, create, or rename any file/directory   | Return `EROFS` (Read-Only File System)                                                      |
| Logging of file operations                                  | A new entry must be added to `/var/log/lawakfs.log` for each `read` and `access` operation. |

### Example Behavior

```bash
$ ls /mnt/lawak/
secret   image   readme

$ cat /mnt/lawak/secret
cat: /mnt/lawak/secret: No such file or directory
# (This output is expected if accessed outside 08:00-18:00)

$ cat /mnt/lawak/image
<base64 string of image content>

$ cat /mnt/lawak/readme
"This is a lawak filesystem."
# (Original "sisop" word was replaced with "lawak")

$ sudo tail /var/log/lawakfs.log
[2025-06-10 14:01:22] [1000] [READ] /readme
[2025-06-10 14:01:24] [1000] [ACCESS] /secret
```

