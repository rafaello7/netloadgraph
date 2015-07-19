#ifndef NETLOADGRAPHAPP_H
#define NETLOADGRAPHAPP_H

#include <gtk/gtk.h>


#define NETLOAD_GRAPH_APP_TYPE (netload_graph_app_get_type ())
#define NETLOAD_GRAPH_APP(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), NETLOAD_GRAPH_APP_TYPE, NetloadGraphApp))

typedef struct {
    GtkApplication parent;
} NetloadGraphApp;

typedef struct {
    GtkApplicationClass parent_class;
} NetloadGraphAppClass;


GType netload_graph_app_get_type(void);
NetloadGraphApp *netload_graph_app_new(void);


#endif /* NETLOADGRAPHAPP_H */
