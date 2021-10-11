Description:
===========
A WASM port of the popular [units](https://www.gnu.org/software/units/) command line tool, that can now be used in your browser.


API reference:
==============

When you load the WASM module in your browser, you can expose the `convert_unit` function, which works the same way as calling `units --terse HAVE WANT`.
The returned value is the results from the convertion.

Examples:
=========

```
	// Will return "1.609344"
	Module.ccall('convert_unit', 'string', ['string', 'string'], ['1 mile', 'km']);

	// Will return "0.3048"
	Module.ccall('convert_unit', 'string', ['string', 'string'], ['1 foot', 'm']);

	// Will return "0.27777778"
	Module.ccall('convert_unit', 'string', ['string', 'string'], ['1 kilometer per hour', 'meters per second'])
```

To build and use the WASM library:
==================================


1. Install the Emscripten SDK (emsdk):
--------------------------------------
```
    # Get the emsdk repo
    git clone https://github.com/emscripten-core/emsdk.git
    
    # Enter that directory
    cd emsdk

    # Fetch the latest version of the emsdk (not needed the first time you clone)
    git pull

    # Download and install the latest SDK tools.
    ./emsdk install latest

    # Make the "latest" SDK "active" for the current user. (writes .emscripten file)
    ./emsdk activate latest

    # Activate PATH and other environment variables in the current terminal
    source ./emsdk_env.sh
```


2. Configure and make the units project:
----------------------------------------
```
    # Go to the gnu-wasmunits folder
    cd gnu-wasmunits

    # Configure the project using emconfigure utility
    emconfigure ./configure

    # Clean the project (not needed the first time you make the project)
    emmake make clean

    # Make the project
    emmake make
    
    # Compile the wasm wrapper (this will generate a.out.js, a.out.wsm, a.out.data):
    emcc -O3 wasmunits.c units.o getopt.o getopt1.o parse.tab.o -s EXPORTED_FUNCTIONS='["_convert_unit"]' -s EXPORTED_RUNTIME_METHODS='["ccall", "cwrap"]' --preload-file usr/local/share/units/ -s EXIT_RUNTIME=1
```


3. Use the compiled JS in your project:
---------------------------------------
```
    # Copy all a.out.* files to your project folder
    cp a.out.* /srv/web/my-project/
	
	# Include the main javascript file
	<script src="./a.out.js"></script>

    # Call the "convert_unit" using ccall():
    Module.ccall('convert_unit', 'string', ['string', 'string'], ['1inch', 'm']);
```