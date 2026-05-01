/**
 * Chrono-shift 跨平台兼容层 — 目录迭代
 * 包含: dir_open / dir_next / dir_close
 */
#include "platform_compat.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef PLATFORM_WINDOWS

int dir_open(DirIterator* it, const char* path)
{
    if (!it || !path) return -1;
    memset(it, 0, sizeof(DirIterator));
    snprintf(it->search_path, sizeof(it->search_path), "%s/*", path);
    it->first_call = true;
    it->valid = false;
    return 0;
}

int dir_next(DirIterator* it, char* name, size_t max_len)
{
    if (!it || !name || max_len == 0) return -1;

    if (it->first_call) {
        it->first_call = false;
        it->hFind = FindFirstFileA(it->search_path, &it->find_data);
        if (it->hFind == INVALID_HANDLE_VALUE) {
            return -1;
        }
        it->valid = true;
    } else {
        if (!it->valid) return -1;
        if (!FindNextFileA(it->hFind, &it->find_data)) {
            it->valid = false;
            return -1;
        }
    }

    /* 跳过 "." 和 ".." */
    if (strcmp(it->find_data.cFileName, ".") == 0 ||
        strcmp(it->find_data.cFileName, "..") == 0) {
        return dir_next(it, name, max_len);
    }

    strncpy(name, it->find_data.cFileName, max_len - 1);
    name[max_len - 1] = '\0';
    return 0;
}

void dir_close(DirIterator* it)
{
    if (it && it->valid) {
        FindClose(it->hFind);
        it->valid = false;
    }
}

#else /* PLATFORM_LINUX */

#include <dirent.h>

int dir_open(DirIterator* it, const char* path)
{
    if (!it || !path) return -1;
    it->dp = opendir(path);
    return it->dp ? 0 : -1;
}

int dir_next(DirIterator* it, char* name, size_t max_len)
{
    if (!it || !it->dp || !name || max_len == 0) return -1;

    struct dirent* entry;
    while ((entry = readdir(it->dp)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        strncpy(name, entry->d_name, max_len - 1);
        name[max_len - 1] = '\0';
        return 0;
    }
    return -1;
}

void dir_close(DirIterator* it)
{
    if (it && it->dp) {
        closedir(it->dp);
        it->dp = NULL;
    }
}

#endif
