/* 
 * tsh - A tiny shell program with job control
 * 
 * <Put your name and login ID here>
 * name: chj
 * contact: 506933131@qq.com
 * 
 * Challenge 1: test05, in sigchld_handler
 * Challenge 2: process group
 * Challenge 3: test16, ./mystop 2 and ./myint 2, read book about SIGCHLD and SIGTSTP&SIGINT
 * Challenge 4: relation between SIGINT and SIGCHLD in test07 after finishing test16
 *              reason -- execve doesn't inherit self-registerd signal handler 
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <limits.h>

/* Misc manifest constants */
#define MAXLINE 1024   /* max line size */
#define MAXARGS 128    /* max args on a command line */
#define MAXJOBS 16     /* max jobs at any point in time */
#define MAXJID 1 << 16 /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;   /* defined in libc */
char prompt[] = "tsh> "; /* command line prompt (DO NOT CHANGE) */
int verbose = 0;         /* if true, print additional output */
int nextjid = 1;         /* next job ID to allocate */
char sbuf[MAXLINE];      /* for composing sprintf messages */

struct job_t
{                          /* The job struct */
    pid_t pid;             /* job PID */
    int jid;               /* job ID [1, 2, ...] */
    int state;             /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE]; /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */

/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv);
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs);
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid);
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid);
int pid2jid(pid_t pid);
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv)
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF)
    {
        switch (c)
        {
        case 'h': /* print help message */
            usage();
            break;
        case 'v': /* emit additional diagnostic info */
            verbose = 1;
            break;
        case 'p':            /* don't print a prompt */
            emit_prompt = 0; /* handy for automatic testing */
            break;
        default:
            usage();
        }
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT, sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler); /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler); /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler);

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1)
    {

        /* Read command line */
        if (emit_prompt)
        {
            printf("%s", prompt);
            fflush(stdout);
        }
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
            app_error("fgets error");
        if (feof(stdin))
        { /* End of file (ctrl-d) */
            fflush(stdout);
            exit(0);
        }

        /* Evaluate the command line */
        eval(cmdline);
        fflush(stdout);
        fflush(stdout);
    }

    exit(0); /* control never reaches here */
}

/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
void eval(char *cmdline)
{
    char *argv[MAXARGS];
    char buf[MAXLINE];
    int bg;
    pid_t pid;

    strcpy(buf, cmdline);
    bg = parseline(buf, argv);
    if (argv[0] == NULL)
        return;

    sigset_t mask, prev;

    if (builtin_cmd(argv) == 0)
    {
        sigemptyset(&mask);
        sigaddset(&mask, SIGCHLD);
        sigprocmask(SIG_BLOCK, &mask, &prev);
        pid = fork();

        if (pid == 0)
        {
            sigprocmask(SIG_SETMASK, &prev, NULL);
            setpgid(0, 0); // Challenge 2: important, read Hints in shlab.pdf
            if (execve(argv[0], argv, NULL) < 0)
            {
                printf("%s: Command not found\n", argv[0]);
                exit(0);
            }
            /* control never reaches here */
        }
        addjob(jobs, pid, bg ? BG : FG, cmdline);
        sigprocmask(SIG_SETMASK, &prev, NULL);
        if (!bg)
            waitfg(pid);
        else
        {
            printf("[%d] (%d) %s", pid2jid(pid), pid, cmdline);
            fflush(stdout);
        }
    }

    return;
}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.  
 */
int parseline(const char *cmdline, char **argv)
{
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to first space delimiter */
    int argc;                   /* number of args */
    int bg;                     /* background job? */

    strcpy(buf, cmdline);
    buf[strlen(buf) - 1] = ' ';   /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
        buf++;

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'')
    {
        buf++;
        delim = strchr(buf, '\'');
    }
    else
    {
        delim = strchr(buf, ' ');
    }

    while (delim)
    {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* ignore spaces */
            buf++;

        if (*buf == '\'')
        {
            buf++;
            delim = strchr(buf, '\'');
        }
        else
        {
            delim = strchr(buf, ' ');
        }
    }
    argv[argc] = NULL;

    if (argc == 0) /* ignore blank line */
        return 1;

    /* should the job run in the background? */
    if ((bg = (*argv[argc - 1] == '&')) != 0)
    {
        argv[--argc] = NULL;
    }
    return bg;
}

/* 
    if (JID)
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
int builtin_cmd(char **argv)
{
    if (strcmp(argv[0], "quit") == 0)
        exit(0);
    else if (strcmp(argv[0], "bg") == 0)
        do_bgfg(argv);
    else if (strcmp(argv[0], "fg") == 0)
        do_bgfg(argv);
    else if (strcmp(argv[0], "jobs") == 0)
        listjobs(jobs);
    else
        return 0; /* not a builtin command */

    return 1;
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 * only implement the simplest version
 * check bg or fg and %jid
 */
void do_bgfg(char **argv)
{
    if (argv[1] == NULL)
    {
        printf("%s command requires PID or %%jobid argument\n", argv[0]);
        return;
    }

    // check bg or fg arguments
    int JID = (argv[1][0] == '%');
    int id = 0;
    long int tmp;

    tmp = strtol(argv[1] + JID, NULL, 10);
    if (!tmp)
    {
        printf("%s: argument must be a PID or %%jobid\n", argv[0]);
        return;
    }
    if (tmp > INT_MAX)
    {
        printf("%s: argument PID or %%jobid greater than %d\n", argv[0], INT_MAX);
        return;
    }
    id = (int)tmp;

    sigset_t mask_all, prev_all;
    sigfillset(&mask_all);
    sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
    struct job_t *job = JID ? getjobjid(jobs, id) : getjobpid(jobs, id);
    if (!job)
    {
        if (JID)
            printf("%s: No such job\n", argv[1]);
        else
            printf("(%s): No such process\n", argv[1]);
        sigprocmask(SIG_SETMASK, &prev_all, NULL);
        return;
    }

    if (strcmp(argv[0], "bg") == 0)
    {
        printf("[%d] (%d) %s", job->jid, job->pid, job->cmdline);
        job->state = BG;
        kill(-job->pid, SIGCONT);
        sigprocmask(SIG_SETMASK, &prev_all, NULL);
    }

    if (strcmp(argv[0], "fg") == 0)
    {
        job->state = FG;
        int pid = job->pid;
        /* Challenge 2: send SIGCONT to a specific process group instead of a single process
         * otherwise see test13 and mysplit
         * in which case the child of fg process doesn't receive SIGCONT
         */
        kill(-job->pid, SIGCONT);
        sigprocmask(SIG_SETMASK, &prev_all, NULL);
        waitfg(pid);
    }
}

/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
    while (pid == fgpid(jobs))
        sleep(1);

    if (verbose)
        printf("waitfg: Process (%d) no longer the fg process\n", pid);
}

/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig)
{
    if (verbose)
        printf("sigchld_handler(%d): entering\n", getpid());

    int olderrno = errno;
    sigset_t mask_all, prev_all;
    pid_t pid;
    int status;

    /* Challenge 1: specify WNOHANG | WUNTRACED (no WUNTRACED actually)
     * otherwise with 0, 
     * waitpid will by default always wait for running child process, if no child process already terminate
     * 
     * Possible Cause: (might also due to the shell or perl process, I cannot fully understand the mechanism)
     * with while loop (which is necessary to reap all zombie process) and case in test05
     * once sigchld_handler entered due to ./myspin 2 &
     * interrupting parent process after fork ./myspin 3 & and before printing the relevant info
     * this instance will wait until ./myspin 3 & terminates and reap it
     * then bad result occurs: jobs command lists nothing
     */

    /* Challenge 3: handle SIGTSTP from other process instead of the terminal
     * see test16 and ./mystop 2
     * ./mystop 2 instance send SIGTSTP to itself
     * it seems ./mystop 2 instance doesn't trig its SIGTSTP handler,
     * but trig its parent process's SIGCHLD handler, which is here
     * P.S. usage of WSTOPSIG and WTERMSIG
     */
    sigfillset(&mask_all);
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0)
    {
        int jid;
        struct job_t *job;

        sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
        jid = pid2jid(pid);
        job = getjobpid(jobs, pid);

        if (WIFEXITED(status))
        {
            int deleted = deletejob(jobs, pid);
            if (verbose)
            {
                if (deleted)
                {
                    printf("sigchld_handler: Job [%d] (%d) deleted\n", jid, pid);
                    printf("sigchld_handler: Job [%d] (%d) terminates %s (status %d)\n",
                           jid, pid, WEXITSTATUS(status) ? "abnormally" : "OK", status);
                }
                else
                {
                    printf("sigchld_handler: Job [%d] (%d) not deleted\n", jid, pid);
                }
            }
        }

        if (WIFSIGNALED(status))
        {
            deletejob(jobs, pid);
            printf("Job [%d] (%d) terminated by signal %d\n", jid, pid, WTERMSIG(status));
        }

        if (WIFSTOPPED(status))
        {
            job->state = ST;
            printf("Job [%d] (%d) stopped by signal %d\n", job->jid, job->pid, WSTOPSIG(status));
        }

        sigprocmask(SIG_SETMASK, &prev_all, NULL);
    }
    errno = olderrno;

    if (verbose)
        printf("sigchld_handler: exiting\n");
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig)
{
    // commented by challenge 4
    // if (verbose)
    //     printf("sigint_handler: entering\n");

    // int olderrno = errno;
    // sigset_t mask_all, prev_all;
    // sigfillset(&mask_all);
    // sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
    // pid_t pid = fgpid(jobs);
    // if (pid)
    // {
    //     int jid = pid2jid(pid);
    //     // Chanllenge 2: send int signal to the fg process group
    //     kill(-pid, sig);
    //     deletejob(jobs, pid);
    //     printf("Job [%d] (%d) terminated by signal %d\n", jid, pid, sig);
    // }
    // sigprocmask(SIG_SETMASK, &prev_all, NULL);
    // errno = olderrno;

    // if (verbose)
    //     printf("sigint_handler: exiting\n");

    /*  Challenge 4: do not handle SIGINT in parent process,
     *  instead we should forward it to child fg group process,
     *  then handle them in sigchld_handler.
     */

    if (verbose)
        printf("sigint_handler: entering\n");

    int olderrno = errno;
    sigset_t mask_all, prev_all;
    sigfillset(&mask_all);
    sigprocmask(SIG_BLOCK, &mask_all, &prev_all);

    pid_t pid = fgpid(jobs);
    if (pid)
        kill(-pid, SIGINT);

    sigprocmask(SIG_SETMASK, &prev_all, NULL);
    errno = olderrno;

    if (verbose)
        printf("sigint_handler: exiting\n");
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig)
{
    /*  Challenge 4: same as sigint_handler
     *  just forward it to child fg process group
     */

    if (verbose)
        printf("sigtstp_handler: entering\n");

    int olderrno = errno;
    sigset_t mask_all, prev_all;
    sigfillset(&mask_all);
    sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
    pid_t pid = fgpid(jobs);
    if (pid)
    {
        /*  Challege 2: send tstp signal to the fg process group
         *  i.e. remember to use -pid instead of pid
         */
        kill(-pid, sig);
        // commented by challenge 4
        // int jid = pid2jid(pid);
        // struct job_t *job = getjobpid(jobs, pid);
        // if (job)
        //     job->state = ST;
        // else
        //     printf("No job has pid %d in sigtstp_handler", pid);
        // printf("Job [%d] (%d) stopped by signal %d\n", jid, pid, sig);
    }
    sigprocmask(SIG_SETMASK, &prev_all, NULL);
    errno = olderrno;

    if (verbose)
        printf("sigtstp_handler: exiting\n");
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job)
{
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++)
        clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs)
{
    int i, max = 0;

    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].jid > max)
            max = jobs[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline)
{
    int i;

    if (pid < 1)
        return 0;

    for (i = 0; i < MAXJOBS; i++)
    {
        if (jobs[i].pid == 0)
        {
            jobs[i].pid = pid;
            jobs[i].state = state;
            jobs[i].jid = nextjid++;
            if (nextjid > MAXJOBS)
                nextjid = 1;
            strcpy(jobs[i].cmdline, cmdline);
            if (verbose)
            {
                printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }
            return 1;
        }
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid)
{
    int i;

    if (pid < 1)
        return 0;

    for (i = 0; i < MAXJOBS; i++)
    {
        if (jobs[i].pid == pid)
        {
            clearjob(&jobs[i]);
            nextjid = maxjid(jobs) + 1;
            return 1;
        }
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].state == FG)
            return jobs[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid)
{
    int i;

    if (pid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].pid == pid)
            return &jobs[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid)
{
    int i;

    if (jid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].jid == jid)
            return &jobs[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid)
{
    int i;

    if (pid < 1)
        return 0;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].pid == pid)
        {
            return jobs[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++)
    {
        if (jobs[i].pid != 0)
        {
            printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
            switch (jobs[i].state)
            {
            case BG:
                printf("Running ");
                break;
            case FG:
                printf("Foreground ");
                break;
            case ST:
                printf("Stopped ");
                break;
            default:
                printf("listjobs: Internal error: job[%d].state=%d ",
                       i, jobs[i].state);
            }
            printf("%s", jobs[i].cmdline);
        }
    }
}
/******************************
 * end job list helper routines
 ******************************/

/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void)
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler)
{
    struct sigaction action, old_action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
        unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig)
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}

/*
 * self added helper function
 */