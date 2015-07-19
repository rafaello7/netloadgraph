#include <gtk/gtk.h>
#include "netloadgraphapp.h"

int main(int argc, char *argv[])
{
    int res;
    NetloadGraphApp *app;

    app = netload_graph_app_new();
    res = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return res;
}
