#include <io.h>
#include <env.h>

int main() {
    for (usize i = 0; environ[i] != NULL; i++) {
        printf("%s\n", environ[i]);
    }
    return 0;
}