#include <android/log.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <unistd.h>

/* Prepare only SystemUI's native per-user custom icon; never replace its contents. */
int main(void) {
    /* packages.list is PackageManager's authoritative UID map; no app data is read. */
    FILE *packages = fopen("/data/system/packages.list", "re");
    if (!packages) { perror("package UID map"); return 1; }
    char *line = NULL;
    size_t capacity = 0;
    unsigned long appid = 0;
    while (getline(&line, &capacity, packages) >= 0) {
        char package[256];
        unsigned long uid;
        if (sscanf(line, "%255s %lu", package, &uid) == 2 &&
            !strcmp(package, "com.android.systemui") && uid >= 10000 && uid <= 19999) {
            appid = uid;
            break;
        }
    }
    free(line);
    fclose(packages);
    if (!appid) { fprintf(stderr, "SystemUI UID not available yet\n"); return 1; }

    int base = open("/data/oplus/os", O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if (base < 0) { perror("icon directory"); return 1; }
    if (fchmod(base, 0771)) { perror("icon directory mode"); close(base); return 1; }
    DIR *users = opendir("/data/user_de");
    if (!users) { perror("user directory"); close(base); return 1; }
    int failures = 0, prepared = 0;
    struct dirent *entry;
    while ((entry = readdir(users))) {
        const char *name = entry->d_name;
        if (!*name || strspn(name, "0123456789") != strlen(name)) continue;
        errno = 0;
        char *end;
        unsigned long user = strtoul(name, &end, 10);
        if (errno || *end || user > 21473) continue;
        struct stat userdir;
        if (fstatat(dirfd(users), name, &userdir, AT_SYMLINK_NOFOLLOW) ||
            !S_ISDIR(userdir.st_mode)) { failures++; continue; }
        uid_t icon_uid = (uid_t)(user * 100000 + appid);
        char filename[64];
        snprintf(filename, sizeof(filename), "%lucustom_icon.png", user);
        int fd = openat(base, filename, O_RDONLY|O_CREAT|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC, 0640);
        if (fd < 0) { perror(filename); failures++; continue; }
        struct stat st;
        if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_nlink != 1) {
            fprintf(stderr, "unsafe icon inode for user %lu\n", user);
            close(fd); failures++; continue;
        }
        const char context[] = "u:object_r:os_data_file:s0";
        if (fchown(fd, icon_uid, 1000) ||
            fchmod(fd, (st.st_mode & 0004) ? 0644 : 0640) ||
            fsetxattr(fd, "security.selinux", context, sizeof(context), 0)) {
            perror(filename); failures++;
        } else { prepared++; }
        close(fd);
    }
    closedir(users);
    close(base);
    __android_log_print(failures ? ANDROID_LOG_ERROR : ANDROID_LOG_INFO,
                        "RodinFodIcon", "prepared=%d failures=%d", prepared, failures);
    printf("custom fingerprint icon files prepared=%d failures=%d\n", prepared, failures);
    return failures ? 1 : 0;
}
