# New ports collection makefile for: stasher
# Date created:		September, 2012
# Whom:                 Sam Varshavchik <mrsam@courier-mta.com>
# $FreeBSD$

PORTNAME=		stasher
PORTVERSION=		0.01
CATEGORIES=		databases

MAINTAINER=		mrsam@courier-mta.com
COMMENT=		Stasher object repository

LIB_DEPENDS=		cxxtls:${PORTSDIR}/devel/libcxx
USE_BZIP2=1
USE_GMAKE=yes
GNU_CONFIGURE=1
USE_PKGCONFIG=build

CONFIGURE_ARGS=		CPPFLAGS="-D__LONG_LONG_SUPPORTED"
MAN1=			stasher.1 stashermg.1

MAN1COMPRESSED=		yes

USE_RC_SUBR=		stasherd

post-install:
	PKG_PREFIX=${PREFIX} ${SH} ${PKGINSTALL} ${PKGNAME} POST-INSTALL
	$(INSTALL_SCRIPT) $(WRKSRC)/stasher.sysinit ${PREFIX}/share/stasher
	$(INSTALL_SCRIPT) $(WRKSRC)/sysinit.rc ${PREFIX}/share/stasher
	mkdir -p ${PREFIX}/etc/periodic/daily
	ln -fs ../../../share/stasher/maillogs.sh ${PREFIX}/etc/periodic/daily

post-deinstall:
	PKG_PREFIX=${PREFIX} ${SH} ${PKGDEINSTALL} ${PKGNAME} POST-DEINSTALL

.include <bsd.port.mk>
