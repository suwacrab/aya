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
	bool do_twiddle = false;
	bool do_palette = false;
	bool do_compress = true;
	bool do_showusage = false;

	std::string param_srcfile;
	std::string param_outfile;
	std::string param_pixelfmt;
	std::string param_filetype;

	std::string param_pga_json;

	int pixelfmt_flags = 0xFF;
	
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
	// PGA-specific
	if(argparser.arg_isValid("-pga_json",1)) {
		param_pga_json = argparser.arg_get("-pga_json",1).at(1);
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

	std::printf("out file: %s\n",param_outfile.c_str());
	std::printf("src file: %s\n",param_srcfile.c_str());
	std::printf("filetype: %s\n",param_filetype.c_str());
	std::printf("pixelfmt: %s\n",param_pixelfmt.c_str());
	std::printf("pga json: %s\n",param_pga_json.c_str());

	/* convert ------------------------------------------*/
	if(param_filetype == "mgi") {
		/* get format -----------------------------------*/
		const std::map<std::string,int> pixelformat_table = {
			{"i4",aya::marisa_graphfmt::i4},
			{"i8",aya::marisa_graphfmt::i8},
			{"rgb565",aya::marisa_graphfmt::rgb565},
			{"rgb5a1",aya::marisa_graphfmt::rgb5a1},
			{"argb4444",aya::marisa_graphfmt::argb4444}
		};
		if(pixelformat_table.count(param_pixelfmt) <= 0) {
			std::printf("aya: error: unknown pixel format '%s'\nplease make sure the format's name is correct.\n",
				param_pixelfmt.c_str()
			);
			std::exit(-1);
		}

		pixelfmt_flags = pixelformat_table.at(param_pixelfmt);
		if(!do_twiddle) pixelfmt_flags |= aya::marisa_graphfmt::nontwiddled;
		
		auto pic = aya::CPhoto(param_srcfile,do_palette);
		auto pic_blob = pic.convert_fileMGI(pixelfmt_flags, do_compress);
		if(!pic_blob.send_file(param_outfile)) {
			std::printf("aya: error: unable to write to file %s\n",param_outfile.c_str());
			std::exit(-1);
		}
	} 
	else if(param_filetype == "pgi") {
		/* get format -----------------------------------*/
		const std::map<std::string,int> pixelformat_table = {
			{"i4",aya::patchu_graphfmt::i4},
			{"i8",aya::patchu_graphfmt::i8},
			{"rgb565",aya::patchu_graphfmt::rgb565},
			{"rgb5a1",aya::patchu_graphfmt::rgb5a1},
			{"argb4",aya::patchu_graphfmt::argb4},
			{"argb8",aya::patchu_graphfmt::argb8}
		};
		if(pixelformat_table.count(param_pixelfmt) <= 0) {
			std::printf("aya: error: unknown pixel format '%s'\nplease make sure the format's name is correct.\n",
				param_pixelfmt.c_str()
			);
			std::exit(-1);
		}

		pixelfmt_flags = pixelformat_table.at(param_pixelfmt);

		auto pic = aya::CPhoto(param_srcfile,do_palette);
		auto pic_blob = pic.convert_filePGI(pixelfmt_flags, do_compress);
		if(!pic_blob.send_file(param_outfile)) {
			std::printf("aya: error: unable to write to file %s\n",param_outfile.c_str());
			std::exit(-1);
		}
	} 
	else if(param_filetype == "pga") {
		/* get format -----------------------------------*/
		const std::map<std::string,int> pixelformat_table = {
			{"i4",aya::patchu_graphfmt::i4},
			{"i8",aya::patchu_graphfmt::i8},
			{"rgb565",aya::patchu_graphfmt::rgb565},
			{"rgb5a1",aya::patchu_graphfmt::rgb5a1},
			{"argb4",aya::patchu_graphfmt::argb4},
			{"argb8",aya::patchu_graphfmt::argb8}
		};
		if(pixelformat_table.count(param_pixelfmt) <= 0) {
			std::printf("aya: error: unknown pixel format '%s'\nplease make sure the format's name is correct.\n",
				param_pixelfmt.c_str()
			);
			std::exit(-1);
		}

		pixelfmt_flags = pixelformat_table.at(param_pixelfmt);

		auto pic = aya::CPhoto(param_srcfile,do_palette);
		auto pic_blob = pic.convert_filePGA(pixelfmt_flags, param_pga_json, do_compress);
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
		"\t-fmt <fmt> <type> specify output's pixel format & filetype\n"
		"\t-t                twiddle texture\n"
		"\t-nc               don't use gz compression\n"
		"\t-p                use palette\n"
		"\t.MGI specifics:\n"
		"\t\tformats: i4,i8,rgb565,rgb5a1,argb4444\n"
		"\t.PGI specifics:\n"
		"\t\tformats: i4,i8,rgb565,rgb5a1,argb4,argb8\n"
		"\t\t-pga_json <json>  specifies aseprite spritesheet .json to use\n"
	);
	std::printf("\taya graphic converter ver. %s\n",aya_ver.build_date.c_str());
};


