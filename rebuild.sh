cd ~/IdeaProjects/quiet-project/quiet-lwip

rm -rf build

cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build --target all examples

#cmake --install build
