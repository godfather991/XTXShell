### Install xtxshell
```Bash
make # 编译
```
注意：若未安装`readline`库，makefile会自动安装，请授予sudo权限。

### Design

xtxshell是一个使用C语言实现的简单shell，支持以下功能：

-  用户输入命令与参数，能正常执行命令

    - 常规命令使用C语言的`execvp`实现，从而无需从PATH中查找命令的路径。

    - `cd`命令及以下特性使用C语言中的额外处理实现。

-  输入、输出重定向到文件

-  管道

-  后台执行程序

-  作业控制(jobs)

-  历史命令(history)记录

    - 使用文件记录，从而实现shell关闭后历史命令仍然存在。

-  文件名tab补全，各种快捷键

-  环境变量、简单脚本

### Usage

xtxshell是一个使用C语言实现的简单shell，支持以下功能：

-  用户输入命令与参数，能正常执行命令

    ```Bash
    ls -l
    cd .
    cd ..
    ```

-  输入、输出重定向到文件

    ```Bash
    ls -l > ls.txt
    cat < ls.txt
    ```

-  管道

    ```Bash
    ls -l | grep README
    ```

-  后台执行程序

    ```Bash
    sleep 10 &
    ```

-  作业控制(jobs)

    ```Bash
    # In Bash 1
    sleep 10
    
    # In Bash 2
    sleep 10 &
    jobs
    ```

-  历史命令(history)记录

    - 打开shell时自动加载历史命令，关闭shell时自动保存历史命令

    - 上下键切换历史命令

-  文件名tab补全，各种快捷键

    - 支持文件名tab补全

    - 支持`Ctrl+C`在内的快捷键

    - 支持`Ctrl+D`退出shell

-  环境变量、简单脚本

    ```Bash
    # 设置环境变量
    export aaa=1000
    echo ${aaa}
    
    # 运行脚本
    chmod -R 777 .
    ./test.sh
    ```