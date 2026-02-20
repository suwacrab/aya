#include <aya.h>
#include <functional>
#include <rapidjson/document.h>
#include <tinyxml.h>

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
auto aya::CWorkingFrameList::create_fromAseJSON(aya::CPhoto& baseimage, const std::string& json_filename, CWorkingFrameCreateInfo createinfo) -> void {
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

	std::vector<aya::CWorkingFrame> framebuf;
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
		framebuf.push_back(frame);
	}

	m_frames = framebuf;
}

aya::CAGBSubframe::CAGBSubframe() {
	m_posX = 0;
	m_posY = 0;
	m_agbSizeID = -1;
	m_agbShapeID = -1;
}
aya::CAGBSubframeList::CAGBSubframeList() {
	m_subframes.clear();
}
aya::CAGBSubframeList::CAGBSubframeList(aya::CPhoto& basephoto,int lenient_count) {
	m_subframes.clear();
	/*
	 * divide each subframe into AGB subframes.
	 * before we can do that, we need a used-tile grid to know which areas
	 * of the image are empty. after that, we build a list of coordinates
	 * and sizes for each image.
	*/

	if(basephoto.width()%8 != 0 || basephoto.height()%8 != 0) {
		std::puts("aya::CAGBSubframeList::CAGBSubframeList(): error: subimage x/y size must be multiple of 8!!");
		std::exit(-1);
	}

	// setup tile grid ----------------------------------@/
	auto subframe_tiles = basephoto.rect_split(8,8);
	const int grid_width = basephoto.width()/8;
	const int grid_height = basephoto.height()/8;
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
		AGBShape_Square,  AGBShape_Hori,    AGBShape_Hori,
		AGBShape_Vert,    AGBShape_Square,  AGBShape_Hori,
		AGBShape_Vert,    AGBShape_Vert,    AGBShape_Square, AGBShape_Hori,
		                                    AGBShape_Vert,   AGBShape_Square
	};
	const std::array<int,12> AGBSizeIDs {
		0,  0,  1,
		0,  1,  2,
		1,  2,  2,  3,
		        3,  3
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


	// add subframes ------------------------------------@/
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
		//	const size_t bmp_tileOffset = blob_bmpsection.size();
		//	const size_t bmp_tileNum = bmp_tileOffset / 32;

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

			// write subframe 
			for(int y=0; y<size_y; y++) {
				for(int x=0; x<size_x; x++) {
					// mark area as used
					usedgrid_get(x+ix,y+iy) = true;
				}
			}
			
			aya::CPhoto subframe_photo(size_x*8,size_y*8);
			basephoto.rect_blit(subframe_photo,
				ix*8,iy*8,
				0,0,
				size_x*8,size_y*8
			);
			aya::CAGBSubframe subframe;
			subframe.m_photo = subframe_photo;
			subframe.m_posX = ix*8;
			subframe.m_posY = iy*8;
			subframe.m_sizeX = subframe_photo.width();
			subframe.m_sizeY = subframe_photo.height();
			subframe.m_agbSizeID = AGBSizeIDs.at(largest_areaIdx);
			subframe.m_agbShapeID = AGBShapes.at(largest_areaIdx);

			m_subframes.push_back(subframe);
		}
	}
}

// edge animation -----------------------------------------------------------@/
aya::CEdgeAnimPattern::CEdgeAnimPattern() {
	m_name.clear();
	m_usedPhotos.clear();
	m_frames.clear();
}

aya::CEdgeAnim::CEdgeAnim() {
	m_patterns.clear();
	m_photoList.clear();
}
aya::CEdgeAnim::CEdgeAnim(const std::string& filename_xml) {
	// loading XML file ---------------------------------@/
	std::printf("loading from file %s\n",filename_xml.c_str());
	TiXmlDocument xmldoc( filename_xml );
	if(!xmldoc.LoadFile()) {
		std::puts("aya::CEdgeAnim::CEdgeAnim(): error: unable to load XML file");
		std::exit(-1);
	}
	TiXmlHandle hXML(&xmldoc);

	// misc. fns ----------------------------------------@/
	auto intlist_parse = [&](const std::string& basetext) {
		std::stringstream ss(basetext);
		std::vector<std::string> list_str;
		std::vector<int> list_num;

		while(ss.good()) {
			std::string substr;
			std::getline(ss,substr,',');
			list_str.push_back(substr);
		}
		for(auto str : list_str) {
			list_num.push_back(std::stoi(str));
		}

		return list_num;
	};

	std::puts("xml loaded");

	// setup patterns -----------------------------------@/
	{
		auto hElemBase = hXML.FirstChild("CCaptureData").FirstChild("Pattern");
		
		TiXmlElement* pElemPattern = hElemBase.Element();

		for(; pElemPattern; pElemPattern = pElemPattern->NextSiblingElement()) {
			auto hElemPattern = TiXmlHandle(pElemPattern);

			aya::CEdgeAnimPattern pattern;
			pattern.m_name = std::string(hElemPattern.FirstChild("Name").Element()->GetText());
			
			std::map<int,bool> pattern_imgMap;

			// get frames -------------------------------@/
			TiXmlElement* pElemFrame = hElemPattern.FirstChild("Frame").Element();
			for(; pElemFrame; pElemFrame = pElemFrame->NextSiblingElement()) {
				auto hElemFrame = TiXmlHandle(pElemFrame);
				aya::CEdgeAnimFrame frame;

				frame.m_name = std::string(hElemFrame.FirstChild("Name").Element()->GetText());
				frame.m_delayFrame = std::stoi( std::string(hElemFrame.FirstChild("Delay").Element()->GetText()) );
				auto frame_destposStr = std::string(hElemFrame.FirstChild("DestPos").Element()->GetText());
				auto frame_destpos = intlist_parse( frame_destposStr );

				std::map<int,bool> frame_imgMap;

				// get parts ----------------------------@/
				TiXmlElement* pElemPart = hElemFrame.FirstChild("Part").Element();
				for(; pElemPart; pElemPart = pElemPart->NextSiblingElement()) {
					auto hElemPart = TiXmlHandle(pElemPart);
					aya::CEdgeAnimPart part;

					// srcrect is x1,y1,x2,y2.
					// it has to be converted to x,y,w,h.
					auto srcrect_str = std::string(hElemPart.FirstChild("SrcRect").Element()->GetText());
					auto srcrect = intlist_parse(srcrect_str);
					auto part_destposStr = std::string(hElemPart.FirstChild("DestPos").Element()->GetText());
					auto part_destpos = intlist_parse(part_destposStr);

					// load photo -----------------------@/
					auto photoBase_filename = std::string(hElemPart.FirstChild("SrcImagePath").Element()->GetText());
					aya::CPhoto photoBase;
					if(m_photoBaseFilenames.contains(photoBase_filename)) {
						photoBase = m_photoBaseFilenames[photoBase_filename];
					} else {
						photoBase = aya::CPhoto( photoBase_filename,true );
						m_photoBaseFilenames[photoBase_filename] = photoBase;
					}

					aya::CPhoto photoPart(srcrect.at(2),srcrect.at(3));
					photoBase.rect_blit(photoPart,
						srcrect.at(0),srcrect.at(1),
						0,0,
						srcrect.at(2),srcrect.at(3)
					);
					auto photoPartHash = photoPart.hash_getIndexed(0);
					auto photoPartIdxOpt = photo_exists(photoPartHash);
					int photoPartIdx = -1;

					if(!photoPartIdxOpt.has_value()) {
						photoPartIdx = m_photoList.size();
						m_photoList.push_back(photoPart);
					} else {
						photoPartIdx = photoPartIdxOpt.value();
					}

					if(!frame_imgMap.contains(photoPartIdx)) {
						frame_imgMap[photoPartIdx] = true;
						frame.m_usedPhotos.push_back(photoPartIdx);
					}
					if(!pattern_imgMap.contains(photoPartIdx)) {
						pattern_imgMap[photoPartIdx] = true;
						pattern.m_usedPhotos.push_back(photoPartIdx);
					}

					// add part -------------------------@/
					part.m_posX = frame_destpos.at(0) + part_destpos.at(0);
					part.m_posY = frame_destpos.at(1) + part_destpos.at(1);
					part.m_srcX = srcrect.at(0);
					part.m_srcY = srcrect.at(1);
					part.m_imgID = photoPartIdx;
					frame.m_parts.push_back(part);

				//	std::printf("\tadded part (imgid=%3d)\n",part.m_imgID);
				}
				/*
				std::printf("added frame[%2zu]: delay=%3d, pos=(%3d,%3d), (%s)\n",
					pattern.m_frames.size(),
					frame.m_delayFrame,
					frame_destpos.at(0),
					frame_destpos.at(1),
					frame.m_name.c_str()
				);
				*/
				pattern.m_frames.push_back(frame);
			}
			
			m_patterns.push_back(pattern);
			// std::printf("added pattern (%s)\n",pattern.m_name.c_str());
		}
	}
	std::puts("iterated thru");
}

auto aya::CEdgeAnim::photo_get(int id) -> aya::CPhoto& {
	if(id < 0 || id >= m_photoList.size()) {
		std::printf("aya::CEdgeAnim::photo_get(): error: invalid ID (%d)\n",
			id
		);
		std::exit(-1);
	}
	return m_photoList.at(id);
}
auto aya::CEdgeAnim::photo_exists(uint64_t hash) -> std::optional<int> {
	int id = -1;
	for(int i=0; i<m_photoList.size(); i++) {
		auto& photo = m_photoList.at(i);
		if(photo.hash_getIndexed(0b00) == hash) {
			id = i;
			break;
		}
	}

	if(id != -1) {
		return std::optional<int>(id);
	} else {
		return std::optional<int>();
	}
}

// conversion ---------------------------------------------------------------@/
auto aya::CPhoto::convert_filePGA(int format, const std::string& json_filename, bool do_compress) -> scl::blob {
	scl::blob out_blob;
	scl::blob blob_headersection;
	scl::blob blob_framesection;
	scl::blob blob_tilesection;
	scl::blob blob_bmpsection;
	scl::blob blob_paletsection;
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
		scl::blob palet_blob;
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
auto aya::CPhoto::convert_filePGI(int format, bool do_compress) -> scl::blob {
	scl::blob out_blob;
	
	int width_po2 = aya::conv_po2(width());
	int height_po2 = aya::conv_po2(height());
	int bpp = patchu_graphfmt::getBPP(format);

	CPhoto newpic(width_po2,height_po2);	// new bitmap, po2-sized
	rect_blit(newpic,0,0,0,0);				// copy entire old photo

	// generate bitmap ----------------------------------@/
	size_t bmpsize_orig = 0;
	scl::blob blob_bmp; {
		scl::blob temp_bmp;

		auto raw_bmp = convert_raw(format);
		temp_bmp.write_blob(raw_bmp);

		// compile bitmap, but keep orig size -----------@/
		bmpsize_orig = temp_bmp.size();
		auto comp_blob = aya::compress(temp_bmp,do_compress);
		blob_bmp.write_blob(comp_blob);
	}

	// generate palette ---------------------------------@/
	size_t palsize_orig = 0;
	scl::blob blob_palette;
	if(bpp <= 8) {
		scl::blob temp_pal;
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

auto aya::CPhoto::convert_fileMGI(int format, bool do_compress) -> scl::blob {
	bool do_twiddle = marisa_graphfmt::isTwiddled(format);
	scl::blob out_blob;
	
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
	scl::blob blob_bmp; {
		scl::blob temp_bmp;

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
	scl::blob blob_palette;
	if(bpp <= 8) {
		scl::blob temp_pal;
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

auto aya::CPhoto::convert_fileNGA(const aya::CNarumiNGAConvertInfo& info) -> scl::blob {
//	int format, const std::string& json_filename, bool do_compress) -> scl::blob {
	
	// validate info struct -----------------------------@/
	const int format = info.format;
	const std::string filename_json = info.filename_json;
	const bool do_compress = info.do_compress;

	const int useroffset_x = info.useroffset_x;
	const int useroffset_y = info.useroffset_y;

	// setup frame list ---------------------------------@/
	scl::blob out_blob;
	scl::blob blob_headersection;
	scl::blob blob_framesection;
	scl::blob blob_subframesection;
	scl::blob blob_paletsection;
	scl::blob blob_bmpsection;
	scl::blob blob_bmpsection_real;

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
		scl::blob palet_blob;
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
		scl::blob bmpblobComp = aya::compress(blob_bmpsection,do_compress);
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
auto aya::CPhoto::convert_fileNGI(const aya::CNarumiNGIConvertInfo& info) -> scl::blob {
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
	scl::blob out_blob;
	scl::blob blob_headersection;
	scl::blob blob_paletsection;
	scl::blob blob_bmpsection;
	scl::blob blob_bmpsection_real;

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
		scl::blob palet_blob;
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
		scl::blob bmpblobComp = aya::compress(blob_bmpsection,do_compress);
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
auto aya::CPhoto::convert_fileNGM(const aya::CNarumiNGMConvertInfo& info) -> scl::blob {
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
	scl::blob out_blob;
	scl::blob blob_headersection;
	scl::blob blob_paletsection;
	scl::blob blob_mapsection;
	scl::blob blob_mapsection_real;
	scl::blob blob_bmpsection;
	scl::blob blob_bmpsection_real;

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
						auto get_tileXY = [&](int idx, int &x, int &y) {
							x = 8 * (idx % (this->width()/8));
							y = 8 * (idx / (this->width()/8));
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
		scl::blob palet_blob;
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
		scl::blob mapblobComp = aya::compress(blob_mapsection,do_compress);
		blob_mapsection_real.write_be_u32(blob_mapsection.size());
		blob_mapsection_real.write_be_u32(mapblobComp.size());
		blob_mapsection_real.write_blob(mapblobComp);
	}

	blob_bmpsection_real.write_str("CEL"); {
		scl::blob bmpblobComp = aya::compress(blob_bmpsection,do_compress);
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

auto aya::CPhoto::convert_fileAGA(const aya::CAliceAGAConvertInfo& info) -> scl::blob {
	// validate info struct -----------------------------@/
	const int format = info.format;
	const std::string filename_json = info.filename_json;

	const int useroffset_x = info.useroffset_x;
	const int useroffset_y = info.useroffset_y;
	const int lenient_count = info.lenient_count;

	// setup frame list ---------------------------------@/
	scl::blob out_blob;
	scl::blob blob_headersection;
	scl::blob blob_framesection;
	scl::blob blob_subframesection;
	scl::blob blob_paletsection;
	scl::blob blob_bmpsection;

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
			if(subframe_photo.all_equals(aya::CColor())) {
				continue;
			}

			// split original subframe into multiple ----@/
			auto agb_subframeList = CAGBSubframeList(subframe_photo,lenient_count);
			for(auto agb_subframe : agb_subframeList.m_subframes) {
				const size_t bmp_tileOffset = blob_bmpsection.size();
				const size_t bmp_tileNum = bmp_tileOffset / 32;

				// convert cels -------------------------@/
				auto subframe_cels = agb_subframe.photo().rect_split(8,8);
				int num_cels = subframe_cels.size();
				for(auto cel : subframe_cels) {
					auto bmpblob = cel->convert_rawAGI(format);
					fileframe.bmp_size += bmpblob.size();
					blob_bmpsection.write_blob(bmpblob);
				}

				// create attribute ---------------------@/
				int attr_bpp = 0;
				if(aya::alice_graphfmt::getBPP(format) == 8) {
					attr_bpp = 1;
				}
				int attr = 
					(agb_subframe.m_agbShapeID << 6) |
					(attr_bpp << 5) |
					(agb_subframe.m_agbSizeID << 14);

				// create file subframe -----------------@/
				aya::ALICE_AGAFILE_SUBFRAME filesubframe = {};
				filesubframe.pos_x = agb_subframe.m_posX - useroffset_x;
				filesubframe.pos_y = agb_subframe.m_posY - useroffset_y;
				filesubframe.attr = attr;
				filesubframe.charnum = bmp_tileNum;
				filesubframe.size_xy = 
					(agb_subframe.m_sizeX) |
					(agb_subframe.m_sizeY) << 8;
				filesubframe.charcnt = num_cels;

				subframestruct_table.push_back(filesubframe);
				fileframe.subframe_len++;
			}

			// print verbose output ---------------------@/
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
				if(flip_h) entry.pos_x = (-entry.pos_x) - size_x - 1;
				if(flip_v) entry.pos_y = (-entry.pos_y) - size_y - 1;
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
	header.format_flags = format;
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
auto aya::convert_fileAGE(const std::string& filename_xml, const aya::CAliceAGEConvertInfo& info) -> scl::blob {
	// validate info struct -----------------------------@/
	const int format = info.format;
	const int pad_word = 0xAA;
	const int pad_size = 32;

	auto edgeanim = aya::CEdgeAnim(filename_xml);

	scl::blob blob_all;
	scl::blob blob_segHeader;
	scl::blob blob_segLoaddesc;
	scl::blob blob_segPattern;
	scl::blob blob_segStrings;
	scl::blob blob_segFrame;
	scl::blob blob_segPart;
	scl::blob blob_segBmp;
	scl::blob blob_segPalet;
	
	size_t numtotal_frames = 0;
	size_t numtotal_loaddesc = 0;
	size_t numtotal_cels = 0;
	size_t numtotal_parts = 0;

	// load cels ----------------------------------------@/
	std::vector<std::vector<aya::ALICE_AGEFILE_PART>> part_table;
	std::vector<size_t> part_tableCelSize;
	std::vector<size_t> part_tableCelOffset;
	for(auto partphoto : edgeanim.m_photoList) {
		part_tableCelOffset.push_back(numtotal_cels * 32);
		auto agb_subframeList = CAGBSubframeList(partphoto,info.lenient_count);
		
		std::vector<aya::ALICE_AGEFILE_PART> filepart_list;

		size_t cel_size = 0;
		size_t cel_id = 0;
		for(auto agb_subframe : agb_subframeList.m_subframes) {
			// get attribute ----------------------------@/
			int attr_bpp = (aya::alice_graphfmt::getBPP(format) == 8) ? 1 : 0;

			int attr = 
				(agb_subframe.m_agbShapeID << 6) |
				(attr_bpp << 5) |
				(agb_subframe.m_agbSizeID << 14);

			// create file part -------------------------@/
			auto& subframephoto = agb_subframe.photo();
			aya::ALICE_AGEFILE_PART filepart;
			filepart.pos_x = agb_subframe.m_posX;
			filepart.pos_y = agb_subframe.m_posY;
			filepart.attr = attr;
			filepart.cel_idPerFrame = cel_id;
			filepart.cel_idPerAnim = cel_id;
			filepart.cel_idPerBank = numtotal_cels;
			filepart.size_xy = 
				(agb_subframe.m_sizeX) |
				(agb_subframe.m_sizeY) << 8;

			// write cels -------------------------------@/
			auto celtable = subframephoto.rect_split(8,8);
			for(auto cel : celtable) {
				auto bmpblob = cel->convert_rawAGI(format);
				blob_segBmp.write_blob(bmpblob);
				cel_id += 1;
				numtotal_cels += 1;
				cel_size += bmpblob.size();
			}

			filepart_list.push_back(filepart);
		}

		part_table.push_back(filepart_list);
		part_tableCelSize.push_back(cel_size);
	}

	// setup patterns ---------------------------------@/
	for(auto pattern : edgeanim.m_patterns) {
		aya::ALICE_AGEFILE_PATTERN filepattern = {};
		filepattern.frame_idx = numtotal_frames;
		filepattern.frame_count = pattern.m_frames.size();
		filepattern.name_offset = blob_segStrings.size();
		blob_segStrings.write_str(pattern.name());

		// write frames ---------------------------------@/
		for(auto frame : pattern.m_frames) {
			aya::ALICE_AGEFILE_FRAME fileframe = {};
			fileframe.delay = frame.m_delayFrame;
			fileframe.name_offset = blob_segStrings.size();
			blob_segStrings.write_str(frame.name());

			// queue up parts to write ------------------@/
			std::vector<aya::ALICE_AGEFILE_PART> partqueue;
			for(auto part : frame.m_parts) {
				auto filepart_list = part_table.at(part.m_imgID);
				
				for(auto filepart : filepart_list) {
					filepart.pos_x += part.m_posX;
					filepart.pos_y += part.m_posY;
					partqueue.push_back(filepart);
				}
			}

			// flip parts -------------------------------@/
			fileframe.part_count = partqueue.size();
			for(int i=0; i<4; i++) {
				// write offset
				fileframe.part_idx[i] = numtotal_parts;

				// write objects, flipped & nonflipped
				int flip_h = (i&1);
				int flip_v = (i>>1)&1;
				for(const auto &entry_orig : partqueue) {
					aya::ALICE_AGEFILE_PART entry = entry_orig;
					const int sizedat = entry.size_xy;
					int size_x = sizedat & 0xFF;
					int size_y = (sizedat >> 8);
					if(flip_h) entry.pos_x = (-entry.pos_x) - size_x - 1;
					if(flip_v) entry.pos_y = (-entry.pos_y) - size_y - 1;
					entry.attr |= (i<<12);
					
					blob_segPart.write_raw(&entry,sizeof(entry));
					numtotal_parts++;
				//	std::printf("obj (f%d): (%d,%d) [%d,%d]\n",i,entry.pos_x,entry.pos_y,size_x,size_y);
				}
			}

			// setup load desc --------------------------@/
			fileframe.loaddesc_PF_idx = numtotal_loaddesc;
			fileframe.loaddesc_PF_count = 0;
			fileframe.loaddesc_PF_totalsize = 0;
			
			for(auto imgID : frame.m_usedPhotos) {
				auto celsize = part_tableCelSize.at(imgID);
				auto celoffset = part_tableCelOffset.at(imgID);
				fileframe.loaddesc_PF_count++;
				fileframe.loaddesc_PF_totalsize += celsize / 32;

				aya::ALICE_AGEFILE_LOADDESC fileloaddesc_PF = {};
				fileloaddesc_PF.size = celsize;
				fileloaddesc_PF.src_celOffset = celoffset;
				blob_segLoaddesc.write_raw(&fileloaddesc_PF,sizeof(fileloaddesc_PF));
				numtotal_loaddesc++;
			}

			blob_segFrame.write_raw(&fileframe,sizeof(fileframe));
		}

		// setup load desc --------------------------@/
		filepattern.loaddesc_PP_idx = numtotal_loaddesc;
		filepattern.loaddesc_PP_count = 0;
		filepattern.loaddesc_PP_totalsize = 0;
		
		for(auto imgID : pattern.m_usedPhotos) {
			auto celsize = part_tableCelSize.at(imgID);
			auto celoffset = part_tableCelOffset.at(imgID);
			filepattern.loaddesc_PP_count++;
			filepattern.loaddesc_PP_totalsize += celsize / 32;

			aya::ALICE_AGEFILE_LOADDESC fileloaddesc_PF = {};
			fileloaddesc_PF.size = celsize;
			fileloaddesc_PF.src_celOffset = celoffset;
			blob_segLoaddesc.write_raw(&fileloaddesc_PF,sizeof(fileloaddesc_PF));
			numtotal_loaddesc++;
		}

		numtotal_frames += pattern.m_frames.size();
		blob_segPattern.write_raw(&filepattern,sizeof(filepattern));
	}

	// pad data -----------------------------------------@/
	blob_segLoaddesc.pad(pad_size,pad_word);
	blob_segPattern.pad(pad_size,pad_word);
	blob_segStrings.pad(pad_size,pad_word);
	blob_segFrame.pad(pad_size,pad_word);
	blob_segPart.pad(pad_size,pad_word);
	blob_segBmp.pad(pad_size,pad_word);
	blob_segPalet.pad(pad_size,pad_word);

	// create header ------------------------------------@/
	size_t header_size = 96;
	size_t offset_segLoaddesc = header_size;
	size_t offset_segPattern = offset_segLoaddesc + blob_segLoaddesc.size();
	size_t offset_segStrings = offset_segPattern + blob_segPattern.size();
	size_t offset_segFrame = offset_segStrings + blob_segStrings.size();
	size_t offset_segPart = offset_segFrame + blob_segFrame.size();
	size_t offset_segBmp = offset_segPart + blob_segPart.size();
	size_t offset_segPalet = offset_segBmp + blob_segBmp.size();

	aya::ALICE_AGEFILE_HEADER header = {};
	header.magic[0] = 'A';
	header.magic[1] = 'G';
	header.magic[2] = 'E';
	header.format_flags = format;
	header.offset_segLoaddesc = offset_segLoaddesc;
	header.offset_segPattern = offset_segPattern;
	header.offset_segStrings = offset_segStrings;
	header.offset_segFrame = offset_segFrame;
	header.offset_segPart = offset_segPart;
	header.offset_segBmp = offset_segBmp;
	header.offset_segPalet = offset_segPalet;

	blob_segHeader.write_raw(&header,sizeof(header));
	blob_segHeader.pad(header_size,pad_word);

	blob_all.write_blob(blob_segHeader);
	blob_all.write_blob(blob_segLoaddesc);
	blob_all.write_blob(blob_segPattern);
	blob_all.write_blob(blob_segStrings);
	blob_all.write_blob(blob_segFrame);
	blob_all.write_blob(blob_segPart);
	blob_all.write_blob(blob_segBmp);
	blob_all.write_blob(blob_segPalet);
	return blob_all;
}
auto aya::CPhoto::convert_fileAGI(const aya::CAliceAGIConvertInfo& info) -> scl::blob {
	// validate info struct -----------------------------@/
	const int format = info.format;

	const int subimage_xsize = info.subimage_xsize;
	const int subimage_ysize = info.subimage_ysize;

	bool use_subimage = false;

	if(subimage_xsize%8 != 0) {
		std::puts("aya::CPhoto::convert_fileAGI(): error: subimage X size must be multiple of 8!!");
		std::exit(-1);
	}

	if(subimage_xsize==0 && subimage_ysize==0) {
		use_subimage = false;
	} else if(subimage_xsize>0 && subimage_ysize>0) {
		use_subimage = true;
	} else {
		std::printf("aya::CPhoto::convert_fileAGI(): error: bad subimage size (%d,%d)\n",
			subimage_xsize,subimage_ysize
		);
		std::exit(-1);	
	}

	// setup bitmap info --------------------------------@/
	scl::blob out_blob;
	scl::blob blob_headersection;
	scl::blob blob_paletsection;
	scl::blob blob_bmpsection;

	constexpr int header_size = 56;
	int subimage_count = 0;
	size_t subimage_datasize = 0;

	// write frames -------------------------------------@/
	if(use_subimage) {
		auto imagetable = rect_split(subimage_xsize,subimage_ysize);
		for(auto pic : imagetable) {
			if(info.split_cels) {
				auto celtable = pic->rect_split(8,8);
				for(auto cel : celtable) {
					auto bmpblob = cel->convert_rawAGI(format);
					blob_bmpsection.write_blob(bmpblob);
					subimage_datasize = bmpblob.size();
					subimage_count++;
				}
			} else {
				auto bmpblob = pic->convert_rawAGI(format);
				blob_bmpsection.write_blob(bmpblob);
				subimage_datasize = bmpblob.size();
			}
		}
	} else {
		// write bitmap data ----------------------------@/
		auto bmpblob = convert_rawAGI(format);
		blob_bmpsection.write_blob(bmpblob);
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
	blob_paletsection.pad(16,pad_word); // pad to nearest 16;
	blob_bmpsection.pad(16,pad_word); // pad to nearest 16;
	
	// write header & complete file ---------------------@/
	size_t offset_paletsection = header_size;
	size_t offset_bmpsection = offset_paletsection + blob_paletsection.size();

	aya::ALICE_AGIFILE_HEADER header = {};
	header.magic[0] = 'A';
	header.magic[1] = 'G';
	header.magic[2] = 'I';
	header.format_flags = format;
	header.width = width();
	header.height = height();
	header.subimage_count = subimage_count;
	header.subimage_size = subimage_datasize;
	header.palet_size = blob_paletsection.size();
	header.bitmap_size = blob_bmpsection.size();
	header.offset_paletsection = offset_paletsection;
	header.offset_bmpsection = offset_bmpsection;

	blob_headersection.write_raw(&header,sizeof(header));
	blob_headersection.pad(header_size,pad_word);

	out_blob.write_blob(blob_headersection);
	out_blob.write_blob(blob_paletsection);
	out_blob.write_blob(blob_bmpsection);

	return out_blob;
}
auto aya::CPhoto::convert_fileAGM(const aya::CAliceAGMConvertInfo& info) -> scl::blob {
	// validate info struct -----------------------------@/
	const int format = info.format;

	if((width()%8) != 0 || (height()%8) != 0) {
		std::puts("aya::CPhoto::convert_fileAGM(): error: image dimensions must be multiple of 8!!");
		std::exit(-1);
	}

	const int max_numtiles = 1024;
	const int map_width = width() / 8;
	const int map_height = height() / 8;

	int subimage_count = 0;

	scl::blob out_blob;
	scl::blob blob_headersection;
	scl::blob blob_paletsection;
	scl::blob blob_mapsection;
	scl::blob blob_bmpsection;

	// write frames -------------------------------------@/
	auto imagetable = rect_split(8,8); {
		std::map<uint64_t,size_t> imghash_map;
		std::map<uint64_t,size_t> imghash_mapRealIdx;
		size_t num_processedCel = 0;

		int num_flips = 4;

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
						auto get_tileXY = [&](int idx, int &x, int &y) {
							x = 8 * (idx % (this->width()/8));
							y = 8 * (idx / (this->width()/8));
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
				blob_mapsection.write_u16(tile_index | (flip_index<<10));
			} else {
				int index = subimage_count;

				if(index > max_numtiles) {
					std::printf(
						"aya::CPhoto::convert_fileAGM(): error: cel count over! (cel count: >=%3d)\n"
						"consider using the 12-bit flag to use more tiles.\n",
						index
					);
					std::exit(-1);
				}

				imghash_map[image_hashes[0]] = index;
				imghash_mapRealIdx[image_hashes[0]] = num_processedCel;
				auto bmpblob = srcpic->convert_rawAGI(format);
				blob_bmpsection.write_blob(bmpblob);
				blob_mapsection.write_u16(index);
				subimage_count++;
			}

			num_processedCel++;
		}
	}

	// create palette -----------------------------------@/
	if(aya::alice_graphfmt::getBPP(format) <= 8) {
		scl::blob palet_blob;
		int color_count = 1 << aya::alice_graphfmt::getBPP(format);
		for(int p=0; p<color_count; p++) {
			palet_get(p).write_rgb5a1_agb(blob_paletsection);
		}
	} else {
		blob_paletsection.write_u32(0);
	}

	// pad sections out ---------------------------------@/
	constexpr int header_size = 40;
	constexpr int pad_word = 0xAA;
	blob_paletsection.pad(32,pad_word); // pad to nearest 16;
	blob_mapsection.pad(32,pad_word); // pad to nearest 16;
	blob_bmpsection.pad(32,pad_word); // pad to nearest 16;

	// create header ------------------------------------@/
	if(info.verbose) {
		std::printf("\tCEL section: %.2f K\n",
			((float)blob_bmpsection.size()) / 1024.0
		);
		std::printf("\tCHP section: %.2f K (n.cels == %d)\n",
			((float)blob_mapsection.size()) / 1024.0,
			subimage_count
		);
	}
	
	size_t offset_paletsection = header_size;
	size_t offset_mapsection = offset_paletsection + blob_paletsection.size();
	size_t offset_bmpsection = offset_mapsection + blob_mapsection.size();

	aya::ALICE_AGMFILE_HEADER header = {};
	header.magic[0] = 'A';
	header.magic[1] = 'G';
	header.magic[2] = 'M';
	header.format_flags = format;
	header.width_dot = width();
	header.width_chr = map_width;
	header.height_dot = height();
	header.height_chr = map_height;
	header.palet_size = blob_paletsection.size();
	header.map_size = blob_mapsection.size();
	header.bitmap_size = blob_bmpsection.size();
	header.offset_paletsection = offset_paletsection;
	header.offset_mapsection = offset_mapsection;
	header.offset_bmpsection = offset_bmpsection;

	blob_headersection.write_raw(&header,sizeof(header));
	blob_headersection.pad(header_size,pad_word);

	out_blob.write_blob(blob_headersection);
	out_blob.write_blob(blob_paletsection);
	out_blob.write_blob(blob_mapsection);
	out_blob.write_blob(blob_bmpsection);

	return out_blob;
}

auto aya::CPhoto::convert_fileHGI(const aya::CHouraiHGIConvertInfo& info) -> scl::blob {
	// validate info struct -----------------------------@/
	const int format = info.format;

	const int subimage_xsize = info.subimage_xsize;
	const int subimage_ysize = info.subimage_ysize;

	bool use_subimage = false;

	if(subimage_xsize%8 != 0) {
		std::puts("aya::CPhoto::convert_fileHGI(): error: subimage X size must be multiple of 8!!");
		std::exit(-1);
	}

	if(subimage_xsize==0 && subimage_ysize==0) {
		use_subimage = false;
	} else if(subimage_xsize>0 && subimage_ysize>0) {
		use_subimage = true;
	} else {
		std::printf("aya::CPhoto::convert_fileHGI(): error: bad subimage size (%d,%d)\n",
			subimage_xsize,subimage_ysize
		);
		std::exit(-1);	
	}

	// setup bitmap info --------------------------------@/
	scl::blob out_blob;
	scl::blob blob_headersection;
	scl::blob blob_paletsection;
	scl::blob blob_bmpsection;

	constexpr int header_size = 64;
	int subimage_count = 0;
	size_t subimage_datasize = 0;

	// write frames -------------------------------------@/
	if(use_subimage) {
		auto imagetable = rect_split(subimage_xsize,subimage_ysize);
		for(auto pic : imagetable) {
			auto bmpblob = pic->convert_rawHGI(format);
			blob_bmpsection.write_blob(bmpblob);
			subimage_datasize = bmpblob.size();
		}
	} else {
		// write bitmap data ----------------------------@/
		auto bmpblob = convert_rawHGI(format);
		blob_bmpsection.write_blob(bmpblob);
	}

	// create palette -----------------------------------@/
	size_t color_count = 0;
	if(aya::hourai_graphfmt::getBPP(format) <= 8) {
		color_count = 1 << aya::hourai_graphfmt::getBPP(format);
		for(int p=0; p<color_count; p++) {
			palet_get(p).write_rgb5a1_agb(blob_paletsection);
		}
	} else {
		blob_paletsection.write_u32(0);
	}

	// pad sections out ---------------------------------@/
	constexpr int pad_word = 0xAA;
	blob_paletsection.pad(16,pad_word); // pad to nearest 16;
	blob_bmpsection.pad(16,pad_word); // pad to nearest 16;
	
	// write header & complete file ---------------------@/
	size_t offset_paletsection = header_size;
	size_t offset_bmpsection = offset_paletsection + blob_paletsection.size();

	aya::HOURAI_HGIFILE_HEADER header = {};
	header.magic[0] = 'H';
	header.magic[1] = 'G';
	header.magic[2] = 'I';
	header.width = width();
	header.height = height();
	header.subimage_count = subimage_count;
	header.subimage_size = subimage_datasize;
	header.palet_size = color_count * sizeof(uint16_t);
	header.bitmap_size = blob_bmpsection.size();
	header.offset_paletsection = offset_paletsection;
	header.offset_bmpsection = offset_bmpsection;

	blob_headersection.write_raw(&header,sizeof(header));
	blob_headersection.pad(header_size,pad_word);

	out_blob.write_blob(blob_headersection);
	out_blob.write_blob(blob_paletsection);
	out_blob.write_blob(blob_bmpsection);

	return out_blob;
}
auto aya::CPhoto::convert_fileHGM(const aya::CHouraiHGMConvertInfo& info) -> scl::blob {
	// validate info struct -----------------------------@/
	const int format = info.format;

	if((width()%8) != 0 || (height()%8) != 0) {
		std::puts("aya::CPhoto::convert_fileHGM(): error: image dimensions must be multiple of 8!!");
		std::exit(-1);
	}

	const int max_numtiles = 256;
	const int map_width = width() / 8;
	const int map_height = height() / 8;

	int subimage_count = 0;

	scl::blob out_blob;
	scl::blob blob_headersection;
	scl::blob blob_paletsection;
	scl::blob blob_mapsection;
	scl::blob blob_attrsection;
	scl::blob blob_bmpsection;

	// write frames -------------------------------------@/
	auto imagetable = rect_split(8,8); {
		std::map<uint64_t,size_t> imghash_map;
		std::map<uint64_t,size_t> imghash_mapRealIdx;
		size_t num_processedCel = 0;

		int num_flips = 4;

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
				const uint64_t hash = image_hashes.at(fi);
				if(imghash_map.count(hash) > 0) {
					flip_index = fi;
					tile_index = imghash_map[hash];
					found_used = true;
					
					if(info.verbose) {
						auto get_tileXY = [&](int idx, int &x, int &y) {
							x = 8 * (idx % (this->width()/8));
							y = 8 * (idx / (this->width()/8));
						};
						
						int orig_index = imghash_mapRealIdx[hash];
						int src_x,src_y;
						int cel_x,cel_y;
						get_tileXY(num_processedCel,src_x,src_y);
						get_tileXY(orig_index,cel_x,cel_y);
						if(fi == 0) {
							// printf("tile hit! (%3d,%3d) == tile %3d\n",src_x,src_y,tile_index);
						} else {
							printf("flip hit! (%3d,%3d) == tile $%02X (%3d,%3d) [fi=%d]\n",src_x,src_y,orig_index,cel_x,cel_y,fi);
						}
					}
					
					break;
				}
			}

			// write tile to bmp/map
			if(found_used) {
				blob_mapsection.write_u8(tile_index);
				blob_attrsection.write_u8(flip_index<<5);
			} else {
				int index = subimage_count;

				if(index > max_numtiles) {
					std::printf(
						"aya::CPhoto::convert_fileHGM(): error: cel count over! (cel count: >=%3d)\n"
						"consider using the 12-bit flag to use more tiles.\n",
						index
					);
					std::exit(-1);
				}

				imghash_map[image_hashes[0]] = index;
				imghash_mapRealIdx[image_hashes[0]] = num_processedCel;
				auto bmpblob = srcpic->convert_rawHGI(format);
				blob_bmpsection.write_blob(bmpblob);
				blob_mapsection.write_u8(index);
				blob_attrsection.write_u8(0);
				subimage_count++;
			}

			num_processedCel++;
		}
	}

	// create palette -----------------------------------@/
	if(aya::hourai_graphfmt::getBPP(format) <= 8) {
		scl::blob palet_blob;
		int color_count = 1 << aya::hourai_graphfmt::getBPP(format);
		for(int p=0; p<color_count; p++) {
			palet_get(p).write_rgb5a1_agb(blob_paletsection);
		}
	} else {
		blob_paletsection.write_u32(0);
	}

	// pad sections out ---------------------------------@/
	constexpr size_t header_size = 64;
	constexpr int pad_word = 0xAA;
	const size_t size_paletsection = blob_paletsection.size();
	const size_t size_mapsection = blob_mapsection.size();
	const size_t size_bmpsection = blob_bmpsection.size();
	blob_paletsection.pad(16,pad_word); // pad to nearest 16;
	blob_mapsection.pad(16,pad_word); // pad to nearest 16;
	blob_attrsection.pad(16,pad_word); // pad to nearest 16;
	blob_bmpsection.pad(16,pad_word); // pad to nearest 16;

	// create header ------------------------------------@/
	if(info.verbose) {
		std::printf("\tCEL section: %.2f K\n",
			((float)blob_bmpsection.size()) / 1024.0
		);
		std::printf("\tCHP section: %.2f K (n.cels == %d)\n",
			((float)blob_mapsection.size()) / 1024.0,
			subimage_count
		);
	}
	
	size_t offset_paletsection = header_size;
	size_t offset_mapsection = offset_paletsection + blob_paletsection.size();
	size_t offset_attrsection = offset_mapsection + blob_mapsection.size();
	size_t offset_bmpsection = offset_attrsection + blob_attrsection.size();

	aya::HOURAI_HGMFILE_HEADER header = {};
	header.magic[0] = 'H';
	header.magic[1] = 'G';
	header.magic[2] = 'M';
	header.width_dot = width();
	header.width_chr = map_width;
	header.height_dot = height();
	header.height_chr = map_height;
	header.palet_size = size_paletsection;
	header.map_size = size_mapsection;
	header.bitmap_size = size_bmpsection;
	header.offset_paletsection = offset_paletsection;
	header.offset_mapsection = offset_mapsection;
	header.offset_attrsection = offset_attrsection;
	header.offset_bmpsection = offset_bmpsection;

	blob_headersection.write_raw(&header,sizeof(header));
	blob_headersection.pad(header_size,pad_word);

	out_blob.write_blob(blob_headersection);
	out_blob.write_blob(blob_paletsection);
	out_blob.write_blob(blob_mapsection);
	out_blob.write_blob(blob_attrsection);
	out_blob.write_blob(blob_bmpsection);

	return out_blob;
}

