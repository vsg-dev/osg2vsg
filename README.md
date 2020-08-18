osg2vsg is DECPREACTED.  Please use vsgXchange instead.

--

osg2vsg is a small utility library and application that converts OpenSceneGraph images and 3D modules into [VSG](https://github.com/vsg-dev/VulkanSceneGraph)/Vulkan equivalents.

## osg2vsg application usage

The osg2vsg application can be used to load OSG models, rendering them using the VSG, and write these converted scene graphs to native VSG ascii or binary files.

To load an OSG model, convert to VSG and render it with VSG:

	osg2vsg dumptruck.osgt

To load an OpenSceneGraph and convert to VSG, and write out to VSG binary file:

	osg2vsg lz.osgt -o lz.vsgb

To view a generated VSG file you can use the vsgviewer from vsgExamples:

	vsgviewer lz.vsgb

To aid performance comparisons between the OSG and VSG you can record animation paths using osgviewer and then run osg2vsg or vsgviewer to render them and follow the animation path.  To record in osgviewer press 'z' to start recording, press 'Z' to complete recording and will write out a saved_animation.path file.  This file then can be used in both osgviewer, osg2vsg and vsgviewer:

	osgviewer lz.osgt -p saved_animation.path
	osg2vsg lz.osgt -p saved_animation.path
	vsgviewer lz.vsgb -p saved_animation.path

The osg2vsg command line options to help control the loading and rendering:

    -o file.ext       # write out loaded/converted models to OSG or VSG (.vsga, .vsgb) files
	--fs              # select fullscreen
	-w width height   # select window of width and height dimensions
	--IMMEDIATE       # select presentation IMMEDIATE mode to switch frame wait for vysnc
    -p animation.path # load an OSG animation path and animate the viewer camera along path
                      # average framerate is written to the console, in the same manner
                      # that osgviewer does when following the path to allow 1:1 comparison
    -d 				  # enable Vulkan debug layer which outputs errors to console
    -a 				  # enable Vulkan API layer which outputs Vulkan API calls to console

## Quick build instructions for Unix from the command line

To build and install in source

    git clone https://github.com/vsg-dev/osg2vsg.git
    cd osg2vsg
    cmake .
    make -j 8
    sudo make install


## Quick build instructions for Windows using Visual Studio 2017

You'll need a build of OpenSceneGraph to use osg2vsg, if you don't have one go ahead and download and build the source.

    https://github.com/openscenegraph/OpenSceneGraph

You'll also need glslLang inorder to compile GLSL shaders to Vulkan compatible SPIR-V shaders at runtime. glslLang can be downloaded and built using the following.

    git clone https://github.com/KhronosGroup/glslang.git
    cd ./glsllang
    ./update_glslang_sources.py
    cmake . -G "Visual Studio 15 2017 Win64"
    
Open the generated Visual Studio solution file (Ensure you start Visual Studi as Admin if installing to the default location). Build install target for debug and release and finally add the install location (default C:\Program Files\glslang) to CMAKE_PREFIX_PATH environment variable.

Now you have OpenSceneGraph and glslLang installed you can go ahead and generate the osg2vsg Visual Studio project.

    git clone https://github.com/tomhog/osg2vsg.git
    cd osg2vsg
    cmake ./ -G "Visual Studio 15 2017 Win64" -DOSG_DIR:PATH="path/to/your/osg/install"
