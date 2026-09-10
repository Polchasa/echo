QT += widgets sql

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    database/database.cpp \
    database/patientrepository.cpp \
    main.cpp \
    mainwindow.cpp \
    patientdialog.cpp \
    patienteditdialog.cpp

HEADERS += \
    database/database.h \
    database/patientrepository.h \
    mainwindow.h \
    models/patient.h \
    patientdialog.h \
    patienteditdialog.h

FORMS += \
    mainwindow.ui \
    patientdialog.ui \
    patienteditdialog.ui

win32:RC_FILE = app.rc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
