Some quick tools for decoding images from [Stella Glow](http://www.atlus.com/stellaglow/).

They should work on most non-windows systems.

**If you are compiling using gcc: `export CFLAGS="-lstdc++"` now**

## cmp

A standalone version of the [LZSS decompressor from Ohana3DS](https://github.com/gdkchan/Ohana3DS-Rebirth/blob/master/Ohana3DS%20Rebirth/Ohana/Compressions/LZSS.cs).

    make cmp
    # writes decompressed data to stdout - "> out.arc" is shell syntax.
    ./cmp in.arc.cmp > out.arc

    # decompress every .cmp file in ./romfs
    find ./romfs -name "*.arc.cmp" | while read file; do
        mkdir -p path_to_mirror_romfs/$(dirname $file)
        /full_path_to/cmp $file > path_to_mirror_romfs/${file%.cmp}
    done

## arc

An extractor for [SARC](http://mk8.tockdom.com/wiki/SARC_(File_Format)) files.
Uses code from [EveryFileExplorer](https://github.com/Gericom/EveryFileExplorer) for BFLIM->PNG conversion.

    make arc
    ./arc l in.arc
    ./arc x in.arc out_folder
    ./arc o in.arc timg/base.bflim out.png

General syntax:

    ./arc [cmds...] [args for cmd 1]... [args for cmd 2]... [args for cmd n]

You can extract multiple files at once by chaining commands:

    ./arc xx in0.arc in0 in1.arc in1

You can control the formats `arc` outputs with the `f` command:

    # I want to extract only BFLIM files (and convert them to PNG)
    ./arc fx b in.arc out
    # I want to disable conversion
    ./arc fx _ in.arc out

| Format code | Description                     |
|-------------|---------------------------------|
| `_`         | Raw files                       |
| `b`         | BFLIM texture (converts to PNG) |
