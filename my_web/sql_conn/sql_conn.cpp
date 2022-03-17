/*
 * @Date: 2022-03-15 04:53:02
 * @LastEditors: mgpdian
 * @LastEditTime: 2022-03-18 03:32:03
 * @FilePath: /data/my_web/sql_conn/sql_conn.cpp
 */
#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "sql_conn.h"

using namespace std;


//默认构造函数
connection_pool::connection_pool()
{
    m_CurConn = 0;  //初始化已使用的连接数
    m_FreeConn = 0; //当前空闲的连接数
}

//析构函数
connection_pool::~connection_pool()
{
    DestroyPool(); //摧毁所有连接
}

//单例模型 初始化连接池
connection_pool *connection_pool::GetInstance()
{
    static connection_pool connPool;
    return &connPool;
}

//构造初始化  初始化 数据库的基本信息，后面可以使用这些基本信息来链接linux中已有的数据库
void connection_pool::init(string url, string User, string PassWord, string DBName, int Port, int MaxConn, int close_log)
{
    m_url = url; //主机名或IP地址
    m_Port = Port; //端口号
    m_User = User; //用户名(是指root用户之列 不是客户的用户名)
    m_PassWord = PassWord; //密码(root的密码)
    m_DatabaseName = DBName; //数据库名字
    m_close_log = close_log; //是否关闭


    //初始化数据库连接池
    for(int i = 0; i < MaxConn; ++i)
    {
        //初始化数据库类并连接到本地数据库
        MYSQL *con = NULL;
        con = mysql_init(con);  //放入本地数据库
                                //也就是初始化一个连接句柄
        if(con == NULL)
        {
            LOG_ERROR("MYSQL  ERROR");
            exit(1);
        }
        //连接数据库?
        //如果连接成功，一个 MYSQL*连接句柄。如果连接失败，NULL。对一个成功的连接，返回值与第一个参数值相同，除非你传递NULL给该参数。
        con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DBName.c_str(), Port, NULL, 0);

        //mysql_query(con,"SET NAMES GBK");
        // 改变编码格式
	    //mysql_set_character_set(con, "GB2312");
        //mysql_set_character_set(con,"utf8"); 
        //mysql_query(con, "SET NAMES UTF8");
        //mysql_query(con, "SET NAMES GB2312");
        if(con == NULL)
        {
            LOG_ERROR("MySql Error");
            exit(1);
        }
        connList.push_back(con);    //将连接成功的con放入连接池
        ++m_FreeConn;   //当前空闲的连接数++
                        //MaxConn看来是最大连接数

    }

    reserve = my_sem(m_FreeConn);  //初始化m_FreeConn信号量 

    m_MaxConn = m_FreeConn; //完成初始化 当前的空闲连接数就是以后的最大连接数
}

//当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL *connection_pool::GetConnection()
{
    MYSQL *con  = NULL;

    if( 0 == connList.size())   //连接池为空 表示没有可用的连接
    {
        return NULL;
    }

    //如果有空闲的连接 要加锁
    reserve.wait(); //这是信号锁
    lock.lock();    //这是互斥锁

    con = connList.front();//从池中取出连接
    connList.pop_front();

    --m_FreeConn;  //空闲连接减1
    ++m_CurConn; //忙碌连接++


    lock.unlock();
    return con;
}


//释放当前使用的连接    ReleaseConnection
bool connection_pool :: ReleaseConnection( MYSQL *con)
{
    if(NULL == con)
    {
        return false;
    }

    lock.lock();    //上锁防止其他线程释放

    connList.push_back(con);//将空闲连接返回到池中
    ++m_FreeConn;
    --m_CurConn;

    lock.unlock();

    reserve.post(); //信号量加一
    return true;

}


//销毁数据库连接池
void connection_pool::DestroyPool()
{
    lock.lock();
    if(connList.size() > 0)
    {
        list<MYSQL *>::iterator it;
        for(it = connList.begin(); it != connList.end(); ++it)
        {
            MYSQL *con = *it;
            mysql_close(con); //关闭连接
        }
        m_CurConn = 0;
        m_FreeConn = 0;
        connList.clear(); //将list的长度清除
    }
    lock.unlock();
}

//当前空闲的连接数
int connection_pool::GetFreeConn()
{
    return this -> m_FreeConn;
}

//### RAII机制释放数据库连接

//将数据库连接的获取与释放通过RAII机制封装，避免手动释放。
//和connectionRAII相关函数
//获取
connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool)
{
    *SQL = connPool -> GetConnection(); //获得一个可用的连接

    conRAII = *SQL; //赋值

    poolRAII = connPool;//赋值  疑问这俩个有用吗 有 可以通过析构自动释放
} 

//释放
connectionRAII::~connectionRAII()
{
    poolRAII -> ReleaseConnection(conRAII);
}