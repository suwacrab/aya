/*
PGA files contain 3 main sections:
	*	header section
	*	frame section
	*	frame tile positioning section
	*	frame bitmap section
*/
#include <aya.h>
#include <functional>
#include <rapidjson/document.h>

constexpr int PGA_TILE_SIZE = 32;
constexpr int PGA_LINE_SIZE = 16;

struct PGAWorkingFrame {
	int duration_ms;
	int width,height;
	int src_x,src_y;
};

auto aya::CPhoto::convert_filePGA(int format, const std::string& json_filename, bool do_compress) -> Blob {
	Blob out_blob;
	Blob blob_headersection;
	Blob blob_framesection;
	Blob blob_tilesection;
	Blob blob_bmpsection;
	rapidjson::Document jsondoc;

	std::vector<PGAWorkingFrame> frame_info_wrking;
	std::vector<aya::PATCHU_PGAFILE_FRAME> list_file_frame;
	std::vector<aya::PATCHU_PGAFILE_TILE> list_file_tiles;

	const int tilesize = PGA_TILE_SIZE;
	
	/*int orig_width = (width()/PGA_TILE_SIZE)*PGA_TILE_SIZE;
	int orig_height = (height()/PGA_TILE_SIZE)*PGA_TILE_SIZE;
	if((orig_width%PGA_TILE_SIZE) != 0) orig_width += PGA_TILE_SIZE;
	if((orig_height%PGA_TILE_SIZE) != 0) orig_height += PGA_TILE_SIZE;

	CPhoto orig_pic(orig_width,orig_height);	// new original sheet, rounded to tiles
	rect_blit(orig_pic,0,0,0,0);				// copy entire original sheet
	*/

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

	// write frames -------------------------------------@/
	for(int f=0; f<num_frames; f++) {
		const auto& wrkframe = frame_info_wrking.at(f);
		
		// create picture
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

		// setup frame
		PATCHU_PGAFILE_FRAME fileframe = {};
		fileframe.offset_tile = blob_tilesection.size();
		fileframe.offset_bmp = blob_bmpsection.size();

		// create tiled image
		int num_tiles = (orig_width/tilesize) * (orig_height/tilesize);
		int tilebmp_sizeX = tilesize*PGA_LINE_SIZE;
		int tilebmp_sizeY = aya::conv_po2(tilesize * (num_tiles/PGA_LINE_SIZE));
		if(tilebmp_sizeY == 1) tilebmp_sizeY = tilesize;
		
		CPhoto tilebmp(tilebmp_sizeX,tilebmp_sizeY);
	//	std::printf("number of tiles: %d\n",num_tiles);
	//	std::printf("orisheet size: (%d,%d)\n",sheetframe.width(),sheetframe.height());
	//	std::printf("tilebmp size:  (%d,%d)\n",tilebmp.width(),tilebmp.height());
		int tile_cnt = 0;
		for(int iy=0; iy<orig_height; iy += tilesize) {
			for(int ix=0; ix<orig_width; ix += tilesize) {
				int ox = (tile_cnt%PGA_LINE_SIZE) * tilesize;
				int oy = (tile_cnt/PGA_LINE_SIZE) * tilesize;
	//			std::printf("src blitpos: (%d,%d)\n",ix,iy);
	//			std::printf("out blitpos: (%d,%d)\n",ox,oy);
				sheetframe.rect_blit(tilebmp,
					ix,iy, // source
					ox,oy, // dest
					tilesize,tilesize // size
				);

				// create tile
				PATCHU_PGAFILE_TILE filetile = {};
				filetile.pos_x = ix;
				filetile.pos_y = iy;
				filetile.src_x = ox;
				filetile.src_y = oy;
				blob_tilesection.write_raw(&filetile,sizeof(filetile));
				tile_cnt += 1;
			}
		}

		auto tilebmp_blob = tilebmp.convert_rawPGI(format);
		auto tilebmp_blob_cmp = aya::compress(tilebmp_blob,true);
		fileframe.num_tiles = tile_cnt;
		fileframe.size_bmp = tilebmp_blob_cmp.size();
		blob_bmpsection.write_blob(tilebmp_blob_cmp);
		blob_framesection.write_raw(&fileframe,sizeof(fileframe));
	}

	// create header ------------------------------------@/
	aya::PATCHU_PGAFILE_HEADER header = {};
	header.magic[0] = 'P';
	header.magic[1] = 'G';
	header.magic[2] = 'A';
	header.format_flags = format;
	header.num_frames = num_frames;
	header.offset_framesection = sizeof(header);
	header.offset_tilesection = header.offset_framesection + blob_framesection.size();
	header.offset_bmpsection = header.offset_tilesection + blob_tilesection.size();
	blob_headersection.write_raw(&header,sizeof(header));

	out_blob.write_blob(blob_headersection);
	out_blob.write_blob(blob_framesection);
	out_blob.write_blob(blob_tilesection);
	out_blob.write_blob(blob_bmpsection);
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

