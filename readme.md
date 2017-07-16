This is a tool I've been making for fun over the past few weeks. It's purpose
is to take "munged" file for Pandemic's Star Wars Battlefront (2004) and Star Wars Battlefront
II (2005) and get them back into a state the modtools can understand.

It is by no means perfect (or finished), but I may never be happy with it so I
may as well release it into the wild now. So that if nothing else I at least
released it. Hopefully someone finds some good use for it, although given the
age of the games it's targetting I have doubts about that.

## Usage

```
swbf-unmunge <options>

Options:
 -file <filepath> Set the input file to operate on.
 -version <version> Set the game version of the input file. Can be 'swbf_ii' or 'swbf. Default is 'swbf_ii'.
 -imgfmt <format> Set the output image format for textures. Can be 'tga', 'png' or 'dds'. Default is 'tga'.
 -platform <format> Set the platform the input file was munged for. Can be 'pc', 'ps2' or 'xbox'. Default is 'pc'.
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
World Info | Most world info is recovered in a good quality fashion. There may be issues with the positioning of objects and the like however.
Path Planning | All info is recovered except dynamic pathing groups and path weights.
Terrain | Height, colour, most texture info and some limited water info is recovered. Terrain cuts and foliage are still not recovered.
Models | Recovered mostly. The recovered collision mesh for models is rejected by the munger and other things like material information may be wrong.
Localization | Barely recovered, it will save a dump of the hash keys and their values while also saving the munged chunk as well.

For everything else it will be saved as a `chunk_*.munged` that can be passed back to levelpack.

## Building

If you have Visual Studio 2017 all you need to do is use [vcpkg](https://github.com/Microsoft/vcpkg)
to grab these libraries and you'll be good to go.

* [gsl](https://github.com/Microsoft/gsl/)
* [DirectXTex](https://github.com/Microsoft/DirectXTex/)
* [glm](https://github.com/g-truc/glm)
* [Threading Building Blocks](https://www.threadingbuildingblocks.org/)

Otherwise things are going to be a bit more complicated if you're wanting to build it
for a platform that isn't Windows. Most of the code is clean standard C++ though, save a
couple `#pragma` directives.

If you for some reason do want to build it on Linux or something feel free to get in
touch I am happy to help point out what bits of the codebase are non-portable and what
could be done.
