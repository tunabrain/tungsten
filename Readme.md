![Tungsten Sample Render](https://raw.githubusercontent.com/tunabrain/tungsten/master/Header.jpg "Tungsten Sample Render")

# The Tungsten Renderer #

## About ##

Tungsten is a physically based renderer originally written for the [yearly renderer competition at ETH](http://graphics.ethz.ch/teaching/imsynth14/competition/competition.php). It simulates full light transport through arbitrary geometry based on unbiased integration of the [rendering equation](http://en.wikipedia.org/wiki/Rendering_equation). To do this, Tungsten supports various light transport algorithms such as bidirectional path tracing, progressive photon mapping, primary sample space metropolis light transport and more.

Tungsten is written in C++11 and makes use of Intel's high-performance geometry intersection library [embree](http://embree.github.io/). Tungsten takes full advantage of multicore systems and tries to offer good performance through frequent benchmarking and optimization. At least SSE3 support is required to run the renderer.

## Documentation ##

Documentation is planned, but currently unavailable (sorry!). A lengthy overview of features is available in the [final project report](http://graphics.ethz.ch/teaching/imsynth14/competition//1st%20Place..%20Benedikt%20Bitterli/report.html) written as part of the submission for the rendering competition, although that document may be outdated.

A small selection of example scenes can be found in `data/example-scenes`. There is also a material test scene found in `data/materialtest`, which contains the Tungsten material test ball that you can use to test different materials and lighting setups..

## License ##

To give developers as much freedom as is reasonable, Tungsten is distributed under the [libpng/zlib](http://opensource.org/licenses/Zlib) license. This allows you to modify, redistribute and sell all or parts of the code without attribution.

Note that Tungsten includes several third-party libraries in the `src/thirdparty` folder that come with their own licenses. Please see the `LICENSE.txt` file for more information.

## Compilation ##

### Windows + MinGW
To build using MinGW, you will need a recent CMake + gcc version. CMake binary distributions are available [here](http://www.cmake.org/download/). I recommend using MinGW-w64 available [here](http://sourceforge.net/projects/mingw-w64/).

In the root folder of the repo, use these commands in an MSYS shell to build:

    ./setup_builds.sh
	cd builds/release
	make

The binaries will be placed in the `build/release` directory after buiding.

Optionally, you can install the Qt framework to build the scene editor utility. If Qt is not detected during setup, the scene editor will not be built (you will still be able to use the renderer).

### Windows + MSVC
To build using MSVC, you will need a recent CMake version and MSVC 2013. CMake binary distributions are available [here](http://www.cmake.org/download/).

After installing CMake, double click the `setup_builds.bat` file. The MSVC project will be created in the folder `vstudio`. Open the `vstudio/Tungsten.sln` solution and press F7 to build.

### Linux ###

Building on Linux works just like building on MINGW.

## Usage ##

The core renderer can be invoked using

	tungsten scene.json

You can also queue up multiple scenes using

	tungsten scene1.json scene2.json scene3.json

and so forth. All renderer parameters, including output files, are specified in the json file.

You can test your local build by rendering the material test scene in `data/materialtest/materialtest.json`.

You can also use

    tungsten --help
    
for more information.

## Code structure ##

`src/core/` contains all the code for primitive intersection, materials, sampling, integration and so forth. It is the beefy part of the renderer and the place to start if you're interested in studying the code.

`src/thirdparty` contains all the libraries used in the project. They are included in the repository, since most of them are either tiny single-file libraries or, in the case of embree, had to be modified to work with the renderer.

`src/tungsten` contains the rendering application itself, which is just a small command line interface to the core rendering code.

All other folders in `src` are small utilities described below.

## Additional utilities ##

Apart from the core renderer, Tungsten also comes with several tools to make content creation easier.

### tungsten_server ##

This is a standalone version of the renderer that comes with a built-in HTTP status server.

It will serve the following files:

- `/render`: The current framebuffer (possibly in an incomplete state).
- `/status`: A JSON string containing information about the current render status.
- `/log`: A text version of the render log.

Use

    tungsten_server --help
    
for more information.

### scenemanip ##

`scenemanip` comes with a range of tools to manipulate scene files, among others the capability to package scenes and all references resources (textures, meshes, etc.) into a zip archive.

Use

    scenemanip --help
    
for more information.

### hdrmanip ##

`hdrmanip` comes with a few useful tools for manipulating HDR output files produced by the renderer. Where available, we recommend using established image manipulation tools, but where HDR support is sparsely available, this tool can be useful.

To convert an HDR image to low dynamic range, use

	hdrmanip -o output.png input.exr

You can also specify the tonemapping operator to be applied when converting. For example, to use a filmic tonemap, use

	hdrmanip --tonemap filmic -o output.png input.exr

To adjust the exposure of an HDR file, you can use

	hdrmanip --exposure -1.0 -o output.exr input.exr

If you don't specify an output file, the tool will overwrite the input file. You can also modify several input files at once. For example, you can use 

	hdrmanip --exposure -1.0 input1.exr input2.exr input3.exr

to adjust the exposure of several files at once.

If you don't want to overwrite the input files, you can specify an output directory with `-o`

	hdrmanip -o outputdir --exposure -1.0 input1.exr input2.exr input3.exr 

When specifying multiple files, `-o` is interpreted as an output directory, and its file extension is ignored. If you want to convert multiple input files to a different file format or even low dynamic range, you can use `--file-type`

	hdrmanip --file-type png input1.exr input2.exr input3.exr

Finally, `hdrmanip` also supports merging multiple HDR files together. This is useful when you split up a render across several machines and want to average the outputs to get a less noisy image. You can use `--merge` for this

	hdrmanip --merge -o output.exr input1.exr input2.exr input3.exr

Of course, you can also adjust the exposure and convert to low dynamic range while merging, all in one step.

Use

	hdrmanip --help

for more information.

### obj2json ##
The command

    obj2json srcFile.obj dstFile.json

will parse the Wavefront OBJ `srcFile.obj`, including object hierarchy and materials, and create a scene file `dstFile.json` reproducing hierarchy and material assignments.

In addition, meshes in the OBJ file will be automatically converted and saved to Tungsten's native `*.wo3` mesh format, which significantly reduces loading time for large meshes. This may create a large number of new files - consider pointing the `dstFile.json` path into an empty subfolder to avoid file clutter.

### json2xml ##
The command

    json2xml srcFile.json dstFile.xml

will parse the scene file `srcFile.json` and convert it to an XML scene description compatible with the [Mitsuba renderer](https://www.mitsuba-renderer.org/). All `*.wo3` files will be converted to Mitsuba compatible OBJs, which may create a large number of new files - consider putting `dstFile.xml` into an empty subfolder to avoid file clutter. 

Note that this tool does not experience heavy testing and does not support all features of Tungsten, so it may not always work as expected.

### editor ##
This is a minimalist scene editor written in Qt and OpenGL. It supports camera setup, manipulating transforms, compositing scenes and a few more features that I found useful.

It is usable, but a few crucial features are currently missing from it (including documentation). The code is neither exceptionally clean nor stable, so use at your own risk.
