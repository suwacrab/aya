/*
PGA files contain 4 main sections:
	*	header section
	*	frame section
	*	frame tile positioning section
	*	frame bitmap section

NGA files are the format used for saturn animation conversion. Of course, all
data is big-endian, NOT little-endian.

NGA files contain 5 main sections:
	*	header section
		0x00 | char[4]  | header ("NGA\0")
		0x04 | int      | format
		0x08 | short    | frame count
		0x0A | short    | frame section count
		0x0C | int      | frame section offset
		0x10 | int      | subframe section offset
		0x14 | int      | palette section offset
		0x18 | int      | bitmap section offset
	*	frame section
		0x00 | char[4]  | header ("FR1\0")
		*	then, for each frame, it's the following:
			0x00 | short    | number of subframes frame has
			0x02 | short    | time duration (in frames) this frame will display for
			0x04 | int      | index of first subframe, in subframe section	
	*	subframe section
		0x00 | char[4]  | header ("FR2\0")
		*	then, for each subframe, it's the following:
			0x00 | int      | bmp offset (divided by 8)
			0x04 | int      | bmp size (divided by 8)
			0x08 | int      | palette number
			0x0C | short    | format
			0x0E | short    | bitmap width (rounded up to nearest 8 dots)
			0x10 | short[2] | bitmap dimensions (X,Y)
			0x14 | short[2] | X,Y offset when drawing
	*	palette section
		0x00 | char[4]  | header ("PAL\0")
		0x04 | int      | palette size (uncompressed, 0 if file contains no palette)
		*	then, only if the file has a palette, the following:
			0x08 | int      | palette size (compressed)
			0x0C | short[]  | palette data (zlib-compressed)
	*	bitmap section
		0x00 | char[4]  | header ("CEL\0")
		0x04 | int      | bitmap size (uncompressed)
		0x08 | int      | bitmap size (compressed)
		0x0C | char[]   | bitmap data (zlib-compressed)

NGI files contain 3 sections:
	*	header section
		0x00 | char[4]  | header ("NGI\0")
		0x04 | int      | format
		0x08 | short    | bitmap width (rounded up to nearest 8 dots)
		0x0A | short[2] | bitmap dimensions (X,Y)
		0x0E | short    | sub-image count
		0x10 | short[2] | sub-image dimensions
		0x14 | int      | size of each sub-image
		0x18 | int      | palette section offset
		0x1C | int      | bitmap section offset
	*	palette section
		0x00 | char[4]  | header ("PAL\0")
		0x04 | int      | palette size (uncompressed, 0 if file contains no palette)
		*	then, only if the file has a palette, the following:
			0x08 | int      | palette size (compressed)
			0x0C | short[]  | palette data (zlib-compressed)
	*	bitmap section
		0x00 | char[4]  | header ("CEL\0")
		0x04 | int      | bitmap size (uncompressed)
		0x08 | int      | bitmap size (compressed)
		0x0C | char[]   | bitmap data (zlib-compressed)
*/
#include <aya.h>
#include <functional>
#include <rapidjson/document.h>

#include <cmath>

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

	if(info.verbose) {
		std::printf("subimage used: %s\n", use_subimage? "yes" : "no");
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

