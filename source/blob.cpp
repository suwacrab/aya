#include <blob.h>
#include <cstdio>
#include <zlib.h>

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

void Blob::write_str(const std::string& str, bool no_terminator) {
	for(int i=0; i<str.size(); i++) {
		char chr = str.at(i);
		write_u8(chr);
	}
	if(!no_terminator) {
		write_u8(0);
	}
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
Blob Blob::compress(bool do_compress) {
	std::vector<Bytef> comp_data(size()*3 + 32);

	// just a level below Z_BEST_COMPRESSION (9)
	// but above Z_BEST_SPEED(1)
	int compress_mode = Z_NO_COMPRESSION;
	if(do_compress) compress_mode = Z_BEST_COMPRESSION;
//	if(do_compress) compress_mode = Z_BEST_SPEED;

	z_stream zlstrm;
	zlstrm.zalloc = Z_NULL;
	zlstrm.zfree = Z_NULL;
	zlstrm.opaque = Z_NULL;

	zlstrm.avail_in = size();
	zlstrm.next_in = data<Bytef>();
	zlstrm.avail_out = comp_data.size();
	zlstrm.next_out = comp_data.data();

	auto compstat = deflateInit(&zlstrm,compress_mode);
	if(compstat != Z_OK) {
		std::printf("Blob::compress(): error: compression failed to init (%d)...\n",compstat);
		std::exit(-1);
	}
	auto succ_deflate = deflate(&zlstrm,Z_FINISH);
	if(succ_deflate == Z_STREAM_ERROR) {
		std::printf("Blob::compress(): error: deflate() failed (%d)\n",succ_deflate);
		std::exit(-1);
	}
	auto succ_deflateEnd = deflateEnd(&zlstrm);

	if(succ_deflateEnd != Z_OK) {
		std::printf("Blob::compress(): error: deflateEnd() failed (%d)\n",succ_deflateEnd);
		std::exit(-1);
	}

	Blob comp_blob;
	comp_blob.write_raw(comp_data.data(),zlstrm.total_out);
	if(comp_blob.size() > comp_data.size()) {
		std::puts("Blob::compress(): error: compression was > orig size...");
		std::exit(-1);	
	}

	return comp_blob;
}

