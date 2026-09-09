The cjsonGWAppCoDec is just a tiny project for sanity check of cJSON.

Usage: 
> cd cjsonGWAppCoDec
> mkdir build
> cd build
> cmake -DCMAKE_BUILD_TYPE=Debug ..
> make all

Then, the executable file *cjsonGWAppCoDec* generates in *build/*. When running on the ARM-based platform, please copy the all the file in lib/lib/ to the platform. The path should be same as the path of cjsonGWAppCoDec.
