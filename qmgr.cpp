#include <QApplication>
#include <QWidget>
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
#include <QComboBox>
#include <QLabel>

#ifdef Q_OS_WIN
#include <windows.h>
#include <intrin.h>
#endif

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
    bool accel_override = false;
    QString accel_type = "default";
};

static QString getDatabasePath() {
    return QDir(QCoreApplication::applicationDirPath()).filePath("database.ini");
}

static bool hasVirtualization() {
#ifdef Q_OS_WIN
    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 1);
    if ((cpuInfo[2] & (1 << 5)) != 0) {
        return true;
    }
    __cpuid(cpuInfo, 0x80000001);
    if ((cpuInfo[2] & (1 << 2)) != 0) {
        return true;
    }
    return false;
#else
    return QFile::exists("/dev/kvm");
#endif
}

static QString findQemuExecutable() {
#ifdef Q_OS_WIN
    QString defaultPath = "C:/Program Files/qemu/qemu-system-x86_64.exe";
    if (QFileInfo::exists(defaultPath)) return defaultPath;
    return "qemu-system-x86_64.exe";
#else
    return "qemu-system-x86_64";
#endif
}

static QString findQemuImgExecutable() {
#ifdef Q_OS_WIN
    QString defaultPath = "C:/Program Files/qemu/qemu-img.exe";
    if (QFileInfo::exists(defaultPath)) return defaultPath;
    return "qemu-img.exe";
#else
    return "qemu-img";
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
    s.setValue("accel_override", vm.accel_override ? 1 : 0);
    s.setValue("accel_type", vm.accel_type);
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
    vm.accel_override = s.value("accel_override", 0).toInt() == 1;
    vm.accel_type = s.value("accel_type", "default").toString();
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
        
        accelOverrideCheck = new QCheckBox(this);
        accelTypeCombo = new QComboBox(this);
        accelTypeCombo->addItem("default");
#ifdef Q_OS_WIN
        accelTypeCombo->addItem("whpx");
        accelTypeCombo->addItem("hax");
        accelTypeCombo->addItem("tcg");
#else
        accelTypeCombo->addItem("kvm");
        accelTypeCombo->addItem("tcg");
#endif

        QPushButton *browseDisk = new QPushButton("Browse...", this);
        QPushButton *browseIso = new QPushButton("Browse...", this);

        connect(browseDisk, &QPushButton::clicked, this, [this](){
            QString f = QFileDialog::getOpenFileName(this, "Select Disk Image", QDir::homePath(),
                                                     "Disk Images (*.qcow2 *.img *.raw);;All Files (*)");
            if (!f.isEmpty()) diskEdit->setText(f);
        });
        connect(browseIso, &QPushButton::clicked, this, [this](){
            QString f = QFileDialog::getOpenFileName(this, "Select ISO Image", QDir::homePath(),
                                                     "ISOs (*.iso);;All Files (*)");
            if (!f.isEmpty()) isoEdit->setText(f);
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
        form->addRow("Override Accelerator:", accelOverrideCheck);
        form->addRow("Accelerator Type:", accelTypeCombo);


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
        accelOverrideCheck->setChecked(vm.accel_override);
        accelTypeCombo->setCurrentText(vm.accel_type);
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
        vm.accel_override = accelOverrideCheck->isChecked();
        vm.accel_type = accelTypeCombo->currentText();
        return vm;
    }

private slots:
    void onSave() {
        if (nameEdit->text().trimmed().isEmpty()) { QMessageBox::warning(this, "Validation", "VM name is required."); return; }
        if (diskEdit->text().trimmed().isEmpty() && hdaCheck->isChecked()) { QMessageBox::warning(this, "Validation", "Disk image required for primary HDD."); return; }
        accept();
    }

private:
    QLineEdit *nameEdit, *diskEdit, *isoEdit, *cpuEdit;
    QSpinBox *memSpin, *vncPortSpin;
    QCheckBox *netCheck, *audioCheck, *hdaCheck, *vncCheck, *vncPassCheck;
    QCheckBox *accelOverrideCheck;
    QComboBox *accelTypeCombo;
};

class DeleteConfirmDialog : public QDialog {
    Q_OBJECT
public:
    DeleteConfirmDialog(const VM& vm, QWidget *parent = nullptr) : QDialog(parent), m_vm(vm) {
        setWindowTitle("Confirm Deletion and Cleanup");
        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        
        QLabel *prompt = new QLabel(QString("Are you sure you want to delete the configuration for VM '<b>%1</b>'?").arg(m_vm.name));
        mainLayout->addWidget(prompt);

        QString diskPath = m_vm.disk;
        QString diskText = diskPath.isEmpty() ? "No hard drive image associated." : QString("Delete Hard Drive Image: <b>%1</b>").arg(QFileInfo(diskPath).fileName());
        deleteDiskCheck = new QCheckBox(diskText);
        deleteDiskCheck->setChecked(!diskPath.isEmpty() && QFileInfo::exists(diskPath));
        deleteDiskCheck->setEnabled(!diskPath.isEmpty() && QFileInfo::exists(diskPath));
        mainLayout->addWidget(deleteDiskCheck);

        QString isoPath = m_vm.iso;
        QString isoText = isoPath.isEmpty() ? "No ISO file associated." : QString("Delete ISO File: <b>%1</b>").arg(QFileInfo(isoPath).fileName());
        deleteIsoCheck = new QCheckBox(isoText);
        deleteIsoCheck->setChecked(!isoPath.isEmpty() && QFileInfo::exists(isoPath));
        deleteIsoCheck->setEnabled(!isoPath.isEmpty() && QFileInfo::exists(isoPath));
        mainLayout->addWidget(deleteIsoCheck);

        QHBoxLayout *buttonLayout = new QHBoxLayout;
        QPushButton *okButton = new QPushButton("Delete", this);
        QPushButton *cancelButton = new QPushButton("Cancel", this);
        buttonLayout->addStretch();
        buttonLayout->addWidget(okButton);
        buttonLayout->addWidget(cancelButton);
        mainLayout->addLayout(buttonLayout);

        connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
        connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    }

    bool shouldDeleteDisk() const { return deleteDiskCheck->isChecked(); }
    bool shouldDeleteIso() const { return deleteIsoCheck->isChecked(); }

private:
    VM m_vm;
    QCheckBox *deleteDiskCheck;
    QCheckBox *deleteIsoCheck;
};


class MainWindow : public QWidget {
    Q_OBJECT
public:
    MainWindow(QWidget *parent=nullptr) : QWidget(parent) {
        setWindowTitle("QMGR");
        resize(800, 450);

        listWidget = new QListWidget(this);
        createBtn = new QPushButton("Create VM", this);
        editBtn = new QPushButton("Edit VM", this);
        renameBtn = new QPushButton("Rename VM", this);
        launchBtn = new QPushButton("Launch", this);
        killBtn = new QPushButton("Kill VM", this);
        deleteBtn = new QPushButton("Delete VM", this);
        createDiskBtn = new QPushButton("Create Disk", this);
        exportBtn = new QPushButton("Export", this);
        importBtn = new QPushButton("Import", this);
        quitBtn = new QPushButton("Quit", this);

        QHBoxLayout *btns = new QHBoxLayout;
        btns->addWidget(createBtn); btns->addWidget(editBtn); btns->addWidget(renameBtn);
        btns->addWidget(deleteBtn); btns->addWidget(launchBtn); btns->addWidget(killBtn);
        btns->addWidget(createDiskBtn); btns->addWidget(exportBtn);
        btns->addWidget(importBtn); btns->addStretch(); btns->addWidget(quitBtn);

        QVBoxLayout *main = new QVBoxLayout(this);
        main->addWidget(listWidget);
        main->addLayout(btns);

        connect(createBtn, &QPushButton::clicked, this, &MainWindow::onCreate);
        connect(editBtn, &QPushButton::clicked, this, &MainWindow::onEdit);
        connect(renameBtn, &QPushButton::clicked, this, &MainWindow::onRename);
        connect(deleteBtn, &QPushButton::clicked, this, &MainWindow::onDelete);
        connect(launchBtn, &QPushButton::clicked, this, &MainWindow::onLaunch);
        connect(killBtn, &QPushButton::clicked, this, &MainWindow::onKill);
        connect(createDiskBtn, &QPushButton::clicked, this, &MainWindow::onCreateDisk);
        connect(exportBtn, &QPushButton::clicked, this, &MainWindow::onExport);
        connect(importBtn, &QPushButton::clicked, this, &MainWindow::onImport);
        connect(quitBtn, &QPushButton::clicked, &QWidget::close);

        loadList();
    }

private slots:
    void loadList() {
        listWidget->clear();
        QSettings s(getDatabasePath(), QSettings::IniFormat);
        for (const QString &group : s.childGroups()) listWidget->addItem(group);
    }

    void onCreate() {
        VMDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            VM vm = dlg.getVM();
            QSettings s(getDatabasePath(), QSettings::IniFormat);
            if (s.childGroups().contains(vm.name)) {
                QMessageBox::warning(this, "Error", "A VM with this name already exists. Creation aborted.");
                return;
            }
            vmToSettings(vm);
            loadList();
        }
    }

    void onEdit() {
        auto item = listWidget->currentItem();
        if (!item) return;
        QString name = item->text();
        VM vm = vmFromSettings(name);
        VMDialog dlg(this);
        dlg.setVM(vm);
        if (dlg.exec() == QDialog::Accepted) {
            VM newvm = dlg.getVM();
            if (newvm.name != name) {
                QSettings s_check(getDatabasePath(), QSettings::IniFormat);
                if (s_check.childGroups().contains(newvm.name)) {
                    QMessageBox::warning(this, "Error", "A VM with the new name already exists. Save aborted.");
                    return;
                }
                
                QSettings s_remove(getDatabasePath(), QSettings::IniFormat);
                s_remove.remove(name);
                s_remove.sync();

                if (runningProcs.contains(name)) {
                    QProcess *proc = runningProcs.value(name);
                    runningProcs.remove(name);
                    runningProcs[newvm.name] = proc;
                }
            }
            vmToSettings(newvm);
            loadList();
        }
    }

    void onRename() {
        auto item = listWidget->currentItem();
        if (!item) return;
        QString oldName = item->text();

        bool ok;
        QString newName = QInputDialog::getText(this, "Rename VM",
                                                QString("Enter new name for VM '%1':").arg(oldName),
                                                QLineEdit::Normal, oldName, &ok);

        if (ok && !newName.trimmed().isEmpty() && newName != oldName) {
            QSettings s(getDatabasePath(), QSettings::IniFormat);
            if (s.childGroups().contains(newName)) {
                QMessageBox::warning(this, "Error", "A VM with this name already exists.");
                return;
            }

            VM vm = vmFromSettings(oldName);

            s.remove(oldName);
            vm.name = newName;
            vmToSettings(vm);

            if (runningProcs.contains(oldName)) {
                QProcess *proc = runningProcs.value(oldName);
                runningProcs.remove(oldName);
                runningProcs[newName] = proc;
            }

            loadList();
            QMessageBox::information(this, "Rename Success", QString("VM successfully renamed to '%1'").arg(newName));
        }
    }
    
    void onDelete() {
        auto item = listWidget->currentItem();
        if (!item) return;
        QString name = item->text();

        VM vm = vmFromSettings(name);
        
        DeleteConfirmDialog confirmDlg(vm, this);
        if (confirmDlg.exec() == QDialog::Rejected) return;

        if (runningProcs.contains(name)) {
            QProcess *proc = runningProcs[name];
            if (proc->state() != QProcess::NotRunning) {
                proc->kill();
                proc->waitForFinished(5000);
            }
            runningProcs.remove(name);
        }

        bool fileCleanupSuccess = true;
        QString deletedFiles = "";
        
        if (confirmDlg.shouldDeleteDisk() && !vm.disk.isEmpty() && QFileInfo::exists(vm.disk)) {
            if (QFile::remove(vm.disk)) {
                deletedFiles += QFileInfo(vm.disk).fileName() + " (Disk Image)\n";
            } else {
                QMessageBox::warning(this, "Cleanup Error", QString("Failed to delete disk image:\n%1").arg(vm.disk));
                fileCleanupSuccess = false;
            }
        }
        
        if (confirmDlg.shouldDeleteIso() && !vm.iso.isEmpty() && QFileInfo::exists(vm.iso)) {
            if (QFile::remove(vm.iso)) {
                deletedFiles += QFileInfo(vm.iso).fileName() + " (ISO File)\n";
            } else {
                QMessageBox::warning(this, "Cleanup Error", QString("Failed to delete ISO image:\n%1").arg(vm.iso));
                fileCleanupSuccess = false;
            }
        }

        QSettings s(getDatabasePath(), QSettings::IniFormat);
        s.remove(name);
        s.sync();

        loadList();
        
        QString statusMessage = QString("VM '<b>%1</b>' configuration has been deleted.").arg(name);
        if (!deletedFiles.isEmpty()) {
            statusMessage += "\n\nThe following files were also deleted:\n" + deletedFiles;
        }
        
        if (fileCleanupSuccess) {
            QMessageBox::information(this, "Deleted", statusMessage);
        } else {
            QMessageBox::warning(this, "Deleted (Partial Cleanup)", statusMessage + "\n\nOne or more selected files could not be deleted.");
        }
    }


    void onLaunch() {
        auto item = listWidget->currentItem();
        if (!item) return;
        QString name = item->text();
        VM vm = vmFromSettings(name);

        if (vm.hda && vm.disk.isEmpty()) {
            QMessageBox::warning(this, "Launch", "Primary HDD is enabled but no disk image is set.");
            return;
        }

        QString qemu = findQemuExecutable();
        QStringList args;

        QString accelArg;
        if (vm.accel_override && vm.accel_type != "default") {
            accelArg = vm.accel_type;
        } else {
#ifdef Q_OS_WIN
            accelArg = hasVirtualization() ? "whpx" : "tcg";
#else
            accelArg = hasVirtualization() ? "kvm" : "tcg";
#endif
        }
        
#ifdef Q_OS_WIN
        if (accelArg == "kvm") {
            accelArg = hasVirtualization() ? "whpx" : "tcg";
        }
#else
        if (accelArg == "whpx" || accelArg == "hax") {
            accelArg = hasVirtualization() ? "kvm" : "tcg";
        }
#endif
        
        args << "-accel" << accelArg;


        args << "-m" << QString::number(vm.mem);
        if (!vm.cpu.trimmed().isEmpty())
            args << "-cpu" << vm.cpu;

        if (vm.hda) args << "-hda" << vm.disk;
        if (!vm.iso.isEmpty()) args << "-cdrom" << vm.iso;

        args << "-boot" << "menu=on";
        args << "-vga" << "std";
        args << "-usb" << "-device" << "usb-tablet";
        args << "-name" << vm.name;

        if (vm.net) args << "-net" << "nic" << "-net" << "user";

        if (vm.audio) {
#ifdef Q_OS_WIN
            args << "-audiodev" << "dsound,id=snd0"
                 << "-device" << "ich9-intel-hda"
                 << "-device" << "hda-output,audiodev=snd0";
#else
            args << "-audiodev" << "pa,id=snd0"
                 << "-device" << "ich9-intel-hda"
                 << "-device" << "hda-output,audiodev=snd0";
#endif
        }

        if (vm.vnc) {
            QString vncArg = QString(":%1").arg(vm.vnc_port - 5900);
            if (vm.vnc_pass) vncArg += ",password=on";
            args << "-vnc" << vncArg;
        }

        args << "-display" << "sdl";
        args << "-monitor" << "stdio";

        QProcess *proc = new QProcess(this);
        proc->setProgram(qemu);
        proc->setArguments(args);
        proc->setProcessChannelMode(QProcess::ForwardedChannels);
        proc->start();

        if (!proc->waitForStarted()) {
            QMessageBox::critical(this, "Error", QString("Failed to start QEMU: %1").arg(proc->errorString()));
            proc->deleteLater();
            return;
        }

        runningProcs[name] = proc;
    }

    void onKill() {
        auto item = listWidget->currentItem();
        if (!item) return;
        QString name = item->text();
        if (runningProcs.contains(name)) {
            QProcess *proc = runningProcs[name];
            if (proc->state() != QProcess::NotRunning) {
                proc->kill();
                proc->waitForFinished();
                QMessageBox::information(this, "Killed", "VM process terminated.");
            }
            runningProcs.remove(name);
        } else {
            QMessageBox::warning(this, "Info", "No running VM process found for this VM.");
        }
    }

    void onCreateDisk() {
        QString file = QFileDialog::getSaveFileName(this, "Create QCOW2 Disk", QDir::homePath(), "QCOW2 Disk (*.qcow2)");
        if (file.isEmpty()) return;

        bool ok;
        int sizeGB = QInputDialog::getInt(this, "Disk Size", "Enter size in GB:", 10, 1, 1024, 1, &ok);
        if (!ok) return;

        QString qemuImg = findQemuImgExecutable();
        QStringList args;
        args << "create" << "-f" << "qcow2" << file << QString::number(sizeGB) + "G";

        QProcess proc;
        proc.setProgram(qemuImg);
        proc.setArguments(args);
        proc.setProcessChannelMode(QProcess::ForwardedChannels);
        proc.start();

        if (!proc.waitForFinished()) {
            QMessageBox::critical(this, "Error", "Failed to create QCOW2 disk.");
            return;
        }

        if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
            QMessageBox::critical(this, "Error", "qemu-img reported an error creating the disk.");
            return;
        }

        QMessageBox::information(this, "Success", "Disk created successfully:\n" + file);
    }

    void onExport() {
        auto item = listWidget->currentItem();
        if (!item) return;
        QString name = item->text();
        QString folder = QFileDialog::getExistingDirectory(this, "Select export folder", QDir::homePath());
        if (folder.isEmpty()) return;

        VM vm = vmFromSettings(name);
        QDir().mkpath(folder);

        if (!vm.disk.isEmpty()) {
            QString dest = QDir(folder).filePath(QFileInfo(vm.disk).fileName());
            if (dest != vm.disk) QFile::copy(vm.disk, dest);
        }
        if (!vm.iso.isEmpty()) {
            QString dest = QDir(folder).filePath(QFileInfo(vm.iso).fileName());
            if (dest != vm.iso) QFile::copy(vm.iso, dest);
        }

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
        exportSettings.setValue("accel_override", vm.accel_override ? 1 : 0);
        exportSettings.setValue("accel_type", vm.accel_type);
        exportSettings.endGroup();
        exportSettings.sync();

        QMessageBox::information(this, "Export", "Export complete.");
    }

    void onImport() {
        QString folder = QFileDialog::getExistingDirectory(this, "Select import folder", QDir::homePath());
        if (folder.isEmpty()) return;

        QDir d(folder);
        QStringList files = d.entryList(QStringList() << "*.ini", QDir::Files);
        if (files.isEmpty()) { QMessageBox::warning(this, "Import", "No INI file found."); return; }

        for (const QString &file : files) {
            QSettings s(d.filePath(file), QSettings::IniFormat);
            for (const QString &section : s.childGroups()) {
                s.beginGroup(section);
                VM vm;
                vm.name = section;
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
                vm.accel_override = s.value("accel_override", 0).toInt() == 1;
                vm.accel_type = s.value("accel_type", "default").toString();
                s.endGroup();

                QString exeDir = QCoreApplication::applicationDirPath();
                if (!vm.disk.isEmpty()) {
                    QString src = d.filePath(vm.disk);
                    QString dest = QDir(exeDir).filePath(QFileInfo(vm.disk).fileName());
                    if (QFileInfo::exists(src)) {
                        QFile::copy(src, dest);
                        vm.disk = dest;
                    } else {
                        vm.disk.clear();
                    }
                }
                if (!vm.iso.isEmpty()) {
                    QString src = d.filePath(vm.iso);
                    QString dest = QDir(exeDir).filePath(QFileInfo(vm.iso).fileName());
                    if (QFileInfo::exists(src)) {
                        QFile::copy(src, dest);
                        vm.iso = dest;
                    } else {
                        vm.iso.clear();
                    }
                }

                vmToSettings(vm);
            }
        }
        loadList();
        QMessageBox::information(this, "Import", "Import complete.");
    }

private:
    QListWidget *listWidget;
    QPushButton *createBtn, *editBtn, *renameBtn, *launchBtn, *killBtn, *deleteBtn, *createDiskBtn, *exportBtn, *importBtn, *quitBtn;
    QMap<QString, QProcess*> runningProcs;
};

int main(int argc, char **argv) {
    QCoreApplication::setOrganizationName("YourOrganization");
    QCoreApplication::setApplicationName("QMGR");

    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    return app.exec();
}

#include "qmgr.moc"
