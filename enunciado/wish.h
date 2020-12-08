#define MAXLINE 1024

struct cmd {
    /**
     * The arguments for the entry point, usually main
     * function, of a program; must be NULL-terminated.
     */
    char *argv[MAXLINE];
    /**
     * Number of arguments to be passed 
     * to the entry point of a program.
     */
    int argc;
};

/**
 * Display the shell prompt and get the line from it as well.
 */
ssize_t prompt_and_get_input(const char *, char **, size_t *);

/** 
 * Returns a pointer to a substring of the original 
 * string trimming leading and trailing whitespaces.
 */
char *trim(char *);

/** 
 * Split str and put each substring into cmd.argv deleting 
 * all whitespaces between each substring. It put the 
 * number of substrings got from str into cmd.argc.
 */
struct cmd get_command_from(char *, char **);
