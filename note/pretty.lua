do

local ls_header = {
	{ 'header ("NGA\\0")','char',4 };
	{ 'format','int' };
	{ 'frame count','short' };
	{ 'frame section count','short' };
	{ 'frame section offset','int' };
	{ 'subframe section offset','int' };
	{ 'palette section offset','int' };
	{ 'bitmap section offset','int' };
}

local ls_frame = {
	{ 'header ("FR1\\0")','char',4 };
}
local ls_frameIn = {
	{ 'number of subframes frame has', 'short' };
	{ 'time duration (in frames) this frame will display for', 'short' };
	{ 'index of first subframe, in subframe section', 'int' };
}
local ls_subframe = {
	{ 'header ("FR2\\0")','char',4 };
}
local ls_subframeIn = {
	{ 'bmp offset (divided by 8)', 'int' };
	{ 'bmp size (divided by 8)', 'int' };
	{ 'palette number', 'int' };
	{ 'format', 'short' };
	{ 'bitmap width (rounded up to nearest 8 dots)', 'short' };
	{ 'bitmap dimensions (X,Y)','short',2 };
	{ 'X,Y offset when drawing','short',2 };
}
local ls_palet = {
	{ 'header ("PAL\\0")','char',4 };
	{ 'palette size (uncompressed, 0 if file contains no palette)', 'int' };
	{ 'compressed size', 'int' };
	{ 'palette data (zlib-compressed)', 'short',0 };
}
local ls_bitmap = {
	{ 'header ("CEL\\0")','char',4 };
	{ 'bitmap size (uncompressed)', 'int' };
	{ 'bitmap size (compressed)', 'int' };
	{ 'bitmap data (zlib-compressed)', 'char',0 };
}

local ls_ngi_header = {
	{ 'header ("NGI\\0")','char',4 };
	{ 'format','int' };
	{ 'bitmap width (rounded up to nearest 8 dots)', 'short' };
	{ 'bitmap dimensions (X,Y)', 'short', 2 };
	{ 'sub-image count', 'short' };
	{ 'sub-image dimensions','short', 2 };
	{ 'size of each sub-image', 'int' };
	{ 'palette section offset','int' };
	{ 'bitmap section offset','int' };
}

local ls_ngm_header = {
	{ 'header ("NGM\\0")','char',4 };
	{ 'format','int' };
	{ 'bitmap dimensions (X,Y)', 'short', 2 };
	{ 'sub-image count', 'short' };
	{ 'size of each sub-image', 'short' };
	{ 'palette section offset','int' };
	{ 'map section offset','int' };
	{ 'bitmap section offset','int' };
}

local ls_ngm_map = {
	{ 'header ("CHP\\0")','char',4 };
	{ 'map dimensions (X,Y)', 'short', 2 };
	{ 'map data size (uncompressed)', 'int' };
	{ 'map data size (compressed)', 'int' };
	{ 'map data size (zlib-compressed)', 'char',0 };
}

local ls_aga_header = {
	{ 'header ("AGA\\0")','char',4 };
	{ 'image dimensions','short',2 };
	{ 'format','int' };
	{ 'palette size','ushort' };
	{ 'frame count','ushort' };
	{ 'bitmap size','int' };
	{ 'frame section offset','int' };
	{ 'subframe section offset','int' };
	{ 'palette section offset','int' };
	{ 'bitmap section offset','int' };
}
local ls_aga_frame = {
	{ 'number of subframes frame has', 'ushort' };
	{ 'bitmap size','ushort' };
	{ 'frame\'s bitmap data offset (relative to bitmap section offset)','int' };
	{ 'index of subframes in subframe section, for each mirror orientation', 'ushort',4 };
	{ 'duration (in frames) this frame will display for', 'int' };
}
local ls_aga_subframe = {
	{ 'X,Y offset for drawing', 'short',2 };
	{ 'OAM attributes*','ushort' };
	{ 'character number**', 'ushort' };
	{ 'character count', 'ushort' };
	{ 'bitmap dimensions (X,Y)', 'uchar',2 };
}

local printf = function(str,...) print(str:format(...)) end

local function print_flist(fl)
	print("entry!")
	local sizes = {
		['char'] = 1;
		['uchar'] = 1;
		['short'] = 2;
		['ushort'] = 2;
		['int'] = 4;
	}
	local offset = 0
	for _,entry in next,fl do
		local count <const> = entry[3]
		local ftype <const> = entry[2]
		local ftype_str = entry[2]
		if count then
			if count == 0 then
				ftype_str = ("%s[]"):format(ftype)
			else
				ftype_str = ("%s[%d]"):format(ftype,count)
			end
		end
		if ftype ~= 'char' then
			if (offset % sizes[ftype]) ~= 0 then
				error("field is not aligned!")
			end
		end
		printf("0x%02X | %-9s | %s",offset,ftype_str,entry[1])
		local size = sizes[ftype] * (count and count or 1)
		offset = offset + size
	end
end

--[[print_flist(ls_header)
print_flist(ls_frame)
print_flist(ls_frameIn)
print_flist(ls_subframe)
print_flist(ls_subframeIn)
print_flist(ls_palet)
print_flist(ls_bitmap)
print_flist(ls_ngi_header)
print_flist(ls_ngm_header)
print_flist(ls_ngm_map)]]
print_flist(ls_aga_header)
print_flist(ls_aga_frame)
print_flist(ls_aga_subframe)

end

