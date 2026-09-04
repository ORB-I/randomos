#include <io.h>
#include <kbd.h>
#include <sys/process.h>
#include <sys/sysfn.h>
#include <time.h>

#define BAR_WIDTH 28
#define CORE_ROWS 4

static void print_bar(const char* label, const char* value) {
    printf("%-12s %3s [", label, value);
    for (int i = 0; i < BAR_WIDTH; i++) {
        putchar('=');
    }
    printf("]\n");
}

static void draw_dashboard(void) {
    termctl(TCTL_CLEAR, 0);
    printf("RandomOS system monitor\n\n");

    for (int core = 0; core < CORE_ROWS; core++) {
        char label[16];
        snprintf(label, sizeof(label), "Core %d", core);
        print_bar(label, "N/A");
    }
    print_bar("Ram Amount", "N/A");
    print_bar("Swap Amount", "N/A");

    printf("\n%-24s | %-10s | %s\n", "Name", "CPU Usage", "PID");
    printf("%-24s | %-10s | %d\n", "rosyst", "N/A", getpid());
    printf("\nPress Ctrl+C to exit.\n");
}

int main(void) {
    int ctrl_pressed = 0;

    for (;;) {
        draw_dashboard();

        // Service keys until one second has elapsed, then refresh.
        u64 refresh_at = getclock(CLOCK_MONOMS) + 1000;
        for (;;) {
            u64 now = getclock(CLOCK_MONOMS);
            if (now >= refresh_at) break;

            u64 wait = refresh_at - now;
            if (wait > 100) wait = 100;

            u8 scancode = kbd_get_raw_to(wait);
            if (scancode == 0) continue;

            if (scancode == 0x1d) {
                ctrl_pressed = 1;
            } else if (scancode == 0x9d) {
                ctrl_pressed = 0;
            } else if (ctrl_pressed && (scancode == 0x2e || scancode == 0xae)) {
                return 0;
            }
        }
    }
}