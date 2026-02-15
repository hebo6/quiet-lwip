cd /home/ubuntu/quiet-lwip

rm -rf build
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make all examples
