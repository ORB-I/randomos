#include <sys/types.h>
#include <sys/sysfn.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdio.h>
#include <printf.h>
#include <assert.h>

int main() {
    printf("=== Starting ioctl & errno test ===\n");

    set_errno(0);
    assert(get_errno() == 0);
    assert(errno == 0);

    set_errno(EINVAL);
    assert(get_errno() == EINVAL);
    assert(errno == EINVAL);
    printf("strerror(EINVAL): %s\n", strerror(errno));

    int flush_state = ioctl(STDOUT, TCTL_GAFLH, 0);
    printf("ioctl(STDOUT, TCTL_GAFLH) returned: %d\n", flush_state);

    int clear_res = ioctl(STDOUT, TCTL_FLUSH, 0);
    assert(clear_res == 0);
    printf("ioctl(STDOUT, TCTL_FLUSH) succeeded\n");

    set_errno(0);
    int bad_res = ioctl(999, TCTL_FLUSH, 0);
    assert(bad_res == -1);
    assert(errno == EBADF);
    printf("ioctl(999) correctly failed with -1, errno=%d (%s)\n", errno, strerror(errno));
    perror("Expected failure test");

    printf("=== All ioctl & errno tests passed! ===\n");
    return 0;
}

