#include <cstdio>
#include <memory>
#include <map>

#include <freeimage.h>
#include <aya.h>
#include <blob.h>
#include <argparse.h>

static void disp_usage();

int main(int argc,const char* argv[]) {
	FreeImage_Initialise();

	auto argparser = CArgParser(argc,argv);

	if(!argparser.has_arguments()) {
		disp_usage();
		std::exit(-1);
	}

	/* read command-line inputs -------------------------*/
	bool do_palette = false;
	bool do_compress = true;
	bool do_showusage = false;
	bool do_verbose = false;

	std::string param_srcfile;
	std::string param_outfile;
	std::string param_pixelfmt;
	std::string param_filetype;

	bool param_mgi_twiddled = false;

	std::string param_pga_json;
	
	std::string param_nga_json;
	int param_nga_useroffsetX = 0;
	int param_nga_useroffsetY = 0;

	int param_ngi_subimageX = 0;
	int param_ngi_subimageY = 0;

	bool param_ngm_12bit = false;

	std::string param_aga_json;
	int param_aga_useroffsetX = 0;
	int param_aga_useroffsetY = 0;
	int param_aga_leniency = 0;

	int pixelfmt_flags = 0xFF;

	std::string param_exportpal_filename;
	std::string param_exportpal_format;
	
	if(argparser.arg_isValid("--help")) {
		do_showusage = true;
	}
	if(argparser.arg_isValid("-i",1)) {
		param_srcfile = argparser.arg_get("-i",1)[1];
	}
	if(argparser.arg_isValid("-o",1)) {
		param_outfile = argparser.arg_get("-o",1)[1];
	}
	if(argparser.arg_isValid("-fmt",2)) {
		param_filetype = argparser.arg_get("-fmt",2).at(1);
		param_pixelfmt = argparser.arg_get("-fmt",2).at(2);
	}
	if(argparser.arg_isValid("-nc")) {
		do_compress = false;
	}
	if(argparser.arg_isValid("-p")) {
		do_palette = true;
	}
	if(argparser.arg_isValid("-v")) {
		do_verbose = true;
	}
	if(argparser.arg_isValid("-pexport",2)) {
		param_exportpal_filename = argparser.arg_get("-pexport",2).at(1);
		param_exportpal_format = argparser.arg_get("-pexport",2).at(2);	
	}

	// MGI-specific
	if(argparser.arg_isValid("mgi_twiddled")) {
		param_mgi_twiddled = true;
	}

	// PGA-specific
	if(argparser.arg_isValid("-pga_json",1)) {
		param_pga_json = argparser.arg_get("-pga_json",1).at(1);
	}

	// NGA-specific
	if(argparser.arg_isValid("-nga_json",1)) {
		param_nga_json = argparser.arg_get("-nga_json",1).at(1);
	}
	if(argparser.arg_isValid("-nga_useroffset",2)) {
		param_nga_useroffsetX = std::stoi(argparser.arg_get("-nga_useroffset",2).at(1));
		param_nga_useroffsetY = std::stoi(argparser.arg_get("-nga_useroffset",2).at(2));
	}

	// NGI-specific
	if(argparser.arg_isValid("-ngi_subimage",2)) {
		param_ngi_subimageX = std::stoi(argparser.arg_get("-ngi_subimage",2).at(1));
		param_ngi_subimageY = std::stoi(argparser.arg_get("-ngi_subimage",2).at(2));
	}

	// NGM-specific
	if(argparser.arg_isValid("-ngm_12bit")) {
		param_ngm_12bit = true;
	}

	// AGA-specific
	if(argparser.arg_isValid("-aga_json",1)) {
		param_aga_json = argparser.arg_get("-aga_json",1).at(1);
	}
	if(argparser.arg_isValid("-aga_useroffset",2)) {
		param_aga_useroffsetX = std::stoi(argparser.arg_get("-aga_useroffset",2).at(1));
		param_aga_useroffsetY = std::stoi(argparser.arg_get("-aga_useroffset",2).at(2));
	}
	if(argparser.arg_isValid("-aga_leniency",1)) {
		param_aga_leniency = std::stoi(argparser.arg_get("-aga_leniency",1).at(1));
	}

	if(do_showusage) {
		disp_usage();
		std::exit(0);
	}

	if(param_srcfile.empty()) {
		std::puts("aya: error: no source file specified");
		disp_usage();
		std::exit(-1);
	}
	if(param_outfile.empty()) {
		std::puts("aya: error: no output file specified");
		disp_usage();
		std::exit(-1);
	}

	/*std::printf("out file: %s\n",param_outfile.c_str());
	std::printf("src file: %s\n",param_srcfile.c_str());
	std::printf("filetype: %s\n",param_filetype.c_str());
	std::printf("pixelfmt: %s\n",param_pixelfmt.c_str());
	std::printf("nga json: %s\n",param_nga_json.c_str());*/

	// export palette -----------------------------------@/
	if(!param_exportpal_filename.empty()) {
		//auto pal_format = param_exportpal_format;
		auto pic = aya::CPhoto(param_srcfile,do_palette);
		Blob pal_blob;
		for(int i=0; i<256; i++) {
			pal_blob.write_u8(pic.palet_get(i).r);
			pal_blob.write_u8(pic.palet_get(i).g);
			pal_blob.write_u8(pic.palet_get(i).b);
		}
		// quick-export 
		if(!pal_blob.send_file(param_exportpal_filename)) {
			std::printf("aya: error: unable to write to file %s\n",param_exportpal_filename.c_str());
			std::exit(-1);
		}
	}

	// convert ------------------------------------------@/
	const std::map<std::string,int> pixelformat_table_marisa = {
		{"i4",aya::marisa_graphfmt::i4},
		{"i8",aya::marisa_graphfmt::i8},
		{"rgb565",aya::marisa_graphfmt::rgb565},
		{"rgb5a1",aya::marisa_graphfmt::rgb5a1},
		{"argb4444",aya::marisa_graphfmt::argb4444}
	};
	static const std::map<std::string,int> pixelformat_table_patchouli = {
		{"i4",aya::patchu_graphfmt::i4},
		{"i8",aya::patchu_graphfmt::i8},
		{"rgb565",aya::patchu_graphfmt::rgb565},
		{"rgb5a1",aya::patchu_graphfmt::rgb5a1},
		{"argb4",aya::patchu_graphfmt::argb4},
		{"argb8",aya::patchu_graphfmt::argb8}
	};
	static const std::map<std::string,int> pixelformat_table_narumi = {
		{"i4",aya::narumi_graphfmt::i4},
		{"i8",aya::narumi_graphfmt::i8},
		{"rgb",aya::narumi_graphfmt::rgb},
	};
	static const std::map<std::string,int> pixelformat_table_alice = {
		{"i4",aya::alice_graphfmt::i4},
		{"i8",aya::alice_graphfmt::i8},
		{"rgb",aya::alice_graphfmt::rgb},
	};

	if(param_filetype == "mgi") {
		/* get format -----------------------------------*/
		if(pixelformat_table_marisa.count(param_pixelfmt) <= 0) {
			std::printf("aya: error: unknown pixel format '%s'\nplease make sure the format's name is correct.\n",
				param_pixelfmt.c_str()
			);
			std::exit(-1);
		}

		pixelfmt_flags = pixelformat_table_marisa.at(param_pixelfmt);
		if(!param_mgi_twiddled) pixelfmt_flags |= aya::marisa_graphfmt::nontwiddled;
		
		auto pic = aya::CPhoto(param_srcfile,do_palette);
		auto pic_blob = pic.convert_fileMGI(pixelfmt_flags, do_compress);
		if(!pic_blob.send_file(param_outfile)) {
			std::printf("aya: error: unable to write to file %s\n",param_outfile.c_str());
			std::exit(-1);
		}
	} 
	else if(param_filetype == "pgi") {
		/* get format -----------------------------------*/
		if(pixelformat_table_patchouli.count(param_pixelfmt) <= 0) {
			std::printf("aya: error: unknown pixel format '%s'\nplease make sure the format's name is correct.\n",
				param_pixelfmt.c_str()
			);
			std::exit(-1);
		}

		pixelfmt_flags = pixelformat_table_patchouli.at(param_pixelfmt);

		auto pic = aya::CPhoto(param_srcfile,do_palette);
		auto pic_blob = pic.convert_filePGI(pixelfmt_flags, do_compress);
		if(!pic_blob.send_file(param_outfile)) {
			std::printf("aya: error: unable to write to file %s\n",param_outfile.c_str());
			std::exit(-1);
		}
	} 
	else if(param_filetype == "pga") {
		/* get format -----------------------------------*/
		if(pixelformat_table_patchouli.count(param_pixelfmt) <= 0) {
			std::printf("aya: error: unknown pixel format '%s'\nplease make sure the format's name is correct.\n",
				param_pixelfmt.c_str()
			);
			std::exit(-1);
		}

		pixelfmt_flags = pixelformat_table_patchouli.at(param_pixelfmt);

		auto pic = aya::CPhoto(param_srcfile,do_palette);
		auto pic_blob = pic.convert_filePGA(pixelfmt_flags, param_pga_json, do_compress);
		if(!pic_blob.send_file(param_outfile)) {
			std::printf("aya: error: unable to write to file %s\n",param_outfile.c_str());
			std::exit(-1);
		}
	} 
	else if(param_filetype == "nga") {
		/* get format -----------------------------------*/
		if(pixelformat_table_narumi.count(param_pixelfmt) <= 0) {
			std::printf("aya: error: unknown pixel format '%s'\nplease make sure the format's name is correct.\n",
				param_pixelfmt.c_str()
			);
			std::exit(-1);
		}

		pixelfmt_flags = pixelformat_table_narumi.at(param_pixelfmt);

		auto pic = aya::CPhoto(param_srcfile,do_palette);
		auto info = (aya::CNarumiNGAConvertInfo){
			.filename_json = param_nga_json,
			.do_compress = do_compress,
			.format = pixelfmt_flags,
			.useroffset_x = param_nga_useroffsetX,
			.useroffset_y = param_nga_useroffsetY,
			.verbose = do_verbose
		};
		auto pic_blob = pic.convert_fileNGA(info);
		if(!pic_blob.send_file(param_outfile)) {
			std::printf("aya: error: unable to write to file %s\n",param_outfile.c_str());
			std::exit(-1);
		}
	} 
	else if(param_filetype == "ngi") {
		/* get format -----------------------------------*/
		if(pixelformat_table_narumi.count(param_pixelfmt) <= 0) {
			std::printf("aya: error: unknown pixel format '%s'\nplease make sure the format's name is correct.\n",
				param_pixelfmt.c_str()
			);
			std::exit(-1);
		}

		pixelfmt_flags = pixelformat_table_narumi.at(param_pixelfmt);

		auto pic = aya::CPhoto(param_srcfile,do_palette);
		auto info = (aya::CNarumiNGIConvertInfo){
			.do_compress = do_compress,
			.format = pixelfmt_flags,
			.subimage_xsize = param_ngi_subimageX,
			.subimage_ysize = param_ngi_subimageY,
			.verbose = do_verbose
		};
		auto pic_blob = pic.convert_fileNGI(info);
		if(!pic_blob.send_file(param_outfile)) {
			std::printf("aya: error: unable to write to file %s\n",param_outfile.c_str());
			std::exit(-1);
		}
	} 
	else if(param_filetype == "ngm") {
		/* get format -----------------------------------*/
		if(pixelformat_table_narumi.count(param_pixelfmt) <= 0) {
			std::printf("aya: error: unknown pixel format '%s'\nplease make sure the format's name is correct.\n",
				param_pixelfmt.c_str()
			);
			std::exit(-1);
		}

		pixelfmt_flags = pixelformat_table_narumi.at(param_pixelfmt);

		auto pic = aya::CPhoto(param_srcfile,do_palette);
		auto info = (aya::CNarumiNGMConvertInfo){
			.do_compress = do_compress,
			.format = pixelfmt_flags,
			.is_12bit = param_ngm_12bit,
			.verbose = do_verbose
		};
		auto pic_blob = pic.convert_fileNGM(info);
		if(!pic_blob.send_file(param_outfile)) {
			std::printf("aya: error: unable to write to file %s\n",param_outfile.c_str());
			std::exit(-1);
		}
	} 
	else if(param_filetype == "aga") {
		/* get format -----------------------------------*/
		if(pixelformat_table_alice.count(param_pixelfmt) <= 0) {
			std::printf("aya: error: unknown pixel format '%s'\nplease make sure the format's name is correct.\n",
				param_pixelfmt.c_str()
			);
			std::exit(-1);
		}

		pixelfmt_flags = pixelformat_table_alice.at(param_pixelfmt);

		auto pic = aya::CPhoto(param_srcfile,do_palette);
		auto info = (aya::CAliceAGAConvertInfo){
			.filename_json = param_aga_json,
			.do_compress = do_compress,
			.format = pixelfmt_flags,
			.lenient_count = param_aga_leniency,
			.useroffset_x = param_aga_useroffsetX,
			.useroffset_y = param_aga_useroffsetY,
			.verbose = do_verbose
		};
		auto pic_blob = pic.convert_fileAGA(info);
		if(!pic_blob.send_file(param_outfile)) {
			std::printf("aya: error: unable to write to file %s\n",param_outfile.c_str());
			std::exit(-1);
		}
	} 
	else if(param_filetype == "agm") {
		/* get format -----------------------------------*/
		const auto& pixelformat_table = pixelformat_table_alice;
		if(pixelformat_table.count(param_pixelfmt) <= 0) {
			std::printf("aya: error: unknown pixel format '%s'\nplease make sure the format's name is correct.\n",
				param_pixelfmt.c_str()
			);
			std::exit(-1);
		}

		pixelfmt_flags = pixelformat_table.at(param_pixelfmt);

		auto pic = aya::CPhoto(param_srcfile,do_palette);
		auto info = (aya::CAliceAGMConvertInfo){
			.do_compress = do_compress,
			.format = pixelfmt_flags,
			.verbose = do_verbose
		};
		auto pic_blob = pic.convert_fileAGM(info);
		if(!pic_blob.send_file(param_outfile)) {
			std::printf("aya: error: unable to write to file %s\n",param_outfile.c_str());
			std::exit(-1);
		}
	} 
	else {
		std::printf("aya: error: unknown output filetype '%s'\n",param_filetype.c_str());
		std::exit(-1);
	}

	FreeImage_DeInitialise();
}

static void disp_usage() {
	auto aya_ver = aya::version_get();

	std::puts(
		"aya -i <source_file> <options>\n"
		"usage:\n"
		"\t--help            show this dialog\n"
		"\t-o <output_file>  specify output filename\n"
		"\t-i <source_file>  specify source filename\n"
		"\t-fmt <type> <fmt> specify output's pixel format & filetype\n"
		"\t-nc               don't use gz compression\n"
		"\t-p                use palette\n"
		"\t-v                verbose flag\n"
		"\t.MGI specifics:\n"
		"\t\tformats: i4,i8,rgb565,rgb5a1,argb4444\n"
		"\t\t-mgi_twiddled           twiddle texture\n"
		"\t.PGI specifics:\n"
		"\t\tformats: i4,i8,rgb565,rgb5a1,argb4,argb8\n"
		"\t.PGA specifics:\n"
		"\t\t-pga_json <json>        specifies aseprite spritesheet .json to use\n"
		"\t.NGA specifics:\n"
		"\t\tformats: i4,i8,rgb\n"
		"\t\t-nga_json <json>        specifies aseprite spritesheet .json to use\n"
		"\t\t-nga_useroffset <x> <y> offsets each subframe by (x,y)\n"
		"\t.NGI specifics:\n"
		"\t\tformats: i4,i8,rgb\n"
		"\t\t-ngi_subimage <x> <y>   divides image into subimages, each with size (x,y)\n"
		"\t.NGM specifics:\n"
		"\t\tformats: i4,i8\n"
		"\t\t-ngm_12bit              raises max number of map cels from 1024->4096 (if 4bpp), or 512->2048 (if 8bpp)\n"
		"\t.AGA specifics:\n"
		"\t\tformats: i4,i8,rgb\n"
		"\t\tCompression-related flags are ignored of .AGA files.\n"
		"\t\t-aga_json <json>        specifies aseprite spritesheet .json to use\n"
		"\t\t-aga_leniency <n>       if enabled, each object allows at least <n> empty characters\n"
		"\t\t-aga_useroffset <x> <y> offsets each subframe by (x,y)\n"
	);
	std::printf("\taya graphic converter ver. %s\n",aya_ver.build_date.c_str());
	std::printf("\tavailable filetypes: aga, agm, mgi, pgi, pga, nga, ngi, ngm\n");
};


