/*
 * @Date: 2022-03-15 04:52:56
 * @LastEditors: mgpdian
 * @LastEditTime: 2022-03-17 00:47:11
 * @FilePath: /data/my_web/sql_conn/sql_conn.h
 */
#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "../locker/locker.h"
#include "../log/log.h"


using namespace std;
//连接池
class connection_pool
{

public:
    MYSQL *GetConnection();     //获取数据库连接
    bool ReleaseConnection(MYSQL *conn); //释放连接
    int GetFreeConn();  //获取连接
    void DestroyPool(); //销毁所有连接

    //单例模式
    static connection_pool *GetInstance();

    //添加属性
    void init(string url, string User, string PassWord, string DataBaseName, int Port, int MaxConn, int close_log);


private:
    connection_pool();
    ~connection_pool();


    int m_MaxConn; //最大连接数
    int m_CurConn; //当前已使用的连接数
    int m_FreeConn; //当前空闲的连接数

    my_locker lock;  //互斥锁 保护数据输入
    list<MYSQL *> connList; //连接池
    my_sem reserve; //信号量
public:
    string m_url; //主机地址
    string m_Port; //数据库端口号
    string m_User; //登录数据库用户名
    string m_PassWord; //登录数据库用户名
    string m_DatabaseName; //使用数据库名
    int m_close_log; //日志开关


};


//将数据库连接的获取与释放通过RAII机制封装，避免手动释放。
class connectionRAII
{

public:
    connectionRAII(MYSQL **con, connection_pool* connPool);//获取一个连接
    ~connectionRAII();//返还一个连接


private:
    MYSQL *conRAII;
    connection_pool *poolRAII;


};

#endif