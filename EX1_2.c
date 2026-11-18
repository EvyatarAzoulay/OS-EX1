/*
 * EX1_2.c - Automatic homework checker (Operating Systems, Assignment 1)
 * Usage: ./a.out <path_to_config.txt>
 * Uses system calls only. No strcmp/strncmp/strlen (own versions are written below).
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>

#define MAX_PATH 512
#define BUF_SIZE 1024

/* Global resources so that every error path can release them */
static DIR *g_dir = NULL;
static int g_results_fd = -1;

/* Our own strlen */
static int my_strlen(const char *s)
{
    int n = 0;
    while (s[n] != '\0')
        n++;
    return n;
}

/* Our own strcmp: 0 if equal */
static int my_strcmp(const char *a, const char *b)
{
    int i = 0;
    while (a[i] != '\0' && a[i] == b[i])
        i++;
    return (unsigned char)a[i] - (unsigned char)b[i];
}

/* Release all open resources, print the error and exit */
static void fail(const char *msg)
{
    perror(msg);
    if (g_results_fd >= 0)
        close(g_results_fd);
    if (g_dir != NULL)
        closedir(g_dir);
    exit(-1);
}

/* write() the whole string, with error checking */
static void write_str(int fd, const char *s)
{
    int len = my_strlen(s);
    if (write(fd, s, len) != len)
        fail("Error writing");
}

/* dest = a + b + c  (any part may be NULL), with overflow protection */
static void build_path(char *dest, const char *a, const char *b, const char *c)
{
    const char *parts[3];
    int i = 0, p, j;
    parts[0] = a;
    parts[1] = b;
    parts[2] = c;
    for (p = 0; p < 3; p++) {
        if (parts[p] == NULL)
            continue;
        for (j = 0; parts[p][j] != '\0'; j++) {
            if (i >= MAX_PATH - 1) {
                write_str(2, "Error: path too long\n");
                if (g_results_fd >= 0) close(g_results_fd);
                if (g_dir != NULL) closedir(g_dir);
                exit(-1);
            }
            dest[i++] = parts[p][j];
        }
    }
    dest[i] = '\0';
}

/* Wait for a child process, with error checking. Returns the child's status. */
static int wait_child(void)
{
    int status;
    if (wait(&status) < 0)
        fail("Error wait");
    return status;
}

/* Read config.txt into three paths (one per line) */
static void read_config(const char *config_path, char *students_dir,
                        char *test_input, char *expected_output)
{
    char buf[BUF_SIZE];
    char *targets[3];
    int fd, n, i, line = 0, idx = 0;

    targets[0] = students_dir;
    targets[1] = test_input;
    targets[2] = expected_output;

    fd = open(config_path, O_RDONLY);
    if (fd < 0)
        fail("Error opening config file");

    n = read(fd, buf, BUF_SIZE - 1);
    if (n < 0) {
        close(fd);
        fail("Error reading config file");
    }
    buf[n] = '\0';

    if (close(fd) < 0)
        fail("Error closing config file");

    for (i = 0; i <= n && line < 3; i++) {
        char ch = buf[i];
        /* end of line, or end of file (handles a last line without '\n') */
        if (ch == '\n' || ch == '\r' || ch == '\0') {
            if (idx > 0) {
                targets[line][idx] = '\0';
                line++;
                idx = 0;
            }
        } else if (ch != ' ' && ch != '\t') {
            if (idx >= MAX_PATH - 1) {
                write_str(2, "Error: path in config file is too long\n");
                exit(-1);
            }
            targets[line][idx++] = ch;
        }
    }

    if (line < 3) {
        write_str(2, "Error: config file must contain 3 lines\n");
        exit(-1);
    }
}

int main(int argc, char *argv[])
{
    char students_dir[MAX_PATH], test_input[MAX_PATH], expected_output[MAX_PATH];
    char student_path[MAX_PATH], c_file[MAX_PATH], exe_file[MAX_PATH];
    char buf[BUF_SIZE];
    struct dirent *entry;
    struct stat st;
    pid_t pid;
    int status, n, fd;

    if (argc != 2) {
        write_str(2, "Usage: ./a.out <config_file>\n");
        exit(-1);
    }

    /* 1. Read the configuration file */
    read_config(argv[1], students_dir, test_input, expected_output);

    /* 2. Open the students directory */
    g_dir = opendir(students_dir);
    if (g_dir == NULL)
        fail("Error opening students directory");

    /* 3. Create / truncate results.csv */
    g_results_fd = open("results.csv", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (g_results_fd < 0)
        fail("Error opening results.csv");

    /* 4. Loop over all students */
    while ((entry = readdir(g_dir)) != NULL) {
        if (my_strcmp(entry->d_name, ".") == 0 || my_strcmp(entry->d_name, "..") == 0)
            continue;

        build_path(student_path, students_dir, "/", entry->d_name);
        if (stat(student_path, &st) < 0)
            fail("Error stat student folder");
        if (!S_ISDIR(st.st_mode))
            continue; /* not a student folder */

        /* e.g. .../students/Michael/Michael.c  and  .../students/Michael/main.out */
        build_path(c_file, student_path, "/", entry->d_name);
        build_path(c_file, c_file, ".c", NULL);
        build_path(exe_file, student_path, "/main.out", NULL);

        if (access(c_file, F_OK) < 0)
            fail("Error: student source file is missing");

        /* 4a. Compile: gcc Name.c -o main.out */
        pid = fork();
        if (pid < 0)
            fail("Error fork (gcc)");
        if (pid == 0) {
            execl("/usr/bin/gcc", "gcc", c_file, "-o", exe_file, (char *)NULL);
            perror("Error execl gcc");
            exit(-1);
        }
        status = wait_child();
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            write_str(2, "Error: compilation failed\n");
            close(g_results_fd);
            closedir(g_dir);
            exit(-1);
        }

        /* 4b. Run main.out: keyboard <- test_input.qa, screen -> program_output.txt */
        pid = fork();
        if (pid < 0)
            fail("Error fork (main.out)");
        if (pid == 0) {
            int in_fd, out_fd;
            in_fd = open(test_input, O_RDONLY);
            if (in_fd < 0) {
                perror("Error opening test_input file");
                exit(-1);
            }
            out_fd = open("program_output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (out_fd < 0) {
                perror("Error opening program_output.txt");
                close(in_fd);
                exit(-1);
            }
            if (dup2(in_fd, 0) < 0 || dup2(out_fd, 1) < 0) {
                perror("Error dup2");
                close(in_fd);
                close(out_fd);
                exit(-1);
            }
            close(in_fd);
            close(out_fd);
            execl(exe_file, "main.out", (char *)NULL);
            perror("Error execl main.out");
            exit(-1);
        }
        wait_child();

        /* 4c. Compare with comp.out; its printout goes to results.csv */
        build_path(buf, entry->d_name, ",", NULL);
        write_str(g_results_fd, buf);

        pid = fork();
        if (pid < 0)
            fail("Error fork (comp.out)");
        if (pid == 0) {
            if (dup2(g_results_fd, 1) < 0) {
                perror("Error dup2");
                exit(-1);
            }
            execl("./comp.out", "comp.out", expected_output, "program_output.txt", (char *)NULL);
            perror("Error execl comp.out");
            exit(-1);
        }
        status = wait_child();

        /* comp.out exit code: 2 = identical, 1 = different */
        if (WIFEXITED(status) && WEXITSTATUS(status) == 2)
            write_str(g_results_fd, "2\n");
        else
            write_str(g_results_fd, "1\n");
    }

    if (closedir(g_dir) < 0) {
        g_dir = NULL;
        fail("Error closing students directory");
    }
    g_dir = NULL;

    if (close(g_results_fd) < 0) {
        g_results_fd = -1;
        fail("Error closing results.csv");
    }
    g_results_fd = -1;

    /* 5. Print results.csv to the screen */
    fd = open("results.csv", O_RDONLY);
    if (fd < 0)
        fail("Error opening results.csv for reading");
    while ((n = read(fd, buf, BUF_SIZE)) > 0) {
        if (write(1, buf, n) != n) {
            close(fd);
            fail("Error writing to screen");
        }
    }
    if (n < 0) {
        close(fd);
        fail("Error reading results.csv");
    }
    if (close(fd) < 0)
        fail("Error closing results.csv");

    return 0;
}
