#include <unistd.h>

char buf[] = "hello world!\n";

int main() {
    write(STDOUT_FILENO, (char*)(buf+6), 14);
    return 0;
}
