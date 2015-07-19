#include "netloadstats.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <sys/time.h>

static TransferStats gStats;

void fetchNextTransferStats(void)
{
    unsigned long long timeMs, lastTx, lastRx;
    FILE *fp;
    char buf[1000], *p, *pend;
    int txNo = 0; /* index of number containing tx bytes */
    struct timeval tv;
    struct IfaceStat *ifaceStat;
    int i, newIfaceCnt = 0;

    gettimeofday(&tv, NULL);
    timeMs = 1000ULL * tv.tv_sec + tv.tv_usec / 1000;
    if( timeMs <= gStats.lastTm )
        return;
    if( (fp = fopen("/proc/net/dev", "r")) != NULL ) {
        fgets(buf, sizeof buf, fp);     /* header 1st line, ignore */
        fgets(buf, sizeof buf, fp);     /* headeer 2nd line */
        /* calculate txNo */
        p = strstr(buf, "bytes");       /* at rx bytes header */
        /* search for tx bytes header */
        for(++p; strncmp(p, "bytes ", 6);
                p += strcspn(p, " |"), p += strspn(p, " |"))
            ++txNo;
        if( gStats.lastTm != 0 ) {
            if( ++gStats.newestStat == STATS_MAX )
                gStats.newestStat = 0;
            gStats.timeSpan[gStats.newestStat] =
                timeMs - gStats.lastTm > UINT_MAX ? UINT_MAX :
                timeMs - gStats.lastTm;
            if( gStats.statsCount < STATS_MAX )
                ++gStats.statsCount;
        }
        while( fgets(buf, sizeof buf, fp) != NULL ) {
            p = buf + strspn(buf, " ");     /* at interface name */
            pend = strchr(p, ':');
            *pend = '\0';
            for(i = newIfaceCnt; i < gStats.ifaceCnt &&
                    strcmp(p, gStats.ifaceStats[i]->ifname); ++i)
                ;
            if( i == gStats.ifaceCnt ) {
                gStats.ifaceStats = realloc(gStats.ifaceStats,
                        (gStats.ifaceCnt+1) * sizeof(gStats.ifaceStats[0]));
                gStats.ifaceStats[gStats.ifaceCnt] =
                    calloc(1, sizeof(struct IfaceStat));
                strcpy(gStats.ifaceStats[gStats.ifaceCnt]->ifname, p);
                ++gStats.ifaceCnt;
            }
            ifaceStat = gStats.ifaceStats[i];
            if( i > newIfaceCnt ) {
                gStats.ifaceStats[i] = gStats.ifaceStats[newIfaceCnt];
                gStats.ifaceStats[newIfaceCnt] = ifaceStat;
            }
            ++newIfaceCnt;
            p = pend + 1 + strspn(pend+1, " ");
            lastRx = strtoull(p, NULL, 10);
            for(i = 0; i < txNo; ++i) {
                p += strcspn(p, " ");
                p += strspn(p, " ");
            }
            lastTx = strtoull(p, NULL, 10);
            if( gStats.lastTm != 0 ) {
                ifaceStat->tdiffs[TDIR_TX][gStats.newestStat] =
                    lastTx - ifaceStat->lastTx > UINT_MAX ? UINT_MAX :
                    lastTx - ifaceStat->lastTx;
                ifaceStat->tdiffs[TDIR_RX][gStats.newestStat] =
                    lastRx - ifaceStat->lastRx > UINT_MAX ? UINT_MAX :
                    lastRx - ifaceStat->lastRx;
            }
            ifaceStat->lastRx = lastRx;
            ifaceStat->lastTx = lastTx;
        }
        gStats.lastTm = timeMs;
        for(i = newIfaceCnt; i < gStats.ifaceCnt; ++i)
            free(gStats.ifaceStats[i]);
        gStats.ifaceCnt = newIfaceCnt;
        fclose(fp);

    }
}

const TransferStats *getTransferStats(void)
{
    return &gStats;
}

