# wrktcp - 基于wrk做二次开发的实现tcp协议的压测工具

由于wrk不支持tcp协议的压测，本程序主要是用来实现tcp协议下的压测程序,同时也解除了ssl和lua的依赖。

## 支持功能以及与wrk的区别

与wrk的区别主要如下：
1. 支持tcp协议。因为tcp协议的报文一般有特殊约定，因此使用ini压测配置文件来约定。
2. ini压测配置文件。主要来配置压测的地址，报文的约定以及压测时可变参数的配置。
3. 输出格式完全复用wrk的结果。
4. 不依赖任何第三方库，包括luajit和ssl，因此这是一个纯净的(pure)C的压测工程。

## 快速使用

    wrktcp -t4 -c400 -d30s -f./wrktcp_sample.ini

  This runs a benchmark for 30 seconds, using 12 threads, and keeping
  400 HTTP connections open.

  Output:

    Running @30s test @127.0.0.1 @8080
      12 threads and 400 connections
      Thread Stats   Avg      Stdev     Max   +/- Stdev
        Latency   635.91us    0.89ms  12.92ms   93.69%
        Req/Sec    56.20k     8.07k   62.00k    86.54%
      22464657 requests in 30.00s, 17.76GB read
    Requests/sec: 748868.53
    Transfer/sec:    606.33MB
    
关于结果的说明
```
Running 30s test @ http://www.bing.com (压测时间 30s)
  8 threads and 200 connections (共 8 个测试线程, 200 个连接)
  Thread Stats   Avg      Stdev     Max   +/- Stdev
              (平均值) (标准差)(最大值)(正负一个标准差所占比例)
    Latency    46.67ms  215.38ms   1.67s    95.59%
    (延迟)
    Req/Sec     7.91k     1.15k   10.26k    70.77%
    (处理中的请求数)
  Latency Distribution (延迟分布)
     50%    2.93ms
     75%    3.78ms
     90%    4.73ms
     99%    1.35s (99 分位的延迟)
  1790465 requests in 30.01s, 684.08MB read (30.01 秒内共处理完成了 1790465 个请求, 读取了 684.08MB 数据)
Requests/sec:  59658.29 (平均每秒处理完成 59658.29 个请求)
Transfer/sec:     22.79MB (平均每秒读取数据 22.79MB)
```

## 命令行参数的详细说明

    -c, --connections: total number of TCP connections to keep open with
                       each thread handling N = connections/threads
    
    -d, --duration:    duration of the test, e.g. 2s, 2m, 2h
    
    -t, --threads:     total number of threads to use
    
    -f, --file:  ini file, use to define tcp package format
    
    -H, --header:      HTTP header to add to request, e.g. "User-Agent: wrk"
    
        --latency:     print detailed latency statistics
    
        --timeout:     record a timeout if a response is not received within
                       this amount of time.

## 场景配置文件的详细说明
下面是一个典型的配置例子,一个配置文件有如下几部分组成:
- head, 主要用来配置报文头，如果没有可以配置为空
- body, 主要用来配置发送报文,支持各种参数
- rsp, 响应信息的长度
- paramters
```
# wrktcp send message config file
[head]
$(LEN)

[body]
<root>
	<date>$(DATE)</date>
	<branch>$(BRANCH)</branch>
	<traceno>$(TRACENO)</branch>
	<term>$(TERMNO)</term>
	<bankno>313233000017</bankno>
	<name>张三</name>
</root>

[rsp]
rsp_head = 20
rsp_code = head,fixed,10,16
#rsp_code = body,xml, "retCode"
#rsp_code = body,json, "retCode"
rsp_success = 000000

[paramters]
LEN = LENGTH, 20, "%08ld"
TRACENO = COUNTER, 100, 100000, 2, "%08ld"
BRANCH =  FILE, branch.txt
TERMNO = CONNECTID
DATE = DATETIME, "%H:%M:S"
```

## 压力测试的技巧小贴士

## 关于为什么不使用wrk2

关于解决**协调遗漏（Coordinated Omisson）**的wrk2的文章：https://blog.csdn.net/minxihou/article/details/97318121

仅代表个人观点，作者从技术说的很对，但是从实际工程角度没有实际价值。原因有两点：
第一、一般情况下，压测都是希望有一个较好的结果。
第二、如果是压测软件都会有的统计口径问题，那么即使这个统计方式有问题，但是因为是统一的统计口径和方法，因此也是可信赖的。

因此，相对于每次讨论TPS都需要明确使用何种软件并且是否排除了协调遗漏，这种复杂的前提造成的沟通困难来说，统一沟通方法就先的更为重要了。

## 确认须知

  wrk contains code from a number of open source projects including the
  'ae' event loop from redis, the nginx/joyent/node.js 'http-parser',
  and Mike Pall's LuaJIT. Please consult the NOTICE file for licensing
  details.



