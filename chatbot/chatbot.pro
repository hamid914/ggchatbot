QT += core xmlpatterns xml gui
LIBS += -lgadu
TARGET = ggchatbot
CONFIG += console \
    release

TEMPLATE = app

SOURCES += main.cpp \
    connectionthread.cpp \
    sessionclient.cpp \
    eventmanager.cpp \
    profilebase.cpp \
    profile.cpp \
    botsettings.cpp \
    botsettingsto.cpp \
    userdatabase.cpp \
    userinfoto.cpp \
    logscheduler.cpp \
    sessionscheduler.cpp \
    commandresolver.cpp \
    job.cpp \
    pingserverjob.cpp \
    kickuserjob.cpp \
    common.cpp \
    imagedescriptionsettings.cpp \
    debug.cpp

HEADERS += connectionthread.h \
    sessionclient.h \
    eventmanager.h \
    profilebase.h \
    profile.h \
    botsettings.h \
    botsettingsto.h \
    userdatabase.h \
    userinfoto.h \
    logscheduler.h \
    sessionscheduler.h \
    commandresolver.h \
    job.h \
    pingserverjob.h \
    kickuserjob.h \
    sessionclientptr.h \
    profileptr.h \
    common.h \
    imagedescriptionsettings.h \
    resolver.h \
    protocol.h \
    libgadu.h \
    compat.h

