# FTP     
  
## Ubuntu 环境配置：    
### （建议）第一步：更新系统  
      sudo apt update
      sudo apt upgrade -y  
        
### 第二步：安装编译工具  
   
      sudo apt install -y build-essential  
      //build-essential可直接将gcc,g++,make直接安装好；

## 项目结构：  
       FTP/
      ├── ser.c        //服务器
      ├── cli.c        //客户端
      ├── welcome.txt     //测试样本 
      └── README.md       //项目介绍  
          
## 流程步骤：   
### 前置条件：
### 1.Clone:  
        
       git@github.com:lss525/FTP.git  
      
#### 2.运行服务器代码：  
       cd FTP/ser
       ./fuwuqi 2100


  - 当出现以下情况，说明服务器开启成功：    

        duankou:2100
        /tmp/ftp_root
  - 当出现运行结束，说明服务器已经开启；  

#### 3.运行客户端代码：    
  
        cd FTP/cli
        /cli 127.0.0.1.2100
 
  - 当出现以下界面时证明连接服务器成功：  
      
        already link 127.0.0.1:2100
        accept:220 welcome to use ftp
         FTP 
        user ->
        ls  
        get
        put 
        quit 

        ftp>   
            
   - 当出现以下界面时证明服务器没有开启或者连接未成功：    

          connect failed: Connection refused  
    
### 功能简述：    
#### 简图：
| 命令 | 功能 |
|------|------|
| USER | 用户名 |  
| PASS | 密码验证 | 
| PASV | 被动模式 |  
| LIST | 列出目录 | 
| RETR | 下载文件 |  
| STOR | 上传文件 |  
| QUIT | 断开连接 |  

#### 1.登录：
- 进入终端界面后，输入“user ”后加用户名和密码；
  - 用户名为：shandian
  - 密码为：lhy'sftp
- 当出现以下界面时，说明登陆失败，用户名与密码不匹配：  
      
       ftp> user -> admin lhy'sftp
       USER ->
       accept:331,mima、
       PASS admin
       accept:530 no
  
- 当出现以下界面时，说明登陆成功：
      
       ftp> user shandian lhy'sftp
       USER shandian
       accept:331,mima、
       PASS lhy'sftp
       accept:230 ok   
         
#### 2.列出目录：  
  - 进入终端界面后，输入“ls ”后进行目录列出；
  - 当出现以下界面时，说明列出目录成功：

       
        ftp> ls
        PASV
        accept227 (127,0,0,1,175,227)
        pasv begin 127.0.0.1:45027
        already link 127.0.0.1:45027
        LIST
        accept:150 open mulu
        -rw-rw-r-- 1 ftp ftp 0 Jun 03 22:10 welcome.txt
        -rw-rw-r-- 1 ftp ftp 9 Jun 03 22:10 test.txt
        accept:226 mulu put  
          
#### 3.下载文件：  
  - 先在命令行将所需文件cp到服务器/tmp/ftp_root/的根目录；
  - 然后进入终端界面后，输入“get ”后可对服务器的文件进行下载：  
     
        ftp>  get 2.c
        PASV
        accept227 (127,0,0,1,152,41)
        pasv begin 127.0.0.1:38953
        already link 127.0.0.1:38953
        RETR 2.c
        accept:150 
        file already download 2.c
        accept:226 put wancheng
  
  - 出现以上界面时说明文件下载成功；  
      

  - 当出现以下界面时，说明服务器根目录不存在所输入文件：  
    
        ftp> get 3.c
        PASV
        accept227 (127,0,0,1,166,143)
        pasv begin 127.0.0.1:42639
        already link 127.0.0.1:42639
        RETR 3.c
        accept:550 文件不存在  
           
 
#### 4.上传文件：  
  - 进入终端界面后，输入“put ”后可将客户端本地文件通过网络上传到服务器根目录里；  
    
  - 当出现以下界面时，说明客户端上传文件成功：  
       
        ftp> put 1.1.c
        PASV
        accept227 (127,0,0,1,180,79)
        pasv begin 127.0.0.1:46159
        already link 127.0.0.1:46159
        STOR 1.1.c
        accept:150 open fpfile already upload: 1.1.c
        accept:226 put wancheng  
          
  - 当出现以下界面时，说明正在运行的客户端程序目录中不存在所输入文件：  
    
        ftp> put 2.1.c
        PASV
        accept227 (127,0,0,1,138,165)
        pasv begin 127.0.0.1:35493
        local file no there: 2.1.c  
          
  
#### 5.退出：  
  - 进入终端界面后，输入“quit ”后；
  - 出现以下界面时，客户端已退出：  
    
        ftp> quit
        QUIT
        accept:221 byalready duankai

   