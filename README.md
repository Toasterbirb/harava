# harava

*Memory scanner/editor for Linux*

> [!WARNING]
> This program might consume a significant amount of RAM in certain situations. By default, it has an 8GB limit to prevent system crashes, but temporary usage beyond this limit may occur

## Features
- Search for selected data types or all of them at once
    - Supports signed integers (4 and 8 bytes), floats and doubles
- Modify memory values
- Filter with different comparison operators or find values that have or have not changed since the previous scan

## Example usage
First figure out the PID of the process with `pgrep` etc.
```sh
pgrep -i <process_name>
```
Then give the PID to harava with the `-p` flag. You can also directly pass the PID via shell expansion
```sh
./harava -p $(pgrep -i <process_name>)
```
After harava has identified the memory regions to access, use the `help` command for a list of available commands

## Building
> [!NOTE]
> If cloning from git, remember to clone with the `--recursive` flag

Build the project with cmake by running the following commands
```sh
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```
On some platforms you might also need to use the `-DCMAKE_CXX_FLAGS=-ltbb` flag with cmake

## Installation
To install harava to /usr/local/bin, run the following command
```sh
sudo make install
```
You can customize the installation *PREFIX* and *DESTDIR* variables normally with cmake and make.
