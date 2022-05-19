#include "nodeserver.h"
#include "cmd.h"

void NodeServer::OnRecv(){
	LOG_D("接收到链接");
	NET::BaseNet * bn = Accept();
	Cid cid = gen_cid();
	if (cid){
		std::shared_ptr<Node> node = std::shared_ptr<Node>(new Node(*bn, weak_from_this(), cid));
		register_client(node, cid);
		std::weak_ptr<BaseNet> sbn = node;
		NET::Poll::register_poll(node->get_sock(), sbn);
	}else{
		bn->OnClose();
	}
	delete bn;
	// 检查登录超时
}

void Node::disconnect(){
	if (!_nodeserver.expired()){		// NodeServer生命周期一定比Node长
		std::shared_ptr<NodeServer> ns = _nodeserver.lock();
		ns->unregister_client(_cid);
		NET::Poll::deregister_poll(get_sock());
	}else{
		OnClose();
		NET::Poll::deregister_poll(get_sock());
		delete this;					// NodeServer先死掉了 那么就只能自我了断了
	}
}

void Node::disconnect(std::string note){
	LOG_I("断开原因: %s", note.c_str());
	disconnect();
}

void Node::OnRecv(){
	char data[RECV_MAX];
	int ret = Recv(data, RECV_MAX);
	if (ret <= 0){
		disconnect();
		return ;
	}
	
	_buffer.push_back(data, ret);
	if(_buffer.size() > MAX_BUFFER_SIZE){	// 大小超过一定程度 则认为出现了异常 断开连接
		LOG_W("断开Node 长度: %zd", _buffer.size());
		disconnect();
		return ;
	}
	// _buffer.dump();
	// 下面开始解包流程
	if (_buffer.size() > (sizeof(PackSize) + sizeof(Sid))){
		PackSize ps = *(PackSize*)_buffer.data();
		Sid sid;
		if (ps < (sizeof(PackSize)+sizeof(Sid))){disconnect("不正确的包"); _buffer.dump();return ;};
		if (_buffer.size() >= ps){
			// 此时 _buffer 中才有完整的包
			_buffer >> ps >> sid;
			LOG_D("<-- Node: [cid=%d], [sid=%d], [ret=%d]", _cid, sid, ret);
			Buffer buf;
			buf.push_back(_buffer.data(), ps-sizeof(PackSize)-sizeof(Sid));
			_buffer.erase(0, ps-sizeof(PackSize)-sizeof(Sid));
			handle(buf, sid);
		}
	}
}

int Node::proxy_send(Buffer & buf, Sid sid){
	if (!_nodeserver.expired()){
		return _nodeserver.lock()->proxy_send_request(buf, sid);
	}
	return 0;
}

bool Node::hello(Buffer & buf){
	us16 cid, note_size;
	std::shared_ptr<NodeServer> ns = _nodeserver.lock();
	std::string note_str = "无描述";
	bool ret = true;

	buf >> cid >> note_size;
	if (cid){
		ret = ns->change_client(_cid, cid);
	}

	if (ret && note_size){
		std::shared_ptr<char> note = std::shared_ptr<char>(new char[note_size]);
		if (note_size != buf.size()) disconnect("note备注长度有错");
		buf.pop_back(note.get(), note_size);
		note_str = std::string(note.get(), note_size);
	}
	setinfo(note_str);
	LOG_I("新客户端验证完成cid: %d, note: %s, ret: %d", _cid, note_str.c_str(), ret);
	return ret;
}

void Node::handle(Buffer & buf, Sid sid){
	if (sid > MAX_SID) {
		LOG_W("sid超过限制 %d", sid);
		buf.dump();
		return;
	}

	if (sid == 0){
		if (buf.size() < sizeof(CmdId)) {
			LOG_W("buf大小不正确 %zd", buf.size());
			buf.dump();
			return ;
		};

		CmdId cmdid;
		buf >> cmdid;
		if (!_is_logind && cmdid != CMD::CMD_LOGIN) disconnect();		// 要求第一个协议一定是LOGIN 防止非法登录 
		switch(cmdid){
			case CMD::CMD_LOGIN:{
				if (buf.size() < sizeof(CMD::login)){
					disconnect("错误的登录数据包");
					break ;
				}
				if(hello(buf)){
					Buffer bf;
					CMD::Make_login_re(bf, _cid);
					safe_send(bf);
					_is_logind = true;
				}else{
					Buffer bf;
					CMD::Make_login_re(bf, 0);
					safe_send(bf);
					disconnect("登录失败");
				}
				break;
			}
			case CMD::CMD_SESSION_END:{
				// 关闭对应的sid
				if (buf.size() != sizeof(Sid)) return ;
				Sid end_sid;
				buf >> end_sid;
				LOG_D("尝试断开 [sid=%d]", end_sid);
				if (!_nodeserver.expired()){
					_nodeserver.lock()->unregister_request(end_sid);
				}
				break;
			}
			default:{
				LOG_W("错误的命令号: %d", cmdid);
				buf.dump();
			}
		}
	}else{
		// 普通数据
		// LOG_D("--> Request: [cid=%d], [sid=%d], [ret=%zd]", _cid, sid, buf.size());
		if (proxy_send(buf, sid) <= 0){
			// 告诉节点不该再发送数据了
		}
	}
}

int NodeServer::proxy_send(Buffer & buf, Cid cid){
	CidMapIter it = _client_map.find(cid);
	if(it != _client_map.end()){
		return it->second->safe_send(buf);
	}else{
		// LOG_W("没找到[cid=%d] 发往Node", cid);
	}
	return 0;
};

int NodeServer::proxy_send_request(Buffer & buf, Sid sid){
	if (!_proxyserver.expired()){
		return _proxyserver.lock()->proxy_send(buf, sid);
	}
	return 0;
}

void NodeServer::set_proxyserver(std::weak_ptr<ProxyServer> ps){
	_proxyserver = ps;
}

void NodeServer::unregister_client(Cid cid){
	CidMapIter it;
	if ((it = _client_map.find(cid)) != _client_map.end()){
		it->second->OnClose();
		_client_map.erase(it);
		if (!_proxyserver.expired()){
			_proxyserver.lock()->clear_request_bycid(cid);
		}
		LOG_I("移除 cid: %d", cid);
	}
}

void NodeServer::unregister_request(Sid sid){
	if (!_proxyserver.expired()){
		_proxyserver.lock()->unreister_request(sid);
	}
}

void NodeServer::close_terminal(Cid cid, Sid sid){
	CidMapIter it;
	if ((it = _client_map.find(cid)) != _client_map.end()){
		Buffer tmp;
		CMD::Make_session_end(tmp, sid);
		it->second->safe_send(tmp);
	}
}