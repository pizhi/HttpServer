/*
 * http_server.h
 *
 *  Created on: 2015-1-9
 *      Author: root
 */

#ifndef HTTP_SERVER_H_
#define HTTP_SERVER_H_

int read_line(int fd,char *buf,int size);
void do_get(int fd,char *fileName);
char* get_fileName(char *buf);
void data_process(int clientfd);
int init_socket(int port);
void start_server(int port);
void sighandler(int signo);

#endif /* HTTP_SERVER_H_ */
