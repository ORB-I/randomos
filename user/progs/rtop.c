#include <io.h>
#include <kbd.h>
#include <sys/process.h>
#include <sys/sysfn.h>

#define CORE_ROWS 4

static void print_bar(const char* label, const char* value) {
    printf("%-12s %3s [============================]\n", label, value);
}

static void draw_dashboard(void) {
    termctl(TCTL_CLEAR, 0);
    termctl(TCTL_AFLSH, 0);
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
    printf("\nCurrent kernel ABI exposes only the caller PID.\n");
    printf("Press Ctrl+C to exit.\n");
    termctl(TCTL_AFLSH, 1);
    termctl(TCTL_FLUSH, 0);
}

int main(void) {
    int ctrl_pressed = 0;
    draw_dashboard();

    for (;;) {
        u8 scancode = kbd_get_raw_to(100);
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
