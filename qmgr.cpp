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

// Simple VM database
std::map<std::string, std::map<std::string,std::string>> VM_DB;

// Helper functions
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

    // Launch QEMU
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

// Simple create/edit VM dialog
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

// Main window
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QWidget window;
    window.setWindowTitle("QMGR");
    QVBoxLayout layout(&window);

    QListWidget vmList;
    layout.addWidget(&vmList);

    QPushButton createBtn("Create VM");
    QPushButton launchBtn("Launch VM");
    QPushButton killBtn("Kill VM");
    layout.addWidget(&createBtn);
    layout.addWidget(&launchBtn);
    layout.addWidget(&killBtn);

    QObject::connect(&createBtn, &QPushButton::clicked, [&](){
        create_edit_vm(&window);
        vmList.clear();
        for(auto &n : list_vms()) vmList.addItem(QString::fromStdString(n));
    });

    QObject::connect(&launchBtn, &QPushButton::clicked, [&](){
        auto items = vmList.selectedItems();
        if(items.size()) {
            auto vm = load_vm(items[0]->text().toStdString());
            launch_vm(vm);
        }
    });

    QObject::connect(&killBtn, &QPushButton::clicked, [&](){
        QMessageBox::information(&window,"Kill VM","Manually kill the VM process in your system task manager.");
    });

    for(auto &n : list_vms()) vmList.addItem(QString::fromStdString(n));

    window.show();
    return app.exec();
}
