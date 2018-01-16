/*
 * Sample showing how to do SFTP write transfers.
 *
 * The sample code has default values for host name, user name, password
 * and path to copy, but you can specify them on the command line like:
 *
 * "sftp 192.168.0.1 user password sftp_write.c /tmp/secrets"
 */

#include "libssh2_config.h"
#include <libssh2.h>
#include <libssh2_sftp.h>
#include "xt_ssh2.h"
#include "xt_sftp.h"

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
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif

#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>


/**
 *\fn        int sftp_make_dir(my_ssh_param *param, const char *dir)
 *\brief     创建目录
 *\param[in] my_ssh_param * param sftp连接
 *\param[in] const char * dir 目录
 *\return    int 0成功,其它失败
 */
int sftp_make_dir(my_ssh_param *param, const char *dir)
{
    return libssh2_sftp_mkdir(param->channel, dir,
        LIBSSH2_SFTP_S_IRWXU|
        LIBSSH2_SFTP_S_IRGRP|
        LIBSSH2_SFTP_S_IXGRP|
        LIBSSH2_SFTP_S_IROTH|
        LIBSSH2_SFTP_S_IXOTH);
}

/**
 *\fn           int sftp_rm_dir(my_ssh_param *param, const char *dir)
 *\brief        删除目录
 *\param[in]    my_ssh_param * param sftp连接
 *\param[in]    const char * dir 目录
 *\return       int 0成功,其它失败
 */
int sftp_rm_dir(my_ssh_param *param, const char *dir)
{
    return libssh2_sftp_rmdir(param->channel, dir);
}

/**
 *\fn        int sftp_rename(my_ssh_param *param, const char *sourcefile, const char *destfile)
 *\brief     重命名文件
 *\param[in] my_ssh_param * param sftp连接
 *\param[in] const char * sourcefile 源文件
 *\param[in] const char * destfile 目标文件
 *\return    int 0成功,其它失败
 */
int sftp_rename(my_ssh_param *param, const char *sourcefile, const char *destfile)
{
    if (NULL == param ||
        NULL == sourcefile ||
        NULL == destfile ||
        (0 == strcmp(sourcefile, destfile)))
    {
        return -1;
    }

    return libssh2_sftp_rename(param->channel, sourcefile, destfile);
}

/**
 *\fn           int sftp_rm(my_ssh_param *param, const char *filename)
 *\brief        删除文件
 *\param[in]    my_ssh_param * param sftp连接
 *\param[in]    const char * filename 文件名
 *\return       int 0成功,其它失败
 */
int sftp_rm(my_ssh_param *param, const char *filename)
{
    if (NULL == param || NULL == filename) return -1;

    return libssh2_sftp_unlink(param->channel, filename);
}

/**
 *\fn           int sftp_get_link_path(my_ssh_param *param, const char *link, char *path, unsigned int path_max_len)
 *\brief        得到连接的目标路径
 *\param[in]    my_ssh_param * param sftp连接
 *\param[in]    const char * link 连接文件路径
 *\param[out]   char * path 目标路径
 *\param[in]    unsigned int path_max_len 目标路径最大长度
 *\return       int 0-文件连接,1-目录连接
 */
int sftp_get_link_path(my_ssh_param *param, const char *link, char *path, unsigned int path_max_len)
{
    char src[1024];
    size_t len;
    LIBSSH2_SFTP_HANDLE *sftp_handle;
    LIBSSH2_SFTP_ATTRIBUTES attrib;

    if (NULL == param || NULL == link || NULL == path || path_max_len <= 0) return -1;

    strcpy(src, link);
    len = strlen(link);

    if (src[len-1] == '/')
    {
        src[len-1] = '\0';
    }

    libssh2_sftp_realpath(param->channel, src, path, path_max_len);

    sftp_handle = libssh2_sftp_open(param->channel, link, LIBSSH2_FXF_READ, 0);

    if (!sftp_handle) return -2;

    libssh2_sftp_fstat(sftp_handle, &attrib);

    libssh2_sftp_close(sftp_handle);

    return LIBSSH2_SFTP_S_ISDIR(attrib.permissions);
}

/**
 *\fn        int sftp_get_dir(my_ssh_param *param, const char *path, char *dir, unsigned int dir_max_len)
 *\brief     得到目录结构
 *\param[in] my_ssh_param * param sftp连接
 *\param[in] const char * path 远程路径
 *\param[in] char * dir 目录结构, 文件或目录名长(1Byte),文件或目录类型(1Byte),文件或目录名
 *\param[in] unsigned int dir_max_len 目录结构缓冲区长
 *\return    int 0成功,其它失败
 */
int sftp_get_dir(my_ssh_param *param, const char *path, char *dir, unsigned int dir_max_len)
{
    size_t i;
    size_t rc;
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    LIBSSH2_SFTP_HANDLE *sftp_handle;

    if (NULL == param || NULL == path || NULL == dir || dir_max_len <= 0) return -1;

    sftp_handle = libssh2_sftp_opendir(param->channel, path);

    if (NULL == sftp_handle)
    {
        print_msg(param, "libssh2_sftp_opendir fail\n", 0);
        return -2;
    }

    i = 0;
    dir[0] = 0;

    do
    {
        char mem[512] = "";
        char longentry[512] = "";

        rc = libssh2_sftp_readdir_ex(sftp_handle, mem, sizeof(mem)-1, longentry, sizeof(longentry)-1, &attrs);

        rc = strlen(longentry);

        if ((i + rc + 1) > dir_max_len) break;

        if (0 != strcmp(mem, ".") && 0 != strcmp(mem, ".."))
        {
            dir[i] = (char)rc;
            strcpy(&dir[i+1], longentry);
            i += rc + 1;
        }

    } while (rc > 0);

    libssh2_sftp_close(sftp_handle);

    return 0;
}

/**
 *\fn        int sftp_upload_file(my_ssh_param *param, const char *localFilename, const char *remoteFilename)
 *\brief     上传文件
 *\param[in] my_ssh_param * param sftp连接
 *\param[in] const char * localFilename 本地文件名
 *\param[in] const char * remoteFilename 远程文件名
 *\return    int 0成功,其它失败
 */
int sftp_upload_file(my_ssh_param *param, const char *localFilename, const char *remoteFilename)
{
    size_t rc;
    ssize_t nread;
    char buf[4096];
    char *ptr;
    FILE *local;
    time_t now_time;
    struct tm tm_time;
    LIBSSH2_SFTP_HANDLE *sftp_handle;

    if (NULL == param || NULL == localFilename || NULL == remoteFilename) return -1;

    local = fopen(localFilename, "rb");

    if (!local)
    {
        sprintf(buf, "Unable to open local file %s with SFTP\n", localFilename);
        print_msg(param, buf, strlen(buf));
        return -1;
    }

    sftp_handle = libssh2_sftp_open(param->channel, remoteFilename,
        LIBSSH2_FXF_WRITE|LIBSSH2_FXF_CREAT|LIBSSH2_FXF_TRUNC,
        LIBSSH2_SFTP_S_IRUSR|LIBSSH2_SFTP_S_IWUSR|
        LIBSSH2_SFTP_S_IRGRP|LIBSSH2_SFTP_S_IROTH);

    if (!sftp_handle)
    {
        size_t i;
        size_t len = strlen(remoteFilename);
        strcpy(buf, remoteFilename);

        for (i = len - 1; i >= 0; i--)
        {
            if (buf[i] == '/')
            {
                buf[i] = 0;
                break;
            }
        }

        sftp_make_dir(param, buf);

        sftp_handle = libssh2_sftp_open(param->channel, remoteFilename,
            LIBSSH2_FXF_WRITE|LIBSSH2_FXF_CREAT|LIBSSH2_FXF_TRUNC,
            LIBSSH2_SFTP_S_IRUSR|LIBSSH2_SFTP_S_IWUSR|
            LIBSSH2_SFTP_S_IRGRP|LIBSSH2_SFTP_S_IROTH);

        if (!sftp_handle)
        {
            fclose(local);
            sprintf(buf, "Unable to open remote file %s error:%ld\n", remoteFilename, libssh2_sftp_last_error(param->channel));
            print_msg(param, buf, strlen(buf));
            param->run = FALSE;
            return -2;
        }
    }

    do
    {
        nread = fread(buf, 1, sizeof(buf), local);

        if (nread <= 0)
        {
            /* end of file */
            break;
        }

        ptr = buf;

        do
        {
            /* write data in a loop until we block */
            rc = libssh2_sftp_write(sftp_handle, ptr, nread);
            if(rc < 0)
            {
                break;
            }

            ptr += rc;
            nread -= rc;
        } while (nread);

    } while (rc > 0);

    fclose(local);
    libssh2_sftp_close(sftp_handle);

    now_time = time(NULL);
    tm_time = *localtime(&now_time);

    sprintf(buf, "%d-%02d-%02d %d:%02d:%02d %s -> %s OK\n",
        1900+tm_time.tm_year, 1+tm_time.tm_mon, tm_time.tm_mday,
        tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec,
        localFilename, remoteFilename);

    print_msg(param, buf, strlen(buf));

    return 0;
}

/**
 *\fn        int sftp_download_file(my_ssh_param *param, const char *localFilename, const char *remoteFilename)
 *\brief     下载文件
 *\param[in] my_ssh_param * param sftp连接
 *\param[in] const char * localFilename 本地文件名
 *\param[in] const char * remoteFilename 远程文件名
 *\return    int 0成功,其它失败
 */
int sftp_download_file(my_ssh_param *param, const char *localFilename, const char *remoteFilename)
{
    size_t rc;
    ssize_t nread;
    char buf[4096];
    char *ptr;
    FILE *local;
    time_t now_time;
    struct tm tm_time;
    LIBSSH2_SFTP_HANDLE *sftp_handle;

    if (NULL == param || NULL == localFilename || NULL == remoteFilename) return -1;

    sftp_handle = libssh2_sftp_open(param->channel, remoteFilename, LIBSSH2_FXF_READ, 0);

    if (!sftp_handle)
    {
        sprintf(buf, "Unable to open remote file %s error:%ld\n", remoteFilename, libssh2_sftp_last_error(param->channel));
        print_msg(param, buf, strlen(buf));
        param->run = FALSE;
        return -2;
    }

    local = fopen(localFilename, "w+b");

    if (!local)
    {
        libssh2_sftp_close(sftp_handle);
        sprintf(buf, "Unable to open local file %s with SFTP\n", localFilename);
        print_msg(param, buf, strlen(buf));
        return -1;
    }

    do
    {
        nread = libssh2_sftp_read(sftp_handle, buf, sizeof(buf));

        if (nread <= 0)
        {
            /* end of file */
            break;
        }

        ptr = buf;

        do
        {
            /* write data in a loop until we block */
            rc = fwrite(ptr, 1, nread, local);

            if(rc < 0)
            {
                break;
            }

            ptr += rc;
            nread -= rc;
        } while (nread);

    } while (rc > 0);

    fclose(local);
    libssh2_sftp_close(sftp_handle);

    now_time = time(NULL);
    tm_time = *localtime(&now_time);

    sprintf(buf, "%d-%02d-%02d %d:%02d:%02d %s -> %s OK\n",
        1900+tm_time.tm_year, 1+tm_time.tm_mon, tm_time.tm_mday,
        tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec,
        remoteFilename, localFilename);

    print_msg(param, buf, strlen(buf));

    return 0;
}

/**
 *\fn        int run_sftp(p_my_ssh_param param)
 *\brief     ssh参数数据
 *\param[in] p_my_ssh_param param ssh参数数据
 *\return    int 0成功,其它失败
 */
int run_sftp(p_my_ssh_param param)
{
    int rc;
    int sock;
    char info[128];
    struct sockaddr_in srv_addr;
    LIBSSH2_SFTP *channel;
    LIBSSH2_SESSION *session;

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
        return -2;
    }

    sock = (int)socket(AF_INET, SOCK_STREAM, 0);

    if (connect(sock, (struct sockaddr*)(&srv_addr), sizeof(struct sockaddr_in)) != 0)
    {
        sprintf(info, "failed to connect %s:%d!\n", param->server_addr, param->server_port);
        print_msg(param, info, strlen(info));
        return -3;
    }

    rc = libssh2_init(0);

    if (0 != rc)
    {
        sprintf(info, "libssh2 initialization failed (%d)\n", rc);
        print_msg(param, info, strlen(info));
        return -4;
    }

    /* Create a session instance
     */
    if ((session = libssh2_session_init()) == NULL)
    {
        print_msg(param, "libssh2 create session failed\n", 0);
        return -5;
    }

    /* Since we have set non-blocking, tell libssh2 we are blocking */
    libssh2_session_set_blocking(session, 1);

    /* ... start it up. This will trade welcome banners, exchange keys,
     * and setup crypto, compression, and MAC layers
     */
    if (libssh2_session_handshake(session, sock))
    {
        sprintf(info, "Failure establishing SSH session: %d\n", rc);
        print_msg(param, info, strlen(info));
        return -6;
    }

    /* We could authenticate via password */
    if (libssh2_userauth_password(session, param->username, param->password))
    {
        print_msg(param, "Authentication by password failed.\n", 0);
        goto shutdown;
    }

    if ((channel = libssh2_sftp_init(session)) == NULL)
    {
        print_msg(param, "Unable to init SFTP channel\n", 0);
        goto shutdown;
    }

    param->run = TRUE;
    param->session = session;
    param->channel = channel;

    while (param->run)
    {
        Sleep(100);
    }

    libssh2_sftp_shutdown(channel);

shutdown:
    libssh2_session_disconnect(session, "Normal Shutdown, Thank you for playing\n");
    libssh2_session_free(session);

    close(sock);

    sprintf(info, "sftp:%s@%s:%d all done\n", param->username, param->server_addr, param->server_port);
    print_msg(param, info, strlen(info));

    libssh2_exit();

    param->run = FALSE;
    param->channel = NULL;

    return 0;
}

/**
 *\fn        void sftp_thread_func(void *param)
 *\brief     ssh主线程
 *\param[in] void *param p_my_ssh_param
 *\return    void 0成功,其它失败
 */
void sftp_thread_func(void *param)
{
    p_my_ssh_param ssh_param = (p_my_ssh_param)param;

    run_sftp(ssh_param);

    print_msg(ssh_param, "sftp_thread_func exit\r\n", 0);
}
