SUBDIRS = src icons debian

desktopdir = $(datadir)/applications
dist_desktop_DATA = netloadgraph.desktop

deb: dist
	rm -rf dpkg-deb
	mkdir dpkg-deb
	cd dpkg-deb && tar xf ../$(distdir).tar.gz
	cd dpkg-deb/$(distdir) && dpkg-buildpackage -us -uc

arch: dist
	rm -rf archlinux
	mkdir archlinux
	cp PKGBUILD netloadgraph-*.tar.gz archlinux
	cd archlinux && makepkg
