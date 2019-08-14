if ! [ -x "$(command -v python)" ]; then
  echo 'Error: python is not installed. Try sudo apt-get install python' >&2
  exit 1
fi

cd emsdk
source emsdk_set_env.sh
cd ..

OPENCV=opencv-4.0.1

rm cardspotter.html
rm cardspotter.js
rm cardspotter.wasm

rm ../MagicCardSpotter/cardspotter.html
rm ../MagicCardSpotter/cardspotter.js
rm ../MagicCardSpotter/cardspotter.wasm

em++ -s WASM=1 -std=c++11 -O3 -s BINARYEN_ASYNC_COMPILATION=1 --memory-init-file 1 --llvm-lto 1 -s ASSERTIONS=0 -s FETCH=1 -s SAFE_HEAP=0 -s AGGRESSIVE_VARIABLE_ELIMINATION=1 -s EXCEPTION_DEBUG=0 -s NO_EXIT_RUNTIME=1 -s DEMANGLE_SUPPORT=0 -s DISABLE_EXCEPTION_CATCHING=1 -s TOTAL_MEMORY=268435456 -s USE_LIBPNG=1 -s VERBOSE=0 -s ERROR_ON_UNDEFINED_SYMBOLS=1 -s USE_ZLIB=1 cardspotterjs.cpp QueryThread.cpp CardDatabase.cpp CardData.cpp $OPENCV/build_wasm/lib/libopencv_features2d.a $OPENCV/build_wasm/lib/libopencv_imgproc.a $OPENCV/build_wasm/lib/libopencv_imgcodecs.a $OPENCV/build_wasm/lib/libopencv_core.a -I$OPENCV/build_wasm/ -I$OPENCV/modules/core/include/ -I$OPENCV/modules/imgcodecs/include/ -I$OPENCV/modules/highgui/include/ -I$OPENCV/modules/photo/include/ -I$OPENCV/modules/imgproc/include/ -I$OPENCV/modules/features2d/include/ -o cardspotter.html  -s 'EXTRA_EXPORTED_RUNTIME_METHODS=["ccall", "cwrap", "Pointer_stringify"]' -s EXPORTED_FUNCTIONS="['_main','_LoadDatabase', '_TestFile', '_TestBuffer', '_AddScreen', '_FindCard', '_SetCardPool', '_SetSetting']" 

cp cardspotter.js ../MagicCardSpotter/cardspotter.js
cp cardspotter.wasm ../MagicCardSpotter/cardspotter.wasm

