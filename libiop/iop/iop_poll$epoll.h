﻿#ifdef _EPOLL

#include <iop_poll.h>
#include <sys/epoll.h>

struct epolls {
    int fd;                 // epoll 文件描述符
    struct epoll_event e[INT_POLL]; // 事件数组
};

// 发送事件转换
inline uint32_t to_events(uint32_t what) {
    uint32_t events = 0;
    if (what & EV_READ)
        events |= EPOLLIN;
    if (what & EV_WRITE)
        events |= EPOLLOUT;
    return events;
}

// 事件宏转换
inline uint32_t to_what(uint32_t events) {
    uint32_t what;
    if (events & (EPOLLHUP | EPOLLERR))
        what = EV_READ | EV_WRITE;
    else {
        what = 0;
        if (events & EPOLLIN)
            what |= EV_READ;
        if (events & EPOLLOUT)
            what |= EV_WRITE;
    }
    return what;
}

// epolls_free - epoll 句柄释放
inline static void epolls_free(iopbase_t base) {
    struct epolls * mata = base->mata;
    if (mata) {
        base->mata = NULL;
        if (mata->fd >= 0)
            close(mata->fd);
        mata->fd = -1;
        free(mata);
    }
}

// epoll 事件调度处理
int epolls_dispatch(iopbase_t base, uint32_t timeout) {
    int i, n = 0;
    struct epolls * mata = base->mata;
    do
        n = epoll_wait(mata->fd, mata->e, sizeof(mata->e)/sizeof(*mata->e), timeout);
    while (n < SBase && errno == EINTR);

    // 得到当前时间
    time(&base->curt);
    for (i = 0; i < n; ++i) {
        struct epoll_event * ev = mata->e + i;
        uint32_t id = ev->data.u32;
        if (id >= 0 && id < base->maxio) {
            if (id < base->maxio) {
                iop_t iop = base->iops + id;
                int what = to_what(ev->events);
                iop_callback(base, iop, what);
            }
        }
    }
    return n;
}

// epoll 添加处理事件
inline static int epolls_add(iopbase_t base, uint32_t id, socket_t s, uint32_t events) {
    struct epoll_event ev;
    struct epolls * mata = base->mata;
    ev.data.u32 = id;
    ev.events = to_events(events);
    return epoll_ctl(mata->fd, EPOLL_CTL_ADD, s, &ev);
}

// epoll 删除监视操作
inline static int epolls_del(iopbase_t base, uint32_t id, socket_t s) {
    struct epoll_event ev;
    struct epolls * mata = base->mata;
    ev.data.u32 = id;
    return epoll_ctl(mata->fd, EPOLL_CTL_DEL, s, &ev);
}

// epoll 修改句柄注册
inline static int epolls_mod(iopbase_t base, uint32_t id, socket_t s, uint32_t events) {
    struct epoll_event ev;
    struct epolls * mata = base->mata;
    ev.data.u32 = id;
    ev.events = to_events(events);
    return epoll_ctl(mata->fd, EPOLL_CTL_MOD, s, &ev);
}

//
// iop_poll - 为 iop base 对象注入 poll 处理行为
// base     : 总的 iop 对象基础管理器
// return   : SBase 表示成功
//
int
iop_poll(iopbase_t base) {
    struct epolls * mata;
    int fd = epoll_create1(EPOLL_CLOEXEC);
    if (fd < SBase) {
        RETURN(EBase, "epoll_create1 is error");
    }

    mata = malloc(sizeof(struct epolls));
    mata->fd = fd;
    base->mata = mata;

    base->op.ffree = epolls_free;
    base->op.fdispatch = epolls_dispatch;
    base->op.fadd = epolls_add;
    base->op.fdel = epolls_del;
    base->op.fmod = epolls_mod;

    return SBase;
}

#endif//_EPOLL
