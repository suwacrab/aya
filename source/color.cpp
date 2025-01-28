#include <aya.h>

namespace aya {
	void CColor::write_alpha(Blob& out_blob) const {
		out_blob.write_u8(a);
	}
	void CColor::write_argb4(Blob& out_blob) const {
		const auto short_r = ((uint32_t)r)>>4;
		const auto short_g = ((uint32_t)g)>>4;
		const auto short_b = ((uint32_t)b)>>4;
		const auto short_a = ((uint32_t)a)>>4;
		out_blob.write_u16(short_b | (short_g<<4) | (short_r<<8) | (short_a<<12));
	}
	void CColor::write_argb8(Blob& out_blob) const {
		out_blob.write_u8(b);
		out_blob.write_u8(g);
		out_blob.write_u8(r);
		out_blob.write_u8(a);
	}
	void CColor::write_rgb565(Blob& out_blob) const {
		const auto short_r = ((uint32_t)r)>>3;
		const auto short_g = ((uint32_t)g)>>2;
		const auto short_b = ((uint32_t)b)>>3;
		out_blob.write_u16(short_b | (short_g<<5) | (short_r<<11));
	}
	void CColor::write_rgb5a1_sat(Blob& out_blob,bool msb) const {
		const auto short_r = ((uint32_t)r)>>3;
		const auto short_g = ((uint32_t)g)>>3;
		const auto short_b = ((uint32_t)b)>>3;
		uint16_t num = short_r | (short_g<<5) | (short_b<<10);
		if(msb) num |= 0x8000;
		out_blob.write_be_u16(num);
	}
	void CColor::write_rgb5a1(Blob& out_blob,int test) const {
		uint32_t short_a = 1;
		const auto short_r = ((uint32_t)r)>>3;
		const auto short_g = ((uint32_t)g)>>3;
		const auto short_b = ((uint32_t)b)>>3;
		if(a <= test) short_a = 0;
		out_blob.write_u16(short_b | (short_g<<5) | (short_r<<10) | (short_a<<15));
	}
}

