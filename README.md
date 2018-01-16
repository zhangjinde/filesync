# filesync

监控windows目录中文件变更，将变更同步到linux目录中

### 配置文件
```json
{
    "log": {
        "file": "filesync.log",
        "level": "debug",
        "cycle": "day",
        "backup": 7
    },
    "server":[
        {
            "server":"127.0.0.1",
            "port":22,
            "user":"user",
            "pass":"pass",
            "cmd":
            [
                { "cmd":"", "sleep":1000 },
                { "cmd":"0", "sleep":200 },
                { "cmd":"0", "sleep":200 },
                { "cmd":"1.1.1.1", "sleep":200 },
                { "cmd":"1", "sleep":200 }
            ]
        }
    ],
    "monitor":[
        {
            "server":"127.0.0.1",
            "localpath":"D:\\",
            "remotepath":"/root/",
            "whitelist":[".*"],
            "blacklist":[]
        }
    ]
}
```
- log 日志配置
> file 日志文件名
> level 日志级别
> cycle 生成周期(mon,day,hour)
> backup 保存周期

- server linux服务器配置
> server 服务器地址
> port 端口
> user 用户名
> pass 密码
> cmd 初始命令
> sleep 命令间隔时间

- monitor windows监控配置
> server 对应的linux服务器
> localpath windows监控目录
> remotepath linux存储目录
> whitelist 白名单(正则)
> blacklist 黑名单(正则)