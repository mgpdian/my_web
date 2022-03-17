/*
 * @Date: 2022-03-18 07:15:51
 * @LastEditors: mgpdian
 * @LastEditTime: 2022-03-18 08:52:36
 * @FilePath: /data/my_web/LRU/LRU.h
 */
//通过简单的LRU缓存来仿照session
#ifndef LRU_H
#define LRU_H
#include <iostream>
#include <unordered_map>
#include <string.h>
#include <arpa/inet.h>
using namespace std;
struct Node
    {    
        in_addr_t k;
        std::string v;
        Node * l;
        Node * r;
        Node() : k(0), v(""), l(NULL), r(NULL){}
        Node(in_addr_t _k, std::string _v): k(_k), v(_v), l(NULL), r(NULL){}
    };
class LRUCache {
public:
    
    
    LRUCache(int capacity) {
        head = new Node();
        tail = new Node();
        this -> capacity = capacity;
        size = 0;
        head -> r = tail;
        tail -> l = head;
    }
    ~LRUCache()
    {
        Node * temp = head;
        while(head)
        {
            temp = head -> r;
            delete head;
            head = temp;
        }
    }
    string get(in_addr_t key) {
        if( map.count(key))
        {
            Node * temp = map[key];
            reflash(temp);
            return temp->v;
        }
        else
            return "";
        
    }
    
    void put(in_addr_t key, string value) {
        Node * temp = NULL;
        if(map.count(key))
        {
            map[key]->v = value;
            reflash(map[key]);
        }
        else{
            if(size < capacity)
            {
                size++;
                temp = new Node(key, value);
                
                reflash(temp);
                map[key] = temp;
            }
            else
            {
                temp = tail->l;
                
                deletel(temp);
                map.erase(temp->k);
                temp = new Node(key, value);
                
                reflash(temp);
                map[key] = temp;
            }
        }
    }
    //更新LRU链表缓存
    void reflash(Node * temp)
    {
        deletel(temp);
        temp -> r  = head -> r;
        head -> r -> l = temp;
        temp -> l = head;
        head -> r = temp;
    }
    //删除
    void deletel(Node * temp)
    {
        if(temp -> r != NULL)
        {
            temp -> l -> r = temp -> r;
            temp -> r -> l = temp -> l;
            //delete temp;
        }
        
    }
private:    
    
    unordered_map<in_addr_t, Node*> map;
    Node * head;
    Node * tail;
    int size; //当前长度
    int capacity; //容量
};

#endif