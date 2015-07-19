UI = netloadgraphwin.ui appmenu.ui netloadgraphwinprefs.ui

bin_PROGRAMS = netloadgraph

netloadgraph_SOURCES = netloadstats.c netloadgraphapp.c netloadgraphwin.c \
					   main.c netloadgraphwinprefs.c \
					   netloadgraphapp.h netloadgraphwin.h netloadstats.h \
					   netloadgraphwinprefs.h netloadgraph.gresource.xml \
					   $(UI)

nodist_netloadgraph_SOURCES = resources.c

netloadgraph_CFLAGS = $(LIBGTK_CFLAGS)
netloadgraph_LDADD = $(LIBGTK_LIBS)
netloadgraph_LDFLAGS = -rdynamic

gsettings_SCHEMAS = net.sourceforge.netloadgraph.gschema.xml

@GSETTINGS_RULES@

EXTRA_DIST = $(gsettings_SCHEMAS) netloadgraph.gresource.xml

resources.c: netloadgraph.gresource.xml $(UI)
	glib-compile-resources --sourcedir=$(srcdir) $< --target=$@ \
		--generate-source

CLEANFILES = resources.c

