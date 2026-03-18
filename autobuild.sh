#!/bin/bash

#出错就停止
set -e

#如果没有build目录就创建该目录
if [ ! -d $(pwd)/build ]; then
    mkdir $(pwd)/build
fi

rm -rf $(pwd)/build/*

cd $(pwd)/build &&
    cmake .. &&
    make

#回到项目根目录
cd ..

#把文件拷贝到/usr/include/mymuduo   把so库拷贝到/usr/lib    PATH
if [ ! -d /usr/include/mymuduo ]; then
    mkdir /usr/include/mymuduo
fi

for header in $(ls *.h)
do
    cp $header /usr/include/mymuduo
done

cp $(pwd)/lib/libmymuduo.so /usr/lib

ldconfig