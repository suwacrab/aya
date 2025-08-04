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
aya -i <source image> -o <output file> <options>
```

Use `aya` on it's own to view all the available options.

Example: converting a 4bpp aseprite JSON+.PNG to a .NGA file:

```
aya -p -fmt nga i4 -i animation.png -o animation.nga -nga_json animation.json
```

`-p` **must** be specified if the source image has a palette.

### Usage notes: .NGA
---

Each animation frame is divided into multiple subimages (though, as of now,
each frame only has one subimage.)

Each subimage has an X/Y coordinate that's used from offsetting them from the
animation's origin. This is useful if you want to hardcode an image's center
without having to alter your code.

Setting said offset is done via the `nga_useroffset` option.

Also, when it comes to exporting aseprite's .JSON files, make sure all the border
options are set to the defaults (That is, no `Trim Sprite`). Also, in the
`Output` options, make sure the data exports as `Array` instead of `Hash`.

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

AGA files contain sprite animations. They're similar to NGA files.

```
*	header section
	0x00 | char[4]  | header ("AGA\0")
	0x04 | int      | format
	0x08 | short    | frame count
	0x0A | short    | frame section count
	0x0C | int      | frame section offset
	0x10 | int      | subframe section offset
	0x14 | int      | bitmap section offset
	0x18 | int      | palette section offset
*	frame section
	*	for each frame, it's the following:
		0x00 | short    | number of subframes frame has
		0x02 | short    | time duration (in frames) this frame will display for
		0x04 | int      | index of first subframe, in subframe section	
*	subframe section
	*	for each subframe, it's the following:
		0x00 | int      | bmp offset (divided by 8)
		0x04 | int      | bmp size (divided by 8)
		0x08 | int      | palette number
		0x0C | short    | format
		0x0E | short    | bitmap width (rounded up to nearest 8 dots)
		0x10 | short[2] | bitmap dimensions (X,Y)
		0x14 | short[2] | X,Y offset when drawing
*	palette section
	0x00 | short[]  | palette data
*	bitmap section
	0x00 | char[]   | bitmap data
```

