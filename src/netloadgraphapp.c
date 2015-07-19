#include <gtk/gtk.h>
#include "netloadgraphapp.h"
#include "netloadgraphwin.h"
#include "netloadgraphwinprefs.h"
#include "netloadstats.h"
#include <config.h>

typedef struct {
    guint timerId;
    GSettings *settings;
    gdouble refreshPeriod;
} NetloadGraphAppPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(NetloadGraphApp, netload_graph_app,
        GTK_TYPE_APPLICATION);

static void netload_graph_app_init(NetloadGraphApp *app)
{
    NetloadGraphAppPrivate *priv;

    priv = netload_graph_app_get_instance_private(NETLOAD_GRAPH_APP(app));
    priv->timerId = 0;
    priv->settings = g_settings_new("net.sourceforge.netloadgraph");
    priv->refreshPeriod = 2.0;
}

static void preferences_activated(GSimpleAction *action,
        GVariant *parameter, gpointer app)
{
    GList *windows;
    NetloadGraphWinPrefs *winPrefs;
    NetloadGraphAppPrivate *priv;

    priv = netload_graph_app_get_instance_private(NETLOAD_GRAPH_APP(app));
    winPrefs = netload_graph_win_prefs_new(priv->settings);
    windows = gtk_application_get_windows(GTK_APPLICATION(app));
    if( windows != NULL )
        gtk_window_set_transient_for(GTK_WINDOW(winPrefs),
                GTK_WINDOW(windows->data));
    gtk_widget_show(GTK_WIDGET(winPrefs));
}

static void about_activated(GSimpleAction *action,
        GVariant *parameter, gpointer app)
{
    GList *windows;

    windows = gtk_application_get_windows(GTK_APPLICATION(app));
    gtk_show_about_dialog(windows ? GTK_WINDOW(windows->data) : NULL,
            "program-name",     "NetLoad Graph",
            "version",          PACKAGE_VERSION,
            "copyright",        "Copyright (C) 2015 by Rafal Dabrowa",
            "website",          "http://netloadgraph.sourceforge.net",
            "logo-icon-name",   "netloadgraph",
            "license-type",     GTK_LICENSE_GPL_2_0,
            NULL);
}

static void quit_activated(GSimpleAction *action,
        GVariant *parameter, gpointer app)
{
    g_application_quit(G_APPLICATION (app));
}

static gboolean timeout_fun(gpointer data)
{
    GList *windows;

    fetchNextTransferStats();
    windows = gtk_application_get_windows(GTK_APPLICATION(data));
    while( windows != NULL ) {
        onNewDataFetched(NETLOAD_GRAPH_WIN(windows->data));
        windows = windows->next;
    }
    return TRUE;
}

static void netload_graph_app_startup(GApplication *app)
{
    GtkBuilder *builder;
    GMenuModel *appmenu;
    NetloadGraphAppPrivate *priv;
    static GActionEntry app_entries[] = {
        { "preferences", preferences_activated, NULL, NULL, NULL },
        { "about",       about_activated, NULL, NULL, NULL },
        { "quit",        quit_activated, NULL, NULL, NULL }
    };

    G_APPLICATION_CLASS(netload_graph_app_parent_class)->startup(app);
    priv = netload_graph_app_get_instance_private(NETLOAD_GRAPH_APP(app));
    gtk_window_set_default_icon_name("netloadgraph");
    g_action_map_add_action_entries (G_ACTION_MAP (app), app_entries,
            G_N_ELEMENTS(app_entries), app);

    builder = gtk_builder_new_from_resource(
            "/net/sourceforge/netloadgraph/appmenu.ui");
    appmenu = G_MENU_MODEL(gtk_builder_get_object(builder, "menubar"));
    gtk_application_set_menubar(GTK_APPLICATION(app), appmenu);
    g_object_unref(builder);

    fetchNextTransferStats();
    priv->timerId = g_timeout_add(priv->refreshPeriod * 1000, timeout_fun, app);
}

static void netload_graph_app_activate(GApplication *app)
{
    GList *windows;
    NetloadGraphWin *win;
    NetloadGraphAppPrivate *priv;

    windows = gtk_application_get_windows (GTK_APPLICATION (app));
    if (windows)
        win = NETLOAD_GRAPH_WIN(windows->data);
    else{
        priv = netload_graph_app_get_instance_private(NETLOAD_GRAPH_APP(app));
        win = netload_graph_win_new(NETLOAD_GRAPH_APP(app), priv->settings);
        onNewDataFetched(win);
    }
    gtk_window_present (GTK_WINDOW (win));
}

enum NetloadGraphWinProperties {
    PROP_0,
    PROP_REFRESH_PERIOD
};

static void netload_graph_app_set_property(GObject *obj,
        guint property_id, const GValue *value, GParamSpec *pspec)
{
    NetloadGraphAppPrivate *priv;

    priv = netload_graph_app_get_instance_private(NETLOAD_GRAPH_APP(obj));
    switch( property_id ) {
    case PROP_REFRESH_PERIOD:
        priv->refreshPeriod = g_value_get_double(value);
        if( priv->timerId > 0 ) {
            g_source_remove(priv->timerId);
            priv->timerId = g_timeout_add(priv->refreshPeriod * 1000,
                    timeout_fun, obj);
        }
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
        break;
    }
}

static void netload_graph_app_shutdown(GApplication *app)
{
    NetloadGraphAppPrivate *priv = netload_graph_app_get_instance_private(
            NETLOAD_GRAPH_APP(app));

    G_APPLICATION_CLASS(netload_graph_app_parent_class)->shutdown(app);
	g_source_remove(priv->timerId);
    g_clear_object(&priv->settings);
}

static void netload_graph_app_class_init(NetloadGraphAppClass *class)
{
    static const int propInstFlags = G_PARAM_WRITABLE | G_PARAM_STATIC_NAME |
        G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB;

    G_APPLICATION_CLASS(class)->startup = netload_graph_app_startup;
    G_APPLICATION_CLASS(class)->activate = netload_graph_app_activate;
    G_OBJECT_CLASS(class)->set_property = netload_graph_app_set_property;
    G_APPLICATION_CLASS(class)->shutdown = netload_graph_app_shutdown;

    g_object_class_install_property(G_OBJECT_CLASS(class), PROP_REFRESH_PERIOD,
           g_param_spec_double("refresh-period", "Refresh period",
               "Graph refresh period, in seconds", 0.1, 100.0, 2.0,
               propInstFlags));

}

NetloadGraphApp *netload_graph_app_new(void)
{
    static const int bindFlags = G_SETTINGS_BIND_GET |
        G_SETTINGS_BIND_NO_SENSITIVITY;
    NetloadGraphApp *res;
    NetloadGraphAppPrivate *priv;

    res = g_object_new (NETLOAD_GRAPH_APP_TYPE, "application-id",
            "net.sourceforge.netloadgraph",
            "flags", G_APPLICATION_FLAGS_NONE, NULL);
    priv = netload_graph_app_get_instance_private(res);
    g_settings_bind(priv->settings, "refresh-period", res,
            "refresh-period", bindFlags);
    return res;
}

