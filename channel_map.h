#ifndef CHANNEL_MAP_H
#define CHANNEL_MAP_H

#define CHANNELMAP_INITSIZE 32
#define CHANNELMAP_MAXSIZE 1024
/**
 * 套接字描述符与对应channel的映射表，可快速获得套接字fd绑定的channel，从而调用相应回调函数
 * usage: struct channel* chan = channel_map[fd];
 */
struct channel_map {
    /* channel指针数组, 初始化为CHANNELMAP_INITSIZE，之后可自动扩增 */
    void** entries;

    /* 数组大小 */
    int nentry;
};

/* 初始化映射表，初始大小为CHANNELMAP_INITSIZE */
struct channel_map* chanmap_init();

/* 实现映射表扩容，最大为CHANNELMAP_MAXSIZE */
int chanmap_expand(struct channel_map* chanmap, int size);

/* 释放映射表及保存channel堆内存 */
void chanmap_clear(struct channel_map* chanmap);

#endif
