#include <QtWidgets>
#include <QProcess>
#include <map>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

std::string QEMU_EXE = 
#ifdef _WIN32
    "qemu-system-x86_64.exe";
#else
    "qemu-system-x86_64";
#endif

std::map<std::string, std::map<std::string,std::string>> VM_DB;

void save_vm(const std::string &name, const std::map<std::string,std::string> &vm) {
    VM_DB[name] = vm;
}

std::map<std::string,std::string> load_vm(const std::string &name) {
    if(VM_DB.count(name)) return VM_DB[name];
    return {};
}

std::vector<std::string> list_vms() {
    std::vector<std::string> names;
    for(auto &kv : VM_DB) names.push_back(kv.first);
    return names;
}

void launch_vm(const std::map<std::string,std::string> &vm) {
    std::vector<std::string> cmd;
    cmd.push_back(QEMU_EXE);
    cmd.push_back("-enable-kvm");
    cmd.push_back("-m"); cmd.push_back(vm.at("mem"));
    cmd.push_back("-cpu"); cmd.push_back(vm.at("cpu"));
    cmd.push_back("-hda"); cmd.push_back(vm.at("disk"));
    cmd.push_back("-boot"); cmd.push_back("menu=on");
    cmd.push_back("-vga"); cmd.push_back("std");
    cmd.push_back("-usb"); cmd.push_back("-device"); cmd.push_back("usb-tablet");
    cmd.push_back("-name"); cmd.push_back(vm.at("name"));

    if(vm.count("iso") && vm.at("cdrom") == "1") {
        cmd.push_back("-cdrom"); cmd.push_back(vm.at("iso"));
    }

    if(vm.at("net") == "1") {
        cmd.push_back("-net"); cmd.push_back("nic");
        cmd.push_back("-net"); cmd.push_back("user");
    }

    if(vm.at("audio") == "1") {
#ifdef _WIN32
        cmd.push_back("-soundhw"); cmd.push_back("ac97");
#else
        cmd.push_back("-audiodev"); cmd.push_back("pa,id=snd0");
        cmd.push_back("-device"); cmd.push_back("ich9-intel-hda");
        cmd.push_back("-device"); cmd.push_back("hda-output,audiodev=snd0");
#endif
    }

    if(vm.at("vnc") == "1") {
        int port = std::stoi(vm.at("vnc_port"));
        std::string vnc_arg = ":" + std::to_string(port-5900);
        if(vm.at("vnc_pass") == "1") vnc_arg += ",password=on";
        cmd.push_back("-vnc"); cmd.push_back(vnc_arg);
    } else {
#ifdef _WIN32
        cmd.push_back("-display"); cmd.push_back("sdl");
#else
        cmd.push_back("-display"); cmd.push_back("gtk");
#endif
    }

#ifdef _WIN32
    std::vector<const char*> argv;
    for(auto &s : cmd) argv.push_back(s.c_str());
    argv.push_back(nullptr);
    _spawnvp(_P_NOWAIT, argv[0], const_cast<char* const*>(argv.data()));
#else
    std::vector<const char*> argv;
    for(auto &s : cmd) argv.push_back(s.c_str());
    argv.push_back(nullptr);
    if(fork()==0) {
        execvp(argv[0], const_cast<char* const*>(argv.data()));
        perror("execvp failed");
        exit(1);
    }
#endif
}

// Create/Edit VM dialog
void create_edit_vm(QWidget *parent=nullptr, const std::string &vm_name="") {
    QDialog dlg(parent);
    dlg.setWindowTitle("QMGR - Create/Edit VM");
    QFormLayout layout(&dlg);

    std::map<std::string,QLineEdit*> entries;
    std::map<std::string,QComboBox*> combos;

    std::vector<std::tuple<std::string,std::string,std::string>> fields = {
        {"VM Name","name","entry"},
        {"Disk Image","disk","entry"},
        {"ISO Image","iso","entry"},
        {"Memory (MB)","mem","entry"},
        {"CPU Type","cpu","entry"},
        {"Audio","audio","dropdown"},
        {"Network","net","dropdown"},
        {"CD-ROM","cdrom","dropdown"},
        {"Enable VNC","vnc","dropdown"},
        {"VNC Port","vnc_port","entry"},
        {"Enable VNC Password","vnc_pass","dropdown"}
    };

    auto vm = vm_name.empty()? std::map<std::string,std::string>{} : load_vm(vm_name);

    for(auto &[label,key,type] : fields) {
        if(type=="entry") {
            QLineEdit *edit = new QLineEdit(vm.count(key)? QString::fromStdString(vm[key]) : "", &dlg);
            layout.addRow(QString::fromStdString(label), edit);
            entries[key] = edit;
        } else if(type=="dropdown") {
            QComboBox *combo = new QComboBox(&dlg);
            combo->addItem("Yes"); combo->addItem("No");
            if(vm.count(key)) combo->setCurrentIndex(vm[key]=="1"?0:1);
            layout.addRow(QString::fromStdString(label), combo);
            combos[key] = combo;
        }
    }

    QPushButton *saveBtn = new QPushButton("Save", &dlg);
    layout.addWidget(saveBtn);
    QObject::connect(saveBtn, &QPushButton::clicked, [&]() {
        std::map<std::string,std::string> new_vm;
        for(auto &[k,edit]: entries) new_vm[k] = edit->text().toStdString();
        for(auto &[k,combo]: combos) new_vm[k] = combo->currentText()=="Yes"?"1":"0";
        save_vm(new_vm["name"], new_vm);
        dlg.accept();
    });

    dlg.exec();
}

// Create QCOW2 disk
void create_disk(QWidget *parent=nullptr) {
    QString fileName = QFileDialog::getSaveFileName(parent,"Create QCOW2 Disk","","QCOW2 Files (*.qcow2)");
    if(fileName.isEmpty()) return;

    bool ok;
    QString size = QInputDialog::getText(parent,"Disk Size","Enter size (e.g., 10G, 10240M):",QLineEdit::Normal,"10G",&ok);
    if(!ok || size.isEmpty()) return;

    QString cmd = QString("qemu-img create -f qcow2 \"%1\" %2").arg(fileName).arg(size);
    int ret = system(cmd.toStdString().c_str());
    QMessageBox::information(parent,"Disk Creation",ret==0?"Disk created successfully":"Failed to create disk");
}

// Main Window (original layout: listbox + horizontal buttons)
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QWidget window;
    window.setWindowTitle("QMGR");
    window.resize(550,400);

    QVBoxLayout mainLayout(&window);

    QListWidget vmList;
    mainLayout.addWidget(&vmList);

    QHBoxLayout buttonLayout;
    QPushButton createBtn("Create");
    QPushButton editBtn("Edit");
    QPushButton launchBtn("Launch");
    QPushButton exportBtn("Export");
    QPushButton importBtn("Import");
    QPushButton diskBtn("Create Disk");
    QPushButton killBtn("Kill VM");
    QPushButton quitBtn("Quit");

    buttonLayout.addWidget(&createBtn);
    buttonLayout.addWidget(&editBtn);
    buttonLayout.addWidget(&launchBtn);
    buttonLayout.addWidget(&exportBtn);
    buttonLayout.addWidget(&importBtn);
    buttonLayout.addWidget(&diskBtn);
    buttonLayout.addWidget(&killBtn);
    buttonLayout.addWidget(&quitBtn);

    mainLayout.addLayout(&buttonLayout);

    auto refresh_list = [&](){
        vmList.clear();
        for(auto &n : list_vms()) vmList.addItem(QString::fromStdString(n));
    };
    refresh_list();

    QObject::connect(&createBtn, &QPushButton::clicked, [&](){
        create_edit_vm(&window);
        refresh_list();
    });

    QObject::connect(&editBtn, &QPushButton::clicked, [&](){
        if(vmList.currentItem())
            create_edit_vm(&window, vmList.currentItem()->text().toStdString());
        refresh_list();
    });

    QObject::connect(&launchBtn, &QPushButton::clicked, [&](){
        if(vmList.currentItem()) {
            auto vm = load_vm(vmList.currentItem()->text().toStdString());
            launch_vm(vm);
        }
    });

    QObject::connect(&exportBtn, &QPushButton::clicked, [&](){
        if(vmList.currentItem()) {
            QString folder = QFileDialog::getExistingDirectory(&window,"Select Export Folder");
            if(folder.isEmpty()) return;
            auto vm = load_vm(vmList.currentItem()->text().toStdString());
            std::ofstream cfg(folder.toStdString()+"/"+vm["name"]+".ini");
            for(auto &[k,v]: vm) cfg<<k<<"="<<v<<"\n";
            cfg.close();
            QFile::copy(QString::fromStdString(vm["disk"]), folder+"/"+QString::fromStdString(vm["disk"]));
            if(vm.count("iso")) QFile::copy(QString::fromStdString(vm["iso"]), folder+"/"+QString::fromStdString(vm["iso"]));
            QMessageBox::information(&window,"Export","VM exported successfully");
        }
    });

    QObject::connect(&importBtn, &QPushButton::clicked, [&](){
        QString folder = QFileDialog::getExistingDirectory(&window,"Select Import Folder");
        if(folder.isEmpty()) return;
        QDir dir(folder);
        dir.setNameFilters(QStringList() << "*.ini");
        auto files = dir.entryList();
        if(files.isEmpty()) return;
        std::ifstream cfg(folder.toStdString()+"/"+files[0].toStdString());
        std::map<std::string,std::string> vm;
        std::string line;
        while(std::getline(cfg,line)) {
            auto pos = line.find("=");
            if(pos!=std::string::npos) vm[line.substr(0,pos)] = line.substr(pos+1);
        }
        save_vm(vm["name"], vm);
        QFile::copy(folder+"/"+QString::fromStdString(vm["disk"]), QString::fromStdString(vm["disk"]));
        if(vm.count("iso")) QFile::copy(folder+"/"+QString::fromStdString(vm["iso"]), QString::fromStdString(vm["iso"]));
        QMessageBox::information(&window,"Import","VM imported successfully");
        refresh_list();
    });

    QObject::connect(&diskBtn, &QPushButton::clicked, [&](){
        create_disk(&window);
    });

    QObject::connect(&killBtn, &QPushButton::clicked, [&](){
        QMessageBox::information(&window,"Kill VM","Manually kill the VM process in your system task manager.");
    });

    QObject::connect(&quitBtn, &QPushButton::clicked, [&](){
        window.close();
    });

    window.show();
    return app.exec();
}
