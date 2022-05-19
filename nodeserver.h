#ifndef _NODESERBER_H
#define _NODESERBER_H

#include "net.h"
#include "proxyserver.h"
#include <memory> 

class NodeServer;
class Node : public NET::BaseNet{
public:
	enum{
		LOGIN_TIMEOUT	=	3		// 3秒没有登录的话 就主动断开
	};
	Node(NET::BaseNet & bn, std::weak_ptr<NodeServer> ns, Cid cid)
		: NET::BaseNet(bn), _cid(cid), _connect_ts(0), _nodeserver(ns),_is_logind(false){
			_connect_ts = time(NULL);
		};
	// 调用的时候 表示 _buffer 中有一个完整的包了
	// 处理分发不同的数据包
	void handle(Buffer & buf, Sid sid);
	void OnRecv();
    
    // 处理登录消息 得到note和id，验证没问题的话，将会在ProxyServer的_client_map中注册 以便被浏览器找到
	bool hello(Buffer & buf);
	void forward();			// 根据客户端发来的sid找到请求端并且转发数据
	void disconnect();
	void disconnect(std::string note);
	int safe_send(Buffer & buf){
		Buffer tmp;
		PackSize ps = buf.size() + sizeof(PackSize);
		Sid sid = *(Sid*)buf.data();
		if (ps > 5000 || sid > 5000){
			LOG_D("ps 错误 %d:%d", ps, sid);
		}
		tmp << ps << buf;
		// LOG_D("发送到Node: %zd", tmp.size());
		return SendN(tmp.data(), tmp.size());
	}
    void setinfo(Cid cid, std::string & note){
		_cid = cid;
	}

	void setinfo(std::string & note){
		_note = note;
	}

	void setinfo(Cid cid){
		_cid = cid;
	}

	int proxy_send(Buffer & buf, Sid sid);
	// std::shared_ptr<ClientInfo> get_clientinfo();	// 返回和客户端对应的信息
private:
	std::weak_ptr<NodeServer>	_nodeserver;
	std::vector<Sid>    _sids;
	std::string         _note;			// 描述
	Cid                 _cid;			// 客户端对应的唯一ID
	time_t				_connect_ts;	// 连接时间戳 用于判断超时
	Buffer				_buffer;		// 接收缓冲区
	bool				_is_logind;		// 是否已经登录
};

// 服务端的主要逻辑
class ProxyServer;
class NodeServer : public NET::Passive, public std::enable_shared_from_this<NodeServer> {
public:
	typedef std::map<Cid, std::shared_ptr<Node>>::iterator CidMapIter ;
	enum{
		MIN_CID	=	1,
		MAX_CID	=	UINT16_MAX
	};
	NodeServer(std::string host, int port)
		: NET::Passive(host, port), _new_cid(1){};
	void OnRecv();												// 接收到新的连接 创建客户端对应的类
	// int get_clientinfo(vector<shared<ClientInfo>> & ci);		// 获取所有客户端信息

	void register_client(std::shared_ptr<Node> & node, Cid cid){
		_client_map[cid] = node;
	}

	void unregister_client(Cid cid);

	bool change_client(Cid old_cid, Cid cid){
		CidMapIter it;
		if (_client_map.find(cid) != _client_map.end()){
			// 想要修改的Cid已经有了 发送错误提示
			return false;
		}
		if ((it = _client_map.find(old_cid)) != _client_map.end()){
			_client_map[cid] = _client_map[old_cid];
			_client_map[cid]->setinfo(cid);
			_client_map.erase(old_cid);
			LOG_I("cid 切换: %d->%d", old_cid, cid);
			return true;
		}
		return false;
	}

	Cid gen_cid(){
		if (_client_map.size() >= MAX_CID) return 0;
		_new_cid ++;
		while(_client_map.find(_new_cid) != _client_map.end()){
			_new_cid ++;
			if (_new_cid >= MAX_CID){_new_cid = MIN_CID;}
		}
		return _new_cid;
	}

	int proxy_send(Buffer & buf, Cid cid);

	int proxy_send_request(Buffer & buf, Sid sid);

	void set_proxyserver(std::weak_ptr<ProxyServer> ps);
	void unregister_request(Sid sid);		// 断开与请求端的连接
	void close_terminal(Cid cid, Sid sid);
private:
	std::map<Cid, std::shared_ptr<Node>>	_client_map;		// 按照客户端id排列的客户端列表 (原本设计成static的原因是为了方便代理端能够访问到)
	std::weak_ptr<ProxyServer> _proxyserver;
	Cid _new_cid;
};

#endif