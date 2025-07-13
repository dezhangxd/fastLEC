clear;
clear;

# rm -rf build

rm build/bin/fastLEC

mkdir -p build
cd build
cmake ..
make -j
cd ..

# clear

./build/bin/fastLEC -i /mnt/home/zhangxd/EC/ins/miter_right/bit8/mul8o08.aig -m BDD
# ./build/bin/fastLEC -i /mnt/home/zhangxd/EC/ins/miter_error/bit8/miter8o08.aig -m BDD
# -c 1 -t 3600 -v 1 -m BDD -p max_iterations 1000000 -p seed 2