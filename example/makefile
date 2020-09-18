toy : toy.cpp
	clang++ -g -O3 toy.cpp `llvm-config --cxxflags --ldflags --system-libs --libs all` -o toy

main : main.cpp
	clang++ main.cpp output.o -o main

