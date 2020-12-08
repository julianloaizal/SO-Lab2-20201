#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "wish.h"

char *out;

char *paths[25] = {
    "/bin", NULL
};

char error_message[30] = "An error has occurred\n";

/**
 * Print shell prompt *prompt, get the command line from
 * stdin and put it into **line with at most n bytes. 
 * If n is zero and line is NULL, prompt_and_get_input
 * will make malloc.
 */
ssize_t 
prompt_and_get_input(const char *prompt, char **line, size_t *n)
{
    printf("%s", prompt);
    return getline(line, n, stdin);
}

int 
search_executable(char *executable, char *path) 
{
    for (int i = 0; paths[i] != NULL; i++) {
        sprintf(path, "%s/%s", paths[i], executable);
        if (access(path, X_OK) == 0)
            return 0;
    }
    write(STDERR_FILENO, error_message, strlen(error_message));
    return -1;
}

void normal_execute(int argc, char *argv[], char *out) 
{
    char path[25];
    int fd;

    if (search_executable(argv[0], path) != -1) {
        switch (fork()) {
            case -1:
                write(STDERR_FILENO, error_message, 
                        strlen(error_message));
                break;
            case 0:
                if ((fd = open(out, O_CREAT | O_WRONLY | O_TRUNC, 0600)) != -1) {
                    close(1);
                    dup2(1, 2);
                    dup(fd);
                    close(fd);
                }
                execv(path, argv);
                exit(1);
            default:
                wait(NULL);
                break;
        }
    }
}

void 
wish_cd(int argc, char *argv[]) 
{
    if (argc != 2) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        return;
    }
    chdir(argv[1]);
}

void 
wish_exit(int argc, char *argv[])
{
    if (argc != 1) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        return;
    }
    exit(0);
}

void 
wish_path(int argc, char *argv[])
{
    int i;
    paths[0] = NULL;
    for (i = 0; i < (argc - 1); i++)
        paths[i] = strdup(argv[i + 1]);
    paths[++i] = NULL;
}

void 
builtin_execute(int op, int argc, char *argv[]) 
{
    switch (op) {
        case 1:
            wish_cd(argc, argv);
            break;
        case 2:
            wish_path(argc, argv);
            break;
        case 3:
            wish_exit(argc, argv);
            break;
    }
}

int 
is_builtin_command(char *exec)
{
    if (strcmp(exec, "cd") == 0) {
        return 1;
    }
    if (strcmp(exec, "path") == 0) {
        return 2;
    }
    if (strcmp(exec, "exit") == 0) {
        return 3;
    }
    return -1;
}

/** 
 * Split str and put each substring into cmd.argv deleting 
 * all whitespaces between each substring. It put the 
 * number of substrings got from str into cmd.argc.
 */
struct 
cmd get_command_from(char *str, char **r)
{
    struct cmd cmd;
    char *arg, *s, *t, *m;
    int n = 0;

    if (strstr(str, ">") != NULL) {

        while ((t = strsep(&str,">")) != NULL) {
            if (++n == 1)
                s = strdup(t);
            else
                m = strdup(t);
        }

        if (n > 2 || (strcmp(s, "") == 0)) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            cmd.argc = 0;
            return cmd;
        }

        t = NULL;
        n = 0;
        while ((t = strsep(&m, " ")) != NULL) {
            if (*t != '\0' && n < 2) {
                *r = trim(t);
                ++n;
            }
            if (strcmp(*r, "") == 0)
                n = 0;
        }

        if (n != 1) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            cmd.argc = 0;
            return cmd;
        }
        n = 0;
        while ((arg = strsep(&s, " ")))
            if (*arg != '\0')
                cmd.argv[n++] = trim(arg);

        cmd.argv[n] = NULL;
        cmd.argc = n;
        for (int i = 1; cmd.argv[i] != NULL; i++) {
            if (*cmd.argv[i] == '\0') {
                cmd.argv[i] = NULL;
                cmd.argc--;
            }
        }
        return cmd;
    }

    n = 0;
    while ((arg = strsep(&str, " ")))
        if (*arg != '\0')
            cmd.argv[n++] = trim(arg);
    
    cmd.argv[n] = NULL;
    cmd.argc = n;

    for (int i = 1; cmd.argv[i] != NULL; i++) {
        if (*cmd.argv[i] == '\0') {
            cmd.argv[i] = NULL;
            cmd.argc--;
        }
    }
    return cmd;
}

void 
parallel_execute(char *commands) 
{
    int pids[15], pid, n, fd = 1;
    char *c, path[25], *t;
    int red;

    fd = 1;
    n = 0;
    while ((c = strsep(&commands, "&")) != NULL) {
        if (*c != '\0') {
            t = strdup(c);
            red = (strstr(t, ">") != NULL) ? 1 : 0;
            struct cmd cmd = get_command_from(c, &out);
            if (strcmp(cmd.argv[0], "") != 0) {
                if ((pid = fork()) < 0) {
                    fprintf(stderr, "error\n");
                } else if (pid == 0) {
                    if (search_executable(cmd.argv[0], path) != -1) {
                        if  (red == 1 
                                && (fd = open(out, O_CREAT | O_WRONLY | O_TRUNC, 0600)) != -1) {
                            close(1);
                            dup(fd);
                            close(fd);
                        }
                        execv(path, cmd.argv);
                    }
                    exit(1);
                } else {
                    pids[n++] = pid;
                }
            }
        }
        if (red == 1) {
            dup(1);
        }
    }
    for (size_t i = 0; i < n; i++) {
        waitpid(pids[i], NULL, 0);
    }
}

/** 
 * Returns a pointer to a substring of the original 
 * string trimming leading and trailing whitespaces.
 */
char 
*trim(char *str)
{
    char *end;
    /** 
     * It's needed to cast the argument for isspace to unsigned 
     * char, otherwise it may invoke undefined behavior.
     */
    while (isspace((unsigned char)*str))
        str++;

    if (*str == '\0')
        return str;

    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;
    end[1] = '\0';
    return str;
}

int 
main(int argc, char *argv[]) 
{
    char *command = NULL;
    size_t len = 0;
    FILE *fp;
    int b;

    switch (argc) {
        case 1:
            while (prompt_and_get_input("wish> ", &command, &len) != EOF) {
                out = (char *) calloc(25, sizeof(char));
                char *tmp = strdup(command);
                if (strstr(tmp, "&") != NULL) {
                    parallel_execute(tmp);
                } else {
                    struct cmd cmd = get_command_from(command, &out);
                    if (cmd.argc == 0 || strcmp(cmd.argv[0], "\n") == 0) {
                        continue;
                    }
                    if ((b = is_builtin_command(cmd.argv[0])) > 0) {
                        builtin_execute(b, cmd.argc, cmd.argv);
                    } else {        
                        normal_execute(cmd.argc, cmd.argv, out);
                    }
                }
            }
            exit(0);
        case 2:
            if ((fp = fopen(*++argv, "r")) == NULL) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                exit(1);
            }
            while (getline(&command, &len, fp) != EOF) {
                out = (char *) calloc(25, sizeof(char));
                char *tmp = strdup(command);
                if (strstr(tmp, "&") != NULL) {
                    parallel_execute(tmp);
                } else {
                    struct cmd cmd = get_command_from(command, &out);
                    if (cmd.argc == 0 || strcmp(cmd.argv[0], "\n") == 0) {
                        continue;
                    }
                    if ((b = is_builtin_command(cmd.argv[0])) > 0) {
                        builtin_execute(b, cmd.argc, cmd.argv);
                    } else {        
                        normal_execute(cmd.argc, cmd.argv, out);
                    }
                }
            }
            fclose(fp);
            exit(0);
        default:
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
    }
    return 0;
}
