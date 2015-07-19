#include <gtk/gtk.h>
#include "netloadgraphwin.h"
#include "netloadstats.h"
#include <math.h>
#include <string.h>

enum RefreshBehavior {
    RB_MOVE,
    RB_WRAP
};

static const char *const gGraphTypeDispNames[] = { "Move", "Wrap" };

enum ScaleType {
    ST_LINEAR,
    ST_LOGARITHMIC
};

static const char *const gScaleTypeDispNames[] = { "Linear", "Logarithmic" };

enum MaximumLoad {
    ML_1, ML_2, ML_5, ML_10, ML_20, ML_50, ML_100, ML_200, ML_500,
    ML_1k, ML_2k, ML_5k, ML_10k, ML_20k, ML_50k, ML_100k, ML_200k, ML_500k,
    ML_1M, ML_2M, ML_5M, ML_10M, ML_20M, ML_50M, ML_100M, ML_200M, ML_500M,
    ML_1G, ML_2G
};

static const char *const gMaxLoadDispNames[] = {
    "1", "2", "5", "10", "20", "50", "100", "200", "500",
    "1k", "2k", "5k", "10k", "20k", "50k", "100k", "200k", "500k",
    "1M", "2M", "5M", "10M", "20M", "50M", "100M", "200M", "500M",
    "1G", "2G"
};

static const unsigned gMaxLoadValues[] = {
    1, 2, 5, 10, 20, 50, 100, 200, 500,
    1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000,
    1000000, 2000000, 5000000, 10000000, 20000000, 50000000,
    100000000, 200000000, 500000000, 1000000000, 2000000000
};

enum SeparatorType {
    SEP_CIRCLE_TINY,
    SEP_CIRCLE_SMALL,
    SEP_CIRCLE_MEDIUM,
    SEP_CIRCLE_BIG,
    SEP_CIRCLE_HUGE,
    SEP_BAR_THIN,
    SEP_BAR_MEDIUM,
    SEP_BAR_THICK,
    SEP_NONE
};

static const char *const gSepTypeDispNames[] = {
    "Circle tiny", "Circle small", "Circle medium", "Circle big", "Circle huge",
    "Bar thin", "Bar medium", "Bar thick",
    "None"
};

static const int gSepCircleSizes[SEP_NONE+1] = { 3, 4, 5, 7, 9 };
static const int gSepBarSizes[SEP_NONE+1]    = { 0, 0, 0, 0, 0, 1, 2, 4 };

static int indexOf(const char *toFind, const char *const arr[], int elemCount)
{
    int i;

    for(i = 0; i < elemCount; ++i) {
        if( ! g_ascii_strcasecmp(toFind, arr[i]) )
            return i;
    }
    return -1;
}

typedef struct {
    GtkWidget *interfacesView;
    GtkWidget *graphDrawing;
    GtkWidget *labelTxColor;
    GtkWidget *labelTx;
    GtkWidget *labelRxColor;
    GtkWidget *labelRx;

    GdkRGBA txColor;
    GdkRGBA rxColor;
    enum RefreshBehavior refreshBehavior;
    enum ScaleType scaleType;
    enum MaximumLoad maximumLoad;
    gboolean autoExtendMaximum;
    unsigned extendedMaxShrinkDelay;
    guint dispStatsCount;
    enum SeparatorType separatorType;
    int gapSize;
    gchar *interfacesSelect;
    gchar *interfacesUnselect;
    guint scaleLinesMinDistance;
    guint scaleLinesMax;

    int newestPoint;
    int lastNewestStat;
    enum MaximumLoad extendedMaxLoad;
    unsigned extDelayTimerVal;
} NetloadGraphWinPrivate;


G_DEFINE_TYPE_WITH_PRIVATE(NetloadGraphWin, netload_graph_win, GTK_TYPE_APPLICATION_WINDOW);

static gchar *prepareGlobMatch(const gchar *patternStr)
{
    gchar *res = NULL;
    int dest = 0;

    while( g_ascii_isspace( *patternStr ) )
        ++patternStr;
    if( *patternStr ) {
        res = g_malloc(strlen(patternStr) + 2);
        do {
            do res[dest++] = *patternStr++;
            while( *patternStr && ! g_ascii_isspace( *patternStr ) );
            res[dest++] = '\0';
            while( g_ascii_isspace( *patternStr ) )
                ++patternStr;
        }while( *patternStr );
        res[dest++] = '\0';
    }
    return res;
}

static gboolean isMatchAny(const gchar *prepPatterns, const gchar *str,
        gboolean valueIfNull)
{
    if( prepPatterns == NULL )
        return valueIfNull;
    while( prepPatterns[0] ) {
        if( g_pattern_match_simple(prepPatterns, str) )
            return TRUE;
        prepPatterns += strlen(prepPatterns) + 1;
    }
    return FALSE;
}

static void netload_graph_win_init(NetloadGraphWin *win)
{
    NetloadGraphWinPrivate *priv;

    gtk_widget_init_template(GTK_WIDGET (win));
    priv = netload_graph_win_get_instance_private(win);
    priv->refreshBehavior = RB_WRAP;
    priv->scaleType = ST_LINEAR;
    priv->maximumLoad = ML_1M;
    priv->dispStatsCount = 64;
    priv->separatorType = SEP_CIRCLE_MEDIUM;
    priv->gapSize = 2;
    priv->interfacesSelect = NULL;
    priv->interfacesUnselect = NULL;
    priv->scaleLinesMinDistance = 50;
    priv->scaleLinesMax = 16;

    priv->newestPoint = -1;
    priv->lastNewestStat = 0;
    priv->extendedMaxLoad = priv->maximumLoad;
    priv->extDelayTimerVal = 0;
}

static void updateStatusBarValues(NetloadGraphWin *win)
{
    GtkTreeModel *listStore;
    unsigned long long rxTotal = 0, txTotal = 0, speedRx = 0, speedTx = 0;
    unsigned diffTm;
    int i;
    struct IfaceStat *ifaceStat;
    GtkTreeIter iter;
    GtkTreeSelection *sel;
    char buf[80], *ifname;
    gboolean found;
    NetloadGraphWinPrivate *priv;
    const TransferStats *stats;

    priv = netload_graph_win_get_instance_private(win);
    stats = getTransferStats();
    listStore = gtk_tree_view_get_model(GTK_TREE_VIEW(priv->interfacesView));
    sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->interfacesView));
    for(found = gtk_tree_model_get_iter_first(listStore, &iter); found;
        found = gtk_tree_model_iter_next(listStore, &iter) )
    {
        if( gtk_tree_selection_iter_is_selected(sel, &iter ) ) {
            gtk_tree_model_get(listStore, &iter, 0, &ifname, -1);

            for(i = 0; i < stats->ifaceCnt; ++i) {
                ifaceStat = stats->ifaceStats[i];
                if( ! strcmp(ifname, stats->ifaceStats[i]->ifname) ) {
                    rxTotal += ifaceStat->lastRx;
                    txTotal += ifaceStat->lastTx;
                    speedRx += ifaceStat->tdiffs[TDIR_RX][stats->newestStat];
                    speedTx += ifaceStat->tdiffs[TDIR_TX][stats->newestStat];
                }
            }
        }
    }
    diffTm = stats->timeSpan[stats->newestStat];
    if( diffTm > 0 ) {
        speedTx = speedTx * 1000 / diffTm;
        speedRx = speedRx * 1000 / diffTm;
    }else{
        speedTx = speedRx = 0;
    }
    rxTotal /= 1024;
    if( rxTotal < 10240 )
        sprintf(buf, "rx: %lld kB, %lld kB/s", rxTotal, speedRx/1024);
    else
        sprintf(buf, "rx: %lld MB, %lld kB/s", rxTotal / 1024, speedRx/1024);
    gtk_label_set_text(GTK_LABEL(priv->labelRx), buf);
    txTotal /= 1024;
    if( txTotal < 10240 )
        sprintf(buf, "tx: %lld kB, %lld kB/s", txTotal, speedTx/1024);
    else
        sprintf(buf, "tx: %lld MB, %lld kB/s", txTotal / 1024, speedTx/1024);
    gtk_label_set_text(GTK_LABEL(priv->labelTx), buf);
}

void onNewDataFetched(NetloadGraphWin *win)
{
    GtkTreeModel *listStore;
    GtkTreeIter iter;
    GtkTreeSelection *sel;
    char *ifname;
    gboolean found;
    int i;
    NetloadGraphWinPrivate *priv;
    const TransferStats *stats;

    priv = netload_graph_win_get_instance_private(win);
    stats = getTransferStats();
    i = stats->newestStat - priv->lastNewestStat;
    if( i < 0 )
        i += STATS_MAX;
    priv->newestPoint += i;
    priv->extDelayTimerVal += i;
    priv->lastNewestStat = stats->newestStat;
    listStore = gtk_tree_view_get_model(GTK_TREE_VIEW(priv->interfacesView));
    sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->interfacesView));
    found = gtk_tree_model_get_iter_first(listStore, &iter);
    while( found ) {
        gtk_tree_model_get(listStore, &iter, 0, &ifname, -1);
        for(i = 0; i < stats->ifaceCnt; ++i) {
            if( ! strcmp(ifname, stats->ifaceStats[i]->ifname) )
                break;
        }
        if( i == stats->ifaceCnt )
            found = gtk_list_store_remove(GTK_LIST_STORE(listStore), &iter);
        else
            found = gtk_tree_model_iter_next(listStore, &iter);
    }
    for(i = 0; i < stats->ifaceCnt; ++i) {
        for(found = gtk_tree_model_get_iter_first(listStore, &iter); found;
            found = gtk_tree_model_iter_next(listStore, &iter) )
        {
            gtk_tree_model_get(listStore, &iter, 0, &ifname, -1);
            if( ! strcmp(ifname, stats->ifaceStats[i]->ifname) )
                break;
        }
        if( ! found ) {
            gtk_list_store_append(GTK_LIST_STORE(listStore), &iter);
            gtk_list_store_set(GTK_LIST_STORE(listStore), &iter,
                    0, stats->ifaceStats[i]->ifname, -1);
            if( isMatchAny(priv->interfacesSelect,
                        stats->ifaceStats[i]->ifname, TRUE) &&
                ! isMatchAny(priv->interfacesUnselect,
                    stats->ifaceStats[i]->ifname, FALSE) )
            {
                gtk_tree_selection_select_iter(sel, &iter);
            }
        }
    }
    updateStatusBarValues(win);
    gtk_widget_queue_draw(priv->graphDrawing);
}

static void adjustExtendedMaxLoad(NetloadGraphWinPrivate *priv)
{
    int i, curStat, ifNo;
    unsigned long long diffTx, diffRx, tdiff, diffMax = 0;
    const TransferStats *stats;
    struct IfaceStat *ifaceStat;
    char *ifname;
    GtkTreeModel *listStore;
    GtkTreeSelection *sel;
    GtkTreeIter iter;
    gboolean found;
    enum MaximumLoad newMaxLoad;

    if( priv->autoExtendMaximum ) {
        stats = getTransferStats();
        listStore = gtk_tree_view_get_model(
                GTK_TREE_VIEW(priv->interfacesView));
        sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->interfacesView));
        curStat = stats->newestStat;
        for(i = 0; i < priv->dispStatsCount && stats->timeSpan[curStat] != 0;
                ++i)
        {
            diffTx = diffRx = 0;
            for(found = gtk_tree_model_get_iter_first(listStore, &iter); found;
                found = gtk_tree_model_iter_next(listStore, &iter) )
            {
                if( gtk_tree_selection_iter_is_selected(sel, &iter ) ) {
                    gtk_tree_model_get(listStore, &iter, 0, &ifname, -1);
                    for(ifNo = 0; ifNo < stats->ifaceCnt; ++ifNo) {
                        ifaceStat = stats->ifaceStats[ifNo];
                        if( ! strcmp(ifname, ifaceStat->ifname) ) {
                            diffTx += ifaceStat->tdiffs[TDIR_TX][curStat];
                            diffRx += ifaceStat->tdiffs[TDIR_RX][curStat];
                        }
                    }
                }
            }
            tdiff = MAX(diffTx, diffRx) * 1000 / stats->timeSpan[curStat];
            if( tdiff > diffMax )
                diffMax = tdiff;
            if( --curStat < 0 )
                curStat = STATS_MAX-1;
        }
        newMaxLoad = priv->maximumLoad;
        while( newMaxLoad != ML_2G && diffMax > gMaxLoadValues[newMaxLoad] )
            ++newMaxLoad;
        if( priv->extendedMaxLoad <= newMaxLoad ||
                priv->extDelayTimerVal > priv->extendedMaxShrinkDelay)
        {
            priv->extendedMaxLoad = newMaxLoad;
            priv->extDelayTimerVal = 0;
        }
    }else{
        priv->extendedMaxLoad = priv->maximumLoad;
    }
}

static int valueToPoint(unsigned long long value, int height,
        enum ScaleType scaleType, enum MaximumLoad maxLoad)
{
    int res;

    if( scaleType == ST_LINEAR )
        res = height - height * value / gMaxLoadValues[maxLoad];
    else
        res = height -
            (value ? height * log(value)/log(gMaxLoadValues[maxLoad]) : 0);

    return res;
}

static void drawScaleLines(cairo_t *cr, guint width, guint height,
        NetloadGraphWinPrivate *priv)
{
    static const int firstDigits[] = { 1, 2, 5 };
    static const int scaleUnitsAdd[] = { 0, 3, 7 };
    static const char *const suffixes[] = { "", "k", "M", "G" };
    int i, suffixNo;
    gboolean writeMaxLoad = TRUE;

    cairo_set_font_size(cr, 12);
    if( priv->scaleLinesMax > 0 && height > priv->scaleLinesMinDistance ) {
        if( priv->scaleType == ST_LINEAR ) {
            i = height / priv->scaleLinesMinDistance;
            if( i > priv->scaleLinesMax )
                i = priv->scaleLinesMax + 1;
            int maxScaleLines = firstDigits[priv->extendedMaxLoad % 3];
            if( maxScaleLines <= i ) {
                while( 10 * maxScaleLines <= i )
                    maxScaleLines *= 10;
                if( 5 * maxScaleLines <= i )
                    maxScaleLines *= 5;
                else if( 4 * maxScaleLines <= i )
                    maxScaleLines *= 4;
                else if( 2 * maxScaleLines <= i )
                    maxScaleLines *= 2;
                for(i = 1; i < maxScaleLines; ++i) {
                    char buf[20];
                    int len;
                    sprintf(buf, "%llu", 1llu * i *
                        gMaxLoadValues[priv->extendedMaxLoad] / maxScaleLines);
                    len = strlen(buf);
                    suffixNo = 0;
                    while( len > 3 && !strcmp(buf + len - 3, "000") ) {
                        ++suffixNo;
                        buf[len -= 3] = '\0';
                    }
                    if( len >= 4 && buf[len - 1] == '0' ) {
                        buf[len-1] = buf[len-2] == '0' ? '\0' : buf[len-2];
                        buf[len-2] = buf[len-3];
                        buf[len-3] = '.';
                        ++suffixNo;
                    }
                    strcat(buf, suffixes[suffixNo]);
                    cairo_move_to(cr, 0, height - i * height/maxScaleLines+12);
                    cairo_show_text(cr, buf);
                    cairo_new_path(cr);
                    cairo_set_line_width(cr, 1);
                    cairo_move_to(cr, 0, height - i * height / maxScaleLines);
                    cairo_line_to(cr, width,
                            height - i * height / maxScaleLines);
                    cairo_stroke (cr);
                }
            }
        }else{
            int skipMinDist, skipMaxCount, skip;
            /* Scale unit: 10 * distance between lines for values N and 10*N,
             * e.g. between "1M" line and "10M" line.
             * Distance between "1M" line and "2M" line is ~3 scale units
             * between "2M" and "5M" - 4 scale units
             * between "5M" and "10M" - 3 scale units
             */
            int scaleUnitCount = 10 * (priv->extendedMaxLoad / 3) +
                scaleUnitsAdd[priv->extendedMaxLoad % 3];
            /*calculate skip for achieve minimum distance between scale lines*/
            skipMinDist = (3 * priv->scaleLinesMinDistance * scaleUnitCount /
                    height + 11) / 10;
            /* calculate skip for achieve scale line count below maximum */
            skipMaxCount = (priv->extendedMaxLoad + priv->scaleLinesMax) /
                (priv->scaleLinesMax+1);
            skip = skipMinDist > skipMaxCount ? skipMinDist : skipMaxCount;
            for(i = skip; i < priv->extendedMaxLoad; i += skip) {
                int y = valueToPoint(gMaxLoadValues[i], height, ST_LOGARITHMIC,
                        priv->extendedMaxLoad);
                cairo_move_to(cr, 0, y + 12);
                cairo_show_text(cr, gMaxLoadDispNames[i]);
                cairo_new_path(cr);
                cairo_set_line_width(cr, i % (skip%3?3:9) ? 1 : 2);
                cairo_move_to(cr, 0, y);
                cairo_line_to(cr, width, y);
                cairo_stroke (cr);
                if( y < 15 )
                    writeMaxLoad = FALSE;
            }
        }
    }
    if( writeMaxLoad ) {
        cairo_move_to(cr, 0, 12);
        cairo_show_text(cr, gMaxLoadDispNames[priv->extendedMaxLoad]);
    }
}

static gint drawGraphLine(cairo_t *cr, guint width, guint height,
        NetloadGraphWinPrivate *priv, enum TransferDirection tdir)
{
    gint i, curStat, ifNo, x, y, curPoint, rightmostPoint, res = 0;
    unsigned long long tdiff;
    const TransferStats *stats;
    struct IfaceStat *ifaceStat;
    char *ifname;
    GtkTreeModel *listStore;
    GtkTreeSelection *sel;
    GtkTreeIter iter;
    gboolean found;

    stats = getTransferStats();
    listStore = gtk_tree_view_get_model(GTK_TREE_VIEW(priv->interfacesView));
    sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->interfacesView));
    curStat = stats->newestStat;
    if( priv->refreshBehavior == RB_MOVE ) {
        priv->newestPoint = MIN(stats->statsCount, priv->dispStatsCount) - 1;
        rightmostPoint = priv->dispStatsCount - 1;
    }else{  /* RB_WRAP */
        if( priv->newestPoint > 0 )
            priv->newestPoint %= priv->dispStatsCount + priv->gapSize - 1;
        rightmostPoint = priv->dispStatsCount + priv->gapSize - 1;
    }
    curPoint = priv->newestPoint;
    for(i = 0; i < priv->dispStatsCount && i < stats->statsCount; ++i){
        tdiff = 0;
        for(found = gtk_tree_model_get_iter_first(listStore, &iter); found;
            found = gtk_tree_model_iter_next(listStore, &iter) )
        {
            if( gtk_tree_selection_iter_is_selected(sel, &iter ) ) {
                gtk_tree_model_get(listStore, &iter, 0, &ifname, -1);
                for(ifNo = 0; ifNo < stats->ifaceCnt; ++ifNo) {
                    ifaceStat = stats->ifaceStats[ifNo];
                    if( ! strcmp(ifname, ifaceStat->ifname) ) {
                        tdiff += ifaceStat->tdiffs[tdir][curStat];
                    }
                }
            }
        }
        tdiff = tdiff * 1000 / stats->timeSpan[curStat];
        x = curPoint * width / rightmostPoint;
        y = valueToPoint(tdiff, height, priv->scaleType, priv->extendedMaxLoad);
        if( i == 0 ) {
            if( curPoint < rightmostPoint ) {
                int circleSize = gSepCircleSizes[priv->separatorType];
                if( circleSize > 0 ) {
                    cairo_arc(cr, x, y, circleSize, 0, 2 * M_PI);
                    cairo_fill(cr);
                }
                res = x;
            }
            cairo_move_to(cr, x, y);
        }else{
            cairo_line_to(cr, x, y);
        }
        if( curPoint == 0 ) {
            cairo_stroke (cr);
            cairo_move_to(cr, width, y);
            curPoint = rightmostPoint;
        }
        --curPoint;
        if( --curStat < 0 )
            curStat = STATS_MAX-1;
    }
    cairo_stroke (cr);
    return res;
}

gboolean on_graphDrawing_draw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    GdkRGBA color;
    guint width, height;
    gint newestX, barWidth;
    NetloadGraphWinPrivate *priv;

    priv = netload_graph_win_get_instance_private(NETLOAD_GRAPH_WIN(data));
    adjustExtendedMaxLoad(priv);
    width = gtk_widget_get_allocated_width (widget);
    height = gtk_widget_get_allocated_height (widget);
    gtk_style_context_get_color(
            gtk_widget_get_style_context (widget), 0, &color);
    gdk_cairo_set_source_rgba (cr, &color);
    drawScaleLines(cr, width, height, priv);
    cairo_set_line_width(cr, 4);
    cairo_set_source_rgba (cr, priv->txColor.red, priv->txColor.green,
            priv->txColor.blue, priv->txColor.alpha);
    drawGraphLine(cr, width, height, priv, TDIR_TX);
    cairo_set_source_rgba (cr, priv->rxColor.red, priv->rxColor.green,
            priv->rxColor.blue, priv->rxColor.alpha);
    newestX = drawGraphLine(cr, width, height, priv, TDIR_RX);
    barWidth = gSepBarSizes[priv->separatorType];
    if( newestX > 0 && barWidth > 0 ) {
        gdk_cairo_set_source_rgba (cr, &color);
        cairo_rectangle(cr, newestX, 0, barWidth, height);
        cairo_fill(cr);
    }
    return FALSE;
}

void on_interfacesSelection_changed(GtkTreeSelection *sel, gpointer data)
{
    NetloadGraphWinPrivate *priv;

    priv = netload_graph_win_get_instance_private(NETLOAD_GRAPH_WIN(data));
    priv->extendedMaxLoad = priv->maximumLoad;
    updateStatusBarValues(NETLOAD_GRAPH_WIN(data));
    gtk_widget_queue_draw(priv->graphDrawing);
}

enum NetloadGraphWinProperties {
    PROP_0,
    PROP_TX_COLOR,
    PROP_RX_COLOR,
    PROP_REFRESH_BEHAVIOR,
    PROP_SCALE_TYPE,
    PROP_MAXIMUM_LOAD,
    PROP_AUTO_EXTEND_MAXIMUM,
    PROP_EXTENDEDMAX_SHRINK_DELAY,
    PROP_DISP_STATS_COUNT,
    PROP_SEPARATOR_TYPE,
    PROP_GAP_SIZE,
    PROP_INTERFACES_SELECT,
    PROP_INTERFACES_UNSELECT,
    PROP_SCALE_LINES_MIN_DISTANCE,
    PROP_SCALE_LINES_MAX
};

static void netload_graph_win_set_property(GObject *obj,
        guint property_id, const GValue *value, GParamSpec *pspec)
{
    NetloadGraphWinPrivate *priv;
    PangoAttrList *pangoAttrs;
    const gchar *str;
    int item;

    priv = netload_graph_win_get_instance_private(NETLOAD_GRAPH_WIN(obj));
    switch( property_id ) {
    case PROP_TX_COLOR:
        gdk_rgba_parse(&priv->txColor, g_value_get_string(value));
        pangoAttrs = pango_attr_list_new ();
        pango_attr_list_insert(pangoAttrs, pango_attr_background_new(
                65535 * priv->txColor.red,
                65535 * priv->txColor.green,
                65535 * priv->txColor.blue));
        gtk_label_set_attributes(GTK_LABEL(priv->labelTxColor), pangoAttrs);
        pango_attr_list_unref(pangoAttrs);
        gtk_widget_queue_draw(priv->graphDrawing);
        break;
    case PROP_RX_COLOR:
        gdk_rgba_parse(&priv->rxColor, g_value_get_string(value));
        pangoAttrs = pango_attr_list_new ();
        pango_attr_list_insert(pangoAttrs, pango_attr_background_new(
                65535 * priv->rxColor.red,
                65535 * priv->rxColor.green,
                65535 * priv->rxColor.blue));
        gtk_label_set_attributes(GTK_LABEL(priv->labelRxColor), pangoAttrs);
        pango_attr_list_unref(pangoAttrs);
        gtk_widget_queue_draw(priv->graphDrawing);
        break;
    case PROP_REFRESH_BEHAVIOR:
        str = g_value_get_string(value);
        if( (item = indexOf(str, gGraphTypeDispNames,
                G_N_ELEMENTS(gGraphTypeDispNames))) >= 0 )
        {
            priv->refreshBehavior = item;
            gtk_widget_queue_draw(priv->graphDrawing);
        }else
            g_warning("unrecognized refresh type \"%s\", ignored", str);
        break;
    case PROP_SCALE_TYPE:
        str = g_value_get_string(value);
        if( (item = indexOf(str, gScaleTypeDispNames,
                G_N_ELEMENTS(gScaleTypeDispNames))) >= 0 )
        {
            priv->scaleType = item;
            gtk_widget_queue_draw(priv->graphDrawing);
        }else
            g_warning("unrecognized scale type \"%s\", ignored", str);
        break;
    case PROP_MAXIMUM_LOAD:
        str = g_value_get_string(value);
        if( (item = indexOf(str, gMaxLoadDispNames,
                G_N_ELEMENTS(gMaxLoadDispNames))) >= ML_1k )
        {
            priv->maximumLoad = priv->extendedMaxLoad = item;
            gtk_widget_queue_draw(priv->graphDrawing);
        }else
            g_warning("unrecognized maximum load value \"%s\", ignored", str);
        break;
    case PROP_AUTO_EXTEND_MAXIMUM:
        priv->autoExtendMaximum = g_value_get_boolean(value);
        gtk_widget_queue_draw(priv->graphDrawing);
        break;
    case PROP_EXTENDEDMAX_SHRINK_DELAY:
        priv->extendedMaxShrinkDelay = g_value_get_uint(value);
        break;
    case PROP_DISP_STATS_COUNT:
        priv->dispStatsCount = g_value_get_uint(value);
        gtk_widget_queue_draw(priv->graphDrawing);
        break;
    case PROP_SEPARATOR_TYPE:
        str = g_value_get_string(value);
        if( (item = indexOf(str, gSepTypeDispNames,
                G_N_ELEMENTS(gSepTypeDispNames))) >= 0 )
        {
            priv->separatorType = item;
            gtk_widget_queue_draw(priv->graphDrawing);
        }else
            g_warning("unrecognized separator type \"%s\", ignored", str);
        break;
    case PROP_GAP_SIZE:
        priv->gapSize = g_value_get_uint(value);
        gtk_widget_queue_draw(priv->graphDrawing);
        break;
    case PROP_INTERFACES_SELECT:
        g_free(priv->interfacesSelect);
        priv->interfacesSelect = prepareGlobMatch(g_value_get_string(value));
        break;
    case PROP_INTERFACES_UNSELECT:
        g_free(priv->interfacesUnselect);
        priv->interfacesUnselect = prepareGlobMatch(g_value_get_string(value));
        break;
    case PROP_SCALE_LINES_MIN_DISTANCE:
        priv->scaleLinesMinDistance = g_value_get_uint(value);
        gtk_widget_queue_draw(priv->graphDrawing);
        break;
    case PROP_SCALE_LINES_MAX:
        priv->scaleLinesMax = g_value_get_uint(value);
        gtk_widget_queue_draw(priv->graphDrawing);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
        break;
    }
}

static void netload_graph_win_dispose(GObject *obj)
{
    NetloadGraphWinPrivate *priv;

    priv = netload_graph_win_get_instance_private(NETLOAD_GRAPH_WIN(obj));
    g_free(priv->interfacesSelect);
    priv->interfacesSelect = NULL;
    g_free(priv->interfacesUnselect);
    priv->interfacesUnselect = NULL;
    G_OBJECT_CLASS (netload_graph_win_parent_class)->dispose(obj);
}

static void netload_graph_win_class_init(NetloadGraphWinClass *class)
{
    static const int propInstFlags = G_PARAM_WRITABLE | G_PARAM_STATIC_NAME |
        G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB;
    GObjectClass *oclass = G_OBJECT_CLASS(class);

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
            "/org/rafaello7/netloadgraph/netloadgraphwin.ui");
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (class),
            NetloadGraphWin, interfacesView);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (class),
            NetloadGraphWin, graphDrawing);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (class),
            NetloadGraphWin, labelTxColor);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (class),
            NetloadGraphWin, labelTx);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (class),
            NetloadGraphWin, labelRxColor);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (class),
            NetloadGraphWin, labelRx);
    oclass->set_property = netload_graph_win_set_property;
    oclass->dispose = netload_graph_win_dispose;

    g_object_class_install_property(oclass, PROP_TX_COLOR,
           g_param_spec_string("tx-color", "TX color",
               "Transmit speed graph color", "gold", propInstFlags));

    g_object_class_install_property(oclass, PROP_RX_COLOR,
           g_param_spec_string("rx-color", "RX color",
               "Receive speed graph color", "coral", propInstFlags));

    g_object_class_install_property(oclass, PROP_REFRESH_BEHAVIOR,
           g_param_spec_string("refresh-behavior", "Refresh behavior",
               "Where to plot new data at refresh", "Move", propInstFlags));

    g_object_class_install_property(oclass, PROP_SCALE_TYPE,
           g_param_spec_string("scale-type",
            "Scale type", "Scale type.", "Linear", propInstFlags));

    g_object_class_install_property(oclass, PROP_MAXIMUM_LOAD,
           g_param_spec_string("maximum-load", "Maximum load value",
               "Maximum load value displayed on graph.", "1M", propInstFlags));

    g_object_class_install_property(oclass, PROP_AUTO_EXTEND_MAXIMUM,
           g_param_spec_boolean("auto-extend-maximum",
            "Automatically extend maximum",
            "Whether extend maximum value displayed on graph "
            "when some value exceeds current max.", TRUE, propInstFlags));

    g_object_class_install_property(oclass, PROP_EXTENDEDMAX_SHRINK_DELAY,
           g_param_spec_uint("extendedmax-shrink-delay",
            "Extended maximum shrink back delay",
            "Extended maximum shrink back delay", 0, 999, 1, propInstFlags));

    g_object_class_install_property(oclass, PROP_DISP_STATS_COUNT,
           g_param_spec_uint("displayed-statistics-count",
               "Displayed statistics count",
               "Number of statistics included on the graph chart",
               2, 256, 64, propInstFlags));

    g_object_class_install_property(oclass, PROP_SEPARATOR_TYPE,
           g_param_spec_string("separator-type",
               "Newest from oldest separator",
               "Newest from oldest separator", "Circle medium",
               propInstFlags));

    g_object_class_install_property(oclass, PROP_GAP_SIZE,
           g_param_spec_uint("gap-size", "Gap between newest and oldest",
               "Size of gap between newest and oldest point "
               "in Wrap refresh mode", 0, 128, 9, propInstFlags));

    g_object_class_install_property(oclass, PROP_INTERFACES_SELECT,
           g_param_spec_string("interfaces-select",
               "Interfaces selected initially",
               "Interfaces selected initially", "", propInstFlags));

    g_object_class_install_property(oclass, PROP_INTERFACES_UNSELECT,
           g_param_spec_string("interfaces-unselect",
            "Interfaces unselected initially",
            "Interfaces unselected initially", "lo tun* sit* ip6tnl*", propInstFlags));

    g_object_class_install_property(oclass, PROP_SCALE_LINES_MIN_DISTANCE,
           g_param_spec_uint("scale-lines-min-distance",
               "Scale lines minimum distance",
               "Minimum distance between scale lines", 15, 1000, 50,
               propInstFlags));

    g_object_class_install_property(oclass, PROP_SCALE_LINES_MAX,
           g_param_spec_uint("scale-lines-max", "Scale lines max",
               "Maximum number of scale lines", 0, 200, 16, propInstFlags));
}

NetloadGraphWin *netload_graph_win_new(NetloadGraphApp *app,
        GSettings *settings)
{
    static const int bindFlags = G_SETTINGS_BIND_GET |
        G_SETTINGS_BIND_NO_SENSITIVITY;
    NetloadGraphWin *res;
   
    res = g_object_new(NETLOAD_GRAPH_WIN_TYPE, "application", app, NULL);
    g_settings_bind(settings, "tx-color", res, "tx-color", bindFlags);
    g_settings_bind(settings, "rx-color", res, "rx-color", bindFlags);
    g_settings_bind(settings, "refresh-behavior", res,
            "refresh-behavior", bindFlags);
    g_settings_bind(settings, "scale-type", res, "scale-type", bindFlags);
    g_settings_bind(settings, "maximum-load", res, "maximum-load", bindFlags);
    g_settings_bind(settings, "auto-extend-maximum", res,
            "auto-extend-maximum", bindFlags);
    g_settings_bind(settings, "extendedmax-shrink-delay", res,
            "extendedmax-shrink-delay", bindFlags);
    g_settings_bind(settings, "displayed-statistics-count", res,
            "displayed-statistics-count", bindFlags);
    g_settings_bind(settings, "separator-type", res,
            "separator-type", bindFlags);
    g_settings_bind(settings, "gap-size", res, "gap-size", bindFlags);
    g_settings_bind(settings, "interfaces-select", res,
            "interfaces-select", bindFlags);
    g_settings_bind(settings, "interfaces-unselect", res,
            "interfaces-unselect", bindFlags);
    g_settings_bind(settings, "scale-lines-min-distance", res,
            "scale-lines-min-distance", bindFlags);
    g_settings_bind(settings, "scale-lines-max", res,
            "scale-lines-max", bindFlags);
    return res;
}

