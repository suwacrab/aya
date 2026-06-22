#pragma once

#include <aya.h>
#include <rapidjson/document.h>

namespace aya::util {
	auto json_loadFile(std::string src_filename) -> rapidjson::Document;
};

