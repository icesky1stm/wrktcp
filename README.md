# wrktcp - 无lua依赖的tcp协议压测wrk工具

* English Version : README_EN.md *

[TOC]

本项目是基于wrk的基础上 取消了ssl和lua依赖，使用tcpini配置用来实现tcp协议下的压力测试。

## 主要功能

1. 整体框架基于wrk做的扩展，统计、大部分命令、输出结果沿用的wrk，增加了部分参数。

   1. 增加 --html 参数，用于输出html结果文件。
   2. 增加--test参数，用于验证ini文件是否正确。
   3. 增加--trace参数，用于在命令行打印时间趋势信息。

2. 支持tcp协议的压力测试，包括各种协议和通讯格式。

3. 支持定义发送报文信息，应答报文信息，以及应答的成功响应的结果判断对比配置。

4. 支持请求报文的参数变量。参数类似loadrunner，有如下四种：
   1. COUNTER, 迭代器，如唯一流水等，可支持定义范围，定义步长。
   2. DATETIME，时间模式，格式与strtime完全一致。
   3. CONNECTID，连接唯一ID，一般用于做为终端号等使用。
   4. FILE,文件模式，支持从文件中顺序读取内容进行内容赋值。

5. 支持压测过程中，动态刷新TPS和延时。

6. 支持记录压测过程信息，并生成html页面进行展示。

7. 也支持http，因为http也是tcp协议的一种,只要http的报文头明确配置在文件中即可，具体可以参加sample_http.ini。

## 与wrk的区别

1. 取消：lua的依赖，自定义的内容和判断不用学习lua，一个配置文件即可。

2. 取消：不依赖任何第三方库，取消ssl和luajit的deps。因此这是一个纯净的(pure)C的工程。

3. 增加：支持tcp协议。使用ini配置文件来配置tcp报文的格式。连接地址、报文约定、请求报文、应答报文信息都在该文件中配置。

4. 增加：动态刷新TPS和延时，在较长时间的压测过程中也能实时的观察压测情况。

5. 增加：直接生成html报表。

6. 增加：执行过程中的实时TPS信息统计刷新。

7. 增加：每个连接只连接一次的测试参数，用于调试ini文件。

8. 改进：thread stat的分布统计，把最大支持TPS从1000W缩减到100W，TPS精度从1增加到0.1。

9. 改进：wrk输出结果，增加了成功和失败的分类，充分考虑了业务级错误。

   


## 编译安装

- macos

cd wrktcp路径
执行make

- linux

cd wrktcp路径
执行make

- windows

需要下载MINGW编译器，再进行编译。


## 快速使用

- 修改sample_tiny.ini配置

```ini
[common]
host = 127.0.0.1
port = 8000

[request]
req_body = this is a test
```

- 执行命令

```sh
wrktcp -t2 -c20 -d10s --latency sample_tiny.ini
```

和wrk的用法类似，本命令代表如下意义：
使用2个线程(-t2),并发20个连接(-c20),保持运行5秒(-d5s),使用配置sample_tiny.ini来运行.
--latency用来输出响应时间的分布情况。

- 输出结果

```
/Users/suitm/code/c/wrktcp>wrktcp -t2 -c20 -d10s --latency sample_tiny.ini
Running 10s loadtest @ 127.0.0.1:8000 using sample_tiny.ini
  2 threads and 20 connections
  Time:10s TPS:2347.30/0.00 Latency:5.90ms BPS:48.14KB Error:0
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    64.19ms  185.36ms   1.11s    91.91%
    Req/Sec     1.42k   417.52     1.92k    73.46%
  Latency Distribution
     50%    6.03ms
     75%    8.17ms
     90%  168.91ms
     99%  983.51ms
  23503 requests in 10.09s, 482.00KB read
Requests/sec:   2328.22    (Success:2328.22/Failure:0.00)
Transfer/sec:     47.75KB
```


- 命令的详细说明

> ## 命令行参数的详细说明
>
> ```
> -t, --threads:     使用线程总数，一般推荐使用CPU核数的2倍-1
> -c, --connections: 连接总数，与线程无关。每个线程的连接数为connections/threads
> -d, --duration:    压力测试时间, 可以写 2s, 2m, 2h
> --latency:     打印延迟分布情况
> --timeout:     指定超时时间，默认是5000毫秒，越长占用统计内存越大。
> --trace: 	   打印出分布图
> --html: 	   将压测的结果数据，输出到html文件中。
> --test:		   每个连接只会执行一次，一般用于测试配置是否正确。
> -v  --version:     打印版本信息
> ```

- 结果的详细解释

> Running 10s loadtest @ 127.0.0.1:8000 using sample_tiny.ini
>   2 threads and 20 connections

代表命令的基本情况以及连接的基本情况，程序执行加载完成配置后立刻打印。



>     Thread Stats   Avg      Stdev     Max   +/- Stdev
>        Latency    64.19ms  185.36ms   1.11s    91.91%
>        Req/Sec     1.42k   417.52     1.92k    73.46%

Latency代表延时时间，Req/Sec代表每秒的成功应答笔数(TPS)，需要注意的是这部分统计是单个线程的统计数据。

说明依次是：线程状态名(Thread Stats)、平均值(Avg)、标准差(Stdev)、最大值(Max)、正负一个标准差内数据所占比例(+/- Stdev)

如果压测系统稳定，平均值和最大值是期望相等的， 标准差的期望值是0，+/-标准差的期望值是100%。



>   Latency Distribution
>     50%   12.99ms
>     75%   14.02ms
>     90%   15.74ms
>     99%  274.89ms

延迟分布的统计数据，50% - 99%是按相应时间进行排序的。50%代表第50%的数据延时是多少99%代表第99%的数据延时是多少。



>     23503 requests in 10.09s, 482.00KB read
>Requests/sec:   2328.22    (Success:2328.22/Failure:0.00)
>   Transfer/sec:     47.75KB

第一行代表： 在10.11秒内有6862个请求，一共读取了140.72KB的数据。

Requests/sec:    678.90 总请求TPS，包含成功和失败的总数，和wrk相同。

Requests/sec-Success和Requests/sec-Failure :  是区分业务应答是否成功的，可能正常应答了，但是返回的信息是错误信息。

Transfer/sec: 每秒的传输数据大小。

- 配置文件的详细说明

```ini
[common]
host = 127.0.0.1
port = 8000

[request] # 报文长度的长度,默认是8
req_len_len = 8
# 报文长度是全部还是只计算报文体，默认是body可选配置为total
req_len_type = body
# 报文头配置，默认只有长度
req_head = XMLHEAD$(length)20201231
# 必须配置,报文体长度
req_body = \
<root>\
    <head>\
        <date>$(DATE)</date>\
        <branch>$(BRANCH)</branch>\
        <traceno>$(TRACENO)</trace>\
        <term>$(TERMNO)</term>\
        <bankno>313233000017</bankno>\
        <name>张三</name>\
    </head>\
    <body>\
        <msg>HELLO,this is a test</msg>\
    </body>\
</root>

[response]
# 报文头长度,默认是8
rsp_headlen = 23
# 报文头中的报文长度位置,默认是1
rsp_len_beg = 8
# 报文头中的报文长度的长度,默认是8
rsp_len_len = 8
# 报文长度是全部还是只计算报文体，默认是body可选配置为total
rsp_len_type = body

# 响应码类型,默认是fixed
rsp_code_type = xml
# 响应码位置,默认是body
rsp_code_location = body
# 响应码tag，默认是1 6
rsp_code_location_tag = <retCode>
# 成功响应码，默认是000000
rsp_code_success = 000000

[parameters]
TRACENO = COUNTER, 1, 100000, 1, %08ld
BRANCH =  FILE, branch.txt
TERMNO = CONNECTID, %08ld
DATE = DATETIME, %H:%M:%S
```


## 关于为什么不使用wrk2

关于解决**协调遗漏（Coordinated Omisson）**的wrk2的文章：https://blog.csdn.net/minxihou/article/details/97318121

wrk2作者从技术角度的分析解决方案非常的专业而且正确，但是从实际项目工程角度考虑，我没有使用wrk2进行扩展，主要有如下两点原因:
第一、一般情况下，压测都是希望有一个相对较好的结果。
第二、主流的压测软件都存在的统计口径问题，即使这个统计方式有问题，从业界统一性和标准化上考虑，结果也是可信赖的。

因此，相对于每次讨论TPS都需要明确使用何种软件并且是否排除了协调遗漏，这种复杂的前提造成的沟通困难来说，在工程领域统一方法更为重要。

## 确认须知

wrktcp是基于wrk的扩展开发。当然wrk也是使用了大量的redis、nignx的源码。在使用和扩展的时候，请注意对应的开源库的相关协议。

## 压测的参数技巧

macos下必须调整的参数：
```
# 最大连接数，mac默认是128,不调整的话连接经常失败
sysctl kern.ipc.somaxconn
sudo sysctl -w kern.ipc.somaxconn=1024

# TIME_WAIT的回收时间，默认是15秒，压测的时候,需要改成1秒,否则超过17k的随机端口未释放，mac就会报错（linux是30k左右）
sysctl net.inet.tcp.msl
sudo sysctl -w net.inet.tcp.msl=1000

```

linux下的参数调整:
```
修改/etc/sysctl.conf, 是否可以开启需要根据实际情况，总结来说，如果有F5或者负载均衡器，则不能开启tcp_tw_recycle

M.16/root/wrktcp/wrktcp>sysctl -p
net.ipv4.tcp_tw_reuse = 1
net.ipv4.tcp_tw_recycle = 1
net.ipv4.tcp_timestamps = 1
```

## 版本更新说明
- V1.1 2021-06-15
针对test模式优化，不必等待-d时间、无需输入-t和-c默认为1和1、针对test模式增加更多的输出日志。
优化了一些日志html显示的小细节
