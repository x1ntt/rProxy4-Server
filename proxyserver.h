#ifndef _PROXY_SERVER_H
#define _PROXY_SERVER_H

#include "net.h"
#include "nodeserver.h"
#include "httparse.h"

class ProxyServer;
class Request : public NET::BaseNet{
public:
	Request(NET::BaseNet & bn, std::weak_ptr<ProxyServer> ps, Sid sid)
		: NET::BaseNet(bn), _proxy_server(ps), _sid(sid), _is_auth(false){
			_407_str = "HTTP/1.1 407 Proxy Authentication Required\r\nConnection: close\r\nProxy-Authenticate: Basic realm=\"simple\"\r\n\r\n";
			_400_str = "HTTP/1.1 400 Bad Request\r\n\r\n";
			_passwd = "123456";
		}

	~Request(){
		OnClose();
	}

	bool handle_request(Buffer& http_request);

	void OnRecv();

	void disconnect(const char* note){
		LOG_I("断开请求端，原因: %s", note);
		disconnect();
	}

	void disconnect();
	Cid get_cid(){return _cid;}
private:
	Buffer _buffer;
	std::weak_ptr<ProxyServer> _proxy_server;
	Sid _sid;
	Cid _cid;		// 提供服务的节点
	bool _is_auth;	// 是否已经认证过了
	std::string _407_str;
	std::string _400_str;
	std::string _passwd;
};

class NodeServer;
class ProxyServer : public NET::Passive , public std::enable_shared_from_this<ProxyServer> {
public:
	typedef std::map<Sid, std::shared_ptr<Request>>::iterator SidMapIter ;


	ProxyServer(std::string host, int port)
		: NET::Passive(host, port){
			// 
		}
	void OnRecv(){
		// 接收浏览器的连接
		std::shared_ptr<BaseNet> sbn = std::shared_ptr<BaseNet>(Accept());
		Sid sid = gen_sid();
		if (sid){
			std::shared_ptr<Request> rq = std::shared_ptr<Request>(new Request(*sbn.get(), weak_from_this(), sid));
			if(!register_request(rq, sid)){
				OnClose();
				return ;
			}
			std::weak_ptr<BaseNet> wbn = rq;
			NET::Poll::register_poll(rq->get_sock(), wbn);
		}else{
			OnClose();
		}
	}
	Sid gen_sid(){				// 如果没有sid可用，就返回0
		if (_request_map.size() >= MAX_SID) return 0;
		_new_sid ++;
		while(_request_map.find(_new_sid) != _request_map.end()){
			_new_sid ++;
			if (_new_sid >= MAX_SID){_new_sid = MAX_SID;}
		}
		return _new_sid;
	}

	bool register_request(std::shared_ptr<Request> & rq, Sid sid){
		SidMapIter it = _request_map.find(sid);
		if (it == _request_map.end()){
			_request_map[sid] = rq;
			return true;
		}else{
			return false;
		}
	}
	void unreister_request(Sid sid){
		SidMapIter it = _request_map.find(sid);
		if (it != _request_map.end()){
			it->second->OnClose();
			_request_map.erase(sid);
		}else{
			LOG_W("没有发现对应的sid!!");
		}
	}

	void clear_request_bycid(Cid cid){
		SidMapIter it = _request_map.begin();
		int cnt = 0;
		while(it != _request_map.end()){
			if (it->second->get_cid() == cid){
				it->second->OnClose();
				it = _request_map.erase(it);
				cnt ++;
			}else{
				it ++;
			}
		}
		LOG_D("清理request cnd: %d", cnt);
	}

	// 通过这个接口给节点发送数据
	int proxy_send_node(Buffer & buf, Cid cid);

	int proxy_send(Buffer & buf, Sid sid);

	void set_nodeserver(std::weak_ptr<NodeServer> ns);

	void close_terminal(Cid cid, Sid sid);
private:
	Sid _new_sid;
	std::map<Sid, std::shared_ptr<Request>> _request_map;
	std::weak_ptr<NodeServer> _ns;
};

#endif