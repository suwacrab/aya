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

