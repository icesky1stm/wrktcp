# wrktcp - support TCP protocol without lua

*你可以查看中文版的说明:README_EN.md*

[TOC]

This procedure is mainly based on WRK to remove SSL and Lua dependence, using TCPINI configuration to achieve the TCP protocol under the stress test.

## Functions overview

1. The overall framework is based on WRK extension. Statistics, most commands and output results follow WRK, but some adjustments have been made.
2. Support the stress test of TCP protocol, including various protocols and communication formats.
3. Support the definition of sending message information, reply message information, and reply successful response information comparison configuration.
4. Support parameter variables of request message. Parameters are similar to loadRunner, with the following four types:
   1. COUNTER, iterator, such as unique flow, can define scope and define step size.
   2. DATETIME, is exactly the same format as the Strtime.
   3. CONNECTID, a unique ID for a connection, generally used as a terminal number, etc.
   4. FILE, FILE mode, supports sequential reading of contents from files for content assignment.
5. Support dynamic refresh of TPS and delay during pressure measurement. TODO
6. Support recording pressure test process information, and generate HTML pages for display. TODO
7. In principle, HTTP is also supported, because HTTP is also a kind of TCP protocol.



## The difference with wrk

1. Added support for TCP protocol. Use the INI profile to configure the TCP packet format. The connection address, message convention, request message, and reply message information are all configured in this file.
3. Remove Lua dependencies, customize content and judgment without learning Lua, a configuration file will do.
4. Do not rely on any third-party libraries and deps SSL and Luajit. So this is a pure C project.
4. Distribution statistics for Thread Stat reduce the maximum TPS support from 1000W to 100W, and increase TPS accuracy from 1 to 0.1.
5. The output basically follows WRK, increasing the classification of success and failure. Because the original WRK did not consider business-level errors, configuration was added.
6. TODO: dynamic refresh TPS and delay are added, so that real-time observation of pressure measurement can also be made in the process of pressure measurement for a long time.
7. TODO: generate HTML reports and perform process monitoring.



## install

- macos

cd wrktcp && make

- linux

cd wrktcp && make

- windows

需要下载编译器，或者直接下载release版本。



## Quick Start

- Modify the sample_tiny.ini configuration

```ini
[common]
host = 127.0.0.1
port = 8000

[request]
req_body = this is a test
```

- run the command

```sh
wrktcp -t2 -c20 -d10s --latency sample_tiny.ini
```

Similar to WRK, this command represents the following:
Use 2 threads (-T2), 20 concurrent connections (-c20), keep running for 5 seconds (-d5s), run with configuration sample_tiny.ini.


- output
``````
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


- command detail description

> ```
>-t, --threads:     Use the total number of threads, generally recommended to use 2 times the number of CPU cores -1
> -c, --connections: Total number of connections, regardless of thread. The number of connections per thread is connections/ Threads
> -d, --duration:    write 2S, 2m, 2h
>  --latency:     distribution of printing delay
>  --timeout:     Specify timeout, default is 5 minutes, longer takes up more statistical memory.	
>     --realtime	   refreshes TPS information in realtime and records TPS and delayed process data to generate HTML
>    -v  --version:     Print version information
>    ```

- output detail description

> Running 10s test @ 127.0.0.1:8000 using sample_tiny.ini
>   2 threads and 10 connections

Represents the base case of the command and the base case of the connection. The program executes the load and prints as soon as it completes the configuration.

>   Thread Stats   Avg      Stdev     Max   +/- Stdev
>     Latency    23.02ms   43.78ms 356.62ms   94.88%
>     Req/Sec   354.37     74.31   454.50     90.62%

Latency represents Latency time, and Req/Sec represents the number of successful response strokes per second (TPS). It should be noted that this part of the Latency data is the data of a single thread.

Description: Thread Stats, mean (Avg), standard deviation (Stdev), maximum (Max), percentage of data within one standard deviation (+/ -stdev)

If the pressure measuring system is stable, the mean and the maximum are expected to be the same, the expected standard deviation is 0, and the expected standard deviation is 100%.



>   Latency Distribution
>      50%   12.99ms
>      75%   14.02ms
>      90%   15.74ms
>      99%  274.89ms

The statistics of the delay distribution, 50% to 99%, are sorted by corresponding time. So 50% is what's the 50th data delay and 99% is what's the 99th data delay.



>   6862 requests in 10.11s, 140.72KB read
>
> Requests/sec:    678.90
> Requests/sec-Success:    678.90
> Requests/sec-Failure:      0.00
> Transfer/sec:     13.92KB

The first line represents: 6862 requests in 10.11 seconds, with a total of 140.72KB read.

Requests/sec:    678.90  The total request TPS, containing the total number of successes and failures, is the same as WRK.

Requests/sec-Success & Requests/sec-Failure :  Is to distinguish whether the business reply was successful or not, it may have been correctly answered, but the message returned is an error message.

Transfer/sec: The size of the transmitted data per second.

- ini configuration desrciption

```ini
[common]
host = 127.0.0.1
port = 8000

[request]
# The length of the message's length, 8 by default
req_len_len = 8
# length is all or only calculated body, the default is the optional body configuration of total
req_len_type = body
# request head, default length only
req_head = $(length)
# The content of the request body
req_body = \
<root>\
	<date>$(DATE)</date>\
	<branch>$(BRANCH)</branch>\
	<traceno>20201222$(TRACENO)</branch>\
	<term>$(TERMNO)</term>\
	<bankno>313233000017</bankno>\
	<name>sam</name>\
</root>

[response]
# Then head length 
rsp_headlen = 16
# The length's location in the message ,1 is default
rsp_len_beg = 5
# The length of the message's length, 8 by default
rsp_len_len = 8
# length is all or only calculated body, the default is the optional body configuration of total
rsp_len_type = body

# response code type, default is fixed, optional is xml,json and to be continued
rsp_code_type = xml
# response code location, body or head
rsp_code_location = body
# the tag of the response code
rsp_code_localtion_tag = <retCode>
# response code success
rsp_code_success = 000000

[paramters]
DATE = DATETIME, "%H:%M:S"
BRANCH =  FILE, branch.txt
TRACENO = COUNTER, 100, 100000, 2, "%08ld"
TERMNO = CONNECTID
```



## why not use WRK2

here are sharp contrasts in the WRK2 article on addressing the Coordinated omission of ：https://blog.csdn.net/minxihou/article/details/97318121

The reasons for not extending from WRK2, personal point of view is that the author is technically correct, but has no practical value from a practical engineering point of view. For two reasons:
First, under normal circumstances, pressure measurement is expected to have a better result.
Second, if it is the pressure measurement software will have the statistical caliber problem, then even if the statistical method has problems, but because it is a unified statistical caliber and method, so it is reliable.

Therefore, it is more important to have a unified approach to communication than to have a clear understanding of what software to use and whether coordination omissions are eliminated in every discussion of TPS.



## Acknowledgements

WRKTCP is an extended development based on WRK. Of course, WRK is also the use of a large number of Redis, Nignx, Luajit source code, use and extension, please pay attention to the corresponding open source library protocols.

