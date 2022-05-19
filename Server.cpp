#include "nodeserver.h"
#include "proxyserver.h"

int main(int argv, char** args){
	NET::Poll::init();

	std::shared_ptr<NodeServer> shared_ns = std::shared_ptr<NodeServer>(new NodeServer("0.0.0.0", 7201));
	if (shared_ns->IsError()){
		LOG_E("监听错误 %d", 7201);
		return -1;
	}
	std::shared_ptr<NET::BaseNet> ns = shared_ns;

	std::shared_ptr<ProxyServer> shared_ps = std::shared_ptr<ProxyServer>(new ProxyServer("0.0.0.0", 7202));
	if (shared_ps->IsError()){
		LOG_E("监听错误 %d", 7202);
		return -2;
	}
	std::shared_ptr<NET::BaseNet> ps = shared_ps;

	shared_ns->set_proxyserver(shared_ps);
	shared_ps->set_nodeserver(shared_ns);

	NET::Poll::register_poll(ns->get_sock(), ns);
	NET::Poll::register_poll(ps->get_sock(), ps);
	NET::Poll::loop_poll();
	return 0;
}

/********  TODO  ***********
 * 
 * 1. net需要修改epoll的工作方式，智能指针不能直接放到data.ptr中，看起来需要自己维护一个map
 * 2. 完成登录的编写
 * 3. 添加发送缓冲区以增加发送效率
 * 4. 完成请求端密码认证的优化，缓存认证结果 
 * 
 * **************************/