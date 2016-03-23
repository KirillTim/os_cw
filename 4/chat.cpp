#define _POSIX_C_SOURCE 201505
#define _GNU_SOURCE

#include <netdb.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>


typedef struct buf_t {
    size_t size;
    size_t capacity;
} buf_t;

buf_t *buf_new(size_t capacity);
void buf_free(buf_t *);
size_t buf_capacity(buf_t *);
size_t buf_size(buf_t *);

ssize_t buf_fill(int fd, buf_t *buf, size_t required);
ssize_t buf_flush(int fd, buf_t *buf, size_t required);

ssize_t buf_getline(int fd, buf_t *buf, char linesep, void *ebuf);

char *buf_get_data(buf_t *buf) {
    return ((char*) buf) + 2 * sizeof(size_t);
}

struct buf_t *buf_new(size_t capacity) {
    buf_t *rv = (buf_t*) malloc(sizeof(size_t) * 2 + capacity);
    if(rv == 0)
        return rv;
    rv->size = 0;
    rv->capacity = capacity;
    return rv;
}

void buf_free(struct buf_t *buf) {
    free(buf);
}

size_t buf_capacity(buf_t *buf) {
    return buf->capacity;
}

size_t buf_size(buf_t *buf) {
    return buf->size;
}

ssize_t buf_fill(int fd, buf_t *buf, size_t required) {
    char *data = buf_get_data(buf); 
    
    while(buf->size < required) {
        ssize_t rres = read(fd, data + buf->size, buf->capacity - buf->size);
        if(rres < 0) {
            return -1; 
        }
        if(rres == 0) {
            return buf->size;
        }
        buf->size += rres;
    }
    return buf->size;
}

ssize_t buf_flush(int fd, buf_t *buf, size_t required) {
    char *data = buf_get_data(buf);
    
    size_t offset = 0;
    while(buf->size > 0 && offset < required) {
        ssize_t wres = write(fd, data + offset, buf->size - offset);
        if(wres < 0) {
            memmove(data, data + offset, buf->size - offset);
            buf->size -= offset;
            return -1;
        }
        offset += wres;
    }
    memmove(data, data + offset, buf->size - offset);
    buf->size -= offset;
    return offset;
}
//////////////////////////////
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include <map>
#include <utility>
using namespace std;

const size_t BUF_SIZE = 4096;
#define LISTEN_QUEUE 5

struct pollfd fds[256];
int fds_sz = 1;

struct user {
    int socket = -1;
    string room;
    buf_t* input;
    buf_t* output;
    bool active;
    user(int sock) {
        this->socket = sock;
        this->active = false;
        this->input = buf_new(BUF_SIZE);
        this->output = buf_new(BUF_SIZE);
    }
};

map<int, user> users;

void add_user(int socket) {
	users.insert(make_pair(socket, user(socket)));
}
void disconnect(int socket) {
    close(socket);
	//user us = get_user(socket);
	buf_free(users.find(socket)->second.input);
    buf_free(users.find(socket)->second.output);
    users.erase(users.find(socket));
}
void move_buf(buf_t* buf, int pos) {
	char* str = buf_get_data(buf);
	buf->size -= pos;
	memmove(str, str+pos, buf_size(buf));
}
void push_to_room(string room, char* str, int count) {
	for (auto& i : users) {
		if (i.second.room == room) {
			int oldsize = buf_size(i.second.output);
			i.second.output->size += count;
			char* dat = buf_get_data(i.second.output);
			for (int j = 0; j <= count; j++)
				dat[oldsize+j] = str[j];
		}
	}

}
void process(int socket) {
	buf_t* buf = users.find(socket)->second.input;
	char* str = buf_get_data(buf);
	if (!users.find(socket)->second.active) {
		for (int i = 0; i < buf_size(buf); i++) {
			if (str[i] == '\n') {
				for (int j = 0; j < i; j++)
					users.find(socket)->second.room += str[j];
				users.find(socket)->second.active = true;
				move_buf(buf, i+1);
				break;
			}
		}
	}
	for (int i = buf_size(buf)-1; i >= 0; i--) {
		if (str[i] == '\n') {
			push_to_room(users.find(socket)->second.room, str, i+1);
			move_buf(buf, i+1);
		}
	}
}

void set_events() {
	for (auto& i : users) {
		if (i.second.output->size > 0) {
			for (int j = 1; j < fds_sz; j++) {
				if (fds[j].fd == i.second.socket) 
					fds[j].events |= POLLOUT;
			}
		}
	}
}

int make_server(struct addrinfo *localhost) {
	int sock1 = socket(localhost->ai_family, SOCK_STREAM, 0);
	if(sock1 < 0) {
		return -1;
	}
	if(bind(sock1, localhost->ai_addr, localhost->ai_addrlen)) {
		return -2;
	}
	if(listen(sock1, LISTEN_QUEUE)) {
		return -3;
	}
	return sock1;
}

void print_state() {
	cerr<<"fds: ";
	for (int i = 0; i < fds_sz; i++) {
		cerr<<fds[i].fd<<" (";
		if (fds[i].revents & POLLIN)
			cerr<<"I,";
		if (fds[i].revents & POLLOUT)
			cerr<<"O";
		cerr<<") ";
	}
	cerr<<"\nusers:\n";
	for (auto& i :users) {
		user u = i.second;
		cerr<<u.socket<<" "<<u.room<<" "<<u.active;
		cerr<<"in sz: "<<u.input->size<<" out sz:"<<u.output->size<<"\n";
	}
	cerr<<"--------------\n";
}

int main(int argc, char** argv) {
	signal(SIGPIPE, SIG_IGN);
	
	struct addrinfo *localhost1;
	if(getaddrinfo("0.0.0.0", argv[1], 0, &localhost1)) {
		return 2;
	}
	int server = make_server(localhost1);
	if(server < 0) {
		return 3;
	}
	freeaddrinfo(localhost1);
	
	memset(fds, 0, sizeof(struct pollfd) * 256);
	fds[0].fd = server;
	fds[0].events = POLLIN;

	while (1) {
		int res = poll(fds, fds_sz, -1);
		//print_state();
		if (res == -1) {
			if(errno == EINTR)
				continue;
			return 10;
		}
		short ce = fds[0].revents;
		if (ce) {
			if (ce & POLLIN) {
				int cli = accept(fds[0].fd, 0, 0);
				cerr<<"new client: "<<cli<<"\n";
				if (cli < 0) {
					return 11;
				}
				add_user(cli);
				fds[fds_sz].events = POLLIN | POLLRDHUP;
				fds[fds_sz].fd = cli;
				fds_sz ++;
				fds[0].events = POLLIN;
			}
		}
		for (int i = 1; i < fds_sz; i++) {
			ce = fds[i].revents;
			fds[i].revents = 0;
			int sock = fds[i].fd;
			if (ce) {
				if (ce & POLLOUT) {
					buf_t* buf = users.find(sock)->second.output;
					if (buf_flush(sock, buf, 1) < 0) {
						disconnect(sock);
						fds_sz --;	
						cerr<<"disconnected(write returns -1): "<<sock<<"\n";
						continue;
					}
					if (buf_size(buf) > 0) {
						fds[i].events |= POLLOUT;
					} else {
						fds[i].events &= ~POLLOUT;
					}
				}
				if (ce & POLLIN) {
					buf_t* buf = users.find(sock)->second.input;
					int oldsize = buf_size(buf);
					char* qqq = buf_get_data(buf);
					if (buf_fill(sock, buf, oldsize + 1) < 0) {
						disconnect(sock);
						fds_sz --;	
						cerr<<"disconnected(read returns -1): "<<sock<<"\n";
						continue;
					}
					process(sock);
					set_events();
					fds[i].events |= POLLIN;
				}
				if ((ce & POLLRDHUP) || (ce & POLLHUP) || (ce & POLLERR)) {
					cerr<<"disconnected: "<<sock<<"\n";
					disconnect(sock);
					fds_sz --;
				}
			}
		}
	}
	return 0;
}
