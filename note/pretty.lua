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

local printf = function(str,...) print(str:format(...)) end

local function print_flist(fl)
	print("entry!")
	local sizes = {
		['char'] = 1;
		['short'] = 2;
		['int'] = 4;
	}
	local offset = 0
	for _,entry in next,fl do
		local count = entry[3]
		local ftype = entry[2]
		if count then
			if count == 0 then
				ftype = ("%s[]"):format(ftype)
			else
				ftype = ("%s[%d]"):format(ftype,count)
			end
		end
		printf("0x%02X | %-8s | %s",offset,ftype,entry[1])
		local size = sizes[entry[2]] * (count and count or 1)
		offset = offset + size
	end
end

print_flist(ls_header)
print_flist(ls_frame)
print_flist(ls_frameIn)
print_flist(ls_subframe)
print_flist(ls_subframeIn)
print_flist(ls_palet)
print_flist(ls_bitmap)
print_flist(ls_ngi_header)

end

