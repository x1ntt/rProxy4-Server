#include "cmd.h"

namespace CMD {
	Buffer& Make_head(Buffer& buffer, Sid sid) {
		return buffer << sid;
	}

	Buffer& Make_data(Buffer& buffer, Buffer& data, Sid sid) {
		Buffer tmp;
		tmp << data;
		Make_head(buffer, sid);
		return buffer << tmp;
	}

	// 以下都是常规命令 不需要附带sid 因为sid都是0
	Buffer& Make_login(Buffer& buffer, Cid cid, std::string& note) {
		Buffer tmp;
		CmdId c = CMD_LOGIN;
		PackSize note_size = note.size();
		tmp << c << cid << note_size << note;

		Make_head(buffer, 0);
		return buffer << tmp;
	}

	// cid == 0, 表示登录失败 cid被占用了
	Buffer& Make_login_re(Buffer& buffer, Cid cid) {
		Buffer tmp;
		CmdId c = CMD_LOGIN_RE;
		tmp << c << cid;
		Make_head(buffer, 0);
		return buffer << tmp;
	}

	Buffer& Make_session_end(Buffer& buffer, Sid sid){
		Buffer tmp;
		CmdId c = CMD_SESSION_END;
		tmp << c << sid;
		Make_head(buffer, 0);
		return buffer << tmp;
	}
};
