/*
 * @Date: 2022-03-14 19:48:18
 * @LastEditors: mgpdian
 * @LastEditTime: 2022-03-18 08:57:51
 * @FilePath: /data/my_web/main.cpp
 */
//主函数运行

#include "my_web.h"
   
#include <sys/param.h> 
 #include <sys/types.h>
#include <sys/stat.h>
int main(int argc, char* argv[])
{
    if(argc <= 1)
    {
        printf("usage: %s ip_address port_number\n",basename(argv[0]));
        return 1;
    }     
 
    int port = atoi(argv[1]);
   
    //数据库的用户名 密码 和库名
    string user = "root";    
    string passwd = "515711";
    string databasename = "yourdb";

    int trig_model = 1;// 触发模式 LT: 0 ET:1
    int actor_model = 1; //反应堆模式: proactor 1, reactor: 0
      
    // //忽略终端I/O信号，STOP信号
	// signal(SIGTTOU,SIG_IGN);
	// signal(SIGTTIN,SIG_IGN);
	// signal(SIGTSTP,SIG_IGN);
	// signal(SIGHUP,SIG_IGN); 
   
    // int pid;  
    // //设置守护进程 
    // pid = fork();
    // if(pid > 0)
    // {
    //     //此为父进程
    //     //父进程退出v 
    //     exit(0); 
    // }
    // else if(pid < 0) 
    // {
    //     return -1;
    // } 
    // //子进程创建新会话
    // setsid();

    // //改变当前工作目录 
    // chdir("/data/my_web/root"); 

    // //重设文件掩码 
    // umask(0); 
      
    // //关闭所有从父进程继承的不再需要的文件描述符
	// for(int i=0;i< NOFILE;close(i++));
 

    //初始化设置 
    my_web webserver;
 
    //初始化设置
    webserver.init(port, user, passwd, databasename, 8, trig_model, actor_model );

 
    printf("初始化日志\n");
    //初始化日志  
    webserver.log_write();
        
    //初始化用户数组
     printf("初始化用户数组\n");
    webserver.create_http_conn();  
        
    //初始化连接池 
    printf("初始化连接池\n");   
    webserver.create_mysqlpool();
 
        
    //初始化线程池 
    printf("初始化线程池\n");
    webserver.create_threadpool();  
    
      
      
    printf("初始化listen\n");
    //初始化监听文件描述符 
    webserver.create_listen();
    printf("初始化epoll树\n");
    //初始化epoll树
    webserver.create_epoll();
    printf("初始化定时器\n");
    //初始化定时器
    webserver.create_timer();
    printf("开始循环\n");
    webserver.main_loop();
   
    return 0;
}   