# xxfpmW

#### 介绍

windows系统防php崩溃的固定进程数工具，基于原xxfpm，但改进为可后台运行，运行和错误日志保存在txt文件中。去掉linux相关代码，解决关机时会弹出错误窗口提示的问题。


#### 软件架构

C语言开发，编译工具为MinGW。


#### 使用说明

1. 将xxfpmW 文件夹放到自己系统对应的 php-cgi.exe 所在文件夹下。
 
    ![xxfpmW放置位置](https://images.gitee.com/uploads/images/2020/1227/155011_9f54239d_64383.png "xxfpmW放置位置.png")

2. 鼠标右键编辑修改 xxfpmW.bat 中的启动进程数（默认为5）和监听端口（默认9000），如果不需要修改则忽略此步骤，直接执行步骤3。

3. 双击文件 xxfpmW.vbe 运行即可，可在windows任务管理器中查看已运行的进程，也可通过日志查看运行情况。 

    ![xxfpmW在任务管理器运行情况](https://images.gitee.com/uploads/images/2020/1227/160006_99035bc5_64383.png "xxfpmW运行情况.png")
    
    ![xxfpmW运行日志](https://images.gitee.com/uploads/images/2020/1227/160259_1d318603_64383.png "xxfpmW运行日志.png")


#### 手动关闭

   不能直接关闭 php-cgi.exe （即任务管理器中的 CGI / FastCGI），因为启动了监听规则，会立刻开启新的进程，必须按如下步骤关闭。

1. 任务管理器中右键 Microsoft ® Windows Based Script Host 结束任务。

2. 任务管理器中右键 xxfpmW.exe 结束任务。


#### 编译教程

   如果想自己手动尝试编译生成exe文件，可尝试以下步骤，否则可忽略此内容  

1. 安装 MinGW 编译工具，并配置对应系统环境变量。安装教程自行查询。国内由于墙的原因可能安装gdb失败，（gdb是用于调试的）对于编译没有影响。

2. 复制粘贴编译该项目需要的引用文件。

    > pthread.h 、 sched.h、 semaphore.h 三个头文件放到 MinGW/include文件夹，libpthread.a 放到 MinGW/lib文件夹。
    
3. 启动 MinGW 执行编译，成功后会生成 xxfpmW.exe 文件。

    `gcc -Werror -o xxfpmW win10.c -lpthread -lWs2_32 -s`



#### 鼓励

   因为C语言并不是我的日常工作语言，php也是最近才接触，平时要忙工作，开发此工具耗费了不少精力，具体经过可以查看 [xxfpmW 的诞生过程](http://https://www.cnblogs.com/jying/p/14075547.html)，如果该工具解决了您长久以来使用 php 或 xxfpm 的问题，或者为新晋学员带来了方便，那么欢迎您打赏个馒头钱以示鼓励，多少您随意，毕竟经济基础决定上层建筑，解决温饱后才能继续探索其它更多。


 **图一微信** **图二支付宝** 

![微信](https://images.gitee.com/uploads/images/2020/1227/170423_06c13269_64383.png "微信截图_20201227170341.png")
![支付宝](https://images.gitee.com/uploads/images/2020/1227/170546_a0a88f63_64383.png "微信截图_20201227170404.png")