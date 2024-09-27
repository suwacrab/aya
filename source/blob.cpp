#include <blob.h>
#include <cstdio>

Blob::Blob() {
	reset();
}
Blob::Blob(Blob& orig) {
	reset();
	// write from orig to self
	write_blob(orig);
}

void Blob::reset() {
	m_data.clear();
}

void Blob::write_blob(Blob& source) {
	write_raw(source.data(),source.size());
}
void Blob::write_raw(const void* source, size_t len) {
	if(len == 0) return;
	if(source == nullptr) {
		std::printf("Blob::write_raw(): error: attempt to write to %p from null source",
			source
		);
		std::exit(-1);
	}

	auto src_bytes = static_cast<const uint8_t*>(source);
	for(size_t i=0; i<len; i++) {
		m_data.push_back(*src_bytes++);
	}
}
void Blob::write_u8(uint32_t n) {
	m_data.push_back(n & 0xFF);
}
void Blob::write_u16(uint32_t n) {
	write_u8(n);
	write_u8(n>>8);
}
void Blob::write_u32(uint32_t n) {
	write_u16(n);
	write_u16(n>>16);
}

bool Blob::send_file(std::string filename) {
	auto file = std::fopen(filename.c_str(),"wb");
	if(!file) {
		std::printf("Blob::send_file(fname): error: unable to send to file %s\n",
			filename.c_str()
		);
		std::exit(-1);
		return false;
	}
	std::fwrite(data(),sizeof(char),size(),file);
	std::fclose(file);
	return true;
}

