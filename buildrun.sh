#!/bin/zsh

clang++ -std=c++17 bftdoc.cpp bftpeer.cpp bftpeerwindow.cpp -o bftdoc
echo "All done building"
echo "Running..."
echo ""
./bftdoc 4000 CONNECTED


