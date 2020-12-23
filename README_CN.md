# wrktcp - 无lua依赖的tcp协议压测wrk工具

*你可以查看readme in the file:README_EN.md*

[TOC]

本程序主要是基于wrk的基础上 取消了ssl和lua依赖，使用tcpini配置用来实现tcp协议下的压力测试。

## 主要功能

1. 整体框架基于wrk做的扩展，统计、大部分命令、输出结果沿用的wrk，但是做了部分调整。
2. 支持tcp协议的压力测试，包括各种协议和通讯格式。
3. 支持定义发送报文信息，应答报文信息，以及应答的成功响应信息对比配置。
4. 支持请求报文的参数变量。参数类似loadrunner，有如下四种：
   1. COUNTER, 迭代器，如唯一流水等，可支持定义范围，定义步长。
   2. DATETIME，时间模式，格式与strtime完全一致。
   3. CONNECTID，连接唯一ID，一般用于做为终端号等使用。
   4. FILE,文件模式，支持从文件中顺序读取内容进行内容赋值。
5. 支持压测过程中，动态刷新TPS和延时。TODO
6. 支持记录压测过程信息，并生成html页面进行展示。TODO
7. 原则上也支持http，因为http也是tcp协议的一种。



## 与wrk的区别

1. 增加支持tcp协议。使用ini配置文件来配置tcp报文的格式。连接地址、报文约定、请求报文、应答报文信息都在该文件中配置。
2. 取消lua的依赖，自定义的内容和判断不用学习lua，一个配置文件即可。
3. 不依赖任何第三方库，取消ssl和luajit的deps。因此这是一个纯净的(pure)C的工程。
4. thread stat的分布统计，把最大支持TPS从1000W缩减到100W，TPS精度从1增加到0.1。
5. 输出结果基本沿用wrk，增加了成功和失败的分类。由于原wrk不考虑业务级错误，因此增加了配置。
6. 后续增加动态刷新TPS和延时，在较长时间的压测过程中也能实时的观察压测情况。
7. 后续增加直接生成html报表和执行过程监控。



## 编译安装

- macos

cd wrktcp && make

- linux

cd wrktcp && make

- windows

需要下载编译器，或者直接下载release版本。



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


- 输出结果

```
Running 10s test @ 127.0.0.1:8000 using sample_tiny.ini
  2 threads and 10 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    23.02ms   43.78ms 356.62ms   94.88%
    Req/Sec   354.37     74.31   454.50     90.62%
  Latency Distribution
     50%   12.99ms
     75%   14.02ms
     90%   15.74ms
     99%  274.89ms
  6862 requests in 10.11s, 140.72KB read
Requests/sec:    678.90
Requests/sec-Success:    678.90
Requests/sec-Failure:      0.00
Transfer/sec:     13.92KB
```


- 命令的详细说明

> ## 命令行参数的详细说明
>
> ```
> -t, --threads:     使用线程总数，一般推荐使用CPU核数的2倍-1
> -c, --connections: 连接总数，与线程无关。每个线程的连接数为connections/threads
> -d, --duration:    压力测试时间, 可以写 2s, 2m, 2h
>  --latency:     打印延迟分布情况
>  --timeout:     指定超时时间，默认是5分钟，越长占用统计内存越大。
>  --realtime	   实时刷新tps信息并记录tps和延时的过程数据，生成html
> -v  --version:     打印版本信息
> ```

- 结果的详细解释

> Running 10s test @ 127.0.0.1:8000 using sample_tiny.ini
> 2 threads and 10 connections

代表命令的基本情况以及连接的基本情况，程序执行加载完成配置后立刻打印。



>   Thread Stats   Avg      Stdev     Max   +/- Stdev
>    Latency    23.02ms   43.78ms 356.62ms   94.88%
>    Req/Sec   354.37     74.31   454.50     90.62%

Latency代表延时时间，Req/Sec代表每秒的成功应答笔数(TPS)，需要注意的是这部分统计是单个线程的统计数据。

说明依次是：线程状态名(Thread Stats)、平均值(Avg)、标准差(Stdev)、最大值(Max)、正负一个标准差内数据所占比例(+/- Stdev)

如果压测系统稳定，平均值和最大值是期望相等的， 标准差的期望值是0，+/-标准差的期望值是100%。



>   Latency Distribution
>     50%   12.99ms
>     75%   14.02ms
>     90%   15.74ms
>     99%  274.89ms

延迟分布的统计数据，50% - 99%是按相应时间进行排序的。50%代表第50%的数据延时是多少99%代表第99%的数据延时是多少。



>   6862 requests in 10.11s, 140.72KB read
>
>   Requests/sec:    678.90
>   Requests/sec-Success:    678.90
>   Requests/sec-Failure:      0.00
>   Transfer/sec:     13.92KB

第一行代表： 在10.11秒内有6862个请求，一共读取了140.72KB的数据。

Requests/sec:    678.90 总请求TPS，包含成功和失败的总数，和wrk相同。

Requests/sec-Success和Requests/sec-Failure :  是区分业务应答是否成功的，可能正常应答了，但是返回的信息是错误信息。

Transfer/sec: 每秒的传输数据大小。

- 配置文件的详细说明

```ini
[common]
host = 127.0.0.1
port = 8000

[request]
# 报文长度的长度，默认为8位长
req_len_len = 8
# 报文长度是全部还是只计算报文体，默认是body可选配置为total
req_len_type = body
# 报文头配置，默认只有长度
req_head = $(length)
# 报文体的内容
req_body = \
<root>\
	<date>$(DATE)</date>\
	<branch>$(BRANCH)</branch>\
	<traceno>20201222$(TRACENO)</branch>\
	<term>$(TERMNO)</term>\
	<bankno>313233000017</bankno>\
	<name>张三</name>\
</root>

[response]
# 报文头长度,默认是8
rsp_headlen = 16
# 报文头中的报文长度位置,默认是1
rsp_len_beg = 5
# 报文头中的报文长度的长度,默认是8
rsp_len_len = 8
# 报文长度是全部还是只计算报文体，默认是body可选配置为total
rsp_len_type = body

# 响应码类型,默认是fixed
rsp_code_type = xml
# 响应码位置,默认是body
rsp_code_location = body
# 响应码tag，默认是1 6
rsp_code_localtion_tag = <retCode>
# 成功响应码，默认是000000
rsp_code_success = 000000

[paramters]
DATE = DATETIME, "%H:%M:S"
BRANCH =  FILE, branch.txt
TRACENO = COUNTER, 100, 100000, 2, "%08ld"
TERMNO = CONNECTID
```



## 关于为什么不使用wrk2

关于解决**协调遗漏（Coordinated Omisson）**的wrk2的文章：https://blog.csdn.net/minxihou/article/details/97318121

未从wrk2进行扩展的原因，个人观点是作者从技术说的很对，但是从实际工程角度没有实际价值。原因有两点：
第一、一般情况下，压测都是希望有一个较好的结果。
第二、如果是压测软件都会有的统计口径问题，那么即使这个统计方式有问题，但是因为是统一的统计口径和方法，因此也是可信赖的。

因此，相对于每次讨论TPS都需要明确使用何种软件并且是否排除了协调遗漏，这种复杂的前提造成的沟通困难来说，统一沟通方法就先的更为重要了。



## 确认须知

wrktcp是基于wrk的扩展开发。当然wrk也是使用了大量的redis、nignx， luajit的源码，使用和扩展的时候，请注意对应的开源库的相关协议。

