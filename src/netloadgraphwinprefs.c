#include <gtk/gtk.h>
#include "netloadgraphwinprefs.h"
#include <math.h>


typedef struct {
    GSettings *settings;
    GtkWidget *txColor;
    GtkWidget *rxColor;
    GtkWidget *refreshPeriod;
    GtkWidget *refreshBehavior;
    GtkWidget *scaleType;
    GtkWidget *maximumLoad;
    GtkWidget *autoExtendMaximum;
    GtkWidget *extendedMaxShrinkDelay;
    GtkWidget *dispStatsCount;
    GtkWidget *separatorType;
    GtkWidget *gapSize;
    GtkWidget *interfacesSelect;
    GtkWidget *interfacesUnselect;
    GtkWidget *scaleLinesMinDistance;
    GtkWidget *scaleLinesMax;
} NetloadGraphWinPrefsPrivate;


G_DEFINE_TYPE_WITH_PRIVATE(NetloadGraphWinPrefs, netload_graph_win_prefs, GTK_TYPE_DIALOG);

static void netload_graph_win_prefs_init(NetloadGraphWinPrefs *win)
{
    gtk_widget_init_template(GTK_WIDGET (win));
}

static void netload_graph_win_prefs_class_init(NetloadGraphWinPrefsClass *class)
{
    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
            "/org/rafaello7/netloadgraph/netloadgraphwinprefs.ui");
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(class),
            NetloadGraphWinPrefs, txColor);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(class),
            NetloadGraphWinPrefs, rxColor);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(class),
            NetloadGraphWinPrefs, refreshPeriod);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(class),
            NetloadGraphWinPrefs, refreshBehavior);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(class),
            NetloadGraphWinPrefs, scaleType);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(class),
            NetloadGraphWinPrefs, maximumLoad);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(class),
            NetloadGraphWinPrefs, autoExtendMaximum);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(class),
            NetloadGraphWinPrefs, extendedMaxShrinkDelay);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(class),
            NetloadGraphWinPrefs, dispStatsCount);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(class),
            NetloadGraphWinPrefs, separatorType);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(class),
            NetloadGraphWinPrefs, gapSize);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(class),
            NetloadGraphWinPrefs, interfacesSelect);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(class),
            NetloadGraphWinPrefs, interfacesUnselect);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(class),
            NetloadGraphWinPrefs, scaleLinesMinDistance);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(class),
            NetloadGraphWinPrefs, scaleLinesMax);
}

static gboolean colorGetMapping(GValue *value, GVariant *variant,
        gpointer user_data)
{
    const gchar *colorStr;
    GdkRGBA rgba;
    gboolean res;

    colorStr = g_variant_get_string(variant, NULL);
    res = gdk_rgba_parse(&rgba, colorStr);
    if( res )
        g_value_set_boxed(value, &rgba);
    else
        g_critical("colorGetMapping: bad color string=\"%s\"", colorStr);
    return res;
}

static GVariant *colorSetMapping(const GValue *value,
        const GVariantType *expected_type, gpointer user_data)
{
    GdkRGBA *rgba;
    gchar *colorStr;
    GVariant *res = NULL;

    if( g_variant_type_equal(expected_type, G_VARIANT_TYPE_STRING) ) {
        rgba = g_value_get_boxed(value);
        colorStr = gdk_rgba_to_string(rgba);
        res = g_variant_new_take_string(colorStr);
    }else
        g_critical("colorSetMapping: unsupported result type %.*s\n",
                (int)g_variant_type_get_string_length(expected_type),
                g_variant_type_peek_string(expected_type));
    return res;
}

NetloadGraphWinPrefs *netload_graph_win_prefs_new(GSettings *settings)
{
    NetloadGraphWinPrefsPrivate *priv;
    NetloadGraphWinPrefs *res;

    res = g_object_new(NETLOAD_GRAPH_WIN_PREFS_TYPE, NULL);
    priv = netload_graph_win_prefs_get_instance_private(res);
    priv->settings = settings;
    g_settings_bind_with_mapping(settings, "tx-color", priv->txColor,
            "rgba", G_SETTINGS_BIND_DEFAULT,
            colorGetMapping, colorSetMapping, NULL, NULL);
    g_settings_bind_with_mapping(settings, "rx-color", priv->rxColor,
            "rgba", G_SETTINGS_BIND_DEFAULT,
            colorGetMapping, colorSetMapping, NULL, NULL);
    gtk_range_set_value(GTK_RANGE(priv->refreshPeriod),
            log10(g_settings_get_double(settings, "refresh-period")));
    /*g_settings_bind(settings, "refresh-period", priv->refreshPeriod,
     *        "value", G_SETTINGS_BIND_DEFAULT); */
    g_settings_bind(settings, "refresh-behavior", priv->refreshBehavior,
            "active-id", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(settings, "scale-type", priv->scaleType,
            "active-id", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(settings, "maximum-load", priv->maximumLoad,
            "active-id", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(settings, "auto-extend-maximum", priv->autoExtendMaximum,
            "active", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(settings, "extendedmax-shrink-delay",
            priv->extendedMaxShrinkDelay, "value", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(settings, "displayed-statistics-count",
            priv->dispStatsCount, "value", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(settings, "separator-type", priv->separatorType,
            "active-id", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(settings, "gap-size", priv->gapSize,
            "value", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(settings, "interfaces-select", priv->interfacesSelect,
            "text", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(settings, "interfaces-unselect", priv->interfacesUnselect,
            "text", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(settings, "scale-lines-min-distance",
            priv->scaleLinesMinDistance, "value", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(settings, "scale-lines-max", priv->scaleLinesMax,
            "value", G_SETTINGS_BIND_DEFAULT);
    return res;
}

gchar *on_refreshPeriod_format_value(GtkScale *scale, gdouble value)
{
    return g_strdup_printf("%.*g", MAX(2, (int)value+1), pow(10, value));
}

void on_refreshPeriod_value_changed(GtkScale *scale, gpointer data)
{
    NetloadGraphWinPrefsPrivate *priv;

    priv = netload_graph_win_prefs_get_instance_private(
            NETLOAD_GRAPH_WIN_PREFS(data));
    g_settings_set_double(priv->settings, "refresh-period",
            pow(10, gtk_range_get_value(GTK_RANGE(priv->refreshPeriod))));
}

