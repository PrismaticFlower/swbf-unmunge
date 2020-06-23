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

## Building

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
   
Otherwise things are going to be a bit more complicated if you're wanting to build it
for a platform that isn't Windows. Most of the code is clean standard C++ though, save a
couple `#pragma` directives.

If you for some reason do want to build it on Linux or something feel free to get in
touch I am happy to help point out what bits of the codebase are non-portable and what
could be done.
