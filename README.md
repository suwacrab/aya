An image converter i've been using for my projects. Use it at your own risk.

available formats:

-	PC (mostly unused): `.PGA`, `.PGI`
-	Dreamcast (mostly unused, as of now): `.MGI`
-	Saturn:
	*	Assume all data is big-endian.
	*	`.NGA`: Used for 2D animations.
	*	`.NGI`: Used for static 2D images.
	*	`.NGM`: Used for storing tilemaps.

---
# Building
---

Requires clang, zlib, and freeimage.

to build:

```
make clean && make all
```

---
# Usage
---

```
aya -i <source image> 
```

---
# Format specification
---

Expect all the formats' specifications to be changing rapidly.

### Saturn
---

NGA files are used for storing 2D animations, with each frame being stored
sequentially one frame after the other. Each frame is also divided into
subframes, for animations that use multiple layers/parts (WIP, animations only
have 1 subframe per frame, for now.)

NGA files are created by supplying a .json file (exported from aseprite), along
with a source image.

```
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
```

NGI files are bitmaps that contain one, static image. Optionally,
their bitmap data may be stored split into multiple sub-images. (e.g for fonts,
image atlases, etc.)

```
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
```

NGM files (WIP.)

```
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
```
