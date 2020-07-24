This is a tool whose purpose is to take "munged" file for Pandemic's Star Wars Battlefront (2004) and Star Wars Battlefront
II (2005) and get them back into a state the modtools can understand.

It is by no means perfect but it can do a reasonable job for most file types of interest.

## Usage

```
swbf-unmunge <options>

Options:
 -file <filepath> Specify an input file to operate on.
 -files <files> Specify a list of input files to operate, delimited by ';'.
   Example: "-files foo.lvl;bar.lvl"
 -version <version> Set the game version of the input file. Can be 'swbf_ii' or 'swbf. Default is 'swbf_ii'.
 -outversion <version> Set the game version the output files will target. Can be 'swbf_ii' or 'swbf. Default is 'swbf_ii'.
 -imgfmt <format> Set the output image format for textures. Can be 'tga', 'png' or 'dds'. Default is 'tga'.
 -platform <platform> Set the platform the input file was munged for. Can be 'pc', 'ps2' or 'xbox'. Default is 'pc'.
 -verbose Enable verbose output.
 -mode <mode> Set the mode of operation for the tool. Can be 'extract', 'explode' or 'assemble'.
   'extract' (default) - Extract and "unmunge" the contents of the file.
   'explode' - Recursively explode the file's chunks into their hierarchies.
   'assemble' - Recursively assemble a previously exploded file. Input files will be treated as directories.
```

So as an example.

```bat
swbf-unmunge -file test.lvl
```

Would save the extracted contents of `test.lvl` into a folder named `test`.

## Recovered Files

File Type | Notes
------------ | -------------
Object Definitions | Recovered nearly perfectly.
Config Files (*.fx, *,sky, etc) | Depending on the file type, recovered perfectly or for certain types poorly. In all cases the name of the file is not recovered.
Textures | Recovered nearly perfectly.
World Info | Most world info is recovered in a very good quality fashion. 
Path Planning | All info is recovered except dynamic pathing groups and path weights.
Terrain | Height, colour, most texture info and some limited water info is recovered. Terrain cuts and foliage are still not recovered.
Models | Recovered mostly. 
Localization | Barely recovered, it will save a dump of the hash keys and their values while also saving the munged chunk as well.

For everything else it will be saved as a `chunk_*.munged` that can be passed back to levelpack, or in some cases it will have a pretty name and the correct extension. It depends on the type of chunk. In either case it can be passed to levelpack.

## Building on Windows

If you have Visual Studio 2019 all you need to do is use [vcpkg](https://github.com/Microsoft/vcpkg)
to grab these libraries and you'll be good to go.

* [gsl](https://github.com/Microsoft/gsl/)
* [DirectXTex](https://github.com/Microsoft/DirectXTex/)
* [glm](https://github.com/g-truc/glm)
* [Threading Building Blocks](https://www.threadingbuildingblocks.org/)
* [json](https://github.com/nlohmann/json/)
* [{fmt}](https://github.com/fmtlib/fmt)

You want the 64-bit versions of these libraries.

After you've installed vcpkg, you'll run the following command (in powershell) in the vcpkg directory:

    .\vcpkg install fmt:x64-windows nlohmann-json:x64-windows tbb:x64-windows ms-gsl:x64-windows DirectXTex:x64-windows glm:x64-windows
   


## Building on Mac/Linux

Shares the same depencencies listed above, though you need the master branches of fmt, json, and glm directly from the git repos, as the apt and brew packages are significantly older than the vcpkg ones. Of course you could use vcpkg as mentioned above, though that seems less common on Mac/Linux.  The port uses CMake for building, though if you prefer Visual Studio all you'd need to do is recursively add the source files in the "unixport" directory to your solution, and remove src/{mapped_file.cpp, handle_texture*.cpp, save_image*.cpp}.

The Unix port shares all functionality of the Windows version, but:

* cannot write DDS files
* can only write textures encoded with BCx/EXTx compression, A8R8G8B8, R5G6R5, or luminance formats (which comprise the vast majority of formats in lvl files) 
* has not been tested with or developed for anything related to PS2/Xbox

Tested on MacOS Catalina, and Ubuntu 18.04.  If you have issues with this port, drop an issue and tag WHSnyder.