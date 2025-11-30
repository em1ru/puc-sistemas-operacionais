#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

void mostra_dir(const char *path, int nivel) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char fullpath[1024];
    dir = opendir(path);
    if (!dir) {
        return;
    }
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0) {
            continue;
        }
        if (strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);
        if (stat(fullpath, &st) == 0) {
            printf("%*s[%s]%s\n", nivel*2, "", entry->d_name, S_ISDIR(st.st_mode) ? " (dir)" : "");
            if (S_ISDIR(st.st_mode)) {
                mostra_dir(fullpath, nivel+1);
            }
        }
    }
    closedir(dir);
}

int main() {
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    mostra_dir(cwd, 0);
    return 0;
}
