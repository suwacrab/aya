#include <aya.h>
#include <freeimage.h>

namespace aya {
	CPhoto::CPhoto() {
		m_width = 0;
		m_height = 0;
		m_bmpdata.clear();
	}
	CPhoto::CPhoto(std::string filename,bool paletted, bool opaque_pal) {
		// load image -----------------------------------@/
		auto filetype = FreeImage_GetFileType(filename.c_str(),0);
		auto fbmp = FreeImage_Load(filetype,filename.c_str());

		if(!fbmp) {
			std::printf("aya::CPhoto::CPhoto(fname,pal): error: unable to read file %s\n",
				filename.c_str()
			);
			std::exit(-1);
		}

		if(!paletted) {
			fbmp = FreeImage_ConvertTo32Bits(fbmp);
		}

		if(!fbmp) {
			std::printf("aya::CPhoto::CPhoto(fname,pal): error: unable to convert '%s' to 32-bit\n",
				filename.c_str()
			);
			std::exit(-1);
		}

		// set picture data -----------------------------@/
		m_width = FreeImage_GetWidth(fbmp);
		m_height = FreeImage_GetHeight(fbmp);
		m_bmpdata = std::vector<aya::CColor>(dimensions());
		m_palette = std::array<aya::CColor,256>();
		palet_clear(aya::CColor());
		clear(aya::CColor());

		if(!paletted) {
			RGBQUAD fclr;
			for(int iy=0; iy<height(); iy++) {
				for(int ix=0; ix<width(); ix++) {
					FreeImage_GetPixelColor(fbmp,ix,height()-iy-1,&fclr);
					dot_getRaw(ix,iy) = aya::CColor(
						fclr.rgbReserved,
						fclr.rgbRed,
						fclr.rgbGreen,
						fclr.rgbBlue
					);
				}
			}
		} else {
			RGBQUAD* src_pal = FreeImage_GetPalette(fbmp);
			if(!src_pal) {
				std::printf("aya::CPhoto::CPhoto(fname,pal): error: image %s has no palette\n",
					filename.c_str()
				);
				std::exit(-1);
			}

			// get palette ------------------------------@/
			for(int i=0; i<256; i++) {
				auto fclr = src_pal[i];
				palet_getRaw(i) = aya::CColor(
					0xFF,
					fclr.rgbRed,
					fclr.rgbGreen,
					fclr.rgbBlue
				);
			}
			palet_getRaw(0).a = 0;

			// read from image --------------------------@/
			for(int iy=0; iy<height(); iy++) {
				for(int ix=0; ix<width(); ix++) {
					BYTE index = 0;
					FreeImage_GetPixelIndex(fbmp,ix,height()-iy-1,&index);
					dot_getRaw(ix,iy) = aya::CColor(index);
				}
			}
		}

		FreeImage_Unload(fbmp);
	}
	CPhoto::CPhoto(int newwidth, int newheight) {
		if(newwidth * newheight == 0) {
			std::printf("aya::CPhoto::CPhoto(%4d,%4d): error: bad dimensions\n",
				newwidth,newheight
			);
			std::exit(-1);
		}

		m_width = newwidth;
		m_height = newheight;
		
		m_bmpdata = std::vector<aya::CColor>(dimensions());
		m_palette = std::array<aya::CColor,256>();
		clear(aya::CColor());
		palet_clear(aya::CColor());
	}
	CPhoto::~CPhoto() {
	}

	auto CPhoto::clear(aya::CColor color) -> void {
		for(int iy=0; iy<height(); iy++) {
			for(int ix=0; ix<width(); ix++) {
				dot_getRaw(ix,iy) = color;
			}
		}
	}

	auto CPhoto::palet_clear(aya::CColor color) -> void {
		for(int i=0; i<256; i++) {
			palet_getRaw(i) = color;
		}
	}
	auto CPhoto::palet_getRaw(int pen) -> aya::CColor& {
		return m_palette.at(pen);
	}
	auto CPhoto::palet_getRawC(int pen) const -> const aya::CColor& {
		return m_palette.at(pen);
	}
	auto CPhoto::palet_get(int pen) const -> aya::CColor {
		if(pen < 0 || pen >= 256) {
			std::puts("aya::CPhoto::palet_get(p): error: pen out of range");
			std::exit(-1);
		}
		return palet_getRawC(pen);
	}

	auto CPhoto::dot_getTwiddledIdx(int x,int y) const -> size_t {
		auto z = aya::twiddled_index(x,y,width(),height());
		if(z >= dimensions()) {
			std::puts("aya::CPhoto::dot_getTwiddledIdx(x,y): warning: coord out of range");
			std::exit(-1);
		}
		return z;
	}
	auto CPhoto::dot_inRange(int x, int y) const -> bool {
		if(x < 0 || x >= width()) return false;
		if(y < 0 || y >= height()) return false;
		return true;
	}
	auto CPhoto::dot_setRGB(int x,int y,aya::CColor color) -> void {
		if(!dot_inRange(x,y)) {
			std::printf("aya::CPhoto::dot_setRGB(%4d,%4d,#%08X): warning: coord out of range\n",
				x,y,color.rawdata()
			);
		} else {
			dot_getRaw(x,y) = color;
		}
	}
	auto CPhoto::dot_get(int x,int y) const -> aya::CColor {
		if(!dot_inRange(x,y)) {
			std::printf("aya::CPhoto::dot_get(x,y): error: coord out of range (%3d,%3d)\n",
				x,y
			);
			std::exit(-1);
			return aya::CColor(0,0,0,0);
		}
		return dot_getRawC(x,y);
	}

	auto CPhoto::rect_split(int size_x, int size_y, int count) -> std::vector<std::shared_ptr<CPhoto>> {
		// check if specifiedsize is correct ------------@/
		bool size_invalid = false;
		if((width()%size_x) != 0) size_invalid = true;
		if((height()%size_y) != 0) size_invalid = true;
		if(size_x<= 0 || size_y <= 0) size_invalid = true;

		if(size_invalid) {
			std::printf("aya::CPhoto::rect_split(): error: invalid sub-size (%d,%d)\n",
				size_x,size_y
			);
			std::exit(-1);
		}

		// split into multiple images
		int num_rows = (width() / size_x);
		int num_cols = (height() / size_y);
		int num_images = num_rows * num_cols;
		if(count != -1) num_images = count;

		std::vector<std::shared_ptr<CPhoto>> images;
		
		for(int i=0; i<num_images; i++) {
			int src_x = size_x * (i % (width() / size_x));
			int src_y = size_y * (i / num_rows);
			images.push_back(
				rect_get(src_x,src_y,size_x,size_y)
			);
		}

		return images;
	}
	auto CPhoto::rect_blit(CPhoto& outpic,int sx,int sy,int dx,int dy,int w,int h) const -> void {
		if(w == 0) w = width();
		if(h == 0) h = height();
		auto inrange_xs = dot_inRange(sx,sy) && dot_inRange(sx+w-1,sy);
		auto inrange_ys = dot_inRange(sx,sy+h-1) && dot_inRange(sx+w-1,sy+h-1);
		auto inrange_xd = outpic.dot_inRange(dx,dy) && outpic.dot_inRange(dx+w-1,dy);
		auto inrange_yd = outpic.dot_inRange(dx,dy+h-1) && outpic.dot_inRange(dx+w-1,dy+h-1);

		if( !(inrange_xs && inrange_ys && inrange_xd && inrange_yd) ) {
			std::puts("aya::CPhoto::rect_blit(out,sx,sy,dx,dy,w,h): error: size/pos out of range");
			std::printf("wh: (%d,%d)\n",w,h);
			std::printf("inrange(src): [%d %d] | inrange(dst): [%d %d]\n",
				inrange_xs,inrange_ys,
				inrange_xd,inrange_yd
			);
			std::exit(-1);
		}

		// copy	
		for(int iy=0; iy<h; iy++) {
			for(int ix=0; ix<w; ix++) {
				auto color = dot_getRawC(sx+ix,sy+iy);
				outpic.dot_setRGB(dx+ix,dy+iy,color);
			}
		}
	}
	auto CPhoto::rect_get(int x,int y,int w,int h) const -> std::shared_ptr<CPhoto> {
		if(w==0) w = width();
		if(h==0) h = height();
		auto inrange_x = dot_inRange(x,y) && dot_inRange(x+w-1,y);
		auto inrange_y = dot_inRange(x,y+h-1) && dot_inRange(x+w-1,y+h-1);
		if( !(inrange_x && inrange_y) ) {
			std::puts("aya::CPhoto::rect_get(x,y,w,h): error: size/pos out of range");
			std::exit(-1);
		}

		auto new_pic = std::make_shared<CPhoto>(w,h);
		for(int iy=0; iy<h; iy++) {
			for(int ix=0; ix<w; ix++) {
				auto color = dot_getRawC(x+ix,y+iy);
				new_pic->dot_setRGB(ix,iy,color);
			}
		}
		
		return new_pic;
	}
	auto CPhoto::all_equals(aya::CColor color) const -> bool {
		for(int i=0; i<m_bmpdata.size(); i++) {
			if(m_bmpdata[i].rawdata() != color.rawdata()) return false;
		}
		return true;
	}

	auto CPhoto::hash_get(int flip) const -> uint64_t {
		uint64_t hash = 0x811C9DC5;
		
		int flip_x = (flip>>0)&1;
		int flip_y = (flip>>1)&1;

		int start_x = (flip_x ? width() - 1 : 0);
		int start_y = (flip_y ? height() - 1 : 0);
		int x_delta = (flip_x ? -1 : 1);
		int y_delta = (flip_y ? -1 : 1);

		int x = start_x;
		int y = start_y;

		for(int ly = 0; ly < height(); ly++) {
			x = start_x;
			for(int lx=0; lx < width(); lx++) {
				uint64_t dot = dot_getRawC(x,y).rawdata();
				hash = ((hash ^ dot) * 0x1000193) & 0xFFFFFFFFFFFFFFFF;
				x += x_delta;
			}
			y += y_delta;
		}
		return hash;
	}
	auto CPhoto::hash_getIndexed(int flip) const -> uint64_t {
		uint64_t hash = 0x811C9DC5;
		
		int flip_x = (flip>>0)&1;
		int flip_y = (flip>>1)&1;

		int start_x = (flip_x ? width() - 1 : 0);
		int start_y = (flip_y ? height() - 1 : 0);
		int x_delta = (flip_x ? -1 : 1);
		int y_delta = (flip_y ? -1 : 1);

		int x = start_x;
		int y = start_y;

		for(int ly = 0; ly < height(); ly++) {
			x = start_x;
			for(int lx=0; lx < width(); lx++) {
				uint64_t dot = dot_getRawC(x,y).a;
				hash = ((hash ^ dot) * 0x1000193) & 0xFFFFFFFFFFFFFFFF;
				x += x_delta;
			}
			y += y_delta;
		}
		return hash;
	}

	auto CPhoto::convert_rawPGI(int format) const -> Blob {
		auto format_id = patchu_graphfmt::getID(format);
		Blob blob_bmp;

		switch(format_id) {
			case patchu_graphfmt::i8: {
				for(int iy=0; iy<height(); iy++) {
					for(int ix=0; ix<width(); ix++) {
						dot_getRawC(ix,iy).write_alpha(blob_bmp);
					}
				}		
				break;
			}
			case patchu_graphfmt::rgb565: {
				for(int iy=0; iy<height(); iy++) {
					for(int ix=0; ix<width(); ix++) {
						dot_getRawC(ix,iy).write_rgb565(blob_bmp);
					}
				}
				break;
			}
			case patchu_graphfmt::rgb5a1: {
				for(int iy=0; iy<height(); iy++) {
					for(int ix=0; ix<width(); ix++) {
						dot_getRawC(ix,iy).write_rgb5a1(blob_bmp);
					}
				}
				break;
			}
			case patchu_graphfmt::argb4: {
				for(int iy=0; iy<height(); iy++) {
					for(int ix=0; ix<width(); ix++) {
						dot_getRawC(ix,iy).write_argb4(blob_bmp);
					}
				}
				break;
			}
			case patchu_graphfmt::argb8: {
				for(int iy=0; iy<height(); iy++) {
					for(int ix=0; ix<width(); ix++) {
						dot_getRawC(ix,iy).write_argb8(blob_bmp);
					}
				}
				break;
			}
			default: {
				puts("aya::CPhoto::convert_raw(fmt): error: format not supported ^^;");
				std::exit(-1);
				break;
			}
		}

		return blob_bmp;
	}
	auto CPhoto::convert_raw(int format) const -> Blob {
		auto format_id = marisa_graphfmt::getID(format);
		Blob blob_bmp;

		switch(format_id) {
			case marisa_graphfmt::i8: {
				for(int iy=0; iy<height(); iy++) {
					for(int ix=0; ix<width(); ix++) {
						dot_getRawC(ix,iy).write_alpha(blob_bmp);
					}
				}		
				break;
			}
			case marisa_graphfmt::rgb565: {
				for(int iy=0; iy<height(); iy++) {
					for(int ix=0; ix<width(); ix++) {
						dot_getRawC(ix,iy).write_rgb565(blob_bmp);
					}
				}
				break;
			}
			case marisa_graphfmt::rgb5a1: {
				for(int iy=0; iy<height(); iy++) {
					for(int ix=0; ix<width(); ix++) {
						dot_getRawC(ix,iy).write_rgb5a1(blob_bmp);
					}
				}
				break;
			}
			case marisa_graphfmt::argb4444: {
				for(int iy=0; iy<height(); iy++) {
					for(int ix=0; ix<width(); ix++) {
						dot_getRawC(ix,iy).write_argb4(blob_bmp);
					}
				}
				break;
			}
			default: {
				puts("aya::CPhoto::convert_raw(fmt): error: format not supported ^^;");
				std::exit(-1);
				break;
			}
		}

		return blob_bmp;
	}
	auto CPhoto::convert_rawNGI(int format) const -> Blob {
		auto format_id = narumi_graphfmt::getID(format);
		Blob blob_bmp;

		switch(format_id) {
			case narumi_graphfmt::i4: {
				for(int iy=0; iy<height(); iy++) {
					for(int ix=0; ix<width(); ix += 2) {
						auto dotA = dot_getRawC(ix,iy).a & 0xF;
						auto dotB = dot_getRawC(ix+1,iy).a & 0xF;
						
						blob_bmp.write_u8(dotB | (dotA<<4));
					}
				}		
				break;
			}
			case narumi_graphfmt::i8: {
				for(int iy=0; iy<height(); iy++) {
					for(int ix=0; ix<width(); ix++) {
						dot_getRawC(ix,iy).write_alpha(blob_bmp);
					}
				}		
				break;
			}
			case narumi_graphfmt::rgb: {
				for(int iy=0; iy<height(); iy++) {
					for(int ix=0; ix<width(); ix++) {
						dot_getRawC(ix,iy).write_rgb5a1_sat(blob_bmp,true);
					}
				}
				break;
			}
			default: {
				puts("aya::CPhoto::convert_raw(fmt): error: format not supported ^^;");
				std::exit(-1);
				break;
			}
		}

		return blob_bmp;
	}
	auto CPhoto::convert_rawAGI(int format) const -> Blob {
		auto format_id = alice_graphfmt::getID(format);
		Blob blob_bmp;

		switch(format_id) {
			case alice_graphfmt::i4: {
				for(int iy=0; iy<height(); iy++) {
					for(int ix=0; ix<width(); ix += 2) {
						auto dotA = dot_getRawC(ix,iy).a & 0xF;
						auto dotB = dot_getRawC(ix+1,iy).a & 0xF;
						
						blob_bmp.write_u8(dotA | (dotB<<4));
					}
				}		
				break;
			}
			case alice_graphfmt::i8: {
				for(int iy=0; iy<height(); iy++) {
					for(int ix=0; ix<width(); ix++) {
						dot_getRawC(ix,iy).write_alpha(blob_bmp);
					}
				}		
				break;
			}
			case alice_graphfmt::rgb: {
				for(int iy=0; iy<height(); iy++) {
					for(int ix=0; ix<width(); ix++) {
						dot_getRawC(ix,iy).write_rgb5a1_agb(blob_bmp);
					}
				}
				break;
			}
			default: {
				puts("aya::CPhoto::convert_rawAGI(fmt): error: format not supported ^^;");
				std::exit(-1);
				break;
			}
		}

		return blob_bmp;
	}
	auto CPhoto::convert_twiddled(int format) const -> Blob {
		auto format_id = marisa_graphfmt::getID(format);
		Blob blob_output;
		Blob blob_curdot;

		// bitmap writing fns ---------------------------@/
		switch(format_id) {
			case marisa_graphfmt::i4: {
				//std::vector<uint8_t> bmpbuf(dimensions() / 2);
				for(int iy=0; iy<height(); iy++) {
					for(int ix=0; ix<width(); ix += 2) {
						// ... don't even bother twiddling, i don't know what
						// stupid ass format it's supposed to be in.
						const uint32_t dotA = dot_getRawC(ix,iy).a & 0xF;
						const uint32_t dotB = dot_getRawC(ix+1,iy).a & 0xF;
						const uint32_t Tdot = dotA | (dotB<<4);
						blob_output.write_u8(Tdot);
					//	bmpbuf.push_back(Tdot);
					}
				}
				//blob_output.write_raw(bmpbuf.data(),dimensions() / 2);
				break;
			}
			case marisa_graphfmt::i8: {
				std::vector<uint8_t> bmpbuf(dimensions());
				for(int iy=0; iy<height(); iy++) {
					for(int ix=0; ix<width(); ix++) {
						const auto index = dot_getTwiddledIdx(ix,iy);
						dot_getRawC(ix,iy).write_alpha(blob_curdot);
						bmpbuf[index] = *blob_curdot.data<uint8_t>();
						blob_curdot.reset();
					}
				}
				blob_output.write_raw(bmpbuf.data(),dimensions());
				break;
			}
			case marisa_graphfmt::rgb565: {
				std::vector<uint16_t> bmpbuf(dimensions());
				for(int iy=0; iy<height(); iy++) {
					for(int ix=0; ix<width(); ix++) {
						const auto index = dot_getTwiddledIdx(ix,iy);
						dot_getRawC(ix,iy).write_rgb565(blob_curdot);
						bmpbuf[index] = *blob_curdot.data<uint16_t>();
						blob_curdot.reset();
					}
				}
				blob_output.write_raw(bmpbuf.data(),dimensions() * sizeof(uint16_t));
				break;
			}
			case marisa_graphfmt::rgb5a1: {
				std::vector<uint16_t> bmpbuf(dimensions());
				for(int iy=0; iy<height(); iy++) {
					for(int ix=0; ix<width(); ix++) {
						const auto index = dot_getTwiddledIdx(ix,iy);
						dot_getRawC(ix,iy).write_rgb5a1(blob_curdot);
						bmpbuf[index] = *blob_curdot.data<uint16_t>();
						blob_curdot.reset();
					}
				}
				blob_output.write_raw(bmpbuf.data(),dimensions() * sizeof(uint16_t));
				break;
			}
			case marisa_graphfmt::argb4444: {
				std::vector<uint16_t> bmpbuf(dimensions());
				for(int iy=0; iy<height(); iy++) {
					for(int ix=0; ix<width(); ix++) {
						const auto index = dot_getTwiddledIdx(ix,iy);
						dot_getRawC(ix,iy).write_argb4(blob_curdot);
						bmpbuf[index] = *blob_curdot.data<uint16_t>();
						blob_curdot.reset();
					}
				}
				blob_output.write_raw(bmpbuf.data(),dimensions() * sizeof(uint16_t));
				break;
			}
			default: {
				printf("imgconnv::CPhoto::twiddle(fmt): error: unsupported format (%d)\n",format_id);
				std::exit(-1);
				break;
			}
		}

		return blob_output;
	}
};

