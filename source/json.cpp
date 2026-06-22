#include "json.h"

namespace aya::util {
	auto json_loadFile(std::string src_filename) -> rapidjson::Document {
		rapidjson::Document jsondoc;

		// read json file -----------------------------------@/
		std::string json_str; {
			std::vector<char> filedata;

			// set file buffer's size -----------------------@/
			auto file = std::fopen(src_filename.c_str(),"rb");
			if(!file) {
				std::printf("aya::util::json_loadFile(): error: unable to open file %s for reading\n",
					src_filename.c_str()
				);
				std::exit(-1);
			}
			std::fseek(file,0,SEEK_END);
			filedata.resize(std::ftell(file));

			// read file to string --------------------------@/
			std::fseek(file,0,SEEK_SET);
			std::fread(filedata.data(),filedata.size(),1,file);
			std::fclose(file);

			filedata.push_back(0); // null terminator
			json_str = std::string(filedata.data());
		}
		jsondoc.Parse(json_str.c_str());

		return jsondoc;
	}
};

