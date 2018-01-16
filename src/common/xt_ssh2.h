/*
 * Sample showing how to do SSH2 connect.
 *
 * The sample code has default values for host name, user name, password
 * and path to copy, but you can specify them on the command line like:
 *
 * "ssh2 host user password [-p|-i|-k]"
 */

#ifndef TESTSET_SSH2
#define TESTSET_SSH2

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#ifdef WIN32
#define close closesocket
#pragma comment (lib, "Ws2_32.lib")
#endif

enum
{
    SSH_TYPE_SSH,
    SSH_TYPE_SFTP,
};

typedef int(*ssh_data_callback)(void *param, const char *data, unsigned int data_len);

typedef int(*ssh_worker_callback)(void *param);

typedef struct _tag_my_ssh_param
{
    int type;       // 0-ssh,1-sftp
    int run;        // 0-no,1-running
    void *param;    // client socket
    void *param1;   // local listent port, local client port
    void *param2;   // remote port
    void *param3;
    void *session;
    void *channel;

    const char *server_addr;
    unsigned short server_port;
    const char *username;
    const char *password;

    ssh_data_callback data_cb;      // ssh数据回调
    ssh_worker_callback worker_cb;  // ssh主线程回调

}my_ssh_param, *p_my_ssh_param;

/**
 *\fn        void ssh_thread_func(void *param)
 *\brief     ssh主线程
 *\param[in] void * param p_my_ssh_param
 *\return    void 0成功,其它失败
 */
void ssh_thread_func(void *param);

/**
 *\fn        int ssh_send_data(my_ssh_param *param, const char *data, unsigned int len)
 *\brief     ssh发送数据
 *\param[in] my_ssh_param * param ssh参数数据
 *\param[in] const char * data 要发送的数据
 *\param[in] unsigned int len 要发送的数据长
 *\return    int 0成功,其它失败
 */
int ssh_send_data(my_ssh_param *param, const char *data, unsigned int len);

/**
 *\fn        int ssh_recv_data(my_ssh_param *param, char *data, unsigned int max)
 *\brief     ssh接收数据
 *\param[in] my_ssh_param * param ssh参数数据
 *\param[in] char * data 要接收的数据
 *\param[in] unsigned int max 接收数据缓冲区长度
 *\return    int 接收的数据长
 */
int ssh_recv_data(my_ssh_param *param, char *data, unsigned int max);

/**
 *\fn           int ssh_set_size(my_ssh_param *param, unsigned int width, unsigned int height)
 *\brief        设置屏幕宽高
 *\param[in]    my_ssh_param * param ssh参数数据
 *\param[in]    unsigned int width 屏幕宽
 *\param[in]    unsigned int height 屏幕高
 *\return       int 0成功,其它失败
 */
int ssh_set_size(my_ssh_param *param, unsigned int width, unsigned int height);

/**
 *\fn        int get_server_addr(const char *addr, unsigned int port, struct sockaddr_in *srv_addr)
 *\brief     得到服务地址
 *\param[in] const char *addr 服务器地址
 *\param[in] unsigned int port 服务器端口
 *\param[in] struct sockaddr_in *srv_addr 服务器地址结构
 *\return    int 0成功,其它失败
 */
int get_server_addr(const char *addr, unsigned int port, struct sockaddr_in *srv_addr);

/**
 *\fn        void print_msg(my_ssh_param *param, const char *data, unsigned int len)
 *\brief     sftp接收数据
 *\param[in] my_ssh_param * param ssh参数数据
 *\param[in] const char * data 接收到的数据
 *\param[in] unsigned int len 接收到的数据长
 *\return    void 0成功,其它失败
 */
int print_msg(my_ssh_param *param, const char *data, unsigned int len);

#endif
