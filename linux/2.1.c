#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <pwd.h>
#include <sys/types.h>
#include <ctype.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <username>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct passwd *pwd = getpwnam(argv[1]);
    if (pwd == NULL) {
        fprintf(stderr, "User '%s' not found.\n", argv[1]);
        exit(EXIT_FAILURE);
    }
    uid_t target_uid = pwd->pw_uid;

    DIR *proc_dir = opendir("/proc");
    if (proc_dir == NULL) {
        perror("opendir /proc");
        exit(EXIT_FAILURE);
    }

    struct dirent *entry;
    while ((entry = readdir(proc_dir)) != NULL) {

        if (!isdigit(entry->d_name[0]))
            continue;

        char status_path[256];
        snprintf(status_path, sizeof(status_path), "/proc/%s/status", entry->d_name);

        FILE *fp = fopen(status_path, "r");
        if (fp == NULL) {
            continue; 
        }

        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "Uid:", 4) == 0) {
                uid_t uid;
                sscanf(line + 4, "%u", &uid);
                    printf("PID: %s\n", entry->d_name);
                }
                break;
            }
        }
        fclose(fp);
    }

    closedir(proc_dir);
    return 0;
}