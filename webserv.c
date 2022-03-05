//web服务器程序 -- 使用epoll模型
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <assert.h> //本质是宏 用来判断表达式是否正确
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define   MAX_EVENT_NUMBER  1024 //最大事件树
#define  MAX_STRING_NUMBER  1024 //最常字符串长度


//读取输入
static ssize_t my_read(int fd, char *ptr);

//读取一行
ssize_t Readline(int fd, void * vptr, size_t maxlen);
//int Readline(int sock, char *buf, int size);
//通过文件名字获得文件类型
char *get_mime_type(char *name);

//通过文件名字获得文件类型
char *get_mime_type(char *name);

//具体发送数据
int Write(int connfd, char * buf, int len);
//具体发送数据
int Write(int connfd, char * buf, int len);
//发送头部信息
int send_header(int connfd, char * code, char * msg, char     * fileTye, int len);
//发送数据信息
int send_file(int connfd, char *fileName);
int http_request(int connfd, int epollfd);

int main(int argc, char *argv[])
{
	//若web服务器给浏览器发送数据的时候, 浏览器已经关闭连接, 
	//则web服务器就会收到SIGPIPE信号
	//这会导致服务器自动关闭
	struct sigaction act;
	act.sa_handler = SIG_IGN;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGPIPE, &act, NULL);
	//将服务器的工作目录改变 类似守护进程改变目录
	char path[255] = {0};

	//getenv获取全目录
	sprintf(path, "%s/%s", getenv("HOME"), "webpath");
	//改变工作目录
	chdir(path);
	printf("%s\n", path);
	//服务器和客户端的addr
	struct sockaddr_in address, client_address;

	//服务器和客户端的套接字
	int listenfd, connfd;
	
	int sockfd, client_address_size;
	//epoll树
	int epollfd = 0;

	int i;
	


	if(argc != 2)
	{
		printf("Usage: %s <port>\n", argv[0]);
		exit(1);

	}
	//生成套接字
	listenfd = socket(PF_INET, SOCK_STREAM, 0);
	assert(listenfd >= 0);


	//设置套接字
	
	//struct sockaddr_in address;
	memset(&address, 0x00, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons(atoi(argv[1]));
	address.sin_addr.s_addr = htonl(INADDR_ANY);

	//设置端口复用
	int flag = 1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int));

	//绑定
	int ret = 0;
	ret = bind(listenfd, (struct sockaddr *) &address, sizeof(address));
	assert(ret >= 0);

	//监听
	ret = listen(listenfd, 5);
	assert(ret >= 0);
	//LOG_INFO("监听创建成功! fd == [%d] \n", listenfd);
	
	//开始创建epoll树
	//创建内核事件表用于接收events事件
	struct epoll_event events[MAX_EVENT_NUMBER];

	epollfd = epoll_create(5);
	assert(epollfd != -1);

	//将listenfd上树并开始监听
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = listenfd;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &ev);
   	
	//epoll_wait的返回值
	//返回的是 发生变化的数量
	int number;
	//printf("will epoll\n");
	while(1)
	{
		//等待事件发生
		number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);

		if(number == -1)
		{
			printf("epoll failure\n");
			exit(1);

		}

		//处理发送的事件
		for(i = 0; i < number; ++i)
		{
			sockfd = events[i].data.fd;
	//		printf("what happened socket = %d", sockfd);
	//		//如果socket == listenfd 
			//表示有客户端连接
			if(sockfd == listenfd)
			{
	//			printf("listenfd \n");
				client_address_size = sizeof(client_address);
				connfd = accept(listenfd, (struct sockaddr*) &client_address, &client_address_size);	
				 if(connfd == -1) {  
       				 perror("accept error");
    			    exit(1);
				    }
	
				//将connfd设置为非阻塞
				int flag = fcntl(connfd, F_GETFL);
				flag |= O_NONBLOCK;
				fcntl(connfd, F_SETFL, flag);

				//将客户端上树
				 //struct epoll_event eve;
				ev.events = EPOLLIN;
				ev.data.fd = connfd;

				ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &ev);
	//			printf("connect client: %d \n", connfd);
				if(ret == -1) { 
      					  perror("epoll_ctl add cfd error");
     				   exit(1);
   				 }
	
			}
			else
			{
	//			printf("client\n");
				//有客户端数据发来
				http_request(sockfd, epollfd);
				epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, NULL);
				close(sockfd);

			}


		}

	}
	close(epollfd);
	close(listenfd);
	


	return 0;
}


int http_request(int connfd, int epollfd)
{
	int n;
	char buf[MAX_STRING_NUMBER];
//	printf("now socket = %d", connfd);
	//读取请求行数据, 分析要请求的资源文件名
//	memset(buf, 0x00, sizeof(buf));

	int len = Readline(connfd, buf, sizeof(buf));
	if(len == 0)
	{
		printf("closeclient\n");
		close(connfd);
	
		epoll_ctl(epollfd, EPOLL_CTL_DEL, connfd, NULL);
		return -1;
	}
	printf("buf == [%s]\n", buf);
	//GET /hanzi.c HTTP/1.1
	char reqType[16] = {0};
	char fileName[255] = {0};
	char protocal[16] = {0};
	sscanf(buf, "%[^ ] %[^ ] %[^ \r\n]", reqType, fileName, protocal);
	printf("[%s]\n", reqType);
	printf("[%s]\n", fileName);
	printf("[%s]\n", protocal);
	
	char *pFile = fileName ;
	if(strlen(fileName)<=1)
	{
		strcpy(pFile, "./");
	}
	else
	{
		pFile = fileName+1;
	}
	printf("[%s]\n", pFile);

	//循环读取剩余的数据
	while((n = Readline(connfd, buf, sizeof(buf))) > 0);

	//判断文件是否存在
	struct stat st;
	if((stat(pFile, &st)) < 0)
	{
		printf("file not exist\n");

		//发送头部信息
		send_header(connfd, "404", "NOT FOUND", get_mime_type(".html"), 0);


		//发送文件内容
		send_file(connfd, "error.html");
	}
	else //文件存在
	{
		//判断文件类型
		//普通文件
		if(S_ISREG(st.st_mode))
		{
			printf("file exist\n");
			//发送头部信息
			send_header(connfd, "200", "OK", get_mime_type(pFile),st.st_size);

			//发送文件内容
			send_file(connfd, pFile);

		}
		//目录文件
		else if(S_ISDIR(st.st_mode))
		{
			printf("目录文件\n");
			
			char  buffer[MAX_STRING_NUMBER];

			//发送头部信息
			send_header(connfd, "200", "OK", get_mime_type(".html"), 0);


			//发送html文件头部
			send_file(connfd, "html/dir_header.html");
			
			//文件列表信息
			struct dirent **namelist;
			int num;

			num = scandir(pFile, &namelist, NULL, alphasort);
			if( num < 0 )
			{
				perror("scandir");
				close(connfd);
				epoll_ctl(epollfd, EPOLL_CTL_DEL, connfd, NULL);
				return -1;
			}
			else
			{
				while(num--)
				{
					printf("%s\n", namelist[num] -> d_name);
					memset(buffer, 0x00, sizeof(buffer));
					if(namelist[num] -> d_type == DT_DIR)
					{
						sprintf(buffer, "<li><a href=%s>%s</a></li>", namelist[num] -> d_name,  namelist[num]->d_name);
					}
					else
					{
						sprintf(buffer, "<li><a href=%s>%s</a></li>", namelist[num] -> d_name, namelist[num] -> d_name);

					}
					free(namelist[num]);
					Write(connfd, buffer, strlen(buffer));
				}
				free(namelist);
			}
			//发送html尾部
			sleep(10);
			send_file(connfd, "html/dir_tail.html");
			


		}

	}


	return 0;
}
//发送头部信息
int send_header(int connfd, char * code, char * msg, char * fileType, int len)
{
	char buf[MAX_STRING_NUMBER] = {0};
	sprintf(buf, "HTTP/1.1 %s %s\r\n", code, msg);
	sprintf(buf + strlen(buf), "Content-Type:%s\r\n", fileType);

	if(len > 0)
	{
		sprintf(buf+strlen(buf), "Content-Length:%d\r\n", len);
				
	}
	strcat(buf, "\r\n");
	Write(connfd, buf, strlen(buf));
	return 0;
}
//发送数据信息
int send_file(int connfd, char *fileName)
{
	//打开文件
	int fd = open(fileName, O_RDONLY);
	if(fd < 0)
	{
		perror("open error");
		return -1;
	}
	
	//循环读文件 发送
	int n;
	char buf[1024];
	while(1)
	{
		memset(buf, 0x00, sizeof(buf));
		n = read(fd, buf, sizeof(buf));
//		printf("%s\n", buf);
		if(n <= 0)
		{
			break;
		}
		else
		{
			Write(connfd, buf, n);
		}
	}
	close(fd);
	return 0;

}

//具体发送数据
int Write(int connfd, char * buf, int len)
{
	int n;
	n = write(connfd, buf, len);
	//循环发送数据
	/*while((n = write(connfd, buf, len)) > 0)
	{
		if(errno == EINTR && n == -1)
			continue;
		else if(n == -1)
			return -1;
//		printf("%d", n);
	}*/	
	return n;

}
//读取输入
static ssize_t my_read(int fd, char *ptr)
{

	static int read_cnt;
	static char *read_ptr;
	static char read_buf[100];//定义缓冲区

	if(read_cnt <= 0)
	{
	again:
		//使用缓冲区可以避免多次从底层缓冲读取数据 
		if((read_cnt = read(fd, read_buf, sizeof(read_buf))) < 0)
		{
			if(errno == EINTR)
				goto again;
			return -1;
		}
		else if(read_cnt == 0)
		{
			return 0;

		}
		read_ptr = read_buf;


	}
	read_cnt--;
	*ptr = *read_ptr++;//从缓冲区读取数据

	return 1;

}

//读取一行
ssize_t Readline(int fd, void * vptr, size_t maxlen)
{
	ssize_t n, rc;
	char c, *ptr;

	ptr = vptr;
	for(n = 1; n < maxlen; n++)
	{
		if((rc = my_read(fd, &c)) == 1)
		
		{
			*ptr++ = c;
			if(c == '\n')
				//代表任务完成
				break;
		
		}
		else if(rc == 0)
			{
				//对端关闭
				
				*ptr = 0; //0 = '\0'
				return n - 1;
			}
			
		else 
				return -1;

		

	}
	*ptr = 0;
	return n;


}
/*
//Readline
//解析http请求消息的每一行内容
int Readline(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;
    while ((i < size - 1) && (c != '\n')) {    
        n = read(sock, &c, 1);
        if (n > 0) {        
            if (c == '\r') {            
                n = read(sock, &c, 1);
                if ((n > 0) && (c == '\n')) {               
                    read(sock, &c, 1);
                } else {                            
                    c = '\n';
                }
            }
            buf[i] = c;
            i++;
        } else {       
            c = '\n';
        }
    }
    buf[i] = '\0';
//	printf("buf == %s\n", buf);
    return i;
}*/

//通过文件名字获得文件类型
char *get_mime_type(char *name)
{
    char* dot;

    dot = strrchr(name, '.');	//自右向左查找‘.’字符;如不存在返回NULL
    /*
     *charset=iso-8859-1	西欧的编码，说明网站采用的编码是英文；
     *charset=gb2312		说明网站采用的编码是简体中文；
     *charset=utf-8			代表世界通用的语言编码；
     *						可以用到中文、韩文、日文等世界上所有语言编码上
     *charset=euc-kr		说明网站采用的编码是韩文；
     *charset=big5			说明网站采用的编码是繁体中文；
     *
     *以下是依据传递进来的文件名，使用后缀判断是何种文件类型
     *将对应的文件类型按照http定义的关键字发送回去
     */
    if (dot == (char*)0)
        return "text/plain; charset=utf-8";
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".au") == 0)
        return "audio/basic";
    if (strcmp( dot, ".wav") == 0)
        return "audio/wav";
    if (strcmp(dot, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
        return "video/quicktime";
    if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
        return "video/mpeg";
    if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
        return "model/vrml";
    if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
        return "audio/midi";
    if (strcmp(dot, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(dot, ".ogg") == 0)
        return "application/ogg";
    if (strcmp(dot, ".pac") == 0)
        return "application/x-ns-proxy-autoconfig";

    return "text/plain; charset=utf-8";
}
