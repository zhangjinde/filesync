/****************************************************************
Copyright(c) 2005 Tsinghua Tongfang Optical Disc Co., Ltd.
Developed by Storage Product Department, Beijing, China
*****************************************************************
Filename: zmodem.h
Description:
****************************************************************/

typedef int(*SEND_CALLBACK)(void *param, char *data, unsigned int len);
typedef int(*RECV_CALLBACK)(void *param, char *data, unsigned int max);

int zmodem_set(SEND_CALLBACK send_cb, RECV_CALLBACK recv_cb);

int file_get(void *ssh, const char *local_filename, int *len);
int file_put(void *ssh, const char *local_filename, const char *remote_filename, int *len);
