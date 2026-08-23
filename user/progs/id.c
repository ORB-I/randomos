#include <io.h>
#include <sys/process.h>

int main() {
    uid_t uid = getuid();
    gid_t gid = getgid();
    uid_t euid = geteuid();
    gid_t egid = getegid();

    printf("uid=%u gid=%u euid=%u egid=%u\n", (u32)uid, (u32)gid, (u32)euid, (u32)egid);
    return 0;
}
