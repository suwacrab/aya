#include <aya.h>
#include <functional>
#include <rapidjson/document.h>

#include <cmath>
#include <map>

constexpr int PGA_TILE_SIZE = 32;
constexpr int PGA_LINE_SIZE = 16;

struct PGAWorkingFrame {
	int duration_ms;
	int width,height;
	int src_x,src_y;
};
struct PGAWorkingTile {
	std::shared_ptr<aya::CPhoto> tile_pic;
	int sheet_x,sheet_y;
	int disp_x,disp_y;
};

// working frames -----------------------------------------------------------@/
auto aya::CWorkingFrame::subframe_get(size_t index) -> aya::CWorkingSubframe& {
	if(index >= subframe_count()) {
		std::printf("aya::CWorkingFrame::subframe_get(): error: index %zu out of range\n",
			index
		);
		std::exit(-1);
	}
	return m_subframes.at(index);
}
aya::CWorkingFrame::CWorkingFrame() {
	m_subframes.clear();
	m_durationMS = 0;
	m_durationFrame = 0;
}

aya::CWorkingSubframe::CWorkingSubframe() {
	m_posX = 0;
	m_posY = 0;
}
aya::CWorkingSubframe::CWorkingSubframe(aya::CPhoto& photo, int pos_x, int pos_y) {
	m_photo = photo;
	m_posX = pos_x;
	m_posY = pos_y;
}

aya::CWorkingFrameList::CWorkingFrameList() {
	m_frames.clear();
}
auto aya::CWorkingFrameList::frame_get(size_t index) -> aya::CWorkingFrame& {
	if(index >= frame_count()) {
		std::printf("aya::CWorkingFrameList::frame_get(): error: index %zu out of range\n",
			index
		);
		std::exit(-1);
	}
	return m_frames.at(index);
}
auto aya::CWorkingFrameList::create_fromAseJSON(aya::CPhoto& baseimage, const std::string& json_filename) -> void {
	m_frames.clear();
	rapidjson::Document jsondoc;

	// load json from file ------------------------------@/
	std::string json_str; {
		std::vector<char> filedata;

		// set file buffer's size
		auto file = std::fopen(json_filename.c_str(),"rb");
		if(!file) {
			std::printf("aya::CWorkingFrameList::create_fromAseJSON(): error: unable to open file %s for reading\n",
				json_filename.c_str()
			);
			std::exit(-1);
		}
		std::fseek(file,0,SEEK_END);
		filedata.resize(std::ftell(file));

		// read file to string
		std::fseek(file,0,SEEK_SET);
		std::fread(filedata.data(),filedata.size(),1,file);
		std::fclose(file);

		filedata.push_back(0); // null terminator
		json_str = std::string(filedata.data());
	}
	jsondoc.Parse(json_str.c_str());
	
	auto json_validate = [&](const std::string& keyname) {
		if(!jsondoc.HasMember(keyname.c_str())) {
			std::printf("aya: validation error: json file missing key %s\n",
				keyname.c_str()
			);
			std::exit(-1);
		}
	};
	
	// create working frame table -----------------------@/
	json_validate("frames");
	json_validate("meta");

	const int num_frames = jsondoc["frames"].Size();

	for(int i=0; i<num_frames; i++) {
		auto& src_frame = jsondoc["frames"][i];
		int duration_ms = src_frame["duration"].GetInt();
		int width = src_frame["frame"]["w"].GetInt();
		int height = src_frame["frame"]["h"].GetInt();
		int src_x = src_frame["frame"]["x"].GetInt();
		int src_y = src_frame["frame"]["y"].GetInt();

		// get duration ---------------------------------@/
		auto duration_secs = (double)(duration_ms) / 1000;
		const double FRAME_TIME = 1.0/60;
		auto duration_frame = static_cast<int>(duration_secs / FRAME_TIME);
		if(duration_frame == 0) {
			printf("aya: warning: frame duration for frame %s[%d] is getting corrected to 1\n",
				src_frame["filename"].GetString(),
				i
			);
			duration_frame = 1;
		}

		/*printf("duration: %4df, %8dms\n",
			duration_frame,
			duration_ms
		);*/

		// get subframe (only one, since aseprite) ------@/
		CPhoto sheetframe(width,height);
		baseimage.rect_blit(sheetframe,
			src_x,src_y,
			0,0,	// destination (0,0)
			width,height
		);

		CWorkingFrame frame;
		frame.m_durationMS = duration_ms;
		frame.m_durationFrame = duration_frame;
		frame.m_subframes.push_back(aya::CWorkingSubframe(sheetframe,0,0));
		m_frames.push_back(frame);
	}
}

// conversion ---------------------------------------------------------------@/
auto aya::CPhoto::convert_filePGA(int format, const std::string& json_filename, bool do_compress) -> Blob {
	Blob out_blob;
	Blob blob_headersection;
	Blob blob_framesection;
	Blob blob_tilesection;
	Blob blob_bmpsection;
	Blob blob_paletsection;
	rapidjson::Document jsondoc;

	std::vector<PGAWorkingFrame> frame_info_wrking;

	const int tilesize = PGA_TILE_SIZE;

	// load json from file ------------------------------@/
	std::string json_str; {
		std::vector<char> filedata;

		// set file buffer's size
		auto file = std::fopen(json_filename.c_str(),"rb");
		if(!file) {
			std::printf("aya::CPhoto::convert_filePGA(): error: unable to open file %s for reading\n",
				json_filename.c_str()
			);
			std::exit(-1);
		}
		std::fseek(file,0,SEEK_END);
		filedata.resize(std::ftell(file));

		// read file to string
		std::fseek(file,0,SEEK_SET);
		std::fread(filedata.data(),filedata.size(),1,file);
		std::fclose(file);

		filedata.push_back(0); // null terminator
		json_str = std::string(filedata.data());
	}
	jsondoc.Parse(json_str.c_str());
	
	auto json_validate = [&](const std::string& keyname) {
		if(!jsondoc.HasMember(keyname.c_str())) {
			std::printf("aya: validation error: json file missing key %s\n",
				keyname.c_str()
			);
			std::exit(-1);
		}
	};
	
	// create working frame table -----------------------@/
	json_validate("frames");
	json_validate("meta");

	const int num_frames = jsondoc["frames"].Size();

	for(int i=0; i<num_frames; i++) {
		auto& src_frame = jsondoc["frames"][i];
		PGAWorkingFrame frame = {
			.duration_ms = src_frame["duration"].GetInt(),
			.width = src_frame["frame"]["w"].GetInt(),
			.height = src_frame["frame"]["h"].GetInt(),
			.src_x = src_frame["frame"]["x"].GetInt(),
			.src_y = src_frame["frame"]["y"].GetInt()
		};
		frame_info_wrking.push_back(frame);
	}
	// write section headers ----------------------------@/
	blob_tilesection.write_str("TIL");
	blob_bmpsection.write_str("BMP");
	blob_paletsection.write_str("PAL");

	// write frames -------------------------------------@/
	for(int f=0; f<num_frames; f++) {
		const auto& wrkframe = frame_info_wrking.at(f);
		
		// create picture -------------------------------@/
		int orig_width = (wrkframe.width/tilesize)*tilesize;
		int orig_height = (wrkframe.height/tilesize)*tilesize;
		if((wrkframe.width%tilesize) != 0) orig_width += tilesize;
		if((wrkframe.height%tilesize) != 0) orig_height += tilesize;

		CPhoto sheetframe(orig_width,orig_height);
		rect_blit(sheetframe,
			wrkframe.src_x,wrkframe.src_y,
			0,0,	// destination (0,0)
			wrkframe.width,wrkframe.height
		);

		// setup frame ----------------------------------@/
		PATCHU_PGAFILE_FRAME fileframe = {};
		fileframe.offset_tile = blob_tilesection.size();
		fileframe.offset_bmp = blob_bmpsection.size();
		auto duration_secs = (double)wrkframe.duration_ms;
		auto duration_frame = static_cast<int>((duration_secs/1000.0) / (1.0/60));
		fileframe.duration_f = duration_frame;
		fileframe.duration_ms = wrkframe.duration_ms;

		// create tiled image ---------------------------@/
		std::vector<PGAWorkingTile> tile_table;
		for(int iy=0; iy<orig_height; iy += tilesize) {
			for(int ix=0; ix<orig_width; ix += tilesize) {
				int idx = tile_table.size();
				int ox = (idx%PGA_LINE_SIZE) * tilesize;
				int oy = (idx/PGA_LINE_SIZE) * tilesize;

				// create tile, only if it has pixels
				auto tile_pic = sheetframe.rect_get(ix,iy,tilesize,tilesize);
				if(!tile_pic->all_equals(aya::CColor())) {
					PGAWorkingTile wrktile = {};
					wrktile.tile_pic = tile_pic;
					wrktile.disp_x = ix;
					wrktile.disp_y = iy;
					wrktile.sheet_x = ox; 
					wrktile.sheet_y = oy;
					tile_table.push_back(wrktile);
				}
			}
		}

		int num_tiles = tile_table.size();
		int num_tilesReal = 0;
		int tilebmp_sizeX = tilesize*PGA_LINE_SIZE;
		int tilebmp_sizeY = static_cast<int>(tilesize * std::ceil((float)(num_tiles)/PGA_LINE_SIZE));
		if(tilebmp_sizeY == 0) tilebmp_sizeY = tilesize;

		CPhoto tilebmp(tilebmp_sizeX,tilebmp_sizeY);
	//	std::printf("f[%3d].tiles: %3d\n",f,num_tiles);
	//	std::printf("f[%3d].tilebmp dims: (%3d,%3d)\n",f,tilebmp.width(),tilebmp.height());
		for(int i=0; i<num_tiles;) {
			// combine several tiles if they have the same Y
			int line_size = 0;
			const auto& start_wrktile = tile_table.at(i);
			auto start_ySheet = start_wrktile.sheet_y;
			auto start_yDisp = start_wrktile.disp_y;

			int x = i;
			while(x < num_tiles) {
				const auto& wrktile = tile_table.at(x);
				if(wrktile.sheet_y != start_ySheet) break;
				if(wrktile.disp_y != start_yDisp) break;
				line_size++;

				auto tile_pic = wrktile.tile_pic;
			//	std::printf("f[%3d].tile[%3d]: blitting to %3d,%3d\n",
			//		f,num_tilesReal,
			//		wrktile.sheet_x,wrktile.sheet_y
			//	);
				tile_pic->rect_blit(tilebmp,
					0,0, // source
					wrktile.sheet_x,wrktile.sheet_y // dest
				);
				x++;
			}
			i = x; // set new index

			// create tile (actually a combination of multiple tiles
			const auto& wrktile = start_wrktile;
			PATCHU_PGAFILE_TILE filetile = {};
			filetile.disp_x = wrktile.disp_x;
			filetile.disp_y = wrktile.disp_y;
			filetile.sheet_x = wrktile.sheet_x; 
			filetile.sheet_y = wrktile.sheet_y;
			filetile.tile_sizex = tilesize * line_size;
			blob_tilesection.write_raw(&filetile,sizeof(filetile));
		//	std::printf("line[f%3d]: sizex=%3d sheet(%4d,%4d) -> disp(%4d,%4d)\n",
		//		f,filetile.tile_sizex,
		//		filetile.sheet_x,filetile.sheet_y,
		//		filetile.disp_x,filetile.disp_y
		//	);
			num_tilesReal++;
		}

		auto tilebmp_blob = tilebmp.convert_rawPGI(format);
		auto tilebmp_blob_cmp = aya::compress(tilebmp_blob,do_compress);

		auto& bmpblob = tilebmp_blob_cmp;
		fileframe.num_tiles = num_tilesReal;
		fileframe.size_bmp = bmpblob.size();
		fileframe.img_w = tilebmp.width();
		fileframe.img_h = tilebmp.height();
		blob_bmpsection.write_blob(bmpblob);
		blob_framesection.write_raw(&fileframe,sizeof(fileframe));
	}

	// create palette -----------------------------------@/
	if(aya::patchu_graphfmt::getBPP(format) <= 8) {
		Blob palet_blob;
		for(int p=0; p<256; p++) {
			palet_get(p).write_argb8(palet_blob);
		}
		auto palet_blobComp = aya::compress(palet_blob,true);
		blob_paletsection.write_u32(true);
		blob_paletsection.write_u32(palet_blobComp.size());
		blob_paletsection.write_blob(palet_blobComp);
	} else {
		blob_paletsection.write_u32(false);
	}

	// create header ------------------------------------@/
	aya::PATCHU_PGAFILE_HEADER header = {};
	header.magic[0] = 'P';
	header.magic[1] = 'G';
	header.magic[2] = 'A';
	header.format_flags = format;
	header.num_frames = num_frames;
	header.tilesize = tilesize;
	header.offset_framesection = sizeof(header);
	header.offset_tilesection = header.offset_framesection + blob_framesection.size();
	header.offset_bmpsection = header.offset_tilesection + blob_tilesection.size();
	header.offset_paletsection = header.offset_bmpsection + blob_bmpsection.size();
	blob_headersection.write_raw(&header,sizeof(header));

	out_blob.write_blob(blob_headersection);
	out_blob.write_blob(blob_framesection);
	out_blob.write_blob(blob_tilesection);
	out_blob.write_blob(blob_bmpsection);
	out_blob.write_blob(blob_paletsection);
	return out_blob;
}
auto aya::CPhoto::convert_filePGI(int format, bool do_compress) -> Blob {
	Blob out_blob;
	
	int width_po2 = aya::conv_po2(width());
	int height_po2 = aya::conv_po2(height());
	int bpp = patchu_graphfmt::getBPP(format);

	CPhoto newpic(width_po2,height_po2);	// new bitmap, po2-sized
	rect_blit(newpic,0,0,0,0);				// copy entire old photo

	// generate bitmap ----------------------------------@/
	size_t bmpsize_orig = 0;
	Blob blob_bmp; {
		Blob temp_bmp;

		auto raw_bmp = convert_raw(format);
		temp_bmp.write_blob(raw_bmp);

		// compile bitmap, but keep orig size -----------@/
		bmpsize_orig = temp_bmp.size();
		auto comp_blob = aya::compress(temp_bmp,do_compress);
		blob_bmp.write_blob(comp_blob);
	}

	// generate palette ---------------------------------@/
	size_t palsize_orig = 0;
	Blob blob_palette;
	if(bpp <= 8) {
		Blob temp_pal;
		size_t palet_size = 256;
		if(bpp == 4) palet_size = 16;
		for(int pen=0; pen<palet_size; pen++) {
			palet_getRaw(pen).write_argb8(temp_pal);
		}

		// compile palette, but keep orig size ----------@/
		palsize_orig = temp_pal.size();
		//auto comp_blob = aya::compress(temp_pal,do_compress);
		blob_palette.write_blob(temp_pal);
	}

	// generate header ----------------------------------@/
	aya::PATCHU_PGIFILE_HEADER header = {};
	header.magic[0] = 'P';
	header.magic[1] = 'G';
	header.magic[2] = 'I';

	header.width = width();
	header.height = height();
	header.width_real = width_po2;
	header.height_real = height_po2;
	header.format_flags = format;
	header.palette_size = blob_palette.size();
	header.palette_size_actual = palsize_orig;
	header.palette_offset = sizeof(PATCHU_PGIFILE_HEADER);
	header.bmpdata_size = blob_bmp.size();
	header.bmpdata_size_actual = bmpsize_orig;
	header.bmpdata_offset = header.palette_offset + blob_palette.size();

	// write bitmap
	out_blob.write_raw(&header,sizeof(header));
	out_blob.write_blob(blob_palette);
	out_blob.write_blob(blob_bmp);
	return out_blob;
}
auto aya::CPhoto::convert_fileMGI(int format, bool do_compress) -> Blob {
	bool do_twiddle = marisa_graphfmt::isTwiddled(format);
	Blob out_blob;
	
	int width_po2 = aya::conv_po2(width());
	int height_po2 = aya::conv_po2(height());
	int bpp = marisa_graphfmt::getBPP(format);

	CPhoto newpic(width_po2,height_po2);	// new bitmap, po2-sized
	rect_blit(newpic,0,0,0,0);				// copy entire old photo

	// check if format's correct ------------------------@/
	if((marisa_graphfmt::getBPP(format) <= 8) && (!do_twiddle)) {
		std::puts("aya: warning: converting to paletted format with non-twiddled data");
	}

	// generate bitmap ----------------------------------@/
	size_t bmpsize_orig = 0;
	Blob blob_bmp; {
		Blob temp_bmp;

		if(do_twiddle) {
			auto twiddled_bmp = newpic.convert_twiddled(format);
			temp_bmp.write_blob(twiddled_bmp);
		} else {
			auto raw_bmp = convert_raw(format);
			temp_bmp.write_blob(raw_bmp);
		}

		// compile bitmap, but keep orig size -----------@/
		bmpsize_orig = temp_bmp.size();
		auto comp_blob = aya::compress(temp_bmp,do_compress);
		blob_bmp.write_blob(comp_blob);
	}

	// generate palette ---------------------------------@/
	size_t palsize_orig = 0;
	Blob blob_palette;
	if(bpp <= 8) {
		Blob temp_pal;
		size_t palet_size = 256;
		if(bpp == 4) palet_size = 16;
		for(int pen=0; pen<palet_size; pen++) {
			palet_getRaw(pen).write_argb8(temp_pal);
		}

		// compile palette, but keep orig size ----------@/
		palsize_orig = temp_pal.size();
		//auto comp_blob = aya::compress(temp_pal,do_compress);
		blob_palette.write_blob(temp_pal);
	}

	// generate header ----------------------------------@/
	aya::MARISA_MGIFILE_HEADER header = {};
	header.magic[0] = 'M';
	header.magic[1] = 'G';
	header.magic[2] = 'I';

	header.width = width();
	header.height = height();
	header.width_real = width_po2;
	header.height_real = height_po2;
	header.format_marisa = format;
	header.palette_size = blob_palette.size();
	header.palette_size_actual = palsize_orig;
	header.palette_offset = sizeof(MARISA_MGIFILE_HEADER);
	header.bmpdata_size = blob_bmp.size();
	header.bmpdata_size_actual = bmpsize_orig;
	header.bmpdata_offset = header.palette_offset + blob_palette.size();

	// write bitmap
	out_blob.write_raw(&header,sizeof(header));
	out_blob.write_blob(blob_palette);
	out_blob.write_blob(blob_bmp);
	return out_blob;
}
auto aya::CPhoto::convert_fileNGA(const aya::CNarumiNGAConvertInfo& info) -> Blob {
//	int format, const std::string& json_filename, bool do_compress) -> Blob {
	
	// validate info struct -----------------------------@/
	const int format = info.format;
	const std::string filename_json = info.filename_json;
	const bool do_compress = info.do_compress;

	const int useroffset_x = info.useroffset_x;
	const int useroffset_y = info.useroffset_y;

	// setup frame list ---------------------------------@/
	Blob out_blob;
	Blob blob_headersection;
	Blob blob_framesection;
	Blob blob_subframesection;
	Blob blob_paletsection;
	Blob blob_bmpsection;
	Blob blob_bmpsection_real;

	aya::CWorkingFrameList framelist;
	framelist.create_fromAseJSON(*this,filename_json);

	const int num_frames = framelist.frame_count();
	const int pad_size = 0x20;

	// write section headers ----------------------------@/
	// bmp section's header is written later, though!
	blob_headersection.write_str("NGA");
	blob_framesection.write_str("FR1");
	blob_subframesection.write_str("FR2");
	blob_paletsection.write_str("PAL");

	size_t subframe_index = 0;

	// write frames -------------------------------------@/
	for(int f=0; f<num_frames; f++) {
		auto& wrkframe = framelist.frame_get(f);
		// write regular frame --------------------------@/
		blob_framesection.write_be_u16(wrkframe.subframe_count());
		blob_framesection.write_be_u16(wrkframe.m_durationFrame);
		blob_framesection.write_be_u32(subframe_index);
		
		// write each subframe --------------------------@/
		for(int sf=0; sf<wrkframe.subframe_count(); sf++) {
			auto subframe = wrkframe.subframe_get(sf);
			auto& subframe_photoOrig = subframe.photo();
			int rounded_width = std::ceil((float)(subframe_photoOrig.width())/8.0)*8;

			// get bitmap data --------------------------@/
			CPhoto subframe_photo(
				rounded_width,
				subframe_photoOrig.height()
			);
			// copy to slightly-bigger photo
			subframe_photoOrig.rect_blit(subframe_photo,0,0,0,0); 

			// write subframe data ----------------------@/
			const int palette_num = 0; // only 1 palette, for now...
			blob_subframesection.write_be_u32(blob_bmpsection.size()/8);
			
			auto bmpblob = subframe_photo.convert_rawNGI(format);
			bmpblob.pad(8,0x00); // pad to next 8 bytes with 0
			blob_bmpsection.write_blob(bmpblob);
			
			blob_subframesection.write_be_u32(bmpblob.size()/8);
			blob_subframesection.write_be_u32(palette_num);
			blob_subframesection.write_be_u16(format);
			blob_subframesection.write_be_u16(rounded_width);
			blob_subframesection.write_be_u16(subframe_photoOrig.width());
			blob_subframesection.write_be_u16(subframe_photo.height());
			blob_subframesection.write_be_u16(subframe.m_posX - useroffset_x);
			blob_subframesection.write_be_u16(subframe.m_posY - useroffset_y);

			if(info.verbose) {
				printf("subframe[%d][%d]: (%4d,%4d (x2=%d))\n",
					f,sf,
					subframe_photoOrig.width(),
					subframe_photo.height(),
					rounded_width
				);
			}
			subframe_index++;
			/*printf("subframe[%2d][%d]: bmpsize=%zu\n",
				f,sf,bmpblob.size()
			);*/
		}
	}

	// create palette -----------------------------------@/
	if(aya::narumi_graphfmt::getBPP(format) <= 8) {
		Blob palet_blob;
		int color_count = 1 << aya::narumi_graphfmt::getBPP(format);
		for(int p=0; p<color_count; p++) {
			palet_get(p).write_rgb5a1_sat(palet_blob,true);
		}
		auto palet_blobComp = aya::compress(palet_blob,do_compress);
		blob_paletsection.write_be_u32(palet_blob.size());
		blob_paletsection.write_be_u32(palet_blobComp.size());
		blob_paletsection.write_blob(palet_blobComp);
	} else {
		blob_paletsection.write_u32(0);
	}

	// fix up bmp section -------------------------------@/
	blob_bmpsection_real.write_str("CEL"); {
		Blob bmpblobComp = aya::compress(blob_bmpsection,do_compress);
		blob_bmpsection_real.write_be_u32(blob_bmpsection.size());
		blob_bmpsection_real.write_be_u32(bmpblobComp.size());
		blob_bmpsection_real.write_blob(bmpblobComp);
	}

	// create header ------------------------------------@/
	blob_framesection.pad(pad_size);
	blob_subframesection.pad(pad_size);
	blob_bmpsection_real.pad(pad_size);
	blob_paletsection.pad(pad_size);
	
	size_t offset_framesection = 0x20;
	size_t offset_subframesection = offset_framesection + blob_framesection.size();
	size_t offset_paletsection = offset_subframesection + blob_subframesection.size();
	size_t offset_bmpsection = offset_paletsection + blob_paletsection.size();

	blob_headersection.write_be_u32(format);
	blob_headersection.write_be_u16(num_frames);
	blob_headersection.write_be_u16(subframe_index);

	blob_headersection.write_be_u32(offset_framesection);
	blob_headersection.write_be_u32(offset_subframesection);
	blob_headersection.write_be_u32(offset_paletsection);
	blob_headersection.write_be_u32(offset_bmpsection);
	blob_headersection.pad(pad_size);

	out_blob.write_blob(blob_headersection);
	out_blob.write_blob(blob_framesection);
	out_blob.write_blob(blob_subframesection);
	out_blob.write_blob(blob_paletsection);
	out_blob.write_blob(blob_bmpsection_real);
	return out_blob;
}
auto aya::CPhoto::convert_fileNGI(const aya::CNarumiNGIConvertInfo& info) -> Blob {
	// validate info struct -----------------------------@/
	const int format = info.format;
	const bool do_compress = info.do_compress;

	const int subimage_xsize = info.subimage_xsize;
	const int subimage_ysize = info.subimage_ysize;

	bool use_subimage = false;

	if(subimage_xsize%8 != 0) {
		std::puts("aya::CPhoto::convert_fileNGI(): error: subimage X size must be multiple of 8!!");
		std::exit(-1);
	}

	if(subimage_xsize==0 && subimage_ysize==0) {
		use_subimage = false;
	} else if(subimage_xsize>0 && subimage_ysize>0) {
		use_subimage = true;
	} else {
		std::printf("aya::CPhoto::convert_fileNGI(): error: bad subimage size (%d,%d)\n",
			subimage_xsize,subimage_ysize
		);
		std::exit(-1);	
	}

	// setup bitmap info --------------------------------@/
	Blob out_blob;
	Blob blob_headersection;
	Blob blob_paletsection;
	Blob blob_bmpsection;
	Blob blob_bmpsection_real;

	const int pad_size = 0x800;
	int subimage_count = 0;
	size_t subimage_datasize = 0;
	const int bitmap_width = width();
	const int bitmap_height = height();
	const int bitmap_widthReal = std::ceil((float)(width())/8.0)*8;

	// write section headers ----------------------------@/
	// bmp section's header is written later, though!
	blob_headersection.write_str("NGI");
	blob_paletsection.write_str("PAL");

	// write frames -------------------------------------@/
	if(use_subimage) {
		auto imagetable = rect_split(subimage_xsize,subimage_ysize);
		for(auto pic : imagetable) {
			auto bmpblob = pic->convert_rawNGI(format);
			blob_bmpsection.write_blob(bmpblob);
			subimage_datasize = bmpblob.size();
		}
	} else {
		// get bitmap data ------------------------------@/
		CPhoto new_photo(
			bitmap_widthReal,
			bitmap_height
		);
		// copy to slightly-bigger photo
		rect_blit(new_photo,0,0,0,0); 

		// write bitmap data ----------------------------@/
		auto bmpblob = new_photo.convert_rawNGI(format);
		blob_bmpsection.write_blob(bmpblob);
	}

	// create palette -----------------------------------@/
	if(aya::narumi_graphfmt::getBPP(format) <= 8) {
		Blob palet_blob;
		int color_count = 1 << aya::narumi_graphfmt::getBPP(format);
		for(int p=0; p<color_count; p++) {
			palet_get(p).write_rgb5a1_sat(palet_blob,true);
		}
		auto palet_blobComp = aya::compress(palet_blob,do_compress);
		blob_paletsection.write_be_u32(palet_blob.size());
		blob_paletsection.write_be_u32(palet_blobComp.size());
		blob_paletsection.write_blob(palet_blobComp);
	} else {
		blob_paletsection.write_u32(0);
	}

	// fix up bmp section -------------------------------@/
	blob_bmpsection_real.write_str("CEL"); {
		Blob bmpblobComp = aya::compress(blob_bmpsection,do_compress);
		blob_bmpsection_real.write_be_u32(blob_bmpsection.size());
		blob_bmpsection_real.write_be_u32(bmpblobComp.size());
		blob_bmpsection_real.write_blob(bmpblobComp);
	}

	// create header ------------------------------------@/
	if(info.verbose) {
		std::printf("CEL section: %.2f K\n",
			((float)blob_bmpsection_real.size()) / 1024.0
		);
	}
	
	blob_bmpsection_real.pad(pad_size);
	blob_paletsection.pad(pad_size);
	
	size_t offset_paletsection = pad_size;
	size_t offset_bmpsection = offset_paletsection + blob_paletsection.size();

	blob_headersection.write_be_u32(format);

	blob_headersection.write_be_u16(bitmap_widthReal);
	blob_headersection.write_be_u16(bitmap_width);
	blob_headersection.write_be_u16(bitmap_height);
	blob_headersection.write_be_u16(subimage_count);
	blob_headersection.write_be_u16(subimage_xsize);
	blob_headersection.write_be_u16(subimage_ysize);
	blob_headersection.write_be_u32(subimage_datasize);
	blob_headersection.write_be_u32(offset_paletsection);
	blob_headersection.write_be_u32(offset_bmpsection);
	blob_headersection.pad(pad_size);

	out_blob.write_blob(blob_headersection);
	out_blob.write_blob(blob_paletsection);
	out_blob.write_blob(blob_bmpsection_real);

	return out_blob;
}
auto aya::CPhoto::convert_fileNGM(const aya::CNarumiNGMConvertInfo& info) -> Blob {
	// validate info struct -----------------------------@/
	const int format = info.format;
	const bool do_compress = info.do_compress;

	int max_numtiles = 1024;
	if(info.is_12bit) max_numtiles <<= 2;

	if(width()%8 != 0) {
		std::puts("aya::CPhoto::convert_fileNGM(): error: image X size must be multiple of 8!!");
		std::exit(-1);
	}

	// since character numbers are always in sizes of $20,
	// regardless of whether VDP2's bpp, the boundary of
	// each tile must be set.
	const int pad_size = 0x800;
	int subimage_count = 0;
	size_t subimage_datasize = 0;
	size_t subimage_boundary = 0;
	if(aya::narumi_graphfmt::getBPP(format) == 4) {
		subimage_boundary = 1;
	} else if(aya::narumi_graphfmt::getBPP(format) == 8) {
		subimage_boundary = 2;
	} else {
		std::puts("aya::CPhoto::convert_fileNGM(): error: unsupported pixel format.");
		std::exit(-1);	
	}

	const int map_width = width() / 8;
	const int map_height = height() / 8;

	// setup bitmap info --------------------------------@/
	Blob out_blob;
	Blob blob_headersection;
	Blob blob_paletsection;
	Blob blob_mapsection;
	Blob blob_mapsection_real;
	Blob blob_bmpsection;
	Blob blob_bmpsection_real;

	// write section headers ----------------------------@/
	// bmp & map section's header is written later, though!
	blob_headersection.write_str("NGM");
	blob_paletsection.write_str("PAL");

	// write frames -------------------------------------@/
	auto imagetable = rect_split(8,8); {
		std::map<uint64_t,size_t> imghash_map;
		std::map<uint64_t,size_t> imghash_mapRealIdx;
		size_t num_processedCel = 0;

		int num_flips = 4;
		if(info.is_12bit) num_flips = 1;

		for(auto srcpic : imagetable) {
			const std::array<uint64_t,4> image_hashes = {
				srcpic->hash_getIndexed(0b00),
				srcpic->hash_getIndexed(0b01),
				srcpic->hash_getIndexed(0b10),
				srcpic->hash_getIndexed(0b11)
			};

			bool found_used = false;
			int tile_index = 0;
			int flip_index = 0;

			for(int fi=0; fi<num_flips; fi++) {
				const uint64_t hash = image_hashes[fi];
				if(imghash_map.count(hash) > 0) {
					flip_index = fi;
					tile_index = imghash_map[hash];
					found_used = true;
					
					if(info.verbose) {
						auto get_tileXY = [=](int idx, int &x, int &y) {
							x = 8 * (idx % (width()/8));
							y = 8 * (idx / (width()/8));
						};
						
						int orig_index = imghash_mapRealIdx[hash];
						int src_x,src_y;
						int cel_x,cel_y;
						get_tileXY(num_processedCel,src_x,src_y);
						get_tileXY(orig_index,cel_x,cel_y);
						if(fi == 0) {
							// printf("tile hit! (%3d,%3d) == tile %3d\n",src_x,src_y,tile_index);
						} else {
							printf("flip hit! (%3d,%3d) == tile %3d (%3d,%3d) [fi=%d]\n",src_x,src_y,orig_index,cel_x,cel_y,fi);
						}
					}
					
					break;
				}
			}

			// write tile to bmp/map
			if(found_used) {
				blob_mapsection.write_be_u16(tile_index | (flip_index<<10));
			} else {
				int index = subimage_count * subimage_boundary;

				if(index > max_numtiles) {
					std::printf(
						"aya::CPhoto::convert_fileNGM(): error: cel count over! (cel count: >=%3d)\n"
						"consider using the 12-bit flag to use more tiles.\n",
						index
					);
					std::exit(-1);
				}

				imghash_map[image_hashes[0]] = index;
				imghash_mapRealIdx[image_hashes[0]] = num_processedCel;
				auto bmpblob = srcpic->convert_rawNGI(format);
				blob_bmpsection.write_blob(bmpblob);
				blob_mapsection.write_be_u16(index);
				subimage_datasize = bmpblob.size();
				subimage_count++;
			}

			num_processedCel++;
		}
	}

	// create palette -----------------------------------@/
	if(aya::narumi_graphfmt::getBPP(format) <= 8) {
		Blob palet_blob;
		int color_count = 1 << aya::narumi_graphfmt::getBPP(format);
		for(int p=0; p<color_count; p++) {
			palet_get(p).write_rgb5a1_sat(palet_blob,false);
		}
		auto palet_blobComp = aya::compress(palet_blob,do_compress);
		blob_paletsection.write_be_u32(palet_blob.size());
		blob_paletsection.write_be_u32(palet_blobComp.size());
		blob_paletsection.write_blob(palet_blobComp);
	} else {
		blob_paletsection.write_u32(0);
	}

	// fix up sections ----------------------------------@/
	blob_mapsection_real.write_str("CHP"); {
		blob_mapsection_real.write_be_u16(map_width);
		blob_mapsection_real.write_be_u16(map_height);
		Blob mapblobComp = aya::compress(blob_mapsection,do_compress);
		blob_mapsection_real.write_be_u32(blob_mapsection.size());
		blob_mapsection_real.write_be_u32(mapblobComp.size());
		blob_mapsection_real.write_blob(mapblobComp);
	}

	blob_bmpsection_real.write_str("CEL"); {
		Blob bmpblobComp = aya::compress(blob_bmpsection,do_compress);
		blob_bmpsection_real.write_be_u32(blob_bmpsection.size());
		blob_bmpsection_real.write_be_u32(bmpblobComp.size());
		blob_bmpsection_real.write_blob(bmpblobComp);
	}

	// create header ------------------------------------@/
	if(info.verbose) {
		std::printf("\tCEL section: %.2f K\n",
			((float)blob_bmpsection_real.size()) / 1024.0
		);
		std::printf("\tCHP section: %.2f K (n.cels == %d)\n",
			((float)blob_mapsection_real.size()) / 1024.0,
			subimage_count
		);
	}
	
	blob_mapsection_real.pad(pad_size);
	blob_bmpsection_real.pad(pad_size);
	blob_paletsection.pad(pad_size);
	
	size_t offset_paletsection = pad_size;
	size_t offset_mapsection = offset_paletsection + blob_paletsection.size();
	size_t offset_bmpsection = offset_mapsection + blob_mapsection_real.size();

	blob_headersection.write_be_u32(format);

	blob_headersection.write_be_u16(width());
	blob_headersection.write_be_u16(height());
	blob_headersection.write_be_u16(subimage_count);
	blob_headersection.write_be_u16(subimage_datasize);
	blob_headersection.write_be_u32(offset_paletsection);
	blob_headersection.write_be_u32(offset_mapsection);
	blob_headersection.write_be_u32(offset_bmpsection);
	blob_headersection.pad(pad_size);

	out_blob.write_blob(blob_headersection);
	out_blob.write_blob(blob_paletsection);
	out_blob.write_blob(blob_mapsection_real);
	out_blob.write_blob(blob_bmpsection_real);

	return out_blob;
}
auto aya::CPhoto::convert_fileAGA(const aya::CAliceAGAConvertInfo& info) -> Blob {
	// validate info struct -----------------------------@/
	const int format = info.format;
	const std::string filename_json = info.filename_json;

	const int useroffset_x = info.useroffset_x;
	const int useroffset_y = info.useroffset_y;
	const int lenient_count = info.lenient_count;

	// setup frame list ---------------------------------@/
	Blob out_blob;
	Blob blob_headersection;
	Blob blob_framesection;
	Blob blob_subframesection;
	Blob blob_paletsection;
	Blob blob_bmpsection;

	aya::CWorkingFrameList framelist;
	framelist.create_fromAseJSON(*this,filename_json);

	const int num_frames = framelist.frame_count();

	// write frames -------------------------------------@/
	size_t subframe_index = 0;
	for(int f=0; f<num_frames; f++) {
		auto& wrkframe = framelist.frame_get(f);
		// write regular frame --------------------------@/
		aya::ALICE_AGAFILE_FRAME fileframe = {};
		fileframe.bmp_size = 0;
		fileframe.bmp_offset = blob_bmpsection.size();
		fileframe.duration_f = wrkframe.m_durationFrame;

		std::vector<aya::ALICE_AGAFILE_SUBFRAME> subframestruct_table;

		// write each subframe --------------------------@/
		for(int sf=0; sf<wrkframe.subframe_count(); sf++) {
			auto subframe = wrkframe.subframe_get(sf);
			auto subframe_photo = subframe.photo();
			if(subframe_photo.width()%8 != 0 || subframe_photo.height()%8 != 0) {
				std::puts("aya::CPhoto::convert_fileNGI(): error: subimage X size must be multiple of 8!!");
				std::exit(-1);
			}
			if(subframe_photo.all_equals(aya::CColor())) {
				continue;
			}

			// setup tile grid --------------------------@/
			auto subframe_tiles = subframe_photo.rect_split(8,8);
			const int grid_width = subframe_photo.width()/8;
			const int grid_height = subframe_photo.height()/8;
			const int grid_area = grid_width * grid_height;
			std::vector<bool> emptygrid;
			std::vector<bool> usedgrid;
			emptygrid.resize(grid_area,false);
			usedgrid.resize(grid_area,false);

			enum AGBShape {
				AGBShape_Square,
				AGBShape_Hori,
				AGBShape_Vert
			};
			struct UsableSize { int x,y; };
			const std::array<UsableSize,12> AGBSizes {{
				{ 1,1 }, { 2,1 }, { 4,1 },
				{ 1,2 }, { 2,2 }, { 4,2 },
				{ 1,4 }, { 2,4 }, { 4,4 }, { 8,4 },
				                  { 4,8 }, { 8,8 }
			}};
			const std::array<int,12> AGBShapes {
				AGBShape_Square,	AGBShape_Hori,		AGBShape_Hori,
				AGBShape_Vert,		AGBShape_Square,	AGBShape_Hori,
				AGBShape_Vert,		AGBShape_Vert,		AGBShape_Square,	AGBShape_Hori,
														AGBShape_Vert,		AGBShape_Square
			};
			const std::array<int,12> AGBSizeIDs {
				0,	0,	1,
				0,	1,	2,
				1,	2,	2,	3,
						3,	3
			};

			for(int i=0; i<grid_area; i++) {
				auto tile = subframe_tiles.at(i);
				if(tile->all_equals(aya::CColor())) {
					emptygrid.at(i) = true;
				}
			}

			auto usedgrid_get = [&](int x, int y) {
				return usedgrid.at(x + y * grid_width);
			};
			auto emptygrid_get = [&](int x, int y) {
				return emptygrid.at(x + y * grid_width);
			};
			auto tilegrid_get = [&](int x, int y) {
				return subframe_tiles.at(x + y * grid_width);
			};

			// get tiles --------------------------------@/
			for(int iy=0; iy<grid_height; iy++) {
				for(int ix=0; ix<grid_width; ix++) {
					// check largest rectangle that can be used
					int largest_area = 0;
					size_t largest_areaIdx = 0;
					for(int i=0; i<AGBSizes.size(); i++) {
						const auto& size = AGBSizes.at(i);
						int area = size.x * size.y;
						if(largest_area > area) continue;
						if(iy + size.y > grid_height) continue;
						if(ix + size.x > grid_width) continue;

						// break out if anything in range is marked as used
						int num_empty = 0;
						bool area_usable = true;
						for(int y=0; y<size.y; y++) {
							for(int x=0; x<size.x; x++) {
								if(usedgrid_get(x+ix,y+iy)) {
									area_usable = false;
									break;
								}
								if(emptygrid_get(x+ix,y+iy)) {
									num_empty++;
								}
								if(num_empty > lenient_count) {
									area_usable = false;
									break;
								}
							}
							if(!area_usable) break;
						}
						if(!area_usable) continue;
						largest_area = area;
						largest_areaIdx = i;
					}

					if(largest_area == 0) continue; // means no rect was found
					const auto& sizeentry = AGBSizes.at(largest_areaIdx);
					const int size_x = sizeentry.x;
					const int size_y = sizeentry.y;
					const size_t bmp_tileOffset = blob_bmpsection.size();
					const size_t bmp_tileNum = bmp_tileOffset / 32;

					// if all the tiles are empty, leave.
					int tiles_allEmpty = true;
					for(int y=0; y<size_y; y++) {
						for(int x=0; x<size_x; x++) {
							// mark area as used
							auto tile = tilegrid_get(x+ix,y+iy);
							if(!tile->all_equals(aya::CColor())) {
								tiles_allEmpty = false;
								break;
							}
						}
					}
					if(tiles_allEmpty) continue;

					// write tile
					for(int y=0; y<size_y; y++) {
						for(int x=0; x<size_x; x++) {
						// mark area as used
							usedgrid_get(x+ix,y+iy) = true;
							auto tile = tilegrid_get(x+ix,y+iy);
							auto bmpblob = tile->convert_rawAGI(format);
							fileframe.bmp_size += bmpblob.size();
							blob_bmpsection.write_blob(bmpblob);
						}
					}
					
					int attr_bpp = 0;
					if(aya::alice_graphfmt::getBPP(format) == 8) {
						attr_bpp = 1;
					}
					int attr = 
						(AGBShapes.at(largest_areaIdx) << 6) |
						(attr_bpp << 5) |
						(AGBSizeIDs.at(largest_areaIdx) << 14);

					aya::ALICE_AGAFILE_SUBFRAME filesubframe = {};
					filesubframe.pos_x = (ix*8) - useroffset_x;
					filesubframe.pos_y = (iy*8) - useroffset_y;
					filesubframe.attr = attr;
					filesubframe.charnum = bmp_tileNum;
					filesubframe.size_xy = 
						(size_x*8) |
						(size_y*8) << 8;

					subframestruct_table.push_back(filesubframe);
					fileframe.subframe_len++;
				}
			}

			if(info.verbose) {
				printf("subframe[%d][%d]: (%4d,%4d)\n",
					f,sf,
					subframe_photo.width(),
					subframe_photo.height()
				);
			}
		}
	
		// write subframe data ----------------------@/
		for(int i=0; i<4; i++) {
			// write offset
			fileframe.subframe_offset[i] = subframe_index;
			// write objects, flipped & nonflipped
			int flip_h = (i&1);
			int flip_v = (i>>1)&1;
			for(const auto &entry_orig : subframestruct_table) {
				aya::ALICE_AGAFILE_SUBFRAME entry = entry_orig;
				const int sizedat = entry.size_xy;
				int size_x = sizedat & 0xFF;
				int size_y = (sizedat >> 8);
				if(flip_h) entry.pos_x = width() - 1 - entry.pos_x - size_x;
				if(flip_v) entry.pos_y = height() - 1 - entry.pos_y - size_y;
				entry.attr |= (i<<12);
				blob_subframesection.write_raw(&entry,sizeof(entry));
				subframe_index++;
				//std::printf("obj (f%d): (%d,%d) [%d,%d]\n",i,entry.pos_x,entry.pos_y,size_x,size_y);
			}
		}
		//std::printf("subframe cnt: %d\n",fileframe.subframe_len);
		//std::printf("frame len: %d\n",fileframe.duration_f);
		blob_framesection.write_raw(&fileframe,sizeof(fileframe));
	}

	// create palette -----------------------------------@/
	if(aya::alice_graphfmt::getBPP(format) <= 8) {
		int color_count = 1 << aya::alice_graphfmt::getBPP(format);
		for(int p=0; p<color_count; p++) {
			palet_get(p).write_rgb5a1_agb(blob_paletsection);
		}
	} else {
		blob_paletsection.write_u32(0);
	}

	// pad sections out ---------------------------------@/
	constexpr int pad_word = 0xAA;
	blob_framesection.pad(16,pad_word); // pad to nearest 16;
	blob_subframesection.pad(16,pad_word); // pad to nearest 16;
	blob_paletsection.pad(16,pad_word); // pad to nearest 16;
	blob_bmpsection.pad(16,pad_word); // pad to nearest 16;

	// create header ------------------------------------@/
	size_t offset_framesection = 48; // header is padded to 48 bytes.
	size_t offset_subframesection = offset_framesection + blob_framesection.size();
	size_t offset_paletsection = offset_subframesection + blob_subframesection.size();
	size_t offset_bmpsection = offset_paletsection + blob_paletsection.size();

	aya::ALICE_AGAFILE_HEADER header = {};
	header.magic[0] = 'A';
	header.magic[1] = 'G';
	header.magic[2] = 'A';
	header.width = width();
	header.height = height();
	header.palet_size = blob_paletsection.size();
	header.frame_count = num_frames;
	header.bitmap_size = blob_bmpsection.size();
	header.offset_framesection = offset_framesection;
	header.offset_subframesection = offset_subframesection;
	header.offset_paletsection = offset_paletsection;
	header.offset_bmpsection = offset_bmpsection;

	blob_headersection.write_raw(&header,sizeof(header));
	blob_headersection.pad(offset_framesection,pad_word);

	out_blob.write_blob(blob_headersection);
	out_blob.write_blob(blob_framesection);
	out_blob.write_blob(blob_subframesection);
	out_blob.write_blob(blob_paletsection);
	out_blob.write_blob(blob_bmpsection);
	return out_blob;
}

