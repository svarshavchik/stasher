# New ports collection makefile for: stasher
# Date created:		September, 2012
# Whom:                 Sam Varshavchik <mrsam@courier-mta.com>
# $FreeBSD$

PORTNAME=		stasher
PORTVERSION=		0.04
CATEGORIES=		databases

MAINTAINER=		mrsam@courier-mta.com
COMMENT=		Stasher object repository

LIB_DEPENDS=		cxxtls:${PORTSDIR}/devel/libcxx
USE_BZIP2=1
USE_GMAKE=yes
GNU_CONFIGURE=1
USE_PKGCONFIG=build

USES=perl5
USE_PERL5=5.16+
CONFIGURE_ARGS=		CPPFLAGS="-D__LONG_LONG_SUPPORTED" PERL=$(PERL)$(PERL_VERSION)
MAN1=			stasher.1 stashermg.1

MAN1COMPRESSED=		yes

USE_RC_SUBR=		stasherd

post-configure:
	cd $(WRKSRC); gmake perl-configure

post-build:
	cd $(WRKSRC); gmake perl-build

post-install:
	cd $(WRKSRC)/perl/Stasher && gmake install DESTDIR=$(STAGEDIR)
	PKG_PREFIX=$(STAGEDIR)${PREFIX} ${SH} ${PKGINSTALL} ${PKGNAME} POST-INSTALL
	$(INSTALL_SCRIPT) $(WRKSRC)/stasher.sysinit $(STAGEDIR)${PREFIX}/share/stasher
	$(INSTALL_SCRIPT) $(WRKSRC)/sysinit.rc $(STAGEDIR)${PREFIX}/share/stasher
	mkdir -p $(STAGEDIR)${PREFIX}/etc/periodic/daily
	ln -fs ../../../share/stasher/maillogs.sh $(STAGEDIR)${PREFIX}/etc/periodic/daily

post-deinstall:
	PKG_PREFIX=${PREFIX} ${SH} ${PKGDEINSTALL} ${PKGNAME} POST-DEINSTALL

.include <bsd.port.mk>
