MANPAGESIN=stashermg.1.in stasher.1.in
man1_MANS=stashermg.1 stasher.1
sysconf_DATA=stasherlogconfig

EXTRA_DIST=config.rpath repomgcli.xml opts.parse.inc.tst.C message.xsl $(myXML) $(MANPAGESIN) maillogs.sh.in stasherlogconfig \
	packaging/freebsd9/databases/stasher/Makefile \
	packaging/freebsd9/databases/stasher/pkg-deinstall \
	packaging/freebsd9/databases/stasher/pkg-descr \
	packaging/freebsd9/databases/stasher/pkg-install \
	packaging/freebsd9/databases/stasher/pkg-plist \
	packaging/freebsd9/databases/stasher/files/stasherd.in

BUILT_SOURCES=localstatedir.h repomgcliopts.h stasherdpath.h
CLEANFILES=localstatedir.h repomgcliopts.h stasherdpath.h \
	stasher.sysinit sysinit.rc $(myCLEANFILES) $(man1_MANS)

XLIBS=-lcxx
#XLIBS=/home/mrsam/src/ng/gnutls/libxtls.la /home/mrsam/src/ng/base/libx.la

bin_PROGRAMS=stashermg

define genxsl

$(1): $(2) $(srcdir)/message.xsl
	mkdir -p $(patsubst %/,%,$(dir $(1)))
	xsltproc --xinclude --stringparam mode $(3) $(srcdir)/message.xsl $$< >$$@.tmp
	mv -f $$@.tmp $$@

all: $(1)

# Hack to keep automake from processing these settings, passing through to gmake

$(true) myCLEANFILES += $(1)
$(true) myXML += $(2)

endef

$(eval	\
$(call genxsl,stasher/results.auto.H,message.xml,resultsdecl)	\
$(call genxsl,results.auto.C,message.xml,resultsdef)		\
$(call genxsl,replymsgs.auto.H,message.xml,replymsgdecl)	\
$(call genxsl,replymsgs.auto.C,message.xml,replymsgdef)		\
$(call genxsl,client.auto.C,message.xml,client)			\
$(call genxsl,requestmsgs.auto.H,message.xml,reqdecl)		\
$(call genxsl,requestmsgs.auto.C,message.xml,reqdef)		\
$(call genxsl,localprivconnection.classlist.auto.H,message.xml,privclasslist) \
$(call genxsl,localprivconnection.deserialized.auto.H,message.xml,privdeserialized) \
$(call genxsl,localconnection.classlist.auto.H,message.xml,nonprivclasslist) \
$(call genxsl,localconnection.deserialized.auto.H,message.xml,nonprivdeserialized) \
$(call genxsl,stasher/client.api.auto.H,message.xml,clientdecl) \
$(call genxsl,client.classlist.auto.H,message.xml,clientclasslist) \
$(call genxsl,client.deserialized.auto.H,message.xml,clientdeserialized) \
)

dist-hook:
	cd $(distdir); rm -f $(myCLEANFILES)
	set -e; for f in `cat $(srcdir)/perl/Stasher/MANIFEST`; do $(mkdir_p) `dirname $(distdir)/perl/Stasher/$$f`; s="perl/Stasher/$$f"; if test ! -f "$$s"; then s="$(srcdir)/$$s"; fi; ln $$s $(distdir)/perl/Stasher/$$f; done
	rm -f $(distdir)/perl/Stasher/lib/Stasher.pm

pkginclude_HEADERS=\
	stasher/client.H		\
	stasher/clientfwd.H		\
	stasher/client.api.auto.H	\
	stasher/clusterstate.H		\
	stasher/current.H		\
	stasher/currentbase.H		\
	stasher/contents.H		\
	stasher/heartbeat.H		\
	stasher/managedconnectionupdate.H	\
	stasher/managedhierarchymonitor.H	\
	stasher/managedobject.H			\
	stasher/managedserverstatuscallback.H	\
	stasher/managedsubscriber.H	\
	stasher/manager.H		\
	stasher/namespace.H		\
	stasher/nodeinfo.H		\
	stasher/objname.H		\
	stasher/process_request.H	\
	stasher/puttransaction.H	\
	stasher/quorumstate.H		\
	stasher/quorumstateobj.H	\
	stasher/repoclusterquorumcallbackbase.H \
	stasher/retrobj.H		\
	stasher/serobjname.H		\
	stasher/serverstatuscallback.H  \
	stasher/results.auto.H		\
	stasher/reqstat.H		\
	stasher/userinit.H		\
	stasher/userhelo.H		\
	stasher/userput.H               \
	stasher/versionscollected.H     \
	stasher/versionedcurrent.H	\
	stasher/versionedptr.H          \
	stasher/versionedput.H

noinst_PROGRAMS=\
	stasherd.static \
	stasher.static \
	adminhalt.tst \
	adminsetcert.tst \
	adminsetcert2.tst \
	adminstop.tst \
	appsdir.tst \
	baton.tst \
	baton2.tst \
	clientreconnect.tst \
	clusterconnecter.tst \
	clusterinfo.tst \
	clusterlistener.tst \
	fdobjreaderthread.tst \
	fdobjrwthread.tst \
	fdobjwriterthread.tst \
	heartbeat.tst \
	managedhierarchymonitor.tst \
	managedserverstatuscallback.tst \
	managedusersubscribe.tst \
	managedobject.tst \
	managedobject2.tst \
	managedobject3.tst \
	maxintsize.tst	\
	newnode.tst     \
	newtran.tst	\
	nodeclusterstatus.tst \
	objrepo.tst	\
	objrepocopy.tst \
	objrepocopydstthread.tst \
	objrepocopysrcthread.tst \
	objserializer.tst \
	objsource.tst	\
	objuuidenumerator.tst \
	objwriter.tst	\
	objwriterthread.tst	\
	peerstatus.tst	\
	peerstatusannouncer.tst \
	readtimeout.tst \
	reconnect.tst \
	reconnectsync.tst \
	repoclusterinfo.tst \
	repocontrollermaster.tst \
	repocontrollermaster2.tst \
	repocontrollerslave.tst \
	repopeerconnection.tst \
	repopeerconnection2.tst \
	repopeerconnection3.tst \
	repopeerconnection4.tst \
	repomg.tst \
	statuscallback.tst \
	stoppablethreadtracker.tst \
	tobjrepo.tst	\
	trancanceller.tst \
	trandistributor.tst \
	trandistributor2.tst \
	trandistributor3.tst \
	trandistributor4.tst \
	tranmeta.tst	\
	transerializer.tst \
	userdir.tst \
	usergetuuids.tst \
	usergetuuids2.tst \
	userput.tst \
	usersubscribe.tst \
	verify.tst

lib_LTLIBRARIES=libstasher-client.la libstasher.la

libstasher_client_la_SOURCES=\
	client.C                                           \
	clusterstate.C                                     \
	dirs.C dirs.H                                      \
	heartbeat.C                                        \
	fdobjreaderthread.C fdobjreaderthread.H            \
	fdobjrwthread.C fdobjrwthread.H                    \
	fdobjwriterthread.C fdobjwriterthread.H            \
	managedconnectionupdate.C                          \
	managedhierarchymonitor.C                          \
	managedobject.C                                    \
	managedserverstatuscallback.C                      \
	managedsubscriber.C                                \
	manager.C                                          \
	nodeinfo.C                                         \
	objsink.C objsink.H                                \
	objsinkredirect.C objsinkredirect.H                \
	objwriter.C objwriter.H                            \
	objwriterthread.C objwriterthread.H                \
	puttransaction.C                                   \
	quorumstate.C quorumstateobj.C                     \
	replymsgs.auto.C replymsgs.auto.H                  \
	repoclusterquorumcallbackbase.C                    \
	reqstat.C                                          \
	requestmsgs.auto.C requestmsgs.auto.H              \
	results.auto.C                                     \
	sendupdatesreq.H                                   \
	serverstatuscallback.C                             \
	singlethreadreport.H                               \
	stoppablethreadtracker.C stoppablethreadtracker.H  \
        stoppablethreadtrackerfwd.H                        \
	threadmgr.C threadmgr.H                            \
	threadreportimpl.C threadreportimpl.H              \
	threadreportimpl2.C threadreportimpl2.H            \
	userchanged.C userchanged.H                        \
	usergetuuids.C usergetuuids.H                      \
	usergetuuidsreply.C usergetuuidsreply.H            \
	userhelo.C                                         \
	userput2.C userputreply.H                          \
	versionedput.C					   \
	versionscollected.C				   \
	writtenobj.H                                       \
	writtenobjbase.C writtenobjbase.H

libstasher_la_SOURCES=\
	adminstop.H \
	adminstopint.C adminstopint.H \
	baton.C baton.H batonfwd.H \
	batonhandoverthread.C batonhandoverthread.H \
	boolref.C boolref.H boolreffwd.H \
	clusterconnecter.C clusterconnecter.H \
	clusterconnecterimpl.C clusterconnecterimpl.H \
	clusterinfo.C clusterinfo.H clusterinfofwd.H \
	clusterlistener.C clusterlistener.H clusterlistenerfwd.H \
	clusterlistenerimpl.C clusterlistenerimpl.H clusterlistenerimplfwd.H \
	clusternotifier.C clusternotifier.H clusternotifierfwd.H \
	clusterstatusnotifier.C clusterstatusnotifier.H clusterstatusnotifierfwd.H \
	clustertlsconnectshutdown.C clustertlsconnectshutdown.H \
	deserobjname.H \
	dirmsgreply.H \
	localconnection.C localconnection.H \
	localprivconnection.C localprivconnection.H \
	mastersyncinfo.C mastersyncinfo.H \
	maxintsize.h \
	msgthreadbaseobj.H \
	nslist.H nsmap.C nsmap.H \
	nsview.C nsview.H \
	newtran.C newtran.H newtranfwd.H \
	node.C node.H \
	nodeclusterstatus.H \
	nodeinfoconn.H \
	objrepo.C objrepo.H objrepofwd.H \
	objrepocopy.C objrepocopy.H \
	objrepocopydst.C objrepocopydst.H objrepocopydstfwd.H \
	objrepocopydstinterface.C objrepocopydstinterface.H objrepocopydstinterfacefwd.H \
	objrepocopydstthread.C objrepocopydstthread.H objrepocopydstthreadfwd.H \
	objrepocopysrc.C objrepocopysrc.H objrepocopysrcfwd.H \
	objrepocopysrcinterface.C objrepocopysrcinterface.H objrepocopysrcinterfacefwd.H \
	objrepocopysrcthread.C objrepocopysrcthread.H \
	objserializer.H \
	objsource.H \
	objuuidenumerator.C objuuidenumerator.H \
	objuuidlist.C objuuidlist.H \
	peerstatus.C peerstatus.H \
	peerstatusannouncer.C peerstatusannouncer.H \
	repoclusterinfo.C repoclusterinfo.H repoclusterinfofwd.H \
	repoclusterinfoimpl.C repoclusterinfoimpl.H repoclusterinfoimplfwd.H \
	repoclusterquorum.C repoclusterquorum.H \
	repocontrollerbase.C repocontrollerbase.H \
	repocontrollerbasehandoff.C repocontrollerbasehandoff.H \
	repocontrollermaster.C repocontrollermaster.H repocontrollermasterfwd.H \
	repocontrollermasterslaveconnection.H \
	repocontrollerslave.C repocontrollerslave.H \
	repopeerconnection.C repopeerconnection.H repopeerconnectionfwd.H \
	repopeerconnectionbase.C repopeerconnectionbase.H \
	repomg.C repomg.H \
	slavesyncinfo.H \
	spacemonitor.C spacemonitor.H \
	threadreport.C threadreport.H \
	tobjrepo.C tobjrepo.H tobjrepofwd.H \
	trancancelleduuid.C trancancelleduuid.H \
	trancanceller.C trancanceller.H \
	trandistihave.H trandistcancel.H \
	trandistreceived.C trandistreceived.H trandistreceivedfwd.H \
	trandistributor.C trandistributor.H \
	trandistuuid.C trandistuuid.H trandistuuidfwd.H \
	tranreceived.C tranreceived.H tranreceivedfwd.H \
	tranrecvcanc.C tranrecvcanc.H \
	transerializer.H \
	tranuuid.C tranuuid.H tranuuidfwd.H \
	tran.C tran.H tranfwd.H \
	trancommit.C trancommit.H trancommitfwd.H \
	tranmeta.H \
	userput.C userput_deserialized.H

libstasher_client_la_LIBADD=$(XLIBS) -lpthread
libstasher_client_la_LDFLAGS=-version-info 1

@LIBCXX_AM@

$(call THREADMSGDISPATCHER_GEN,repocontrollermaster.msgs.H,repocontrollermaster.msgs.xml)
$(call THREADMSGDISPATCHER_GEN,trandistributor.msgs.H,trandistributor.msgs.xml)
$(call THREADMSGDISPATCHER_GEN,repopeerconnection.msgs.H,repopeerconnection.msgs.xml)
$(call THREADMSGDISPATCHER_GEN,repocontrollerslave.msgs.H,repocontrollerslave.msgs.xml)
$(call THREADMSGDISPATCHER_GEN,clusterlistenerimpl.msgs.H,clusterlistenerimpl.msgs.xml)
$(call THREADMSGDISPATCHER_GEN,clusterlistener.msgs.H,clusterlistener.msgs.xml)
$(call THREADMSGDISPATCHER_GEN,objrepocopydstthread.msgs.H,objrepocopydstthread.msgs.xml)
$(call THREADMSGDISPATCHER_GEN,objrepocopysrcthread.msgs.H,objrepocopysrcthread.msgs.xml)
$(call THREADMSGDISPATCHER_GEN,peerstatusannouncer.msgs.H,peerstatusannouncer.msgs.xml)
$(call THREADMSGDISPATCHER_GEN,objwriterthread.msgs.H,objwriterthread.msgs.xml)
$(call THREADMSGDISPATCHER_GEN,fdobjwriterthread.msgs.H,fdobjwriterthread.msgs.xml)
$(call THREADMSGDISPATCHER_GEN,repocontrollerbasehandoff.msgs.H,repocontrollerbasehandoff.msgs.xml)
$(call THREADMSGDISPATCHER_GEN,batonhandoverthread.msgs.H,batonhandoverthread.msgs.xml)
$(call THREADMSGDISPATCHER_GEN,localconnection.msgs.H,localconnection.msgs.xml)
$(call THREADMSGDISPATCHER_GEN,localprivconnection.msgs.H,localprivconnection.msgs.xml)

$(call THREADMSGDISPATCHER_GEN,client.msgs.H,client.msgs.xml)
$(call MSGDISPATCHER_GEN,threadreportimpl.msgs,def decl)
$(call THREADMSGDISPATCHER_GEN,threadreportimpl2.msgs.H, threadreportimpl2.msgs.xml)

libstasher_la_LIBADD=libstasher-client.la -lcxxtls -lpthread

$(call OPTIONS_GEN,repomgcliopts.h,repomgcli.xml)

localstatedir.h: Makefile
	echo "#define localstatedir \"@localstatedir@\"" >localstatedir.h.tmp
	mv -f localstatedir.h.tmp localstatedir.h

stashermg_SOURCES=repomgcli.C
stashermg_LDADD=libstasher.la -lcourier-unicode

maxintsize_tst_SOURCES=maxintsize.tst.C
maxintsize_tst_LDADD=-lcxx

newtran_tst_SOURCES=newtran.tst.C
newtran_tst_LDADD=-lcxx

objrepo_tst_SOURCES=objrepo.tst.C
objrepo_tst_LDADD=libstasher.la

objwriter_tst_SOURCES=objwriter.tst.C
objwriter_tst_LDADD=libstasher.la
objwriter_tst_LDFLAGS=-static

objwriterthread_tst_SOURCES=objwriterthread.tst.C
objwriterthread_tst_LDADD=libstasher.la
objwriterthread_tst_LDFLAGS=-static

fdobjwriterthread_tst_SOURCES=fdobjwriterthread.tst.C
fdobjwriterthread_tst_LDADD=libstasher.la
fdobjwriterthread_tst_LDFLAGS=-static

objsource_tst_SOURCES=objsource.tst.C
objsource_tst_LDADD=libstasher.la
objsource_tst_LDFLAGS=-static

fdobjreaderthread_tst_SOURCES=fdobjreaderthread.tst.C
fdobjreaderthread_tst_LDADD=libstasher.la
fdobjreaderthread_tst_LDFLAGS=-static

tobjrepo_tst_SOURCES=tobjrepo.tst.C
tobjrepo_tst_LDADD=libstasher.la
tobjrepo_tst_LDFLAGS=-static

tranmeta_tst_SOURCES=tranmeta.tst.C
tranmeta_tst_LDADD=libstasher.la
tranmeta_tst_LDFLAGS=-static

objuuidenumerator_tst_SOURCES=objuuidenumerator.tst.C
objuuidenumerator_tst_LDADD=libstasher.la
objuuidenumerator_tst_LDFLAGS=-static

objserializer_tst_SOURCES=objserializer.tst.C
objserializer_tst_LDADD=libstasher.la
objserializer_tst_LDFLAGS=-static

nodeclusterstatus_tst_SOURCES=nodeclusterstatus.tst.C
nodeclusterstatus_tst_LDADD=-lcxx
nodeclusterstatus_tst_LDFLAGS=-static

peerstatus_tst_SOURCES=peerstatus.tst.C
peerstatus_tst_LDADD=-lcxx
peerstatus_tst_LDFLAGS=-static

clusterinfo_tst_SOURCES=clusterinfo.tst.C
clusterinfo_tst_LDADD=libstasher.la
clusterinfo_tst_LDFLAGS=-static

objrepocopydstthread_tst_SOURCES=objrepocopydstthread.tst.C
objrepocopydstthread_tst_LDADD=libstasher.la
objrepocopydstthread_tst_LDFLAGS=-static

objrepocopysrcthread_tst_SOURCES=objrepocopysrcthread.tst.C
objrepocopysrcthread_tst_LDADD=libstasher.la
objrepocopysrcthread_tst_LDFLAGS=-static

objrepocopy_tst_SOURCES=objrepocopy.tst.C
objrepocopy_tst_LDADD=libstasher.la
objrepocopy_tst_LDFLAGS=-static

stoppablethreadtracker_tst_SOURCES=stoppablethreadtracker.tst.C
stoppablethreadtracker_tst_LDADD=libstasher.la
stoppablethreadtracker_tst_LDFLAGS=-static

repomg_tst_SOURCES=repomg.tst.C
repomg_tst_LDADD=libstasher.la -lcxxtls
repomg_tst_LDFLAGS=-static

clusterlistener_tst_SOURCES=clusterlistener.tst.C
clusterlistener_tst_LDADD=libstasher.la
clusterlistener_tst_LDFLAGS=-static

clusterconnecter_tst_SOURCES=clusterconnecter.tst.C tst.nodes.C tst.nodes.H
clusterconnecter_tst_LDADD=libstasher.la -lcourier-unicode
clusterconnecter_tst_LDFLAGS=-static

fdobjrwthread_tst_SOURCES=fdobjrwthread.tst.C
fdobjrwthread_tst_LDADD=libstasher.la
fdobjrwthread_tst_LDFLAGS=-static

peerstatusannouncer_tst_SOURCES=peerstatusannouncer.tst.C tst.nodes.C tst.nodes.H
peerstatusannouncer_tst_LDADD=libstasher.la -lcourier-unicode
peerstatusannouncer_tst_LDFLAGS=-static

repoclusterinfo_tst_SOURCES=repoclusterinfo.tst.C
repoclusterinfo_tst_LDADD=libstasher.la
repoclusterinfo_tst_LDFLAGS=-static

repocontrollerslave_tst_SOURCES=repocontrollerslave.tst.C
repocontrollerslave_tst_LDADD=libstasher.la
repocontrollerslave_tst_LDFLAGS=-static

repopeerconnection_tst_SOURCES=repopeerconnection.tst.C
repopeerconnection_tst_LDADD=libstasher.la
repopeerconnection_tst_LDFLAGS=-static

repocontrollermaster_tst_SOURCES=repocontrollermaster.tst.C
repocontrollermaster_tst_LDADD=libstasher.la
repocontrollermaster_tst_LDFLAGS=-static

repocontrollermaster2_tst_SOURCES=repocontrollermaster2.tst.C tst.nodes.C tst.nodes.H
repocontrollermaster2_tst_LDADD=libstasher.la -lcourier-unicode
repocontrollermaster2_tst_LDFLAGS=-static

repopeerconnection2_tst_SOURCES=repopeerconnection2.tst.C
repopeerconnection2_tst_LDADD=libstasher.la
repopeerconnection2_tst_LDFLAGS=-static

transerializer_tst_SOURCES=transerializer.tst.C
transerializer_tst_LDADD=libstasher.la
transerializer_tst_LDFLAGS=-static

trandistributor_tst_SOURCES=trandistributor.tst.C
trandistributor_tst_LDADD=libstasher.la
trandistributor_tst_LDFLAGS=-static

trandistributor2_tst_SOURCES=trandistributor2.tst.C
trandistributor2_tst_LDADD=libstasher.la
trandistributor2_tst_LDFLAGS=-static

repopeerconnection3_tst_SOURCES=repopeerconnection3.tst.C
repopeerconnection3_tst_LDADD=libstasher.la
repopeerconnection3_tst_LDFLAGS=-static

repopeerconnection4_tst_SOURCES=repopeerconnection4.tst.C
repopeerconnection4_tst_LDADD=libstasher.la
repopeerconnection4_tst_LDFLAGS=-static

trancanceller_tst_SOURCES=trancanceller.tst.C
trancanceller_tst_LDADD=libstasher.la
trancanceller_tst_LDFLAGS=-static

trandistributor3_tst_SOURCES=trandistributor3.tst.C tst.nodes.C tst.nodes.H
trandistributor3_tst_LDADD=libstasher.la -lcourier-unicode
trandistributor3_tst_LDFLAGS=-static

trandistributor4_tst_SOURCES=trandistributor4.tst.C tst.nodes.C tst.nodes.H
trandistributor4_tst_LDADD=libstasher.la -lcourier-unicode
trandistributor4_tst_LDFLAGS=-static

baton_tst_SOURCES=baton.tst.C tst.nodes.C tst.nodes.H
baton_tst_LDADD=libstasher.la -lcourier-unicode
baton_tst_LDFLAGS=-static

baton2_tst_SOURCES=baton2.tst.C tst.nodes.C tst.nodes.H
baton2_tst_LDADD=libstasher.la -lcourier-unicode
baton2_tst_LDFLAGS=-static

readtimeout_tst_SOURCES=readtimeout.tst.C tst.nodes.C tst.nodes.H
readtimeout_tst_LDADD=libstasher.la -lcourier-unicode
readtimeout_tst_LDFLAGS=-static

reconnect_tst_SOURCES=reconnect.tst.C tst.nodes.C tst.nodes.H
reconnect_tst_LDADD=libstasher.la -lcourier-unicode
reconnect_tst_LDFLAGS=-static

adminstop_tst_SOURCES=adminstop.tst.C tst.nodes.C tst.nodes.H
adminstop_tst_LDADD=libstasher.la -lcourier-unicode
adminstop_tst_LDFLAGS=-static

userput_tst_SOURCES=userput.tst.C tst.nodes.C tst.nodes.H
userput_tst_LDADD=libstasher.la -lcourier-unicode
userput_tst_LDFLAGS=-static

usergetuuids_tst_SOURCES=usergetuuids.tst.C tst.nodes.C tst.nodes.H
usergetuuids_tst_LDADD=libstasher.la -lcourier-unicode
usergetuuids_tst_LDFLAGS=-static

userdir_tst_SOURCES=userdir.tst.C tst.nodes.C tst.nodes.H
userdir_tst_LDADD=libstasher.la -lcourier-unicode
userdir_tst_LDFLAGS=-static

usergetuuids2_tst_SOURCES=usergetuuids2.tst.C tst.nodes.C tst.nodes.H
usergetuuids2_tst_LDADD=libstasher.la -lcourier-unicode
usergetuuids2_tst_LDFLAGS=-static

usersubscribe_tst_SOURCES=usersubscribe.tst.C tst.nodes.C tst.nodes.H
usersubscribe_tst_LDADD=libstasher.la -lcourier-unicode
usersubscribe_tst_LDFLAGS=-static

adminsetcert_tst_SOURCES=adminsetcert.tst.C tst.nodes.C tst.nodes.H
adminsetcert_tst_LDADD=libstasher.la -lcourier-unicode
adminsetcert_tst_LDFLAGS=-static

statuscallback_tst_SOURCES=statuscallback.tst.C tst.nodes.C tst.nodes.H
statuscallback_tst_LDADD=libstasher.la -lcourier-unicode
statuscallback_tst_LDFLAGS=-static

newnode_tst_SOURCES=newnode.tst.C tst.nodes.C tst.nodes.H
newnode_tst_LDADD=libstasher.la -lcourier-unicode
newnode_tst_LDFLAGS=-static

clientreconnect_tst_SOURCES=clientreconnect.tst.C tst.nodes.C tst.nodes.H
clientreconnect_tst_LDADD=libstasher.la -lcourier-unicode
clientreconnect_tst_LDFLAGS=-static

adminsetcert2_tst_SOURCES=adminsetcert2.tst.C tst.nodes.C tst.nodes.H
adminsetcert2_tst_LDADD=libstasher.la -lcourier-unicode
adminsetcert2_tst_LDFLAGS=-static

adminhalt_tst_SOURCES=adminhalt.tst.C tst.nodes.C tst.nodes.H
adminhalt_tst_LDADD=libstasher.la -lcourier-unicode
adminhalt_tst_LDFLAGS=-static

managedusersubscribe_tst_SOURCES=managedusersubscribe.tst.C tst.nodes.C tst.nodes.H
managedusersubscribe_tst_LDADD=libstasher.la -lcourier-unicode
managedusersubscribe_tst_LDFLAGS=-static

managedserverstatuscallback_tst_SOURCES=managedserverstatuscallback.tst.C tst.nodes.C tst.nodes.H
managedserverstatuscallback_tst_LDADD=libstasher.la -lcourier-unicode
managedserverstatuscallback_tst_LDFLAGS=-static

managedhierarchymonitor_tst_SOURCES=managedhierarchymonitor.tst.C tst.nodes.C tst.nodes.H
managedhierarchymonitor_tst_LDADD=libstasher.la -lcourier-unicode
managedhierarchymonitor_tst_LDFLAGS=-static

managedobject_tst_SOURCES=managedobject.tst.C tst.nodes.C tst.nodes.H
managedobject_tst_LDADD=libstasher.la -lcourier-unicode
managedobject_tst_LDFLAGS=

managedobject2_tst_SOURCES=managedobject2.tst.C tst.nodes.C tst.nodes.H
managedobject2_tst_LDADD=libstasher.la -lcourier-unicode
managedobject2_tst_LDFLAGS=

managedobject3_tst_SOURCES=managedobject3.tst.C tst.nodes.C tst.nodes.H
managedobject3_tst_LDADD=libstasher.la -lcourier-unicode
managedobject3_tst_LDFLAGS=

appsdir_tst_SOURCES=appsdir.tst.C tst.nodes.C tst.nodes.H
appsdir_tst_LDADD=libstasher.la -lcourier-unicode
appsdir_tst_LDFLAGS=-static

$(call THREADMSGDISPATCHER_GEN, verify.tst.msgs.all.H, verify.tst.msgs.xml)

verify_tst_SOURCES=verify.tst.C tst.nodes.C tst.nodes.H
verify_tst_LDADD=libstasher.la -lcourier-unicode
verify_tst_LDFLAGS=-static

reconnectsync_tst_SOURCES=reconnectsync.tst.C tst.nodes.C tst.nodes.H
reconnectsync_tst_LDADD=libstasher.la -lcourier-unicode
reconnectsync_tst_LDFLAGS=-static

heartbeat_tst_SOURCES=heartbeat.tst.C tst.nodes.C tst.nodes.H
heartbeat_tst_LDADD=libstasher.la -lcourier-unicode
heartbeat_tst_LDFLAGS=-static

EXTRATESTS=

if extratests
EXTRATESTS += --set-property extratests=true
endif

pkglibexec_PROGRAMS=stasherd

stasherdpath.h: Makefile
	echo '#define stasherdpath "$(pkglibexecdir)/stasherd"' >stasherdpath.h

$(call OPTIONS_GEN,stasherd.h,stasherd.xml)

stasherd_SOURCES=stasherd.C
stasherd_LDADD=libstasher.la

stasherd_static_SOURCES=stasherd.C
stasherd_static_LDADD=libstasher.la
stasherd_static_LDFLAGS=-static

noinst_PROGRAMS += stashercli
$(call OPTIONS_GEN,stasher.h,stasher.xml)

stashercli_SOURCES=stasher.C
stashercli_LDADD=libstasher-client.la @LIBREADLINE@ -lcourier-unicode

stasher_static_SOURCES=stasher.C
stasher_static_LDADD=libstasher-client.la @LIBREADLINE@ -lcourier-unicode
stasher_static_LDFLAGS=-static

check-am:
	./maxintsize.tst
	./objrepo.tst
	./tranmeta.tst
	./newtran.tst
	./tobjrepo.tst
	./objwriter.tst
	./objwriterthread.tst
	./fdobjwriterthread.tst
	./objsource.tst
	./fdobjreaderthread.tst
	./objuuidenumerator.tst
	./objserializer.tst
	./nodeclusterstatus.tst
	./peerstatus.tst
	./clusterinfo.tst
	./objrepocopydstthread.tst
	./objrepocopysrcthread.tst
	./stoppablethreadtracker.tst
	./repomg.tst
	./clusterlistener.tst
	./clusterconnecter.tst
	./fdobjrwthread.tst
	./peerstatusannouncer.tst
	./repoclusterinfo.tst
	./repopeerconnection.tst
	./repocontrollerslave.tst
	./repocontrollermaster.tst
	./repopeerconnection2.tst
	./transerializer.tst
	./trancanceller.tst
	./trandistributor.tst
	./trandistributor2.tst
	./repopeerconnection4.tst
	./repopeerconnection3.tst
	./trandistributor3.tst $(EXTRATESTS)
	./trandistributor4.tst
	./baton.tst
	./baton2.tst
	./readtimeout.tst
	./reconnect.tst
	./adminstop.tst
	./userput.tst
	./usergetuuids.tst
	./userdir.tst
	./usergetuuids2.tst
	./usersubscribe.tst
	./adminsetcert.tst
	./statuscallback.tst
	./newnode.tst
	./clientreconnect.tst
	./adminsetcert2.tst
	./adminhalt.tst
	./managedusersubscribe.tst
	./managedserverstatuscallback.tst
	./managedhierarchymonitor.tst
	./managedobject.tst
	./managedobject2.tst
	./managedobject3.tst
	./appsdir.tst
	./verify.tst
	./reconnectsync.tst
	./heartbeat.tst
	./repocontrollermaster2.tst

docs: html/index.html html/book.css html/logo.png html/.htaccess html/frame.js html/icon.gif $(patsubst %,html/%,$(notdir $(wildcard $(srcdir)/docbook/*.png)))

html/%.png: html/index.html docbook/%.png
	$(INSTALL) -m 0644 $(srcdir)/docbook/$(notdir $@) html

html/book.css: html/index.html docbook/book.css
	$(INSTALL) -m 0644 $(srcdir)/docbook/book.css html

html/.htaccess: html/index.html docbook/.htaccess
	$(INSTALL) -m 0644 $(srcdir)/docbook/.htaccess html/.htaccess

html/frame.js: html/index.html docbook/frame.js
	$(INSTALL) -m 0644 $(srcdir)/docbook/frame.js html/frame.js

html/icon.gif: html/index.html docbook/icon.gif
	$(INSTALL) -m 0644 $(srcdir)/docbook/icon.gif html/icon.gif

html/index.html: html/ref/index.html $(wildcard docbook/*.xml) docbook/xmllinks.xsl docbook/stasherpm.xml
	xsltproc $(srcdir)/docbook/xmllinks.xsl xml/index.xml >docbook/entities
	rm -rf html.tmp
	mkdir html.tmp
	xsltproc --nonet -o html.tmp/ --xinclude \
		--param local.l10n.xml 'document("'`pwd`/docbook/l10.xml'")' \
		$(srcdir)/docbook/docbook.xsl \
		$(srcdir)/docbook/book.xml
	-rm -f html/* 2>/dev/null >/dev/null || :
	mkdir -p html
	mv html.tmp/* html
	rm -rf html.tmp
	rm -rf html/examples
	find examples -depth \( -type d -o -name '*.[CH]' -o -name '*.xml' -o -name Makefile.am -o -name configure.ac \) -print | cpio -o --quiet | (cd html; cpio -i --quiet --make-directories)
	echo 'ForceType text/plain' >html/examples/.htaccess
	rm -f html/distreboot
	ln -s ../stasher-distreboot html/distreboot
	links --dump html/INSTALL.html >INSTALL.tmp
	mv -f INSTALL.tmp INSTALL

docbook/stasherpm.xml: perl/Stasher/lib/Stasher.pm
	pod2docbook --doctype=refentry --fix-double-quotes --no-header perl/Stasher/lib/Stasher.pm >docbook/stasherpm.xml.new
	$(PERL) -p -i -e 's/<refentry>/<refentry id="stasherpm">/' docbook/stasherpm.xml.new
	mv -f docbook/stasherpm.xml.new docbook/stasherpm.xml

html/ref/index.html: Doxyfile header.html $(wildcard stasher/*.H) $(myCLEANFILES)
	rm -rf html/ref xml
	mkdir -p html/ref
	doxygen; c=$$?;	perl -p -i -e 's/INSERT_STASHER_NAMESPACE/stasher/g' html/ref/*.html; exit $$c

perl-configure: perl/Stasher/lib/Stasher.pm
	set -e; cd perl/Stasher; CC='$(CXX)' SHELL='$(SHELL)' LDFLAGS='$(LDFLAGS)' $(PERL) Makefile.PL OPTIMIZE='$(CXXFLAGS)' $(PERL_CONFIGURE_FLAGS)

perl-build:
	cd perl/Stasher && $(MAKE)

header.html: Doxyfile
	rm -rf header.html.tmp
	mkdir -p header.html.tmp
	cd header.html.tmp && doxygen -w html header.html footer.html doxygen.css
	perl -p -i -e 's@<head>@<head><link rel="icon" href="/stasher/icon.gif" type="image/gif"/>@;s@\$$projectbrief@<a href="/stasher/">\$$projectbrief</a>@;' header.html.tmp/header.html
	mv -f header.html.tmp/header.html .
	rm -rf header.html.tmp

install-exec-hook:
	mkdir -m 0755 -p $(DESTDIR)@bindir@
	$(LIBTOOL) $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=install $(INSTALL) $(INSTALL_STRIP_FLAG) stashercli '$(DESTDIR)@bindir@/stasher'
	mkdir -p $(DESTDIR)$(pkgdatadir)
	sed 's~@''SHELL''@~$(SHELL)~g;s~@''sysconfdir''@~$(sysconfdir)~g;s~@''localstatedir''@~$(localstatedir)~g' <$(srcdir)/maillogs.sh.in >$(DESTDIR)$(pkgdatadir)/maillogs.sh
	chmod 755 $(DESTDIR)$(pkgdatadir)/maillogs.sh

uninstall-hook:
	rm -f $(DESTDIR)@bindir@/stasher
	rm -f $(DESTDIR)$(pkgdatadir)/maillogs.sh

install-data-hook:
	mkdir -m 0755 -p $(DESTDIR)@localstatedir@/stasher/apps
	mkdir -m 0755 -p $(DESTDIR)@localstatedir@/stasher/clusters
	mkdir -m 0755 -p $(DESTDIR)@localstatedir@/stasher/nodes
	mkdir -m 0755 -p $(DESTDIR)@localstatedir@/stasher/newnodes

$(man1_MANS): man.stamp
	touch $(man1_MANS)

man.stamp: $(MANPAGESIN)
	set -e; for f in $(man1_MANS); do sed 's%@''localstatedir''@%$(localstatedir)%g' <$(srcdir)/$$f.in >$$f.tmp; mv -f $$f.tmp $$f; done
	touch man.stamp

CLEANFILES += man.stamp

if HAVE_DOCBOOK

DOCBOOKSRC=$(wildcard $(srcdir)/docbook/*.xml)
BUILT_SOURCES += $(MANPAGESIN)

stashermg.1.in: man.in.stamp
	cp -p manpages/stashermg.1 $@
	touch $@

stasher.1.in: man.in.stamp
	cp -p manpages/stasher.1 stasher.1.in
	touch $@

man.in.stamp: $(DOCBOOKSRC)
	rm -rf manpages
	mkdir -p manpages
	xsltproc --nonet -o manpages/ http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $(srcdir)/docbook/book.xml
	touch man.in.stamp

CLEANFILES += man.in.stamp
endif

SUBDIRS = po packaging/fedora

ACLOCAL_AMFLAGS = -I m4

contracts:
	perl -n -e 'print "$$1\n" if m@//!(.*)@' *.H | perl -n -e 's/(\[[^\]]*\])/{print "$$1\n"}/ge' | sort | uniq -c | sort -n

.PHONY: html


.PHONY: rpm

rpm:
	make dist
	rm -rf rpm/SRPMS/*
	rm -rf rpm/RPMS/*
	rm -rf rpm/BUILD/*
	rpmbuild -ta --clean \
		--define "_topdir `pwd`/rpm" \
		--define '_rpmdir %{_topdir}/RPMS' \
		--define '_srcrpmdir %{_topdir}/SRPMS' \
		--define '_sourcedir %{_topdir}/SOURCES' \
		--define '_specdir %{_topdir}/SPECS' \
		--define '_builddir %{_topdir}/BUILD' \
		--define '_build_name_fmt %%{ARCH}/%%{NAME}-%%{VERSION}-%%{RELEASE}.%%{ARCH}.rpm' \
		--define '_tmppath %{_var}/tmp' \
		--define '__spec_prep_pre %{___build_pre}' \
		--define "extrarelease .`date '+%Y%m%d%H%M%S'`" @PACKAGE@-@VERSION@.tar.bz2
	$$HOME/repos.sh

www: html
	rsync -a --delete-after html/. ~/www/www.courier-mta.org/stasher/.

tag:
	git tag -s -a @PACKAGE@/@VERSION@/`date +%Y%m%d%H%M%S` -m 'Tag version @VERSION@'

scp:
	rm -f @PACKAGE@-@VERSION@.tar.bz2.sig; gpg --detach-sign --default-key mrsam@courier-mta.com @PACKAGE@-@VERSION@.tar.bz2; test -f @PACKAGE@-@VERSION@.tar.bz2.sig || exit 1
	scp @PACKAGE@-@VERSION@.tar.bz2 @PACKAGE@-@VERSION@.tar.bz2.sig fedorahosted.org:libcxx
