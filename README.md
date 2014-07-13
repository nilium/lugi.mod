Intro
-------------------------------------------------------------------------------

LuGI – pronounced loojie (IPA: [ luʤi ]) – is a framework for building Lua glue code for BlitzMax, and provides much of the ground-work needed to get BlitzMax object support working quickly and painlessly in BlitzMax. The way LuGI does this is by firstly generating glue code for your project, and secondly by using its core API to expose that glue code in a manner that best mimics the way one would use the wrapped code in BlitzMax, thereby providing an almost seamless combination of BlitzMax and Lua.

The code is composed of a module for generating the required glue code to make use of LuGI and a very small core API: two functions to push objects and arrays, two functions to get objects and arrays, and an initialization function. The remainder of the API is hidden away by monks who cannot speak, for they have cut out their tongues. Incidentally, the API is also inside of a header file (lgcore.h) – however, the rest of the LuGI core code is strictly for those interested in writing their own code generators.

Documentation
-------------------------------------------------------------------------------

Documentation is currently viewable on the [project's wiki](http://wiki.github.com/nilium/lugi.mod/).

Installation
-------------------------------------------------------------------------------

To install LuGI, you need to first download the source code.  You have two generally have two options for this:

1. Clone the repository into your `BlitzMax/mod` directory using `git clone git://github.com/nilium/lugi.mod.git`
2. Click the 'Download Source' button at the top of the Source section of the GitHub repository and extract this into your `BlitzMax/mod` directory.

After this, simply go into your nearest terminal and run `bmk makemods lugi` or rebuild the LuGI modules from your IDE of choice.

License
-------------------------------------------------------------------------------

LuGI is currently licensed under the MIT license (same as Lua):

    Copyright (c) 2009, 2014 Noel R. Cower

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.

If you require a different license, please contact me via e-mail at `ncower [at] gmail [dot] com` and we can discuss that.  However, for almost all purposes, I believe the MIT license is very permissive and easy to comply with.