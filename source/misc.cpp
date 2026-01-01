#include <aya.h>
#include <zlib.h>

#define TWIDTAB(x) ( (x&1)|((x&2)<<1)|((x&4)<<2)|((x&8)<<3)|((x&16)<<4)| \
                     ((x&32)<<5)|((x&64)<<6)|((x&128)<<7)|((x&256)<<8)|((x&512)<<9) )
#define TWIDOUT(x, y) ( TWIDTAB((y)) | (TWIDTAB((x)) << 1) )
#define MIN(a, b) ( (a)<(b)? (a):(b) )

auto aya::compress(Blob& srcblob, bool do_compress) -> Blob {
	return srcblob.compress(do_compress);
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
auto aya::narumi_graphfmt::getBPP(int format) -> int {
	auto format_id = narumi_graphfmt::getID(format);
	if(!narumi_graphfmt::isValid(format)) {
		puts("imgconv::narumi_graphfmt_getBPP(fmt): error: invalid format");
		std::exit(-1);
	}

	int bpp = 0;
	switch(format_id) {
		case narumi_graphfmt::i4: { bpp = 4; break; }
		case narumi_graphfmt::i8: { bpp = 8; break; }
		case narumi_graphfmt::rgb: {
			bpp = 16;
			break;
		}
		default: break;
	}
	return bpp;
}
auto aya::alice_graphfmt::getBPP(int format) -> int {
	auto format_id = alice_graphfmt::getID(format);
	if(!alice_graphfmt::isValid(format)) {
		puts("imgconv::alice_graphfmt_getBPP(fmt): error: invalid format");
		std::exit(-1);
	}

	int bpp = 0;
	switch(format_id) {
		case alice_graphfmt::i4: { bpp = 4; break; }
		case alice_graphfmt::i8: { bpp = 8; break; }
		case alice_graphfmt::rgb: {
			bpp = 16;
			break;
		}
		default: break;
	}
	return bpp;
}
auto aya::hourai_graphfmt::getBPP(int format) -> int {
	auto format_id = hourai_graphfmt::getID(format);
	if(!hourai_graphfmt::isValid(format)) {
		puts("imgconv::hourai_graphfmt_getBPP(fmt): error: invalid format");
		std::exit(-1);
	}

	int bpp = 0;
	switch(format_id) {
		case hourai_graphfmt::i2: { bpp = 2; break; }
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

