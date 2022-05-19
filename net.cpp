#include "net.h"

namespace NET{

    bool Poll::_running = true;
    std::mutex Poll::_poll_mtx;

    NET::Poll poll;
    struct epoll_event Poll::_ev;
    struct epoll_event Poll::_events[MAX_CONNECT];
	std::map<int, std::weak_ptr<BaseNet>> Poll::_poll_map;

    int Poll::_eph = 0;
};