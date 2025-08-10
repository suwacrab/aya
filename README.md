An image converter i've been using for my projects. WIP, use it at your own risk.

available formats:

-	PC (mostly unused): `.PGA`, `.PGI`
-	Dreamcast (mostly unused, as of now): `.MGI`
-	Saturn:
	*	Assume all data is big-endian.
	*	`.NGA`: Used for 2D animations.
	*	`.NGI`: Used for static 2D images.
	*	`.NGM`: Used for storing tilemaps.
-	GBA:
	*	`.AGA`: Used for 2D animations.
	*	`.AGM`: Used for storing tilemaps.

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
aya -i <source image> -o <output file> <options>
```

Use `aya` on it's own to view all the available options.

Example: converting a 4bpp aseprite JSON+.PNG to a .NGA file:

```
aya -p -fmt nga i4 -i animation.png -o animation.nga -nga_json animation.json
```

`-p` **must** be specified if the source image has a palette.

When it comes to exporting aseprite's .JSON files, make sure all the border
options are set to the defaults (That is, no `Trim Sprite`). Also, in the
`Output` options, make sure the data exports as `Array` instead of `Hash`.

Use the following settings when exporting an animation via aseprite:

![borders](https://files.catbox.moe/xnfkac.png)

![output](https://files.catbox.moe/8ol3k6.png)

.json files not using the `Array` option will not be parsed correctly.

### Usage notes: .NGA
---

Each animation frame is divided into multiple subimages (though, as of now,
each frame only has one subimage.)

Each subimage has an X/Y coordinate that's used from offsetting them from the
animation's origin. This is useful if you want to hardcode an image's center
without having to alter your code.

Setting said offset is done via the `nga_useroffset` option.

### Usage notes: .AGA
---

Each subimage has an X/Y coordinate that's used from offsetting them from the
animation's origin. This is useful if you want to hardcode an image's center
without having to alter your code.

Setting the offset of each subframe is done via the `aga_useroffset` option.

---
# Format specification
---

Expect all the formats' specifications to be changing rapidly.

### Saturn Pixel Formats
---

The `format` field for the following structures is defined as such:

-	`0`: I4 / 16-color graphics
-	`1`: I8 / 256-color graphics
-	`2`: rgb / 32,768-color graphics

### Saturn Image Formats
---

NGA files are used for storing 2D animations. They store the graphics for each
frame, along with their frame durations.

As it's very common for animations to consist of multiple sprites (e.g a
character and the sword they carry), each animation frame actually consists of
one *or more* subframes. Each subframe carries an X and Y coordinate of where
it should be drawn, relative to the coordinate (0,0).

NGA files can be created by supplying a .json file (exported from aseprite),
along with a source image. Animations converted via aseprite only have one
subframe per frame, as aseprite does not support frames with multiple images.

In the future, however, support for EDGE2 conversion is planned, as it supports
multiple images per frame, allowing full use of subframes.

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

NGM files contain graphics, palette, and a tilemap.

```
*	header section
	0x00 | char[4]  | header ("NGM\0")
	0x04 | int      | format
	0x08 | short[2] | original bitmap dimensions, in dots (X,Y)
	0x0C | short    | sub-image count
	0x0E | short    | size of each sub-image
	0x10 | int      | palette section offset
	0x14 | int      | map section offset
	0x18 | int      | bitmap section offset
*	palette section
	0x00 | char[4]  | header ("PAL\0")
	0x04 | int      | palette size (uncompressed, 0 if file contains no palette)
	*	then, only if the file has a palette, the following:
		0x08 | int      | palette size (compressed)
		0x0C | short[]  | palette data (zlib-compressed)
*	map section
	0x00 | char[4]  | header ("CHP\0")
	0x04 | short[2] | map dimensions (X,Y)
	0x08 | int      | map data size (uncompressed)
	0x0C | int      | map data size (compressed)
	0x10 | char[]   | map data size (zlib-compressed)
*	bitmap section
	0x00 | char[4]  | header ("CEL\0")
	0x04 | int      | bitmap size (uncompressed)
	0x08 | int      | bitmap size (compressed)
	0x0C | char[]   | bitmap data (zlib-compressed)
```

### GBA Pixel Formats
---

The `format` field for the following structures is defined as such:

-	`0`: I4 / 16-color graphics
-	`1`: I8 / 256-color graphics
-	`2`: rgb / 32,768-color graphics

### GBA Image Formats
---

AGA files contain sprite animations. In structure, they're a bit similar to .NGA
files. That is, they contain:

-	frames, which store animation metadata (animation frame durations)
-	subframes, which each correspond to one hardware GBA sprite. Each frame consists of zero (if blank) or more subframes.
-	palette data, which stores the colors the frames will use.
-	bitmap data, which is the 16-color or 256-color characters for each frame.

```
*	header section
	0x00 | char[4]  | header ("AGA\0")
	0x04 | short[2] | image dimensions
	0x08 | int      | format
	0x0C | short    | palette size
	0x0E | short    | frame count
	0x10 | int      | bitmap size
	0x14 | int      | frame section offset
	0x18 | int      | subframe section offset
	0x1C | int      | palette section offset
	0x20 | int      | bitmap section offset
*	frame section
	*	for each frame, it's the following:
		0x00 | ushort    | number of subframes frame has
		0x02 | ushort    | bitmap size
		0x04 | int       | frame's bitmap data offset (relative to bitmap section offset)
		0x08 | ushort[4] | index of subframes in subframe section, for each mirror orientation*
		0x10 | int       | duration (in frames) this frame will display for
*	subframe section
	*	for each subframe, it's the following:
		0x00 | short[2]  | X,Y offset for drawing
		0x04 | ushort    | OAM attributes**
		0x06 | ushort    | character number***
		0x08 | ushort    | character count
		0x0A | uchar[2]  | bitmap dimensions (X,Y)
*	palette section
	0x00 | short[]  | palette data
*	bitmap section
	0x00 | char[]   | bitmap data
```

-	`*`: To ease computations during drawing, each frame contains 4 lists of subframes, with one for each possible sprite orientation. 0 is non-flipped, 1 is horizontal-flipped, 2 is vertical-flipped, 3 is HV-flipped.
-	`**`: Bits 0-3 are the palette number. Bits 5-7 correspond to bits 13-15 of OAM attribute 0. Bits 12-15 correspond to bits 12-15 of OAM attribute 1.
-	`***`: The character number gets incremented by the character count for each object. That is, if it were a 8-frame 8x8 sprite, subframe 0 would have character number 0, subframe 1 would have charnum 1...

AGM files contain a background map, it's palette, and it's bitmap data.

```
	0x00 | char[4]   | header ("AGM\0")
	0x04 | short[2]  | image dimensions (characters)
	0x08 | short[2]  | image dimensions (dots)
	0x0C | int       | format
	0x10 | int       | map size
	0x14 | int       | palette size
	0x18 | int       | bitmap size
	0x1C | int       | palette section offset
	0x20 | int       | map section offset
	0x24 | int       | bitmap section offset
*	palette section
	0x00 | short[]  | palette data
*	map section
	0x00 | short[]   | map data
*	bitmap section
	0x00 | char[]   | bitmap data
```

