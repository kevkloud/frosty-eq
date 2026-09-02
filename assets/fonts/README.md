# Display faces

Two faces from Tom Gordon Design, supplied by Frosty for this plugin:

    TG - Minerva Black Black    every label on the panel, with letter spacing
                                added to match the mockup
    TG - Blender                INPUT and OUTPUT only

## They are not in this repository

The `.otf` files are gitignored on purpose. They are licensed, not open, and
this repository is public — committing them would redistribute the font files
themselves to everyone who clones it, which no ordinary font licence permits.

Embedding a face *inside a compiled binary* is the ordinary thing a font
licence is bought for. Publishing the font file is not. Keeping the files out
of git is what separates the two.

So: **to build, put the two `.otf` files in this directory first.** CMake
stops with a message naming the missing file if they are absent, rather than
quietly falling back to whatever face the machine has.

CI gets them from two repository secrets, decoded into this directory before
the configure step:

    FONT_TG_MINERVA_BLACK_B64
    FONT_TG_BLENDER_B64

Both are base64 of the corresponding `.otf`. To set or replace one:

    base64 -i assets/fonts/TG-Blender.otf | gh secret set FONT_TG_BLENDER_B64

## Why embedded rather than looked up by name

Looking a family name up at runtime gives every listener a different panel.
The first round of design feedback on this plugin came from two people
reviewing two different typefaces without either of them knowing, because the
placeholder resolved to whatever each machine happened to have installed.

## Where they are named

`theme::labelFont()` and `theme::captionFont()` in `src/gui/Theme.cpp`, and
nowhere else. To swap a face, drop the file in here, point the matching entry
of `FROSTY_FONTS` in `CMakeLists.txt` at it, and change that function.

## Two glyphs the panel draws itself

Neither face carries a slashed O (U+00D8 maps to a plain `O` in both) or a
minus sign (U+2212). The polarity mark, the plus and the minus are drawn as
strokes in `LookAndFeel.cpp` instead. That is also why they match each other
exactly at any size: they are the same code, not three glyphs from two fonts.
