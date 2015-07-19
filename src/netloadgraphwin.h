#ifndef NETLOADGRAPHWIN_H
#define NETLOADGRAPHWIN_H

#include <gtk/gtk.h>
#include "netloadgraphapp.h"

#define NETLOAD_GRAPH_WIN_TYPE (netload_graph_win_get_type ())
#define NETLOAD_GRAPH_WIN(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), NETLOAD_GRAPH_WIN_TYPE, NetloadGraphWin))

typedef struct {
    GtkApplicationWindow parent;
} NetloadGraphWin;

typedef struct {
    GtkApplicationWindowClass parent_class;
} NetloadGraphWinClass;

GType netload_graph_win_get_type(void);
NetloadGraphWin *netload_graph_win_new(NetloadGraphApp*, GSettings*);

void onNewDataFetched(NetloadGraphWin*);

#endif /* NETLOADGRAPHWIN_H */
