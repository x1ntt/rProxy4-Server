#include "proxyserver.h"
#include "base64.h"

void Request::disconnect(){
	if(!_proxy_server.expired()){
		std::shared_ptr<ProxyServer> ps = _proxy_server.lock();
		ps->close_terminal(_cid, _sid);
		ps->unreister_request(_sid);
	}else{
		delete this;
	}
}

bool Request::handle_request(Buffer & http_request){
	// 解析浏览器请求，
	if (!_is_auth){
		std::string end = "\r\n\r\n";
		std::string http_buf = http_request.get();
		if (http_buf.find(end, 0) != -1){
			// 表示最起码收到一个完整的头
			HttpHeader hh(http_buf);
			if (hh.has_key("Proxy-Authorization")){
				// LOG_D("有代理头: %s", hh.get_value("Proxy-Authorization").c_str());		// 接下来是解码base64之后提取出来cid，转发给对应的节点
				std::string base64, pa;
				pa = hh.get_value("Proxy-Authorization");
				if(HttpHeader::get_authorization_base64(pa, base64)){
					int tmp, pos;
					std::string res = ZBase64::Decode(base64.c_str(), base64.size(), tmp);
					if ((pos=res.find(":")) != -1){
						std::string cid_str, passwd_str;
						cid_str = res.substr(0, pos);
						passwd_str = res.substr(pos+1, res.size());
						// LOG_D("cid: %s, pass: %s", cid_str.c_str(), passwd_str.c_str());
						Cid cid = 0;
						if (passwd_str == _passwd && (cid = atoi(cid_str.c_str()))){
							_cid = cid;
							_is_auth = true;
							return true;
						}else{
							LOG_W("密码不正确: %s", pa.c_str());
							Send(_407_str.c_str(), _407_str.size());
						}
					}else{
						LOG_W("认证格式不正确 %s",pa.c_str());
						Send(_400_str.c_str(), _400_str.size());
					}
				}else{
					LOG_W("没拿到 base64, %s",hh.get_value("Proxy-Authorization").c_str());
					Send(_400_str.c_str(), _400_str.size());
				}
			}else{
				Send(_407_str.c_str(), _407_str.size());
				return false;
			}
		}
	}else{
		return true;
	}
	return false;
}

void Request::OnRecv(){
	std::shared_ptr<char> tmp = std::shared_ptr<char>(new char[RECV_MAX]);
	int ret = Recv(tmp.get(), RECV_MAX);
	if (ret <= 0){
		LOG_I("关闭请求 [sid=%d], fd: %d", _sid, get_sock());
		
		disconnect();
		return ;
	}
	// LOG_D("<-- Request: [cid=%d], [sid=%d], [ret=%d]", _cid, _sid, ret);
	_buffer.push_back(tmp.get(), ret);
	// LOG_D("接收到请求端的数据 ret: %d", ret);
	if (_buffer.size() > MAX_BUFFER_SIZE){
		disconnect("超过接收缓冲区大小限制");	// 这里应该放在Recv前面的，超过限制只要阻塞tcp接收缓冲区就好了
		return ;
	}
	if(_is_auth || handle_request(_buffer)){	// 这里默认是认为第一次发送数据就能收到头部信息，绝大多数情况下应该没有问题，但是确实有可能首次发送不包含完整头
		// 转发数据，失败(大概率是因为没有cid) 断开
		if (!_proxy_server.expired()){
			Buffer tmp;
			tmp << _sid;
			PackSize send_size = std::min((int)_buffer.size(), (int)RECV_MAX);
			tmp.push_back(_buffer.data(), send_size);
			// LOG_D("--> Node: [cid=%d], [sid=%d], [size=%zd]", _cid, _sid, tmp.size());
			if(_proxy_server.lock()->proxy_send_node(tmp, _cid) <= 0){
				disconnect();
				return ;
			}
			_buffer.erase(0, send_size);
		}else{
			disconnect("服务器关闭");
		}
	}else{
		disconnect("请求端认证失败");
	}
}

void ProxyServer::set_nodeserver(std::weak_ptr<NodeServer> ns){
	_ns = ns;
}

// 通过这个接口给节点发送数据
int ProxyServer::proxy_send_node(Buffer & buf, Cid cid){
	if (!_ns.expired()){
		std::shared_ptr<NodeServer> ns = _ns.lock();
		return ns->proxy_send(buf, cid);
	}
	return 0;
};

int ProxyServer::proxy_send(Buffer & buf, Sid sid){
	SidMapIter it = _request_map.find(sid);
	if(it != _request_map.end()){
		//LOG_D("到请求端->[sid=%d] %zd", sid, buf.size());
		return it->second->SendN(buf.data(), buf.size());
	}else{
		LOG_W("没找到[sid=%d] 发往请求端", sid);
	}
	return 0;
}

void ProxyServer::close_terminal(Cid cid, Sid sid){
	if (!_ns.expired()){
		_ns.lock()->close_terminal(cid, sid);
	}
}