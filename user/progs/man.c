#include <io.h>
#include <printf.h>
#include <fs.h>
#include <mem.h>
#include <str.h>
#include <sys/sysfn.h>
#include <sys/types.h>
#include <kbd.h>

static void print_usage(void) {
    printf("Usage: man [options] <topic>\n");
    printf("Options:\n");
    printf("  -l, --list    List all available manual topics\n");
    printf("  -h, --help    Show this help message\n\n");
    printf("Available Topics:\n");
    printf("  Commands:      sh, ls, cat, echo, mkfile, rm, clear, sleep,\n");
    printf("                 reboot, poweroff, id, whoami, environ, random,\n");
    printf("                 login, wm, man\n");
    printf("  Config Files:  passwd\n");
    printf("  System:        syscalls, abi\n");
}

static void print_list(void) {
    printf("RandomOS Manual Topics:\n");
    printf("  sh(1)         Interactive command shell\n");
    printf("  ls(1)         List directory contents\n");
    printf("  cat(1)        Concatenate and display files\n");
    printf("  echo(1)       Print line of text\n");
    printf("  mkfile(1)     Create empty file\n");
    printf("  rm(1)         Remove file or directory\n");
    printf("  clear(1)      Clear terminal screen\n");
    printf("  sleep(1)      Delay execution for specified seconds\n");
    printf("  reboot(1)     Reboot the system\n");
    printf("  poweroff(1)   Power off the machine via ACPI\n");
    printf("  id(1)         Print real and effective user/group IDs\n");
    printf("  whoami(1)     Print effective user name\n");
    printf("  environ(1)    List environment variables\n");
    printf("  random(1)     Read random bytes from hardware RNG\n");
    printf("  login(1)      Authenticate user from /etc/passwd\n");
    printf("  wm(1)         Window manager and desktop environment\n");
    printf("  man(1)        Manual documentation viewer\n");
    printf("  passwd(5)     User password database format\n");
    printf("  syscalls(2)   Kernel syscall interface and numbers\n");
    printf("  abi(7)        x86_64 system call register convention\n");
}

static int display_file(const char* path) {
    int fd = open((char*)path, O_RDONLY, 0);
    if (fd < 0) return -1;

    char buf[512];
    ssize n;
    while ((n = read(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        printf("%s", buf);
    }
    close(fd);
    return 0;
}

static int show_builtin(const char* topic) {
    if (streq(topic, "sh")) {
        printf("SH(1)                       RandomOS Manual                       SH(1)\n\n");
        printf("NAME\n    sh - command language interpreter\n\n");
        printf("SYNOPSIS\n    sh\n\n");
        printf("DESCRIPTION\n    sh is the standard command line interpreter. It reads commands\n");
        printf("    from standard input, searches executables in PATH, and manages process\n");
        printf("    creation and execution.\n\n");
        printf("BUILTINS\n    cd [dir]    Change working directory\n");
        printf("    pwd         Print working directory\n");
        printf("    exit        Exit shell session\n");
        return 0;
    }

    if (streq(topic, "ls")) {
        printf("LS(1)                       RandomOS Manual                       LS(1)\n\n");
        printf("NAME\n    ls - list directory contents\n\n");
        printf("SYNOPSIS\n    ls [directory...]\n\n");
        printf("DESCRIPTION\n    List information about the files and directories in the given path.\n");
        printf("    If no path is specified, the current working directory is listed.\n");
        return 0;
    }

    if (streq(topic, "cat")) {
        printf("CAT(1)                      RandomOS Manual                      CAT(1)\n\n");
        printf("NAME\n    cat - concatenate and display file contents\n\n");
        printf("SYNOPSIS\n    cat <file...>\n\n");
        printf("DESCRIPTION\n    Reads files sequentially and writes them to standard output.\n");
        return 0;
    }

    if (streq(topic, "wm")) {
        printf("WM(1)                       RandomOS Manual                       WM(1)\n\n");
        printf("NAME\n    wm - window manager and graphical desktop environment\n\n");
        printf("SYNOPSIS\n    wm\n\n");
        printf("DESCRIPTION\n    Starts the graphical desktop environment with window dragging,\n");
        printf("    taskbar, application menu, and built-in tools (Terminal, File Manager,\n");
        printf("    System Info, Notes, Calculator).\n");
        return 0;
    }

    if (streq(topic, "man")) {
        printf("MAN(1)                      RandomOS Manual                      MAN(1)\n\n");
        printf("NAME\n    man - documentation viewer\n\n");
        printf("SYNOPSIS\n    man [options] <topic>\n\n");
        printf("DESCRIPTION\n    man displays reference manual pages for commands, syscalls, and\n");
        printf("    configuration files from /share/man/.\n");
        return 0;
    }

    if (streq(topic, "passwd")) {
        printf("PASSWD(5)                   RandomOS Manual                   PASSWD(5)\n\n");
        printf("NAME\n    /etc/passwd - user account database\n\n");
        printf("DESCRIPTION\n    Colon-separated text file storing user credentials:\n");
        printf("      username:argon2id_hash:uid:gid:homedir:shell\n");
        return 0;
    }

    if (streq(topic, "syscalls") || streq(topic, "abi")) {
        printf("SYSCALLS(2)                 RandomOS Manual                 SYSCALLS(2)\n\n");
        printf("NAME\n    syscalls - kernel system call interface\n\n");
        printf("ABI CONVENTION (x86_64)\n");
        printf("    NR:  RAX\n");
        printf("    A0:  RDI\n");
        printf("    A1:  RSI\n");
        printf("    A2:  RDX\n");
        printf("    A3:  R10\n");
        printf("    A4:  R8\n");
        printf("    A5:  R9\n");
        printf("    RET: RAX\n\n");
        printf("PRIMARY CALLS\n");
        printf("    1: SYS_EXIT        2: SYS_READ        3: SYS_WRITE\n");
        printf("    4: SYS_OPEN        5: SYS_CLOSE       6: SYS_CREAT\n");
        printf("    7: SYS_UNLINK      9: SYS_LSEEK      10: SYS_RENAME\n");
        printf("   11: SYS_MKDIR      12: SYS_RMDIR      14: SYS_STAT\n");
        printf("   16: SYS_SLEEP      17: SYS_READDIR    18: SYS_OPENDIR\n");
        printf("   42: SYS_NEWPROC    43: SYS_WAIT       44: SYS_KILL\n");
        printf("   45: SYS_GETPID     46: SYS_GETUID     47: SYS_SETUID\n");
        printf("   56: SYS_EXECVE     57: SYS_RANDOM64   58: SYS_RANDOMBYTES\n");
        printf("   59: SYS_GETPWD     60: SYS_SETPWD\n");
        return 0;
    }

    return -1;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 0;
    }

    const char* topic = argv[1];

    if (streq(topic, "-h") || streq(topic, "--help") || streq(topic, "help")) {
        print_usage();
        return 0;
    }

    if (streq(topic, "-l") || streq(topic, "--list") || streq(topic, "list")) {
        print_list();
        return 0;
    }

    char path[128];

    // Check /share/man/<topic>.txt
    snprintf(path, sizeof(path), "/share/man/%s.txt", topic);
    if (display_file(path) == 0) return 0;

    // Check /share/man/<topic>
    snprintf(path, sizeof(path), "/share/man/%s", topic);
    if (display_file(path) == 0) return 0;

    // Check /share/doc/<topic>.txt
    snprintf(path, sizeof(path), "/share/doc/%s.txt", topic);
    if (display_file(path) == 0) return 0;

    // Check /etc/<topic>.fmt
    snprintf(path, sizeof(path), "/etc/%s.fmt", topic);
    if (display_file(path) == 0) return 0;

    // Builtin fallback
    if (show_builtin(topic) == 0) return 0;

    printf("No manual entry for '%s'. Type 'man --list' for available topics.\n", topic);
    return 1;
}
