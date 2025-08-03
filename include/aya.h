#pragma once

#include <blob.h>
#include <string>
#include <memory>
#include <array>

namespace aya {
	class CPhoto;
	struct CColor;
	struct CAyaVersion;
	class CWorkingFrame;
	class CWorkingSubframe;
	class CWorkingFrameList;

	struct MARISA_MGIFILE_HEADER;
	struct PATCHU_PGIFILE_HEADER;
	
	struct PATCHU_PGAFILE_HEADER;
	struct PATCHU_PGAFILE_FRAME;
	struct PATCHU_PGAFILE_TILE;

	struct ALICE_AGAFILE_HEADER;

	namespace patchu_graphfmt {
		enum {
			i4,
			i8,
			rgb565,
			rgb5a1,
			argb4,
			argb8,
			len,
			invalid = 0xFF,
			strided = (1<<9)
		};
		auto getBPP(int format) -> int;
		constexpr auto getID(int format) -> int { return format & 0xFF; }
		constexpr auto isValid(int format) -> bool {
			auto id = getID(format);
			return (id >= 0) && (id < len);
		}
	};
	namespace marisa_graphfmt {
		enum {
			i4,
			i8,
			rgb565,
			rgb5a1,
			argb4444,
			len,
			invalid = 0xFF,
			nontwiddled = (1<<8),
			strided = (1<<9)
		};
		auto getBPP(int format) -> int;
		constexpr auto getID(int format) -> int { return format & 0xFF; }
		constexpr auto isTwiddled(int format) -> bool { return (format & nontwiddled) == 0; }
		constexpr auto isValid(int format) -> bool {
			auto id = getID(format);
			return (id >= 0) && (id < len);
		}
	};
	namespace narumi_graphfmt {
		enum { i4,i8,rgb,len };
		auto getBPP(int format) -> int;
		constexpr auto getID(int format) -> int { return format & 0xFF; }
		constexpr auto isValid(int format) -> bool {
			auto id = getID(format);
			return (id >= 0) && (id < len);
		}
	};
	namespace alice_graphfmt {
		enum { i4,i8,rgb,len };
		auto getBPP(int format) -> int;
		constexpr auto getID(int format) -> int { return format & 0xFF; }
		constexpr auto isValid(int format) -> bool {
			auto id = getID(format);
			return (id >= 0) && (id < len);
		}
	};

	struct CNarumiNGAConvertInfo {
		std::string filename_json;
		bool do_compress;
		int format;

		int useroffset_x,useroffset_y;
		bool verbose;
	};
	struct CNarumiNGIConvertInfo {
		bool do_compress;
		int format;
		int subimage_xsize,subimage_ysize;
		bool verbose;
	};
	struct CNarumiNGMConvertInfo {
		bool do_compress;
		int format;
		int is_12bit;
		bool verbose;
	};
	struct CAliceAGAConvertInfo {
		std::string filename_json;
		bool do_compress;
		int format;

		int useroffset_x,useroffset_y;
		bool verbose;
	};

	auto conv_po2(int n) -> int;
	auto compress(Blob& srcblob, bool do_compress = true) -> Blob;
	auto version_get() -> CAyaVersion;
	auto twiddled_index(int x, int y, int w, int h) -> size_t;
	auto twiddled_index4b(int x, int y, int w, int h) -> size_t;
};

struct aya::MARISA_MGIFILE_HEADER {
	char magic[4];
	uint16_t width,height;
	uint16_t width_real,height_real;
	uint32_t format_marisa;
	uint32_t palette_size;
	uint32_t palette_size_actual;
	uint32_t palette_offset;
	uint32_t bmpdata_size;
	uint32_t bmpdata_size_actual;
	uint32_t bmpdata_offset;
};
struct aya::PATCHU_PGIFILE_HEADER {
	char magic[4];
	uint16_t width,height;
	uint16_t width_real,height_real;
	uint32_t format_flags;
	uint32_t palette_size;
	uint32_t palette_size_actual;
	uint32_t palette_offset;
	uint32_t bmpdata_size;
	uint32_t bmpdata_size_actual;
	uint32_t bmpdata_offset;
};
struct aya::PATCHU_PGAFILE_HEADER {
	char magic[4];
	uint16_t width,height;
	uint32_t format_flags;
	uint32_t palette_size;
	uint32_t num_frames;
	uint32_t tilesize;
	uint32_t offset_framesection;
	uint32_t offset_tilesection;
	uint32_t offset_bmpsection;
	uint32_t offset_paletsection;
};
struct aya::PATCHU_PGAFILE_FRAME {
	uint16_t img_w,img_h;
	uint32_t num_tiles;
	uint32_t size_bmp;
	uint32_t offset_tile;
	uint32_t offset_bmp;
	uint32_t duration_f;
	uint32_t duration_ms;
};
struct aya::PATCHU_PGAFILE_TILE {
	uint16_t sheet_x,sheet_y;
	uint16_t disp_x,disp_y;
	uint32_t tile_sizex;
};
struct aya::ALICE_AGAFILE_HEADER {
	char magic[4];
	uint16_t width,height;
	uint32_t format_flags;
	uint32_t palette_size;
	uint32_t frame_count;
	uint32_t palet_size;
	uint32_t bitmap_size;
	uint32_t offset_framesection;
	uint32_t offset_subframesection;
	uint32_t offset_paletsection;
	uint32_t offset_bmpsection;
};
struct aya::ALICE_AGAFILE_FRAME {
	uint16_t size_bmp;
	uint32_t offset_bmp;
	uint16_t offset_subframe[4];
	uint16_t duration_f;
};
struct aya::ALICE_AGAFILE_SUBFRAME {
	int16_t pos_x;
	int16_t pos_y;
	uint16_t tilenum;
	uint16_t size_bmp;
	uint32_t offset_tile;
	uint32_t offset_bmp;
	uint32_t duration_f;
	uint32_t duration_ms;
};

struct aya::CColor {
	uint8_t a,r,g,b;
	void write_alpha(Blob& out_blob) const;
	void write_argb8(Blob& out_blob) const;
	void write_rgb565(Blob& out_blob) const;
	void write_rgb5a1(Blob& out_blob,int test = 254) const;
	void write_rgb5a1_sat(Blob& out_blob,bool msb) const;
	void write_rgb5a1_agb(Blob& out_blob,bool msb = false) const;
	void write_argb4(Blob& out_blob) const;

	constexpr auto rawdata() const -> uint32_t {
		return ((uint32_t)(a)<<24) | 
			((uint32_t)(r)<<16) | 
			((uint32_t)(g)<<8) | 
			((uint32_t)(b));
	}
	constexpr CColor() : a(0),r(0),g(0),b(0) {}
	constexpr CColor(uint8_t alpha) : a(alpha),r(0),g(0),b(0) {}
	constexpr CColor(uint8_t na,uint8_t nr, uint8_t ng, uint8_t nb)
		: a(na),r(nr),g(ng),b(nb) {}
};

struct aya::CAyaVersion {
	std::string build_date;
};

class aya::CPhoto {
	private:
		int m_width,m_height;
		std::vector<aya::CColor> m_bmpdata;
		std::array<aya::CColor,256> m_palette;

		constexpr auto dot_getRaw(int x,int y) -> aya::CColor& {
			return m_bmpdata.at(x + y*width());
		}
		constexpr auto dot_getRawC(int x,int y) const -> const aya::CColor& {
			return m_bmpdata.at(x + y*width());
		}
		auto dot_getTwiddledIdx(int x,int y) const -> size_t;
		auto palet_getRaw(int pen) -> aya::CColor&;
		auto palet_getRawC(int pen) const -> const aya::CColor&;
	public:
		constexpr auto width() const -> int { return m_width; }
		constexpr auto height() const -> int { return m_height; }
		constexpr auto dimensions() const -> int { return width() * height(); }

		auto clear(aya::CColor color) -> void;
		auto dot_inRange(int x,int y) const -> bool;
		auto dot_setRGB(int x,int y,aya::CColor color) -> void;
		auto dot_get(int x,int y) const -> aya::CColor;

		auto palet_clear(aya::CColor color) -> void;
		auto palet_get(int pen) const -> aya::CColor;

		auto rect_blit(CPhoto& outpic,int sx,int sy,int dx,int dy,int w=0,int h=0) const -> void;
		auto rect_split(int size_x, int size_y, int count = -1) -> std::vector<std::shared_ptr<CPhoto>>;
		auto rect_get(int x,int y,int w=0,int h=0) const -> std::shared_ptr<CPhoto>;
		auto rect_isZero(int x, int y, int w, int h) const -> bool;
		auto all_equals(aya::CColor color) const -> bool;
		
		auto hash_get(int flip) const -> uint64_t;
		auto hash_getIndexed(int flip) const -> uint64_t;

		auto convert_fileMGI(int format, bool do_compress = true) -> Blob;
		auto convert_filePGI(int format, bool do_compress = true) -> Blob;
		auto convert_filePGA(int format, const std::string& json_filename, bool do_compress = true) -> Blob;
		auto convert_fileNGA(const CNarumiNGAConvertInfo &info) -> Blob;
		auto convert_fileNGI(const CNarumiNGIConvertInfo &info) -> Blob;
		auto convert_fileNGM(const CNarumiNGMConvertInfo &info) -> Blob;
		auto convert_fileAGA(const CAliceAGAConvertInfo &info) -> Blob;
		auto convert_raw(int format) const -> Blob;
		auto convert_rawPGI(int format) const -> Blob;
		auto convert_rawNGI(int format) const -> Blob;
		auto convert_twiddled(int format) const -> Blob;

		CPhoto();
		CPhoto(std::string filename,bool paletted = false, bool opaque_pal=false);
		CPhoto(int newwidth, int newheight);
		~CPhoto();
};

class aya::CWorkingSubframe {
	private:
		aya::CPhoto m_photo;
	public:
		int m_posX,m_posY;
		auto photo() -> aya::CPhoto& { return m_photo; }

		CWorkingSubframe();
		CWorkingSubframe(aya::CPhoto& photo, int pos_x, int pos_y);
		~CWorkingSubframe() {}
};
class aya::CWorkingFrame {
	private:
	public:
		int m_durationMS;
		int m_durationFrame;

		std::vector<aya::CWorkingSubframe> m_subframes;
		auto subframe_get(size_t index) -> aya::CWorkingSubframe&;
		auto subframe_count() const -> size_t { return m_subframes.size(); }

		CWorkingFrame();
		~CWorkingFrame() {}
};
class aya::CWorkingFrameList {
	private:
		std::vector<aya::CWorkingFrame> m_frames;
	public:
		auto create_fromAseJSON(aya::CPhoto& baseimage, const std::string& json_filename) -> void;
		
		auto frame_get(size_t index) -> aya::CWorkingFrame&;
		auto frame_count() const -> size_t { return m_frames.size(); }

		CWorkingFrameList();
		~CWorkingFrameList() {}
};

