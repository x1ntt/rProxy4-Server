#ifndef _NET_H
#define _NET_H

#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <string.h>
#include <iostream>
#include <map>
#include <vector>
#include <mutex>
#include <algorithm>
#include <memory>

#include "type.h"
#include "log.h"
#include "buffer.h"

namespace NET{
    enum NET_CONF{
        LISTEN_COUNT    =   50,
        MAX_CONNECT     =   1000
    };
    class BaseNet {
    protected:
        std::string _host;
        int _port;
        int _sock_fd;
        unsigned int _flag;
        struct sockaddr_in _addr;
        int _packet_size;  // 实际包大小   记得初始化
        int _packet_pos;    // 已经接收的实际大小
    public:
        enum {
            NET_ERROR = 0x0001,     // 连接失败
            NEED_DELETE = 0x0002    // 有这个标志的对象会在poll函数中被删除
        };

        // 基础包结构，只有数据大小头
        typedef struct {
            unsigned short int data_len;
            char data[];
        }BasePacket;

        int& get_sock() { return _sock_fd; };
        void set_sock(int sock) { _sock_fd = sock; };
        std::string& get_host() { return _host; };
        void set_host(std::string host) { _host = host; };
        int& get_port() { return _port; };
        void set_port(int port) { _port = port; };
        struct sockaddr_in& get_sockaddr_in() { return _addr; };
        void set_sockaddr_in(struct sockaddr_in& addr) { memcpy(&_addr, &addr, sizeof(addr)); };

        BaseNet() : _flag(0), _packet_size(0), _packet_pos(0), _sock_fd(0), _port(0) {
            memset(&_addr, 0, sizeof(struct sockaddr_in));
        };
        BaseNet(std::string host, int port) :_host(host), _port(port), _flag(0),
            _packet_size(0),
            _packet_pos(0),
            _sock_fd(0) {
            memset(&_addr, 0, sizeof(struct sockaddr_in));
        };
        virtual ~BaseNet() {};

        virtual void OnRecv() {
            LOG_D("BaseNet的OnRecv()");
        };

        // 不在析构函数中关闭套接字是因为有时delete对象保留连接的需要 所以把对象和连接分离比较好
        void OnClose() {
            close(_sock_fd);
        };

        // 会接收一个连接，并且返回 BaseNet 对象指针
        BaseNet* Accept() {
            BaseNet* bn = new BaseNet();
            socklen_t lt = sizeof(_addr);
            int sock;
            struct sockaddr_in& addr = bn->get_sockaddr_in();
            if ((sock = accept(_sock_fd, (struct sockaddr*)&(addr), &lt)) == -1) {
                delete bn;
                return NULL;
            }
            bn->set_sock(sock);
            bn->set_port(ntohs(addr.sin_port));
            char ip[128];
            inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
            bn->set_host(ip);
            return bn;
        }

        int Recv(char* data, size_t len) {
            int ret = 0;
            do {
                ret = recv(_sock_fd, data, len, 0);
            } while (ret == -1 && errno == EINTR);    // windows 大概是没有errno
            return ret;
        }

        // 保证接受完 len 长度的数据才返回
        int RecvN(char* data, size_t len) {
            int ret = 0;
            int real_recv = 0;
            do {
                ret = Recv(data + real_recv, len - real_recv);
                if (ret <= 0) { return ret; }
                real_recv += ret;
            } while (real_recv != len);
            return real_recv;
        };

        int Send(const char* data, size_t len) {
            int ret = 0;
            ret = send(_sock_fd, data, len, 0);
            //if (ret != len && ret > 0) {
            //    printf("[!!!!!!!!] Send:  ret/len: %d/%d", ret, (int)len);
            //}
            return ret;
        }

        // 不发送完len长度的数据绝不返回 除非接收错误
        int SendN(const char* data, size_t len) {
            int ret = 0;
            int real_send = 0; // 总共发送的数据
            do {
                ret = Send(data + real_send, len - real_send);
                if (ret <= 0) { return ret; }
                real_send += ret;
            } while (real_send != len);
            return real_send;
        }

        unsigned int get_flag() { return _flag; };
        void SetError() { _flag |= NET_ERROR; }
        bool IsError() { return _flag & NET_ERROR; };
        void SetDelete() { _flag |= NEED_DELETE; }
        bool IsDelete() { return _flag & NEED_DELETE; };

    };

    class Passive : public BaseNet {
    public:
        Passive(std::string host, int port) :BaseNet(host, port) {
            _sock_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (_sock_fd < 0) {
                LOG_E("创建服务套接字失败");
                SetError();
                return;
            }
            memset(&_addr, 0, sizeof(_addr));
            if (inet_pton(AF_INET, _host.c_str(), &(_addr.sin_addr)) <= 0) {
                LOG_E("主机地址有错误");
                SetError();
                return;
            }
            _addr.sin_family = AF_INET;
            _addr.sin_port = htons(_port);
            if (bind(_sock_fd, (sockaddr*)&_addr, sizeof(_addr)) < 0) {
                LOG_E("绑定错误");
                SetError();
                return;
            }
            listen(_sock_fd, LISTEN_COUNT);
            LOG_I("开始监听, %s:%d", _host.c_str(), _port);
        };
        virtual void OnRecv() {
            LOG_D("Passivs的虚方法");
        };
    };

    class Active : public BaseNet {
    public:
        Active(std::string host, int port) : BaseNet(host, port) {
            _sock_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (_sock_fd < 0) {
                LOG_E("创建连接套接字失败");
                SetError();
                return;
            }
            memset(&_addr, 0, sizeof(_addr));
            _addr.sin_family = AF_INET;
            _addr.sin_port = htons(_port);
            if (inet_pton(AF_INET, _host.c_str(), &_addr.sin_addr) <= 0) {
                LOG_E("主机地址有错误 ");
                SetError();
                return;
            }
            if (connect(_sock_fd, (struct sockaddr*)&_addr, sizeof(_addr)) < 0) {
                LOG_E("连接错误");
                SetError();
                return;
            }
            // printf("[Info]: 连接成功: %s:%d\n", _host.c_str(), _port);
        }
    };

    class Poll {
    public:
        enum {
            TIMEOUT = 500  // 超时时间 毫秒
        };
    private:
        static struct epoll_event _ev, _events[MAX_CONNECT];
        static int _eph;
        static std::mutex _poll_mtx;
        static bool _running;
		static std::map<int, std::weak_ptr<BaseNet>> _poll_map;
    public:
        Poll() {};

        static void init() {
            _eph = epoll_create(MAX_CONNECT);
            if (_eph < 0) { perror("[Error]: 创建epoll错误"); }
        }

        static void register_poll(int fd, std::weak_ptr<BaseNet> bn) {
            std::lock_guard<std::mutex> l(_poll_mtx);
            _ev.events = EPOLLIN;
			_ev.data.fd = fd;
			_poll_map[fd] = bn;
            epoll_ctl(_eph, EPOLL_CTL_ADD, fd, &_ev);
        }
        static void deregister_poll(int fd) {
            std::lock_guard<std::mutex> l(_poll_mtx);
			if (_poll_map.find(fd) != _poll_map.end()){
				_poll_map.erase(fd);
			}
            epoll_ctl(_eph, EPOLL_CTL_DEL, fd, NULL);
        }

        static void loop_poll() {
            _running = true;
			std::shared_ptr<BaseNet> bn;
			int fd = 0;
            for (; _running;) {
                int n = epoll_wait(_eph, _events, MAX_CONNECT, TIMEOUT);
                for (int i = 0; i < n; i++) {
                    fd = _events[i].data.fd;
					if (_poll_map.find(fd) != _poll_map.end()){
						std::weak_ptr<BaseNet> & bn = _poll_map[fd];
						if (bn.expired()){
							deregister_poll(fd);
						}else{
							bn.lock()->OnRecv();
						}
					}
                }
            }
        }

        static void stop_poll() {
            _running = false;
        }

    };

};

#endif