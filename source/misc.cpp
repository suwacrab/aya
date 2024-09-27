#include <aya.h>
#include <zlib.h>

#define TWIDTAB(x) ( (x&1)|((x&2)<<1)|((x&4)<<2)|((x&8)<<3)|((x&16)<<4)| \
                     ((x&32)<<5)|((x&64)<<6)|((x&128)<<7)|((x&256)<<8)|((x&512)<<9) )
#define TWIDOUT(x, y) ( TWIDTAB((y)) | (TWIDTAB((x)) << 1) )
#define MIN(a, b) ( (a)<(b)? (a):(b) )

auto aya::compress(Blob& srcblob, bool do_compress) -> Blob {
	std::vector<Bytef> comp_data(srcblob.size() + srcblob.size()/2 + 32);

	// just a level below Z_BEST_COMPRESSION (9)
	// but above Z_BEST_SPEED(1)
	int compress_mode = Z_NO_COMPRESSION;
//	if(do_compress) compress_mode = Z_BEST_COMPRESSION;
	if(do_compress) compress_mode = Z_BEST_SPEED;

	z_stream zlstrm;
	zlstrm.zalloc = Z_NULL;
	zlstrm.zfree = Z_NULL;
	zlstrm.opaque = Z_NULL;

	zlstrm.avail_in = srcblob.size();
	zlstrm.next_in = srcblob.data<Bytef>();
	zlstrm.avail_out = comp_data.size();
	zlstrm.next_out = comp_data.data();

	auto compstat = deflateInit(&zlstrm,compress_mode);
	if(compstat != Z_OK) {
		std::printf("aya::compress(blob): error: compression failed to init (%d)...\n",compstat);
		std::exit(-1);
	}
	auto succ_deflate = deflate(&zlstrm,Z_FINISH);
	if(succ_deflate == Z_STREAM_ERROR) {
		std::printf("error: deflate() failed (%d)\n",succ_deflate);
		std::exit(-1);
	}
	auto succ_deflateEnd = deflateEnd(&zlstrm);

	if(succ_deflateEnd != Z_OK) {
		std::printf("error: deflateEnd() failed (%d)\n",succ_deflateEnd);
		std::exit(-1);
	}
	
	/*uLong ucompSize = srcblob.size();
	uLong compsize = compressBound(ucompSize);

	// dst, dstlen, src, srclen
	int succ_compress = ::compress(comp_data.data(),&compsize,srcblob.data<Bytef>(),srcblob.size());
	if(succ_compress != Z_OK) {
		std::printf("compress(): zlib failed (%d)\n",succ_compress);
		std::exit(-1);
	}*/

	Blob comp_blob;
	comp_blob.write_raw(comp_data.data(),zlstrm.total_out);
//	comp_blob.write_raw(comp_data.data(),compsize);
	if(comp_blob.size() > comp_data.size()) {
		std::puts("aya::compress(blob): error: compression was > orig size...");
		std::exit(-1);	
	}

	return comp_blob;
}	
auto aya::conv_po2(int n) -> int {
	int power = 1;
	while(power < n) {
		power <<= 1;
	}
	return power;
}

auto aya::marisa_graphfmt::getBPP(int format) -> int {
	auto format_id = marisa_graphfmt::getID(format);
	if(!marisa_graphfmt::isValid(format)) {
		puts("imgconnv::marisa_graphfmt_getBPP(fmt): error: invalid format");
		std::exit(-1);
	}

	int bpp = 0;
	switch(format_id) {
		case marisa_graphfmt::i4: { bpp = 4; break; }
		case marisa_graphfmt::i8: { bpp = 8; break; }
		case marisa_graphfmt::rgb565:
		case marisa_graphfmt::rgb5a1:
		case marisa_graphfmt::argb4444: {
			bpp = 16;
			break;
		}
		default: break;
	}
	return bpp;
}
auto aya::patchu_graphfmt::getBPP(int format) -> int {
	auto format_id = patchu_graphfmt::getID(format);
	if(!patchu_graphfmt::isValid(format)) {
		puts("imgconv::patchu_graphfmt_getBPP(fmt): error: invalid format");
		std::exit(-1);
	}

	int bpp = 0;
	switch(format_id) {
		case patchu_graphfmt::i4: { bpp = 4; break; }
		case patchu_graphfmt::i8: { bpp = 8; break; }
		case patchu_graphfmt::rgb565:
		case patchu_graphfmt::rgb5a1:
		case patchu_graphfmt::argb4: {
			bpp = 16;
			break;
		}
		case patchu_graphfmt::argb8: { bpp = 32; break; }
		default: break;
	}
	return bpp;
}

auto aya::version_get() -> CAyaVersion {
	std::string build_date(__DATE__);
	build_date += " ";
	build_date += __TIME__;

	CAyaVersion ver = {};
	ver.build_date = build_date;

	return ver;
}

// https://github.com/KallistiOS/KallistiOS/blob/master/utils/kmgenc/kmgenc.c
// https://github.com/KallistiOS/KallistiOS/blob/5fc21196104b589ccf4e32222ea1f9650a5612f5/kernel/arch/dreamcast/hardware/pvr/pvr_texture.c#L4
auto aya::twiddled_index(int x, int y, int w, int h) -> size_t {
	int min = MIN(w,h);
	int mask = min - 1;
	int z = TWIDOUT(x & mask,y & mask);
	z += (x / min + y / min) * min * min;
	return z;
}
auto aya::twiddled_index4b(int x, int y, int w, int h) -> size_t {
	int min = MIN(w,h);
	int mask = min - 1;
	int z = TWIDOUT((x & mask) / 4,(y & mask));
	z += (x / min + y / min) * min * min;
	return z;
}

