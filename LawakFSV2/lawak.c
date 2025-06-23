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
