# rm -rf build

rm build/bin/fastLEC

mkdir -p build
cd build
cmake ..
make -j
cd ..

# clear

./build/bin/fastLEC -i /mnt/home/zhangxd/EC/ins/test_11/TOP6.aiger -c 8 -t 3600 -v 1 -m ES -p max_iterations 1000000 -p seed 2