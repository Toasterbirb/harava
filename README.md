# harava

*Memory scanner/editor for Linux*

> [!WARNING]
> This program might eat up a lot of RAM in certain situations. There's a limit of 8GB by default so that it won't crash the system though

## Building
Build the project with cmake by running the following commands
```sh
mkdir build
cd build
cmake ..
make -j$(nproc)
```

## Installation
To install harava to /usr/local/bin, run the following command
```sh
make install
```
You can customize the installation *PREFIX* and *DESTDIR* variables normally with cmake and make.
