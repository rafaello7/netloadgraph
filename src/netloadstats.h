#ifndef NETLOADSTATS_H
#define NETLOADSTATS_H

enum {
    STATS_MAX = 256
};

enum TransferDirection {
    TDIR_TX,
    TDIR_RX,
    TDIR_COUNT
};

typedef struct {
    int ifaceCnt;
    int statsCount;
    int newestStat;
    unsigned long long lastTm;
    unsigned timeSpan[STATS_MAX];
    struct IfaceStat {
        char ifname[32];
        unsigned long long lastTx;
        unsigned long long lastRx;
        unsigned tdiffs[TDIR_COUNT][STATS_MAX];
    } **ifaceStats;
} TransferStats;

void fetchNextTransferStats(void);

const TransferStats *getTransferStats(void);

#endif /* NETLOADSTATS_H */
