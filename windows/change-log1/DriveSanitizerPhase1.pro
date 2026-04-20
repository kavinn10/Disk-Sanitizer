QT += widgets
CONFIG += c++17
TARGET = DriveSanitizerPhase1
TEMPLATE = app

SOURCES += main.cpp \
           mainwindow.cpp \
           driveutils.cpp \
           certificateutils.cpp

HEADERS += mainwindow.h \
           driveutils.h \
           certificateutils.h 
