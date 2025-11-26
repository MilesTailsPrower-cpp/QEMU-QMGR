#include <QApplication>
#include <QWidget>
#include <QMainWindow>
#include <QListWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QDebug>
#include <QMap>

struct VM {
    QString name;
    QString disk;
    QString iso;
    int mem = 4096;
    QString cpu = "qemu64";
    bool net = true;
    bool audio = false;
    bool hda = true;
    bool vnc = false;
    int vnc_port = 5900;
    bool vnc_pass = false;
};

static QString getDatabasePath() {
    return QDir(QCoreApplication::applicationDirPath()).filePath("database.ini");
}

static QString findQemuExecutable() {
#ifdef Q_OS_WIN
    QString defaultPath = "C:/Program Files/qemu/qemu-system-x86_64.exe";
    if(QFileInfo::exists(defaultPath)) return defaultPath;
    return "qemu-system-x86_64.exe";
#else
    return "qemu-system-x86_64";
#endif
}

static void vmToSettings(const VM &vm) {
    QSettings s(getDatabasePath(), QSettings::IniFormat);
    s.beginGroup(vm.name);
    s.setValue("disk", vm.disk);
    s.setValue("iso", vm.iso);
    s.setValue("mem", vm.mem);
    s.setValue("cpu", vm.cpu);
    s.setValue("net", vm.net ? 1 : 0);
    s.setValue("audio", vm.audio ? 1 : 0);
    s.setValue("hda", vm.hda ? 1 : 0);
    s.setValue("vnc", vm.vnc ? 1 : 0);
    s.setValue("vnc_port", vm.vnc_port);
    s.setValue("vnc_pass", vm.vnc_pass ? 1 : 0);
    s.endGroup();
    s.sync();
}

static VM vmFromSettings(const QString &name) {
    QSettings s(getDatabasePath(), QSettings::IniFormat);
    s.beginGroup(name);
    VM vm;
    vm.name = name;
    vm.disk = s.value("disk").toString();
    vm.iso = s.value("iso").toString();
    vm.mem = s.value("mem", 4096).toInt();
    vm.cpu = s.value("cpu", "qemu64").toString();
    vm.net = s.value("net", 1).toInt() == 1;
    vm.audio = s.value("audio", 0).toInt() == 1;
    vm.hda = s.value("hda", 1).toInt() == 1;
    vm.vnc = s.value("vnc", 0).toInt() == 1;
    vm.vnc_port = s.value("vnc_port", 5900).toInt();
    vm.vnc_pass = s.value("vnc_pass", 0).toInt() == 1;
    s.endGroup();
    return vm;
}

class VMDialog : public QDialog {
    Q_OBJECT
public:
    VMDialog(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("QMGR - Create / Edit VM");
        auto *form = new QFormLayout(this);

        nameEdit = new QLineEdit(this);
        diskEdit = new QLineEdit(this);
        isoEdit = new QLineEdit(this);
        memSpin = new QSpinBox(this);
        memSpin->setRange(64, 65536);
        memSpin->setValue(4096);
        cpuEdit = new QLineEdit(this);
        cpuEdit->setText("qemu64");
        netCheck = new QCheckBox(this); netCheck->setChecked(true);
        audioCheck = new QCheckBox(this);
        hdaCheck = new QCheckBox(this); hdaCheck->setChecked(true);

        vncCheck = new QCheckBox(this);
        vncPortSpin = new QSpinBox(this); vncPortSpin->setRange(5900, 5999); vncPortSpin->setValue(5900);
        vncPassCheck = new QCheckBox(this);

        QPushButton *browseDisk = new QPushButton("Browse...", this);
        QPushButton *browseIso = new QPushButton("Browse...", this);

        connect(browseDisk, &QPushButton::clicked, this, [this](){
            QString f = QFileDialog::getOpenFileName(this,"Select Disk Image", QDir::homePath(), "Disk Images (*.qcow2 *.img *.raw);;All Files (*)");
            if(!f.isEmpty()) diskEdit->setText(f);
        });
        connect(browseIso, &QPushButton::clicked, this, [this](){
            QString f = QFileDialog::getOpenFileName(this,"Select ISO Image", QDir::homePath(), "ISOs (*.iso);;All Files (*)");
            if(!f.isEmpty()) isoEdit->setText(f);
        });

        form->addRow("VM Name:", nameEdit);
        QHBoxLayout *h1 = new QHBoxLayout; h1->addWidget(diskEdit); h1->addWidget(browseDisk);
        form->addRow("Disk Image:", h1);
        QHBoxLayout *h2 = new QHBoxLayout; h2->addWidget(isoEdit); h2->addWidget(browseIso);
        form->addRow("ISO Image (optional):", h2);
        form->addRow("Memory (MB):", memSpin);
        form->addRow("CPU type:", cpuEdit);
        form->addRow("Network Enabled:", netCheck);
        form->addRow("Audio Enabled:", audioCheck);
        form->addRow("Primary HDD Present:", hdaCheck);
        form->addRow("Enable VNC:", vncCheck);
        form->addRow("VNC Port:", vncPortSpin);
        form->addRow("Enable VNC Password:", vncPassCheck);

        QPushButton *ok = new QPushButton("Save", this);
        QPushButton *cancel = new QPushButton("Cancel", this);
        QHBoxLayout *buttons = new QHBoxLayout;
        buttons->addStretch(); buttons->addWidget(ok); buttons->addWidget(cancel);
        form->addRow(buttons);

        connect(ok, &QPushButton::clicked, this, &VMDialog::onSave);
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    }

    void setVM(const VM &vm) {
        nameEdit->setText(vm.name);
        diskEdit->setText(vm.disk);
        isoEdit->setText(vm.iso);
        memSpin->setValue(vm.mem);
        cpuEdit->setText(vm.cpu);
        netCheck->setChecked(vm.net);
        audioCheck->setChecked(vm.audio);
        hdaCheck->setChecked(vm.hda);
        vncCheck->setChecked(vm.vnc);
        vncPortSpin->setValue(vm.vnc_port);
        vncPassCheck->setChecked(vm.vnc_pass);
    }

    VM getVM() const {
        VM vm;
        vm.name = nameEdit->text();
        vm.disk = diskEdit->text();
        vm.iso = isoEdit->text();
        vm.mem = memSpin->value();
        vm.cpu = cpuEdit->text();
        vm.net = netCheck->isChecked();
        vm.audio = audioCheck->isChecked();
        vm.hda = hdaCheck->isChecked();
        vm.vnc = vncCheck->isChecked();
        vm.vnc_port = vncPortSpin->value();
        vm.vnc_pass = vncPassCheck->isChecked();
        return vm;
    }

private slots:
    void onSave() {
        if(nameEdit->text().trimmed().isEmpty()) { QMessageBox::warning(this,"Validation","VM name is required."); return; }
        if(diskEdit->text().trimmed().isEmpty() && hdaCheck->isChecked()) { QMessageBox::warning(this,"Validation","Disk image required for primary HDD."); return; }
        accept();
    }

private:
    QLineEdit *nameEdit,*diskEdit,*isoEdit,*cpuEdit;
    QSpinBox *memSpin,*vncPortSpin;
    QCheckBox *netCheck,*audioCheck,*hdaCheck,*vncCheck,*vncPassCheck;
};

class MainWindow : public QWidget {
    Q_OBJECT
public:
    MainWindow(QWidget *parent=nullptr) : QWidget(parent) {
        setWindowTitle("QMGR");
        resize(800,450);

        listWidget = new QListWidget(this);
        createBtn = new QPushButton("Create VM",this);
        editBtn = new QPushButton("Edit VM",this);
        launchBtn = new QPushButton("Launch",this);
        killBtn = new QPushButton("Kill VM", this);
        createDiskBtn = new QPushButton("Create Disk", this);
        exportBtn = new QPushButton("Export",this);
        importBtn = new QPushButton("Import",this);
        quitBtn = new QPushButton("Quit",this);

        QHBoxLayout *btns = new QHBoxLayout;
        btns->addWidget(createBtn); btns->addWidget(editBtn); btns->addWidget(launchBtn);
        btns->addWidget(killBtn); btns->addWidget(createDiskBtn); btns->addWidget(exportBtn);
        btns->addWidget(importBtn); btns->addStretch(); btns->addWidget(quitBtn);

        QVBoxLayout *main = new QVBoxLayout(this);
        main->addWidget(listWidget);
        main->addLayout(btns);

        connect(createBtn,&QPushButton::clicked,this,&MainWindow::onCreate);
        connect(editBtn,&QPushButton::clicked,this,&MainWindow::onEdit);
        connect(launchBtn,&QPushButton::clicked,this,&MainWindow::onLaunch);
        connect(killBtn,&QPushButton::clicked,this,&MainWindow::onKill);
        connect(createDiskBtn,&QPushButton::clicked,this,&MainWindow::onCreateDisk);
        connect(exportBtn,&QPushButton::clicked,this,&MainWindow::onExport);
        connect(importBtn,&QPushButton::clicked,this,&MainWindow::onImport);
        connect(quitBtn,&QPushButton::clicked,&QWidget::close);

        loadList();
    }

private slots:
    void loadList() {
        listWidget->clear();
        QSettings s(getDatabasePath(),QSettings::IniFormat);
        for(const QString &group : s.childGroups()) listWidget->addItem(group);
    }

    void onCreate() {
        VMDialog dlg(this);
        if(dlg.exec()==QDialog::Accepted) {
            VM vm = dlg.getVM();
            vmToSettings(vm);
            loadList();
        }
    }

    void onEdit() {
        auto item = listWidget->currentItem();
        if(!item) return;
        QString name = item->text();
        VM vm = vmFromSettings(name);
        VMDialog dlg(this);
        dlg.setVM(vm);
        if(dlg.exec()==QDialog::Accepted) {
            VM newvm = dlg.getVM();
            if(newvm.name != name) {
                QSettings s(getDatabasePath(),QSettings::IniFormat);
                s.beginGroup(name);
                s.remove("");
                s.endGroup();
            }
            vmToSettings(newvm);
            loadList();
        }
    }

    void onLaunch() {
        auto item = listWidget->currentItem();
        if(!item) return;
        QString name = item->text();
        VM vm = vmFromSettings(name);
        QString qemu = findQemuExecutable();
        QStringList args;

        args << "-enable-kvm";
        args << "-m" << QString::number(vm.mem);
        if(vm.hda) args << "-hda" << vm.disk;
        if(!vm.iso.isEmpty()) args << "-cdrom" << vm.iso;
        args << "-boot" << "menu=on";
        args << "-vga" << "std";
        args << "-usb" << "-device" << "usb-tablet";
        args << "-name" << vm.name;
        if(vm.net) args << "-net" << "nic" << "-net" << "user";
        if(vm.audio) args << "-audiodev" << "pa,id=snd0" << "-device" << "ich9-intel-hda" << "-device" << "hda-output,audiodev=snd0";
        if(vm.vnc) {
            QString vncArg = QString(":%1").arg(vm.vnc_port - 5900);
            if(vm.vnc_pass) vncArg += ",password=on";
            args << "-vnc" << vncArg;
        }
        args << "-display" << "sdl";
        args << "-monitor" << "stdio";

        QProcess *proc = new QProcess(this);
        proc->setProgram(qemu);
        proc->setArguments(args);
        proc->setProcessChannelMode(QProcess::ForwardedChannels);
        proc->start();

        if(!proc->waitForStarted()) {
            QMessageBox::critical(this,"Error","Failed to start QEMU.");
            proc->deleteLater();
            return;
        }

        runningProcs[name] = proc;
    }

    void onKill() {
        auto item = listWidget->currentItem();
        if(!item) return;
        QString name = item->text();
        if(runningProcs.contains(name)) {
            QProcess *proc = runningProcs[name];
            if(proc->state() != QProcess::NotRunning) {
                proc->kill();
                proc->waitForFinished();
                QMessageBox::information(this,"Killed","VM process terminated.");
            }
            runningProcs.remove(name);
        } else {
            QMessageBox::warning(this,"Info","No running VM process found for this VM.");
        }
    }

    void onCreateDisk() {
        QString file = QFileDialog::getSaveFileName(this, "Create QCOW2 Disk", QDir::homePath(), "QCOW2 Disk (*.qcow2)");
        if(file.isEmpty()) return;

        bool ok;
        int sizeGB = QInputDialog::getInt(this, "Disk Size", "Enter size in GB:", 10, 1, 1024, 1, &ok);
        if(!ok) return;

        QString qemuImg = "qemu-img";
        QStringList args;
        args << "create" << "-f" << "qcow2" << file << QString::number(sizeGB) + "G";

        QProcess proc;
        proc.setProgram(qemuImg);
        proc.setArguments(args);
        proc.setProcessChannelMode(QProcess::ForwardedChannels);
        proc.start();
        if(!proc.waitForFinished()) {
            QMessageBox::critical(this,"Error","Failed to create QCOW2 disk.");
            return;
        }

        QMessageBox::information(this,"Success","Disk created successfully:\n" + file);
    }

    void onExport() {
        auto item = listWidget->currentItem();
        if(!item) return;
        QString name = item->text();
        QString folder = QFileDialog::getExistingDirectory(this,"Select export folder",QDir::homePath());
        if(folder.isEmpty()) return;

        VM vm = vmFromSettings(name);
        QDir().mkpath(folder);

        if(!vm.disk.isEmpty()) QFile::copy(vm.disk,QDir(folder).filePath(QFileInfo(vm.disk).fileName()));
        if(!vm.iso.isEmpty()) QFile::copy(vm.iso,QDir(folder).filePath(QFileInfo(vm.iso).fileName()));

        QSettings exportSettings(QDir(folder).filePath(vm.name + ".ini"), QSettings::IniFormat);
        exportSettings.beginGroup(vm.name);
        exportSettings.setValue("disk", QFileInfo(vm.disk).fileName());
        exportSettings.setValue("iso", QFileInfo(vm.iso).fileName());
        exportSettings.setValue("mem", vm.mem);
        exportSettings.setValue("cpu", vm.cpu);
        exportSettings.setValue("net", vm.net ? 1 : 0);
        exportSettings.setValue("audio", vm.audio ? 1 : 0);
        exportSettings.setValue("hda", vm.hda ? 1 : 0);
        exportSettings.setValue("vnc", vm.vnc ? 1 : 0);
        exportSettings.setValue("vnc_port", vm.vnc_port);
        exportSettings.setValue("vnc_pass", vm.vnc_pass ? 1 : 0);
        exportSettings.endGroup();
        exportSettings.sync();

        QMessageBox::information(this,"Export","Export complete.");
    }

    void onImport() {
        QString folder = QFileDialog::getExistingDirectory(this,"Select import folder",QDir::homePath());
        if(folder.isEmpty()) return;
        QDir d(folder);
        QStringList files = d.entryList(QStringList()<<"*.ini",QDir::Files);
        if(files.isEmpty()) { QMessageBox::warning(this,"Import","No INI file found."); return; }

        for(const QString &file : files) {
            QSettings s(d.filePath(file),QSettings::IniFormat);
            for(const QString &section : s.childGroups()) {
                s.beginGroup(section);
                VM vm;
                vm.name = section;
                vm.disk = s.value("disk").toString();
                vm.iso = s.value("iso").toString();
                vm.mem = s.value("mem",4096).toInt();
                vm.cpu = s.value("cpu","qemu64").toString();
                vm.net = s.value("net",1).toInt()==1;
                vm.audio = s.value("audio",0).toInt()==1;
                vm.hda = s.value("hda",1).toInt()==1;
                vm.vnc = s.value("vnc",0).toInt()==1;
                vm.vnc_port = s.value("vnc_port",5900).toInt();
                vm.vnc_pass = s.value("vnc_pass",0).toInt()==1;
                s.endGroup();

                QString exeDir = QCoreApplication::applicationDirPath();
                if(!vm.disk.isEmpty()) {
                    QString src = d.filePath(vm.disk);
                    QString dest = QDir(exeDir).filePath(vm.disk);
                    QFile::copy(src,dest);
                    vm.disk = dest;
                }
                if(!vm.iso.isEmpty()) {
                    QString src = d.filePath(vm.iso);
                    QString dest = QDir(exeDir).filePath(vm.iso);
                    QFile::copy(src,dest);
                    vm.iso = dest;
                }

                vmToSettings(vm);
            }
        }
        loadList();
        QMessageBox::information(this,"Import","Import complete.");
    }

private:
    QListWidget *listWidget;
    QPushButton *createBtn,*editBtn,*launchBtn,*killBtn,*createDiskBtn,*exportBtn,*importBtn,*quitBtn;
    QMap<QString,QProcess*> runningProcs;
};

int main(int argc, char **argv) {
    QApplication app(argc,argv);
    MainWindow w;
    w.show();
    return app.exec();
}

#include "qmgr.moc"

