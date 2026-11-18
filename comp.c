/*
 * comp.c - compare the content of two files (Operating Systems, Assignment 1)
 * Usage: ./comp.out <file1> <file2>
 * Exit code: 2 if the files are identical, 1 if they are different.
 * Check with: echo $?
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define SIZE 1024

/* Read exactly 'size' bytes unless EOF is reached. Returns bytes read or -1. */
static int read_full(int fd, char *buf, int size)
{
    int total = 0, r;
    while (total < size) {
        r = read(fd, buf + total, size - total);
        if (r < 0)
            return -1;
        if (r == 0)
            break;
        total += r;
    }
    return total;
}

int main(int argc, char *argv[])
{
    char buf1[SIZE], buf2[SIZE];
    int fd1, fd2, r1, r2, i;
    int identical = 1;
    const char usage[] = "Usage: ./comp.out <file1> <file2>\n";

    if (argc != 3) {
        if (write(2, usage, sizeof(usage) - 1) < 0)
            perror("Error write");
        exit(-1);
    }

    fd1 = open(argv[1], O_RDONLY);
    if (fd1 < 0) {
        perror("Error opening first file");
        exit(-1);
    }

    fd2 = open(argv[2], O_RDONLY);
    if (fd2 < 0) {
        perror("Error opening second file");
        close(fd1);
        exit(-1);
    }

    while (identical) {
        r1 = read_full(fd1, buf1, SIZE);
        if (r1 < 0) {
            perror("Error reading first file");
            close(fd1);
            close(fd2);
            exit(-1);
        }
        r2 = read_full(fd2, buf2, SIZE);
        if (r2 < 0) {
            perror("Error reading second file");
            close(fd1);
            close(fd2);
            exit(-1);
        }

        if (r1 != r2) {          /* different lengths */
            identical = 0;
            break;
        }
        if (r1 == 0)             /* both files ended together */
            break;

        for (i = 0; i < r1; i++) {
            if (buf1[i] != buf2[i]) {
                identical = 0;
                break;
            }
        }
    }

    if (close(fd1) < 0) {
        perror("Error closing first file");
        close(fd2);
        exit(-1);
    }
    if (close(fd2) < 0) {
        perror("Error closing second file");
        exit(-1);
    }

    return identical ? 2 : 1;
}
