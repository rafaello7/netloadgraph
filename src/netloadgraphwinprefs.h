#ifndef NETLOADGRAPHWINPREFS_H
#define NETLOADGRAPHWINPREFS_H

#include <gtk/gtk.h>

#define NETLOAD_GRAPH_WIN_PREFS_TYPE (netload_graph_win_prefs_get_type ())
#define NETLOAD_GRAPH_WIN_PREFS(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), NETLOAD_GRAPH_WIN_PREFS_TYPE, NetloadGraphWinPrefs))

typedef struct {
    GtkDialog parent;
} NetloadGraphWinPrefs;

typedef struct {
    GtkDialogClass parent_class;
} NetloadGraphWinPrefsClass;

GType netload_graph_win_prefs_get_type(void);
NetloadGraphWinPrefs *netload_graph_win_prefs_new(GSettings*);

#endif /* NETLOADGRAPHWINPREFS_H */
