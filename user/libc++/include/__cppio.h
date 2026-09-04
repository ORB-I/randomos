#include <io.h>
#include <kbd.h>
namespace std {
    namespace io {
        struct device {
            virtual void putc(char c) { (void)c; }
            virtual int getc() { return 0; }
            virtual bool wrsup() { return false; }
            virtual bool rdsup() { return false; }
        };

        struct builtin_iodev_stdio : public std::io::device {
            void putc(char c) override {
                putchar(c);
            }

            int getc() override {
                return getchar();
            }

            bool wrsup() override { return true; }
            bool rdsup() override { return false; }
        };

        static constexpr int eof = -1;
        struct io {
            io(std::io::device dev) : dev(dev) {}
            void putchar(char c) {
                if (dev.rdsup()) return dev.putc(c);
            }

            int getchar() {
                if (dev.wrsup()) return dev.getc();
                return eof;
            }

            io& operator<<(const char c) {
                this->putchar(c);
                return *this;
            }

            io& operator<<(const char* str) {
                while (*str != '\0') {
                    this->putchar(*str++);
                }
                return *this;
            }

            io& operator<<(long long x) {
                if (x < 0) {
                    dev.putc('-');
                    return *this << (unsigned long long)(-(x + 1)) + 1;
                }
                return *this << (unsigned long long)x;
            }

            io& operator<<(unsigned long long x) {
                if (x == 0) {
                    dev.putc('0');
                    return *this;
                }

                char buf[20];
                usize i = 0;
                while (x) {
                    buf[i++] = '0' + (x % 10);
                    x /= 10;
                }

                while (i--) dev.putc(buf[i]);
                return *this;
            }

        private:
            std::io::device dev;
        };

        using stdio = std::io::builtin_iodev_stdio;
    }
}