/*
 * Sample showing how to do SSH2 connect.
 *
 * The sample code has default values for host name, user name, password
 * and path to copy, but you can specify them on the command line like:
 *
 * "ssh2 host user password [-p|-i|-k]"
 */

#include "xt_ssh2.h"
#include "libssh2_config.h"
#include <libssh2.h>
#include <libssh2_sftp.h>
#include <process.h>

#ifdef HAVE_WINDOWS_H
# include <windows.h>
#endif
#ifdef HAVE_WINSOCK2_H
# include <winsock2.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
# ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
# ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>


const char *keyfile1="~/.ssh/id_rsa.pub";
const char *keyfile2="~/.ssh/id_rsa";

static void kbd_callback(const char *name, int name_len,
                         const char *instruction, int instruction_len,
                         int num_prompts,
                         const LIBSSH2_USERAUTH_KBDINT_PROMPT *prompts,
                         LIBSSH2_USERAUTH_KBDINT_RESPONSE *responses,
                         void **abstract)
{
    (void)name;
    (void)name_len;
    (void)instruction;
    (void)instruction_len;
    if (num_prompts == 1)
    {
        responses[0].text = strdup("password");
        responses[0].length = (int)strlen("password");
    }
    (void)prompts;
    (void)abstract;
}

/**
 *\fn        int get_server_addr(const char *addr, unsigned int port, struct sockaddr_in *srv_addr)
 *\brief     得到服务地址
 *\param[in] const char *addr 服务器地址
 *\param[in] unsigned int port 服务器端口
 *\param[in] struct sockaddr_in *srv_addr 服务器地址结构
 *\return    int 0成功,其它失败
 */
int get_server_addr(const char *addr, unsigned int port, struct sockaddr_in *srv_addr)
{
    if (NULL == addr || NULL == srv_addr)
    {
        return -1;
    }

    struct addrinfo *answer, hint;
    memset(&hint, 0, sizeof(hint));
    hint.ai_family = AF_INET;
    hint.ai_socktype = SOCK_STREAM;

    int ret = getaddrinfo(addr, NULL, &hint, &answer);

    if (ret != 0)
    {
        printf("getaddrinfo: %s\n", gai_strerrorA(ret));
        return -2;
    }

    if (NULL != answer)
    {
        memcpy(srv_addr, (void*)(answer->ai_addr), answer->ai_addrlen);
        srv_addr->sin_port = htons((u_short)port);
    }

    freeaddrinfo(answer);

    return 0;
}

/**
 *\fn        void print_msg(my_ssh_param *param, const char *data, size_t len)
 *\brief     sftp接收数据
 *\param[in] my_ssh_param * param ssh参数数据
 *\param[in] const char * data 接收到的数据
 *\param[in] size_t len 接收到的数据长
 *\return    void 0成功,其它失败
 */
int print_msg(my_ssh_param *param, const char *data, size_t len)
{
    if (NULL == param->data_cb || !param->run) return -1;

    return param->data_cb(param->param, data, len);
}

/**
 *\fn        int ssh_send_data(my_ssh_param *param, const char *data, unsigned int len)
 *\brief     ssh发送数据
 *\param[in] my_ssh_param * param ssh参数数据
 *\param[in] const char * data 要发送的数据
 *\param[in] unsigned int len 要发送的数据长
 *\return    int 0成功,其它失败
 */
int ssh_send_data(my_ssh_param *param, const char *data, unsigned int len)
{
    return (int)libssh2_channel_write(param->channel, data, len);
}

/**
 *\fn        int ssh_recv_data(my_ssh_param *param, char *data, unsigned int max)
 *\brief     ssh接收数据
 *\param[in] my_ssh_param * param ssh参数数据
 *\param[in] char * data 要接收的数据
 *\param[in] unsigned int max 接收数据缓冲区长度
 *\return    int 接收的数据长
 */
int ssh_recv_data(my_ssh_param *param, char *data, unsigned int max)
{
    data[0] = '\0';

    int ret = (int)libssh2_channel_read(param->channel, data, max - 1);

    if (ret > 0)
    {
        data[ret] = '\0';
    }

    return ret;
}

/**
 *\fn           int ssh_set_size(my_ssh_param *param, unsigned int width, unsigned int height)
 *\brief        设置屏幕宽高
 *\param[in]    my_ssh_param * param ssh参数数据
 *\param[in]    unsigned int width 屏幕宽
 *\param[in]    unsigned int height 屏幕高
 *\return       int 0成功,其它失败
 */
int ssh_set_size(my_ssh_param *param, unsigned int width, unsigned int height)
{
    if (NULL == param || width <= 0 || height <= 0) return -1;

    return libssh2_channel_request_pty_size(param->channel, width, height);
}

/**
 *\fn        int run_ssh(p_my_ssh_param param)
 *\brief     ssh主线函数
 *\param[in] p_my_ssh_param param ssh参数数据
 *\return    int 0成功,其它失败
 */
int run_ssh(p_my_ssh_param param)
{
    int rc = 0;
    int sock;
    int auth_pw = 0;
    char info[512];
    const char *userauthlist;
    LIBSSH2_SESSION *session;
    LIBSSH2_CHANNEL *channel;
    struct sockaddr_in srv_addr;

    if (NULL == param)
    {
        return -1;
    }

#ifdef WIN32
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2,0), &wsadata);
#endif

    if (0 != get_server_addr(param->server_addr, param->server_port, &srv_addr))
    {
        print_msg(param, "get_server_addr fail", 0);
        return -2;
    }

    sock = (int)socket(AF_INET, SOCK_STREAM, 0);

    if (connect(sock, (struct sockaddr*)(&srv_addr), sizeof(struct sockaddr_in)) != 0)
    {
        sprintf(info, "failed to connect %s:%d!\r\n", param->server_addr, param->server_port);
        print_msg(param, info, strlen(info));
        return -3;
    }

    rc = libssh2_init(0);

    if (0 != rc)
    {
        sprintf(info, "libssh2 initialization failed (%d)\r\n", rc);
        print_msg(param, info, strlen(info));
        return -4;
    }

    /* Create a session instance and start it up. This will trade welcome
     * banners, exchange keys, and setup crypto, compression, and MAC layers
     */
    session = libssh2_session_init();

    if (NULL == session)
    {
        print_msg(param, "libssh2 create session failed\r\n", 0);
        return -5;
    }

    if (libssh2_session_handshake(session, sock))
    {
        print_msg(param, "Failure establishing SSH session\r\n", 0);
        return -6;
    }

    /* check what authentication methods are available */
    userauthlist = libssh2_userauth_list(session, param->username, (int)strlen(param->username));

    if (strstr(userauthlist, "password") != NULL)
    {
        auth_pw |= 1;
    }

    if (strstr(userauthlist, "keyboard-interactive") != NULL)
    {
        auth_pw |= 2;
    }

    if (strstr(userauthlist, "publickey") != NULL)
    {
        auth_pw |= 4;
    }

    if (auth_pw & 1)
    {
        /* We could authenticate via password */
        if (libssh2_userauth_password(session, param->username, param->password))
        {
            print_msg(param, "Authentication by password failed!\r\n", 0);
            goto shutdown;
        }
    }
    else if (auth_pw & 2)
    {
        /* Or via keyboard-interactive */
        if (libssh2_userauth_keyboard_interactive(session, param->username, &kbd_callback))
        {
            print_msg(param, "Authentication by keyboard-interactive failed!\r\n", 0);
            goto shutdown;
        }
    }
    else if (auth_pw & 4)
    {
        /* Or by public key */
        if (libssh2_userauth_publickey_fromfile(session, param->username, keyfile1, keyfile2, param->password))
        {
            print_msg(param, "Authentication by public key failed!\r\n", 0);
            goto shutdown;
        }
    }
    else
    {
        sprintf(info, "No supported authentication methods found!\r\n");
        print_msg(param, info, strlen(info));
        goto shutdown;
    }

    /* Request a shell */
    channel = libssh2_channel_open_session(session);

    if (NULL == channel)
    {
        print_msg(param, "Unable to open a session\r\n", 0);
        goto shutdown;
    }

    /* Request a terminal with 'vanilla' terminal emulation
     * See /etc/termcap for more options
     */
    if (libssh2_channel_request_pty(channel, "ansi"))//vanilla
    {
        print_msg(param, "Failed requesting pty\r\n", 0);
        goto shutdown;
    }

    /* Open a SHELL on that pty */
    if (libssh2_channel_shell(channel))
    {
        print_msg(param, "Unable to request shell on allocated pty\r\n", 0);
        goto shutdown;
    }

    param->run = TRUE;
    param->session = session;
    param->channel = channel;

    /* At this point the shell can be interacted with using
     * libssh2_channel_read()
     * libssh2_channel_read_stderr()
     * libssh2_channel_write()
     * libssh2_channel_write_stderr()
     *
     * Blocking mode may be (en|dis)abled with: libssh2_channel_set_blocking()
     * If the server send EOF, libssh2_channel_eof() will return non-0
     * To send EOF to the server use: libssh2_channel_send_eof()
     * A channel can be closed with: libssh2_channel_close()
     * A channel can be freed with: libssh2_channel_free()
     */

    if (NULL != param->worker_cb)
    {
        param->worker_cb(param);
    }

    if (channel)
    {
        libssh2_channel_free(channel);
        channel = NULL;
    }

    /* Other channel types are supported via:
     * libssh2_scp_send()
     * libssh2_scp_recv()
     * libssh2_channel_direct_tcpip()
     */

shutdown:

    libssh2_session_disconnect(session, "Normal Shutdown, Thank you for playing");
    libssh2_session_free(session);

    close(sock);

    sprintf(info, "ssh:%s@%s:%d all done\r\n", param->username, param->server_addr, param->server_port);
    print_msg(param, info, strlen(info));

    libssh2_exit();

    param->run = FALSE;
    param->session = NULL;
    param->channel = NULL;

    return 0;
}

/**
 *\fn        void ssh_thread_func(void *param)
 *\brief     ssh主线程
 *\param[in] void * param p_my_ssh_param
 *\return    void 0成功,其它失败
 */
void ssh_thread_func(void *param)
{
    const char *info = "ssh_thread_func exit\r\n\r\n\r\n\r\n\r\n";

    p_my_ssh_param ssh_param = (p_my_ssh_param)param;

    run_ssh(ssh_param);

    print_msg(ssh_param, info, strlen(info));
}
