/********************************************************************************
** Form generated from reading UI file 'settingsform.ui'
**
** Created by: Qt User Interface Compiler version 6.8.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SETTINGSFORM_H
#define UI_SETTINGSFORM_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_SettingsForm
{
public:
    QGridLayout *gridLayout_2;
    QVBoxLayout *verticalLayout_2;
    QGridLayout *gridLayout;
    QComboBox *mbtcpComboBox;
    QLabel *label;
    QLabel *label_2;
    QLabel *label_3;
    QComboBox *melsecComboBox;
    QSpacerItem *horizontalSpacer_2;
    QComboBox *ntpComboBox;
    QLabel *label_6;
    QComboBox *gpsComboBox;
    QHBoxLayout *horizontalLayout;
    QLabel *label_4;
    QLineEdit *ipAddressLineEdit;
    QSpacerItem *horizontalSpacer;
    QHBoxLayout *horizontalLayout_2;
    QPushButton *sendPushButton;
    QPushButton *receivePushButton;
    QPushButton *importPushButton;
    QPushButton *exportPushButton;
    QPushButton *setTimePushButton;
    QVBoxLayout *verticalLayout;
    QLabel *label_5;
    QTextEdit *generalInfoTextEdit;

    void setupUi(QWidget *SettingsForm)
    {
        if (SettingsForm->objectName().isEmpty())
            SettingsForm->setObjectName("SettingsForm");
        SettingsForm->resize(446, 409);
        gridLayout_2 = new QGridLayout(SettingsForm);
        gridLayout_2->setObjectName("gridLayout_2");
        verticalLayout_2 = new QVBoxLayout();
        verticalLayout_2->setObjectName("verticalLayout_2");
        gridLayout = new QGridLayout();
        gridLayout->setObjectName("gridLayout");
        mbtcpComboBox = new QComboBox(SettingsForm);
        mbtcpComboBox->setObjectName("mbtcpComboBox");

        gridLayout->addWidget(mbtcpComboBox, 1, 1, 1, 1);

        label = new QLabel(SettingsForm);
        label->setObjectName("label");

        gridLayout->addWidget(label, 0, 0, 1, 1);

        label_2 = new QLabel(SettingsForm);
        label_2->setObjectName("label_2");

        gridLayout->addWidget(label_2, 1, 0, 1, 1);

        label_3 = new QLabel(SettingsForm);
        label_3->setObjectName("label_3");

        gridLayout->addWidget(label_3, 2, 0, 1, 1);

        melsecComboBox = new QComboBox(SettingsForm);
        melsecComboBox->setObjectName("melsecComboBox");
        melsecComboBox->setEnabled(false);

        gridLayout->addWidget(melsecComboBox, 2, 1, 1, 1);

        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        gridLayout->addItem(horizontalSpacer_2, 2, 2, 1, 1);

        ntpComboBox = new QComboBox(SettingsForm);
        ntpComboBox->setObjectName("ntpComboBox");

        gridLayout->addWidget(ntpComboBox, 0, 1, 1, 1);

        label_6 = new QLabel(SettingsForm);
        label_6->setObjectName("label_6");

        gridLayout->addWidget(label_6, 3, 0, 1, 1);

        gpsComboBox = new QComboBox(SettingsForm);
        gpsComboBox->setObjectName("gpsComboBox");

        gridLayout->addWidget(gpsComboBox, 3, 1, 1, 1);


        verticalLayout_2->addLayout(gridLayout);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        label_4 = new QLabel(SettingsForm);
        label_4->setObjectName("label_4");

        horizontalLayout->addWidget(label_4);

        ipAddressLineEdit = new QLineEdit(SettingsForm);
        ipAddressLineEdit->setObjectName("ipAddressLineEdit");
        QSizePolicy sizePolicy(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(ipAddressLineEdit->sizePolicy().hasHeightForWidth());
        ipAddressLineEdit->setSizePolicy(sizePolicy);
        ipAddressLineEdit->setMaximumSize(QSize(100, 16777213));

        horizontalLayout->addWidget(ipAddressLineEdit);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);


        verticalLayout_2->addLayout(horizontalLayout);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        sendPushButton = new QPushButton(SettingsForm);
        sendPushButton->setObjectName("sendPushButton");

        horizontalLayout_2->addWidget(sendPushButton);

        receivePushButton = new QPushButton(SettingsForm);
        receivePushButton->setObjectName("receivePushButton");

        horizontalLayout_2->addWidget(receivePushButton);

        importPushButton = new QPushButton(SettingsForm);
        importPushButton->setObjectName("importPushButton");

        horizontalLayout_2->addWidget(importPushButton);

        exportPushButton = new QPushButton(SettingsForm);
        exportPushButton->setObjectName("exportPushButton");

        horizontalLayout_2->addWidget(exportPushButton);

        setTimePushButton = new QPushButton(SettingsForm);
        setTimePushButton->setObjectName("setTimePushButton");

        horizontalLayout_2->addWidget(setTimePushButton);


        verticalLayout_2->addLayout(horizontalLayout_2);

        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName("verticalLayout");
        label_5 = new QLabel(SettingsForm);
        label_5->setObjectName("label_5");

        verticalLayout->addWidget(label_5);

        generalInfoTextEdit = new QTextEdit(SettingsForm);
        generalInfoTextEdit->setObjectName("generalInfoTextEdit");
        generalInfoTextEdit->setEnabled(false);
        generalInfoTextEdit->setAcceptDrops(false);
        generalInfoTextEdit->setUndoRedoEnabled(false);
        generalInfoTextEdit->setAcceptRichText(false);

        verticalLayout->addWidget(generalInfoTextEdit);


        verticalLayout_2->addLayout(verticalLayout);


        gridLayout_2->addLayout(verticalLayout_2, 0, 0, 1, 1);


        retranslateUi(SettingsForm);

        QMetaObject::connectSlotsByName(SettingsForm);
    } // setupUi

    void retranslateUi(QWidget *SettingsForm)
    {
        SettingsForm->setWindowTitle(QCoreApplication::translate("SettingsForm", "NTP Server settings", nullptr));
        label->setText(QCoreApplication::translate("SettingsForm", "NTP", nullptr));
        label_2->setText(QCoreApplication::translate("SettingsForm", "Modbus TCP", nullptr));
        label_3->setText(QCoreApplication::translate("SettingsForm", "MELSEC", nullptr));
        label_6->setText(QCoreApplication::translate("SettingsForm", "GPS", nullptr));
        label_4->setText(QCoreApplication::translate("SettingsForm", "IP Address  ", nullptr));
        ipAddressLineEdit->setInputMask(QString());
        sendPushButton->setText(QCoreApplication::translate("SettingsForm", "Send", nullptr));
        receivePushButton->setText(QCoreApplication::translate("SettingsForm", "Receive", nullptr));
        importPushButton->setText(QCoreApplication::translate("SettingsForm", "Import", nullptr));
        exportPushButton->setText(QCoreApplication::translate("SettingsForm", "Export", nullptr));
        setTimePushButton->setText(QCoreApplication::translate("SettingsForm", "Set Time", nullptr));
        label_5->setText(QCoreApplication::translate("SettingsForm", "General info", nullptr));
    } // retranslateUi

};

namespace Ui {
    class SettingsForm: public Ui_SettingsForm {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SETTINGSFORM_H
