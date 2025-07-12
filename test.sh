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

./build/bin/fastLEC -i /mnt/home/zhangxd/EC/ins/test_11/TOP1.aiger -m BDD
# -c 1 -t 3600 -v 1 -m BDD -p max_iterations 1000000 -p seed 2