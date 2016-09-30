# Maintainer: Rafal <fatwildcat@gmail.com>
pkgname=netloadgraph
pkgver=0.1.1
pkgrel=1
pkgdesc="Display network load on a graph"
arch=('x86_64')
url="https://github.com/rafaello7/netloadgraph"
license=('GPL')
depends=('gtk3')
source=("$pkgname-$pkgver.tar.gz")
md5sums=('SKIP')

build() {
	cd "$pkgname-$pkgver"
	./configure --prefix=/usr
	make
}

package() {
	cd "$pkgname-$pkgver"
	make DESTDIR="$pkgdir/" install
}
