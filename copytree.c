#include "copytree.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>

#define BUFFER_SIZE 4096

// function to copy a file from a source path to a destination path.
void copy_file(const char *src, const char *dest, int copy_symlinks, int copy_permissions) {
    struct stat status;
    // copy symbolic links as symbolic links rather than copying the content they point to.
    if (copy_symlinks) {
        // status hold information about the soft link.
        if (lstat(src, &status) == -1) {
            perror("lstat failed");
            return;
        }
        if (S_ISLNK(status.st_mode)) {
            char path_for_symlink[PATH_MAX]; 
            // access and readf the content of the soft link.
            if (readlink(src, path_for_symlink, sizeof(path_for_symlink) - 1) == -1) {
                perror("readlink failed");
                return;
            }
            // create the soft link at the destination.
            if (symlink(path_for_symlink, dest) == -1) {
                perror("symlink failed");
                return;
            }
            return;
        }
    }

    int src_fd, dest_fd;
    ssize_t bytes_read, bytes_written;
    char buffer[BUFFER_SIZE];

    // status hold the information of the file.
    if (stat(src, &status) == -1) {
        perror("stat failed");
        return;
    }

    // open the source file for reading.
    src_fd = open(src, O_RDONLY);
    if (src_fd == -1) {
        perror("open failed");
        return;
    }

    // open the destination file for writing.
    dest_fd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, status.st_mode & 0777);
    if (dest_fd == -1) {
        perror("open failed");
        close(src_fd);
        return;
    }

    // copy data from source to destination.
    bytes_read = read(src_fd, buffer, sizeof(buffer));
    while ( bytes_read > 0) {
        bytes_written = write(dest_fd, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            perror("write failed");
            close(src_fd);
            close(dest_fd);
            return;
        }
        bytes_read = read(src_fd, buffer, sizeof(buffer));
    }

    if (bytes_read == -1) {
        perror("read failed");
        close(src_fd);
        close(dest_fd);
        return;
    }

    // close file descriptors after copying is done.
    close(src_fd);
    close(dest_fd);

    // copy the file permissions from the source file to the destination file.
    if (copy_permissions) {
        if (chmod(dest, status.st_mode & 0777) == -1) {
            perror("chmod failed");
            return;
        }
    }
}

// function to recursively copy a directory tree from a source path to a destination path.
void copy_directory(const char *src, const char *dest, int copy_symlinks, int copy_permissions) {
    // open the source directory.
    DIR *dir = opendir(src);
    if (dir == NULL) {
        perror("opendir failed");
        return;
    }
    // create the destination directory with default permissions.
    if (mkdir(dest, 0755) == -1 && errno != EEXIST) {
        perror("mkdir failed");
        closedir(dir);
        return;
    }

    struct dirent *subtree;
    // copytree of directory recursively.
    while ((subtree = readdir(dir)) != NULL) {
        // avoid processing current directory and parent directory.
        if (strcmp(subtree->d_name, ".") == 0 || strcmp(subtree->d_name, "..") == 0) {
            continue;
        }
        
        // construct the full source and destination paths.
        char src_path[PATH_MAX];
        char dest_path[PATH_MAX];

        snprintf(src_path, sizeof(src_path), "%s/%s", src, subtree->d_name);
        snprintf(dest_path, sizeof(dest_path), "%s/%s", dest, subtree->d_name);

        struct stat subtree_stat;
        if (lstat(src_path, &subtree_stat) == -1) {
            perror("lstat failed");
            continue;
        }

        // check if it is a directory.
        if (S_ISDIR(subtree_stat.st_mode)) {
            // recursively copy directory.
            if (mkdir(dest_path, 0755) == -1 && errno != EEXIST) {
                perror("mkdir failed");
                continue;
            }
            // apply the correct directory permissions after creation.
            if (copy_permissions) {
                if (chmod(dest_path, subtree_stat.st_mode & 0777) == -1) {
                    perror("chmod failed");
                    continue;
                }
            }
            copy_directory(src_path, dest_path, copy_symlinks, copy_permissions);
        } else if (S_ISREG(subtree_stat.st_mode) || S_ISLNK(subtree_stat.st_mode)) {
            // use the copy_file function to copy individual files and symlinks.
            copy_file(src_path, dest_path, copy_symlinks, copy_permissions);
        }
        // apply the correct directory permissions after copying all contents
        if (copy_permissions) {
            struct stat dir_stat;
            if (stat(src, &dir_stat) == 0) {
                if (chmod(dest, dir_stat.st_mode & 0777) == -1) { 
                    perror("chmod failed");
                }
            }
        }
    }
    closedir(dir);
    // apply the correct directory permissions after copying all contents
    if (copy_permissions) {
        struct stat dir_stat;
        if (stat(src, &dir_stat) == 0) {
            if (chmod(dest, dir_stat.st_mode & 0777) == -1) { 
                perror("chmod failed");
            }
        }
    }
}
