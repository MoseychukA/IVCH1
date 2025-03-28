/********************************************************************************
** Form generated from reading UI file 'setuptool.ui'
**
** Created by: Qt User Interface Compiler version 6.8.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SETUPTOOL_H
#define UI_SETUPTOOL_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_setuptool
{
public:
    QWidget *centralWidget;
    QGridLayout *gridLayout;
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout;
    QLabel *readytostartLabel;
    QSpacerItem *horizontalSpacer;
    QLabel *receiveLabel;
    QListWidget *listWidget;
    QHBoxLayout *horizontalLayout_2;
    QPushButton *scanPushButton;
    QProgressBar *scanprogressBar;
    QPushButton *quitPushButton;
    QMenuBar *menuBar;
    QStatusBar *statusBar;

    void setupUi(QMainWindow *setuptool)
    {
        if (setuptool->objectName().isEmpty())
            setuptool->setObjectName("setuptool");
        setuptool->resize(289, 325);
        setuptool->setMaximumSize(QSize(700, 700));
        centralWidget = new QWidget(setuptool);
        centralWidget->setObjectName("centralWidget");
        gridLayout = new QGridLayout(centralWidget);
        gridLayout->setSpacing(6);
        gridLayout->setContentsMargins(11, 11, 11, 11);
        gridLayout->setObjectName("gridLayout");
        verticalLayout = new QVBoxLayout();
        verticalLayout->setSpacing(6);
        verticalLayout->setObjectName("verticalLayout");
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setSpacing(6);
        horizontalLayout->setObjectName("horizontalLayout");
        readytostartLabel = new QLabel(centralWidget);
        readytostartLabel->setObjectName("readytostartLabel");

        horizontalLayout->addWidget(readytostartLabel);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);

        receiveLabel = new QLabel(centralWidget);
        receiveLabel->setObjectName("receiveLabel");

        horizontalLayout->addWidget(receiveLabel);


        verticalLayout->addLayout(horizontalLayout);

        listWidget = new QListWidget(centralWidget);
        listWidget->setObjectName("listWidget");

        verticalLayout->addWidget(listWidget);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setSpacing(6);
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        scanPushButton = new QPushButton(centralWidget);
        scanPushButton->setObjectName("scanPushButton");

        horizontalLayout_2->addWidget(scanPushButton);

        scanprogressBar = new QProgressBar(centralWidget);
        scanprogressBar->setObjectName("scanprogressBar");
        scanprogressBar->setValue(24);
        scanprogressBar->setTextVisible(false);

        horizontalLayout_2->addWidget(scanprogressBar);

        quitPushButton = new QPushButton(centralWidget);
        quitPushButton->setObjectName("quitPushButton");

        horizontalLayout_2->addWidget(quitPushButton);


        verticalLayout->addLayout(horizontalLayout_2);


        gridLayout->addLayout(verticalLayout, 0, 0, 1, 1);

        setuptool->setCentralWidget(centralWidget);
        menuBar = new QMenuBar(setuptool);
        menuBar->setObjectName("menuBar");
        menuBar->setGeometry(QRect(0, 0, 289, 21));
        setuptool->setMenuBar(menuBar);
        statusBar = new QStatusBar(setuptool);
        statusBar->setObjectName("statusBar");
        setuptool->setStatusBar(statusBar);

        retranslateUi(setuptool);

        QMetaObject::connectSlotsByName(setuptool);
    } // setupUi

    void retranslateUi(QMainWindow *setuptool)
    {
        setuptool->setWindowTitle(QCoreApplication::translate("setuptool", "setuptool", nullptr));
        readytostartLabel->setText(QCoreApplication::translate("setuptool", "TextLabel", nullptr));
        receiveLabel->setText(QCoreApplication::translate("setuptool", "TextLabel", nullptr));
        scanPushButton->setText(QCoreApplication::translate("setuptool", "Scan", nullptr));
        quitPushButton->setText(QCoreApplication::translate("setuptool", "Quit", nullptr));
    } // retranslateUi

};

namespace Ui {
    class setuptool: public Ui_setuptool {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SETUPTOOL_H
