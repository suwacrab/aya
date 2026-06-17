An image converter i've been using for my projects. WIP, use it at your own risk.

available formats:

-	PC (mostly unused): `.PGA`, `.PGI`
-	Dreamcast (mostly unused, as of now): `.MGI`
-	Saturn:
	*	**⚠️ Assume all data is big-endian!**
	*	`.NGA`: Used for 2D animations.
	*	`.NGI`: Used for static 2D images.
	*	`.NGM`: Used for storing tilemaps.
-	GBA:
	*	`.AGA`: Used for 2D animations.
	*	`.AGE`: Used for 2D part-based animations.
	*	`.AGI`: Used for static 2D images.
	*	`.AGM`: Used for storing tilemaps.
-	GB/GBC:
    *   `.HGI`: Used for static 2D images.
    *   `.HGM`: Used for storing tilemaps.

---
# Building
---

Requires clang & zlib.

to build:

```bash
make rebuild
```

If your PC has the threads for it, you may instead compile faster using 4
threads via:

```bash
make rebuild -j4 --output-sync
```

Adjust `-j4` depending on how much threads you want to use.

---
# Usage
---

To convert the image file `<source image>`, and `-o`utput it to the file
`<output file>`, you would use the following command:

```
aya -i <source image> -o <output file> <options>
```

Use `aya` on its own without any additional parameters to view all the
available options.

Example: converting a 4bpp aseprite JSON+.PNG to a .NGA animation file:

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

-   .json files not using the `Array` option will not be parsed correctly.
-   Your image file must be a .png.

### Usage notes: .NGA
---

Each animation frame is divided into multiple subimages (though, as of now,
each frame only has one subimage.)

Each subimage has an X/Y coordinate that's used from offsetting them from the
animation's origin. This is useful if you want to hardcode an image's center
without having to alter your code.

Setting the offset of each subframe is done via the `nga_useroffset` option.
For example, to offset your animation to the upper-left by 16 horizontal pixels
and 8 vertical pixels, `-nga_useroffset 16 8`.

### Usage notes: .AGA
---

Each subimage has an X/Y coordinate that's used from offsetting them from the
animation's origin. This is useful if you want to hardcode an image's center
without having to alter your code.

Setting the offset of each subframe is done via the `aga_useroffset` option.
For example, to offset your animation to the upper-left by 16 horizontal pixels
and 8 vertical pixels, `-aga_useroffset 16 8`.

---
# Format specification
---

Expect all the formats' specifications to be changing rapidly. Each file is
separated into multiple sections, and you can access them by using the section
offsets in each file's header.

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

-	`0`: I4 / 16-color graphics. Each byte contains the left pixel as the low
    nybble, and the right pixel as the high nybble.
-	`1`: I8 / 256-color graphics. Each byte corresponds to one pixel.
-	`2`: rgb / 32,768-color graphics. Each `uint16` corresponds to one pixel,
    in the format XBBBBBGGGGGRRRRR.

### GBA Image Formats
---

AGA files contain sprite animations. In structure, they're a bit similar to
.NGA files. That is, they contain:

-	frames, which store animation metadata (animation frame durations)
-	subframes, which each correspond to one hardware GBA sprite. Each frame
    consists of zero (if blank) or more subframes.
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
*	frame section (array)
	*	for each frame, it's the following:
		0x00 | ushort    | number of subframes frame has
		0x02 | ushort    | bitmap size
		0x04 | int       | frame's bitmap data offset (relative to bitmap section offset)
		0x08 | ushort[4] | index of subframes in subframe section, for each mirror orientation*
		0x10 | int       | duration (in frames) this frame will display for
*	subframe section (array)
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

---

AGE files are used for storing animations stored by EDGE2's .anm files. Each
file (referred to as a "bank") contains an array of patterns, and each pattern
contains an array of frames, and each frame contains an array of parts, which
are little sub-images, cut from a larger image.

-	Bank
	-	Header
	-	Pattern table
		-	Frame table
			-	Part table
	-	Load descriptions
	-	String data (stores names of patterns & frames)
	-	Palette data
	-	Bitmap data

For this reason, it's referred to as part-based animation, as the main point of
this format is the fact that parts can be repositioned and reused throughout a
bank, both to save CPU and to save VRAM usage. For a visualization, see how
games like [Princess Crown](https://youtu.be/-Dm4XcPnZq8?t=629) or Yoshi's Island are animated.

To deal with supporting the reuse of parts, AGE files require the use of data
structures referred to as "load description tables". These contain an array of
load descriptions, which contain a list of cels to copy to RAM, and how much
cels to copy to RAM. This is so a frame can use parts that are stored
non-sequentially in ROM. These also allow VRAM to either contain an entire
bank, an entire pattern, or just one single frame, to allow loading animation
data on the fly.

The entire bank has a load description table, so it provides both a starting
index of its first load description in the load description section, along with
the amount of descriptions + the total size of the data the load descriptions
contain. Each pattern and frame also have load description tables as well,
which work the same way.

Pseudocode for dealing with load descriptions from the GBA's side would be the
following:

```
fn vram_loadAGEBank(bank,output_ptr)
	for i = 0,bank.loaddesc_length do
		var loaddesc = bank.loaddesc_section[bank.loaddesc_idx + i]
		// load descriptions' size and offsets are in units of 32 bytes.
		vram_memcpy(
			output_ptr,
			pattern.bitmapsection_ptr + (loaddesc.src_offset * 32),
			loaddesc.src_size * 32
		)

		output_ptr += loaddesc.src_size
	end
end

fn vram_loadAGEFrame(bank,patternID,frameID,output_ptr)
	var pattern = bank.pattern_section[patternID]
	var frame = bank.frame_section[pattern.frame_idx + frameID]

	for i = 0,frame.loaddesc_length do
		var loaddesc = bank.loaddesc_section[frame.loaddesc_idx + i]
		// load descriptions' size and offsets are in units of 32 bytes.
		vram_memcpy(
			output_ptr,
			pattern.bitmapsection_ptr + (loaddesc.src_offset * 32),
			loaddesc.src_size * 32
		)

		output_ptr += loaddesc.src_size
	end
end
```

The .AGE file structure is the following:

```
*	header section
	0x00 | char[4]   | header ("AGE\0")
	0x04 | int       | format
	0x08 | short     | per-bank load description index
	0x0A | short     | per-bank load description length
	0x0C | short     | per-bank load description data size (units of 32 bytes)
	0x0E | short     | pattern count
	0x10 | int       | load description section offset
	0x14 | int       | pattern section offset
	0x18 | int       | string section offset
	0x1C | int       | frame section offset
	0x20 | int       | part section offset
	0x24 | int       | bitmap section offset
	0x28 | int       | palette section offset
*	load description section (array)
	*	for each load description, it's the following:
		0x00 | short     | size (units of 32 bytes)
		0x02 | short     | source cel offset (units of 32 bytes)
*	pattern section (array)
	*	for each pattern, it's the following:
		0x00 | short     | per-pattern load description index
		0x02 | short     | per-pattern load description length
		0x04 | short     | per-pattern load description data size (units of 32 bytes)
		0x06 | short     | starting index of pattern's frames in frame section
		0x08 | short     | number of frames
		0x0A | short     | filler (0)
		0x0C | int       | starting offset of pattern's name in string section
*	string section
	0x00 | char[]    | string data
*	frame section (array)
	*	for each frame, it's the following:
		0x00 | short     | per-frame load description index
		0x02 | short     | per-frame load description length
		0x04 | short     | per-frame load description data size (units of 32 bytes)
		0x06 | short     | delay until next frame is displayed (1==1 frame)
		0x08 | short     | frame's part count
		0x0A | short[4]  | frame's part indices, one for each mirror orientation*
		0x12 | short     | filler (0)
		0x14 | int       | starting offset of frame's name in string section
*	part section (array)
	*	for each part, it's the following:
		0x00 | short[2]  | offset for displaying (X,Y)
		0x04 | ushort    | OAM attributes**
		0x06 | ushort    | cel ID (per-frame)
		0x08 | ushort    | cel ID (per-pattern)
		0x0A | ushort    | cel ID (per-bank)
		0x0C | uchar[2]  | bitmap dimensions (X,Y)
*	bitmap section
	0x00 | char[]    | bitmap data
*	palette section
	0x00 | short[]   | palette data
```

-	`*`: To ease computations during drawing, each frame contains 4 arrays of parts, with one precomputed for each possible sprite orientation. 0 is no flip, 1 is horizontal-flipped, 2 is vertical-flipped, 3 is HV-flipped.
-	`**`: Bits 0-3 are the palette number. Bits 5-7 correspond to bits 13-15 of OAM attribute 0. Bits 12-15 correspond to bits 12-15 of OAM attribute 1.

---

AGM files are used for storing backgrounds. They each contain a background
tilemap, a palette, and bitmap data.

```
*	header section
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
	0x00 | short[]   | palette data
*	map section
	0x00 | short[]   | map data
*	bitmap section
	0x00 | char[]    | bitmap data
```

---

AGI files are bitmaps that contain one, static image. Optionally, their bitmap
data may be stored split into multiple sub-images. (e.g for fonts, image
atlases, etc.)

```
*	header section
	0x00 | char[4]   | header ("AGI\0")
	0x04 | int       | format
	0x08 | short[2]  | bitmap dimensions (X,Y)
	0x0C | short[2]  | sub-image dimensions
	0x10 | short     | sub-image count
	0x12 | short     | size of each sub-image
	0x14 | int       | palette section size
	0x18 | int       | bitmap section size
	0x1C | int       | palette section offset
	0x20 | int       | bitmap section offset
*	palette section
	0x00 | short[]   | palette data
*	bitmap section
	0x00 | char[]    | bitmap data
```

### GB Pixel Formats
---

The `format` field for the following structures is defined as such:

-	`0`: I2 / 4-color graphics. Graphics are bitplaned; they're paired in
    groups of 2 bytes, with each 2 bytes corresponding to a row of pixels. The
    first byte is bits 0 of each pixel, and the second byte is bits 1 of each
    pixel. Bits are reversed, so the LSB of each byte is actually the rightmost
    pixel.

For example, if you had the bytes:

```txt
0b01010101
0b00110011
```

the pixel data would be the following color indices:

```txt
32103210
```

Just as mentioned earlier, since bits are reversed, it'd be `32103210`, not
`01230123`.

### GB Image Formats
---

HGI files are bitmaps that contain one, static image. Optionally, their bitmap
data may be stored split into multiple sub-images. (e.g for fonts, image
atlases, etc.)

```
*   header section
    0x00 | char[4]   | header ("HGI\0")
    0x04 | short[2]  | bitmap dimensions (X,Y)
    0x08 | short[2]  | sub-image dimensions
    0x0C | short     | sub-image count
    0x0E | short     | size of each sub-image
    0x10 | short     | palette section size
    0x12 | short     | bitmap section size
    0x14 | short     | palette section offset
    0x16 | short     | bitmap section offset
*   palette section
	0x00 | short[]   | palette data
*   bitmap section
	0x00 | char[]    | bitmap data
```

HGM files are used for storing backgrounds. They each contain a background
tilemap, tilemap attributes (used for GBC software), a palette, and bitmap
data.

```
*   header section
    0x00 | char[4]   | header ("HGM\0")
    0x04 | short[2]  | image dimensions (characters)
    0x08 | short[2]  | image dimensions (dots)
    0x0C | short     | palette size
    0x0E | short     | map size
    0x10 | short     | bitmap size
    0x12 | short     | palette section offset
    0x14 | short     | map section offset
    0x16 | short     | attribute section offset
    0x18 | short     | bitmap section offset
*	palette section
	0x00 | short[]   | palette data
*	map section
	0x00 | short[]   | map data
*	attribute section
	0x00 | short[]   | map attribute data
*	bitmap section
	0x00 | char[]    | bitmap data
```

