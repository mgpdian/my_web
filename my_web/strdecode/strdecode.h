/*
 * @Date: 2022-03-12 02:12:41
 * @LastEditors: mgpdian
 * @LastEditTime: 2022-03-12 20:35:02
 * @FilePath: /data/my_web/strdecode/strdecode.h
 */
#ifndef STRDECODE_H
#define STRDECODE_H
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <strings.h>
#include <ctype.h>


char *get_mime_type(char *name);
void strdecode(char *to, char *from);
int hexit(char c);//16进制转10进制
void strencode(char* to, size_t tosize, const char* from);//编码
#endif