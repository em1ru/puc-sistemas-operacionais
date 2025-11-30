#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

long soma_dir(const char *path) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char fullpath[1024];
    long total = 0;
    if (!(dir = opendir(path))) return 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);
        if (stat(fullpath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                total += soma_dir(fullpath);
            } else if (S_ISREG(st.st_mode)) {
                total += st.st_size;
            }
        }
    }
    closedir(dir);
    return total;
}

int main() {
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    long total = soma_dir(cwd);
    printf("Tamanho total dos arquivos: %ld bytes\n", total);
    return 0;
}
