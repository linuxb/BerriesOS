##cmake 使用

* `set` 命令
```
set(var x y z)
```
定义变量var
将x ，y， z赋值给这个string list

* `link_directories(deps)` 命令在 `add_executable` 前执行，扫描依赖时添加库文件的搜索路径
* `add_executable(exec lib1 lib2 ... libn)`  添加可执行文件并链接库（静态或动态）
* `add_library(lib ${DIR_LIB_SRCS})` 将这个目录下的源文件编译成静态库
* `aux_source_directory(dir DIR_LIB_SRCS)` 将制定目录下的所有文件添加进制定变量，作为一个string list
* `add_subdirectory(dir)` 添加子目录，在子目录中设定同样的CMakefileLists.txt进行子层目录处理
