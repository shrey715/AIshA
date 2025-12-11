#include "colors.h"
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

void print_colored(const char* color, const char* text) {
    if (COLORS_SUPPORTED) {
        printf("%s%s%s", color, text, COLOR_RESET);
    } else {
        printf("%s", text);
    }
}

void print_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    if (COLORS_SUPPORTED) {
        fprintf(stderr, "%s", COLOR_ERROR);
    }
    vfprintf(stderr, format, args);
    if (COLORS_SUPPORTED) {
        fprintf(stderr, "%s", COLOR_RESET);
    }
    
    va_end(args);
}

void print_warning(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    if (COLORS_SUPPORTED) {
        fprintf(stderr, "%s", COLOR_WARNING);
    }
    vfprintf(stderr, format, args);
    if (COLORS_SUPPORTED) {
        fprintf(stderr, "%s", COLOR_RESET);
    }
    
    va_end(args);
}

void print_success(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    if (COLORS_SUPPORTED) {
        printf("%s", COLOR_SUCCESS);
    }
    vprintf(format, args);
    if (COLORS_SUPPORTED) {
        printf("%s", COLOR_RESET);
    }
    
    va_end(args);
}

void print_info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    if (COLORS_SUPPORTED) {
        printf("%s", COLOR_INFO);
    }
    vprintf(format, args);
    if (COLORS_SUPPORTED) {
        printf("%s", COLOR_RESET);
    }
    
    va_end(args);
}

const char* get_file_color(unsigned int mode, const char* filename) {
    /* Check file type from mode bits */
    if (S_ISDIR(mode)) {
        return COLOR_DIR;
    }
    if (S_ISLNK(mode)) {
        return COLOR_LINK;
    }
    if (S_ISSOCK(mode)) {
        return COLOR_SOCKET;
    }
    if (S_ISFIFO(mode)) {
        return COLOR_PIPE;
    }
    if (S_ISBLK(mode)) {
        return COLOR_BLOCK;
    }
    if (S_ISCHR(mode)) {
        return COLOR_CHAR;
    }
    
    /* Check special permission bits */
    if (mode & S_ISUID) {
        return COLOR_SETUID;
    }
    if (mode & S_ISGID) {
        return COLOR_SETGID;
    }
    if (S_ISDIR(mode) && (mode & S_ISVTX)) {
        return COLOR_STICKY;
    }
    
    /* Check if executable */
    if (mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
        return COLOR_EXEC;
    }
    
    /* Check extension for regular files */
    return get_extension_color(filename);
}

const char* get_extension_color(const char* filename) {
    if (!filename) return COLOR_RESET;
    
    const char* ext = strrchr(filename, '.');
    if (!ext || ext == filename) return COLOR_RESET;
    ext++; /* Skip the dot */
    
    /* Archive files */
    if (strcasecmp(ext, "tar") == 0 || strcasecmp(ext, "gz") == 0 ||
        strcasecmp(ext, "zip") == 0 || strcasecmp(ext, "bz2") == 0 ||
        strcasecmp(ext, "xz") == 0 || strcasecmp(ext, "7z") == 0 ||
        strcasecmp(ext, "rar") == 0 || strcasecmp(ext, "tgz") == 0 ||
        strcasecmp(ext, "deb") == 0 || strcasecmp(ext, "rpm") == 0) {
        return COLOR_ARCHIVE;
    }
    
    /* Image files */
    if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0 ||
        strcasecmp(ext, "png") == 0 || strcasecmp(ext, "gif") == 0 ||
        strcasecmp(ext, "bmp") == 0 || strcasecmp(ext, "svg") == 0 ||
        strcasecmp(ext, "ico") == 0 || strcasecmp(ext, "webp") == 0) {
        return COLOR_IMAGE;
    }
    
    /* Audio files */
    if (strcasecmp(ext, "mp3") == 0 || strcasecmp(ext, "wav") == 0 ||
        strcasecmp(ext, "flac") == 0 || strcasecmp(ext, "ogg") == 0 ||
        strcasecmp(ext, "m4a") == 0 || strcasecmp(ext, "aac") == 0) {
        return COLOR_AUDIO;
    }
    
    /* Video files */
    if (strcasecmp(ext, "mp4") == 0 || strcasecmp(ext, "mkv") == 0 ||
        strcasecmp(ext, "avi") == 0 || strcasecmp(ext, "mov") == 0 ||
        strcasecmp(ext, "wmv") == 0 || strcasecmp(ext, "webm") == 0) {
        return COLOR_VIDEO;
    }
    
    /* Source code - make it easier to spot */
    if (strcasecmp(ext, "c") == 0 || strcasecmp(ext, "h") == 0 ||
        strcasecmp(ext, "cpp") == 0 || strcasecmp(ext, "hpp") == 0 ||
        strcasecmp(ext, "py") == 0 || strcasecmp(ext, "js") == 0 ||
        strcasecmp(ext, "ts") == 0 || strcasecmp(ext, "rs") == 0 ||
        strcasecmp(ext, "go") == 0 || strcasecmp(ext, "java") == 0) {
        return COLOR_GREEN;
    }
    
    /* Config/data files */
    if (strcasecmp(ext, "json") == 0 || strcasecmp(ext, "yaml") == 0 ||
        strcasecmp(ext, "yml") == 0 || strcasecmp(ext, "xml") == 0 ||
        strcasecmp(ext, "toml") == 0 || strcasecmp(ext, "ini") == 0 ||
        strcasecmp(ext, "conf") == 0 || strcasecmp(ext, "cfg") == 0) {
        return COLOR_YELLOW;
    }
    
    /* Markdown/docs */
    if (strcasecmp(ext, "md") == 0 || strcasecmp(ext, "txt") == 0 ||
        strcasecmp(ext, "rst") == 0 || strcasecmp(ext, "doc") == 0 ||
        strcasecmp(ext, "pdf") == 0) {
        return COLOR_DOC;
    }
    
    return COLOR_RESET;
}
