#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <sys/param.h>

#define FALSE 0
#define TRUE 1

int file_select(struct direct *entry) {
    if ((strcmp(entry->d_name, ".") == 0) || (strcmp(entry->d_name, "..") == 0))
        return FALSE;
    else
        return TRUE;
}

int main() {
    char pathname[MAXPATHLEN];
    struct direct **files;
    int count, i;
    if (getwd(pathname) == NULL) {
        printf("Error getting path\n");
        exit(1);
    }
    printf("Current Working Directory = %s\n", pathname);
    count = scandir(pathname, &files, file_select, alphasort);
    if (count <= 0) {
        printf("No files in this directory\n");
        exit(0);
    }
    printf("Number of files = %d\n", count);
    time_t now = time(NULL);
    for (i = 0; i < count; ++i) {
        struct stat st;
        char fullpath[MAXPATHLEN];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", pathname, files[i]->d_name);
        if (stat(fullpath, &st) == 0) {
            int age = (int)((now - st.st_mtime) / (60*60*24));
            printf("%s inode %ld size: %ld age: %d days\n", files[i]->d_name, (long)st.st_ino, (long)st.st_size, age);
        }
        free(files[i]);
    }
    free(files);
    return 0;
}
