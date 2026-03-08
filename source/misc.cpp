#include <aya.h>
#include <zlib.h>

#define TWIDTAB(x) ( (x&1)|((x&2)<<1)|((x&4)<<2)|((x&8)<<3)|((x&16)<<4)| \
                     ((x&32)<<5)|((x&64)<<6)|((x&128)<<7)|((x&256)<<8)|((x&512)<<9) )
#define TWIDOUT(x, y) ( TWIDTAB((y)) | (TWIDTAB((x)) << 1) )
#define MIN(a, b) ( (a)<(b)? (a):(b) )

namespace SPDCommand {
	enum {
		Raw,
		LZ,
		RLE,
		Diff,
		MaxLength = 1<<13,
	};
};

auto aya::compress(scl::blob& srcblob, bool do_compress) -> scl::blob {
	return srcblob.compress_raw(do_compress);
}
auto aya::compress_spd(scl::blob& srcblob, bool do_compress) -> scl::blob {
	/*
		* .SPD (sadza teahouse packed data):
		* 4 bytes size
		* array of commands
			- d.w: header (0MML:LLLL:LLLL:LLLL) OR
			- d.b: header (1MML:LLLL)
			- d.b: data
			- the length is actually +1!

		* we need the following modes:
			- raw
			- lz
			- rle
			- increasing (word)

		* an rle command takes up 3 bytes by itself. if it was an rle command
		  that only sent 1 byte, it'd be a 1:3 compression ratio.
		* a raw command takes up X+2 bytes, where X is the length of the line.
		  if it only sent one byte, it'd be a 1:3 compression ratio. however,
		  if it sent two, it'd be a 1:4 compression ratio. meanwhile, an rle
		  line would be 2:3.
		* for this reason, rle should only be considered if its length is > the
		  raw command, *and* its length is >= 3.
	*/

	scl::blob blobAll;
	scl::blob blobHeader;
	scl::blob blobData;

	const size_t src_size = srcblob.size();

	// compresss to new blob ----------------------------@/
	for(int src_index=0; src_index<src_size;) {
		size_t lz_len = 0;
		size_t lz_offset = 0;
		size_t raw_len = 0;
		size_t rle_len = 0;

		int line_firstChar = srcblob.at(src_index);

		// check for raw data ---------------------------@/
		int raw_lastChar = line_firstChar;
		for(int i=src_index; i<src_size; i++,raw_len++) {
			if(i == src_index) continue;
			int operand = srcblob.at(i);
			if(operand == raw_lastChar) break;
			raw_lastChar = operand;
		}

		// check for rle data ---------------------------@/
		for(int i=src_index; i<src_size; i++,rle_len++) {
			int operand = srcblob.at(i);
			if(operand != line_firstChar) break;
		}

		// check for lz data ----------------------------@/
		for(size_t startoffset=1; startoffset<=65536; startoffset++) {
			int startpos = src_index - startoffset;
			if(startpos < 0) break;
			size_t cur_len = 0;
			for(int i=0; i<SPDCommand::MaxLength && (i+src_index) < src_size; i++,cur_len++) {
				if(startpos+i >= src_index) break;
				int opSrc = srcblob.at(src_index + i);
				int opBack = srcblob.at(startpos + i);
				if(opSrc != opBack) break;
			}
			if(cur_len > lz_len) {
				lz_len = cur_len;
				lz_offset = startoffset;
			}
		}

		// write command --------------------------------@/
		scl::blob blobCmdData;
		size_t cmd_len = 0;
		size_t cmd_name = 0;
		constexpr int cmdmode_shift = 13;
		if(rle_len >= lz_len && rle_len >= 2) {
			cmd_name = SPDCommand::RLE;
			cmd_len = rle_len;
			blobCmdData.write_u8(line_firstChar);

			src_index += rle_len;
		//	std::printf("rle cmd (%zu)\n",rle_len);
		} else if(lz_len >= 3) {
			if(lz_len > SPDCommand::MaxLength) {
				std::puts("lz len over.");
				std::exit(-1);
			}
			if(lz_offset > 65536) {
				std::puts("lz len over.");
				std::exit(-1);
			}

			cmd_name = SPDCommand::LZ;
			cmd_len = lz_len;
			blobCmdData.write_u16(lz_offset-1);

			src_index += lz_len;
		//	std::printf("lz cmd (%zu)\n",lz_len);
		} else {
			cmd_name = SPDCommand::Raw;
			cmd_len = raw_len;
			for(int i=0; i<raw_len; i++) {
				blobCmdData.write_u8(srcblob.at(src_index + i));
			}

			src_index += raw_len;
		//	std::printf("raw cmd (%zu)\n",raw_len);
		}
	//	std::printf("did cmd (lz=%3zu, rle=%3zu, raw=%3zu)\n", lz_len,rle_len,raw_len );
		if(cmd_len > 32) {
			blobData.write_u16((cmd_len-1) | (cmd_name<<cmdmode_shift));
			blobData.write_blob(blobCmdData);
		} else {
			blobData.write_u8(0x80 | (cmd_len-1) | (cmd_name<<5));
			blobData.write_blob(blobCmdData);
		}
	}

	// combine data -------------------------------------@/
	blobHeader.write_str("SPD");
	blobHeader.write_u32(src_size);

	blobAll.write_blob(blobHeader);
	blobAll.write_blob(blobData);

	return blobAll;
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
	std::string build_date("aya graphic converter");
	build_date += " ver. ";
	build_date += __DATE__;
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

