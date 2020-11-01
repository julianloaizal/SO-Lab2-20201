#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>

int
main (int argc, char *argv[])
{
  struct timeval start, end;
  int fd[2];

  if (argc == 1)
    {
      printf ("usage: time -C FILE [OPTION]\n");
      exit (1);
    }
  if (pipe (fd) < 0)
    {
      fprintf (stderr, "time: pipe(fd) failed\n");
      exit (1);
    }
  switch (fork ())
    {
    case -1:
      fprintf (stderr, "time: fork() failed\n");
      exit (1);
    case 0:
      close (fd[0]);
      /*
       * All statements after execlp call are ignored if it's 
       * executed successfully, that's why it's needed getting 
       * the time two instrucctions before execlp is called.
       */
      gettimeofday (&start, NULL);
      write (fd[1], &start, sizeof (struct timeval));
      execlp (argv[1], ".", argv[2], NULL);
      fprintf (stderr, "time: execlp(\"%s\", \".\", \"%s\") failed\n",
	       argv[1], argv[2]);
      exit (1);
    default:
      wait (NULL);
      gettimeofday (&end, NULL);
      close (fd[1]);
      if ((read (fd[0], &start, sizeof (struct timeval))) == -1)
        {
          fprintf (stderr, "time: can't read from child pipe");
          exit (1);
        } 
      printf ("\nElapsed time: %ld µs\n", end.tv_usec - start.tv_usec);
      close (fd[0]);
      exit (0);
    }
}
