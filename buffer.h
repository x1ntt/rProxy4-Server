#ifndef _BUFFER_H
#define _BUFFER_H

#include "type.h"

#include <mutex>
#include <string>
#include <string.h>
#include <thread>
#include <iostream>

// 线程安全的缓存器
class Buffer {

public:
	Buffer() {}
	Buffer(Buffer & buf) {
		*this = buf;
	}
	void operator=(Buffer& buf) {
		if (&buf == this) return;
		std::lock(buf._buffer_mtx, _buffer_mtx);
		std::lock_guard<std::mutex> lock_a(buf._buffer_mtx, std::adopt_lock);
		std::lock_guard<std::mutex> lock_b(_buffer_mtx, std::adopt_lock);
		_buffer = buf._buffer;
	};
	size_t size() {
		std::lock_guard<std::mutex> keeper(_buffer_mtx);
		return _buffer.size();
	}

	const char* data() {
		std::lock_guard<std::mutex> keeper(_buffer_mtx);
		return _buffer.data();
	}

	Buffer& push_back(const char* data, size_t size) {
		std::lock_guard<std::mutex> keeper(_buffer_mtx);
		_buffer.append(data, size);
		return *this;
	}

	Buffer& pop_back(char* data, size_t size) {
		std::lock_guard<std::mutex> keeper(_buffer_mtx);
		if (!_buffer.size()) { return *this; }
		memcpy(data, _buffer.data(), size);
		_buffer.erase(0, size);
		return *this;
	}

	Buffer& erase(std::size_t start, std::size_t end) {
		std::lock_guard<std::mutex> keeper(_buffer_mtx);
		_buffer.erase(start, end);
		return *this;
	}

	std::string& get() {
		return _buffer;
	}

	void dump(size_t len_limit = 30) {
		std::lock_guard<std::mutex> keeper(_buffer_mtx);
		for (int i = 0; i < _buffer.size() || i == len_limit; i++) {
			printf("%02X ", *(char*)(_buffer.data() + i));
		}
		printf("(size: %zd)\n", _buffer.size());
	}

	Buffer& operator<<(us16& data) { push_back((char*)&data, sizeof(us16)); return *this; }
	Buffer& operator<<(int& data) { push_back((char*)&data, sizeof(int)); return *this; }
	Buffer& operator<<(size_t& data) { push_back((char*)&data, sizeof(size_t)); return *this; }
	Buffer& operator<<(Buffer& data) { push_back(data.data(), data.size()); return *this; }
	Buffer& operator<<(std::string& data) { push_back(data.data(), data.size()); return *this; }

	Buffer& operator>>(us16& data) { pop_back((char*)&data, sizeof(us16)); return *this; }
	Buffer& operator>>(us32& data) { pop_back((char*)&data, sizeof(us32)); return *this; }
	Buffer& operator>>(int& data) { pop_back((char*)&data, sizeof(int)); return *this; }
	Buffer& operator>>(Buffer& buf) { buf.push_back(_buffer.data(), _buffer.size()); return *this; }

	std::mutex _buffer_mtx;
private:
	std::string _buffer;

};
#endif