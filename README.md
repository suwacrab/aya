An image converter i've been using for my projects. Use it at your own risk.

Requires clang, zlib, and freeimage.

available formats:

-	PC: `.PGA`, `.PGI`
-	Dreamcast (mostly unused, as of now): `.MGI`
-	Saturn:
	*	`.NGA`: Used for 2D animations.
	*	`.NGI`: Used for static 2D images.
	*	`.NGM`: Used for storing tilemaps.

---
# Format specification
---

Assume all saturn formats are big-endian, while the rest are little-endian.

NGA files are the format used for saturn animation conversion. Of course, all
data is big-endian, NOT little-endian.

NGA files contain 5 main sections:
	*	header section
		0x00 | char[4]  | header ("NGA\0")
		0x04 | int      | format
		0x08 | short    | frame count
		0x0A | short    | frame section count
		0x0C | int      | frame section offset
		0x10 | int      | subframe section offset
		0x14 | int      | palette section offset
		0x18 | int      | bitmap section offset
	*	frame section
		0x00 | char[4]  | header ("FR1\0")
		*	then, for each frame, it's the following:
			0x00 | short    | number of subframes frame has
			0x02 | short    | time duration (in frames) this frame will display for
			0x04 | int      | index of first subframe, in subframe section	
	*	subframe section
		0x00 | char[4]  | header ("FR2\0")
		*	then, for each subframe, it's the following:
			0x00 | int      | bmp offset (divided by 8)
			0x04 | int      | bmp size (divided by 8)
			0x08 | int      | palette number
			0x0C | short    | format
			0x0E | short    | bitmap width (rounded up to nearest 8 dots)
			0x10 | short[2] | bitmap dimensions (X,Y)
			0x14 | short[2] | X,Y offset when drawing
	*	palette section
		0x00 | char[4]  | header ("PAL\0")
		0x04 | int      | palette size (uncompressed, 0 if file contains no palette)
		*	then, only if the file has a palette, the following:
			0x08 | int      | palette size (compressed)
			0x0C | short[]  | palette data (zlib-compressed)
	*	bitmap section
		0x00 | char[4]  | header ("CEL\0")
		0x04 | int      | bitmap size (uncompressed)
		0x08 | int      | bitmap size (compressed)
		0x0C | char[]   | bitmap data (zlib-compressed)

NGI files are bitmaps that contain one, static image. Optionally,
they may be split into multiple sub-images.

NGI files contain 3 sections:
	*	header section
		0x00 | char[4]  | header ("NGI\0")
		0x04 | int      | format
		0x08 | short    | bitmap width (rounded up to nearest 8 dots)
		0x0A | short[2] | bitmap dimensions (X,Y)
		0x0E | short    | sub-image count
		0x10 | short[2] | sub-image dimensions
		0x14 | int      | size of each sub-image
		0x18 | int      | palette section offset
		0x1C | int      | bitmap section offset
	*	palette section
		0x00 | char[4]  | header ("PAL\0")
		0x04 | int      | palette size (uncompressed, 0 if file contains no palette)
		*	then, only if the file has a palette, the following:
			0x08 | int      | palette size (compressed)
			0x0C | short[]  | palette data (zlib-compressed)
	*	bitmap section
		0x00 | char[4]  | header ("CEL\0")
		0x04 | int      | bitmap size (uncompressed)
		0x08 | int      | bitmap size (compressed)
		0x0C | char[]   | bitmap data (zlib-compressed)

NGM files contain 4 sections:
	*	header section
	*	map section
		
	*	palette section
		0x00 | char[4]  | header ("PAL\0")
		0x04 | int      | palette size (uncompressed, 0 if file contains no palette)
		*	then, only if the file has a palette, the following:
			0x08 | int      | palette size (compressed)
			0x0C | short[]  | palette data (zlib-compressed)
	*	bitmap section
		0x00 | char[4]  | header ("CEL\0")
		0x04 | int      | bitmap size (uncompressed)
		0x08 | int      | bitmap size (compressed)
		0x0C | char[]   | bitmap data (zlib-compressed)
