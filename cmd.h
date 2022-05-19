#ifndef _CMD_H
#define _CMD_H

#include "buffer.h"

#define MAKE_HEAD Make_head(buffer,tmp.size(),0);

namespace CMD {
	enum CMDID {
		CMD_ERROR = 0,		// 错误值 保留
		CMD_LOGIN,				// 登录
		CMD_LOGIN_RE,			// 登录响应
		CMD_SESSION_END,			// 会话结束
	};

	struct login {
		CmdId cmdid;
		Cid cid;
		us16 note_size;
		char note[];
	};

	Buffer& Make_head(Buffer& buffer, Sid sid);

	Buffer& Make_data(Buffer& buffer, Buffer& data, Sid sid);

	// 以下都是常规命令 不需要附带sid 因为sid都是0
	Buffer& Make_login(Buffer& buffer, Cid cid, std::string& note);

	// cid == 0, 表示登录失败 cid被占用了
	Buffer& Make_login_re(Buffer& buffer, Cid cid);
	Buffer& Make_session_end(Buffer& buffer, Sid sid);
};

#endif