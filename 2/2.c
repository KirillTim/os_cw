#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define BUF_SIZE 10
#define MAX_LEN 1000

int sed(char* str, char* from, char* to, char* rv) {
    size_t str_len = strlen(str);
    size_t from_len = strlen(from);
    size_t to_len = strlen(to);
    int rv_len = 0, i = 0;
    while (i < str_len) {
        int j = 0;
        while (j < from_len && i+j < str_len && str[i+j] == from[j]) 
            j ++;
        if (j == from_len) {
            i += j;
            for (int k = 0; k < to_len; k++) {
                rv[rv_len] = to[k];
                rv_len ++;
            }
        }
        else if (j + i == str_len) {
            return i;
        }
        else {
            rv[rv_len] = str[i];
            rv_len++; i++;
        }
    }
    return -1;
}

void sed_action(int fd, char* from, char* to) {
    char buf[BUF_SIZE];
    int count = 0, start = 0;
    for (;;) {
        count = read(STDIN_FILENO, (char*)(buf+start), BUF_SIZE-start);
        buf[count+start] = 0;
        char* data = malloc(MAX_LEN);
        memset(data, 0, MAX_LEN);
        int r = sed(buf, from, to, data);
        if (r >= 0) {
            memmove(buf+r, buf, count+start - r);
            start = count+start - r;
            buf[start] = 0;
        } else {
            start = 0;
        }
        write(fd, data, strlen(data));
        free(data);
        if (count == 0) {
            write(fd, buf, strlen(buf));
            return ;
        }
    }
}

void grep_action(int fd, char* what) {
    
}

int grep(char* str, char* what) {
    int rv = 0;
    size_t str_len = strlen(str);
    size_t what_len = strlen(what);
    for (size_t i = 0; i < str_len-what_len+1; i++) {
        int ok = 1;
        for (size_t j = 0; j < what_len; j++) {
            if (str[i+j] != what[j])
                ok = 0;
        }
        rv += ok;
    }
    return rv;
}

int main(int argc, char** argv) {
    /*char* data = malloc(MAX_LEN);
    memset(data, 0, MAX_LEN);
    int last = sed("ab", "abc", "x", data);
    fprintf(stderr, "%s", data);
    fprintf(stderr,"\n%d\n", last);
    free(data);*/
    sed_action(STDOUT_FILENO, argv[1], argv[2]);
    return 0;    
}
