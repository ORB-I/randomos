#include <io>

int main() {
    std::io::device stdio_dev = std::io::stdio();
    std::io::io stdio(stdio_dev);

    stdio << "Hello\n";
    return 0;
}