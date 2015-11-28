#include <unistd.h>

#define BUF_SIZE 10

char buf[BUF_SIZE+1];

int main() {
    int start = 0;
    int write_from = 0;
    ssize_t count = 0;
    for (;;) {
        count = read(STDIN_FILENO,(char*)(buf+start), BUF_SIZE-start);
        if (count == 0) {
            break;
        }
        count += start;
        int i = 0;
        for (;;) {
            if (i == count) {
                write_from = start = 0;
                break;
            }
            if (i == count-1) {
                if (buf[i] == 'b') {
                    write(STDOUT_FILENO, (char*)(buf+write_from), i-write_from);
                    start = 1;
                    buf[0] = 'b';
                } 
                else {
                    write(STDOUT_FILENO, (char*)(buf+write_from), i-write_from+1);
                    start = 0;
                }
                write_from = 0;
                break;
            }
            if (buf[i] == 'b' && buf[i+1] == 'b') {
                buf[i] = 'a';
                write(STDOUT_FILENO, (char*)(buf+write_from), i-write_from+1);
                write_from = i+2;
                i += 2;
            } else {
                i ++;
            }
        }
    }

    return 0;
}
