#!/bin/bash

make clean
rm logs/*
rm results/*
rm tests/*
rm peer_configs/super_peers/*
rm peer_configs/weak_peers/*
for i in {1..16}
do
    rm peer_files/p${i}_files/*
done