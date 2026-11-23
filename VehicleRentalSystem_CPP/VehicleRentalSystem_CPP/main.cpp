#include <QSqlQueryModel>
#include <QTextCharFormat>
#include <QPair>
#include <QSet>
#include <QIntValidator>
#include <algorithm>
#include <QStackedWidget>
#include <QMap>
#include <QColor>
#include <QFileDialog>
#include <QFileInfo>
#include <QCheckBox>
#include <QCalendarWidget>
#include <QSignalBlocker>
#include <QMenu>
#include <QPixmap>
#include <QCoreApplication>
#include <QApplication>
#include <QDialog>
#include <QStatusBar>
#include <QAbstractItemView>
#include <QMainWindow>
#include <QTabWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QDateEdit>
#include <QTableWidget>
#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>
#include <QLabel>
#include <QGroupBox>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDate>
#include <QDateTime>
#include <QCryptographicHash>
#include <QTextEdit>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QLocale>
#include <QFrame>
#include <QScrollArea>
#include <QGridLayout>
#include <QFont>
#include <QDateEdit>
#include <QCalendarWidget>

// ============================================================
// MODEL: entities and database access
// ============================================================
struct User {
    int id = 0;
    QString name, email, phone, address, role;
};

struct Vehicle {
    int id = 0;
    QString code, name, category, plate, make, fuel, transmission, summary;
    int seats = 4, rentalCount = 0, views = 0;
    double price = 0, discount = 0;
    bool available = true;
};
QString getVehicleImage(const Vehicle &v){
    QString name = v.name.toLower();

    if (name.contains("feliz s plus"))
        return "feliz_s_plus.jpg";

    if (name.contains("feliz s"))
        return "feliz_s.jpg";

    if (name.contains("vf 8 eco"))
        return "vf8_eco.jpg";

    if (name.contains("vf 8"))
        return "vf8.jpg";

    if (name.contains("vf 9 plus"))
        return "vf9_plus.jpg";

    if (name.contains("vf 9"))
        return "vf9.jpg";

    return "";
}

struct CartItem {
    int vehicleId = 0, quantity = 1, days = 1;
    QDate pickupDate;   // Ngày nhận xe
    QDate dropoffDate; // Ngày trả xe
};

static QString hashPassword(const QString &plain){
    return QString::fromLatin1(QCryptographicHash::hash(("vrs$"+plain).toUtf8(),QCryptographicHash::Sha256).toHex());
}
static bool validPhone(const QString &p){
    if(p.size()<9||p.size()>12) return false;
    for(const QChar &c : p) if(!c.isDigit()) return false;
    return true;
}
static bool validEmail(const QString &e){
    const int at=e.indexOf('@');
    return at>0 && e.indexOf('.',at)>at+1 && !e.endsWith('.') && !e.contains(' ');
}

class Database {
public:
    bool open() {
        if (QSqlDatabase::contains("vehicle_connection"))
            db = QSqlDatabase::database("vehicle_connection");
        else {
            db = QSqlDatabase::addDatabase("QSQLITE", "vehicle_connection");
            db.setDatabaseName("vehicle_rental.db");
        }
        if (!db.open()) return false;
        QSqlQuery q(db);
        const QStringList schema = {
            "PRAGMA foreign_keys=ON",
            "CREATE TABLE IF NOT EXISTS users(id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT,email TEXT UNIQUE,password TEXT,phone TEXT,address TEXT,role TEXT, dob TEXT, gender TEXT)",
            "CREATE TABLE IF NOT EXISTS categories(id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT UNIQUE)",
            "CREATE TABLE IF NOT EXISTS vehicles(id INTEGER PRIMARY KEY AUTOINCREMENT,code TEXT UNIQUE,name TEXT,category TEXT,plate TEXT,make TEXT,seats INTEGER,fuel TEXT,transmission TEXT,summary TEXT,price REAL,discount REAL,available INTEGER,rental_count INTEGER DEFAULT 0,views INTEGER DEFAULT 0)",
            "CREATE TABLE IF NOT EXISTS orders(id INTEGER PRIMARY KEY AUTOINCREMENT,user_id INTEGER,recipient TEXT,address TEXT,phone TEXT,license TEXT,delivery_method TEXT,delivery_fee REAL,total REAL,status TEXT,payment TEXT,created_at TEXT)",
            "CREATE TABLE IF NOT EXISTS order_items(id INTEGER PRIMARY KEY AUTOINCREMENT,order_id INTEGER,vehicle_id INTEGER,vehicle_name TEXT,quantity INTEGER,days INTEGER,unit_price REAL,pickup_date TEXT,dropoff_date TEXT)",
            "CREATE TABLE IF NOT EXISTS cart(user_id INTEGER,vehicle_id INTEGER,quantity INTEGER,days INTEGER,PRIMARY KEY(user_id,vehicle_id))"
        };
        for (const auto &sql : schema) if (!q.exec(sql)) qWarning() << q.lastError();
        // Migration: bo sung cot con thieu cho file .db tao boi cac phien ban cu
        // (CREATE TABLE IF NOT EXISTS khong tu them cot). Loi "duplicate column" duoc bo qua.
        const QStringList addColumns = {
            "ALTER TABLE users ADD COLUMN phone TEXT", "ALTER TABLE users ADD COLUMN address TEXT",
            "ALTER TABLE users ADD COLUMN role TEXT", "ALTER TABLE users ADD COLUMN dob TEXT", "ALTER TABLE users ADD COLUMN gender TEXT",
            "ALTER TABLE vehicles ADD COLUMN plate TEXT", "ALTER TABLE vehicles ADD COLUMN make TEXT",
            "ALTER TABLE vehicles ADD COLUMN seats INTEGER", "ALTER TABLE vehicles ADD COLUMN fuel TEXT",
            "ALTER TABLE vehicles ADD COLUMN transmission TEXT", "ALTER TABLE vehicles ADD COLUMN summary TEXT",
            "ALTER TABLE vehicles ADD COLUMN discount REAL DEFAULT 0", "ALTER TABLE vehicles ADD COLUMN available INTEGER DEFAULT 1",
            "ALTER TABLE vehicles ADD COLUMN rental_count INTEGER DEFAULT 0", "ALTER TABLE vehicles ADD COLUMN views INTEGER DEFAULT 0",
            "ALTER TABLE orders ADD COLUMN recipient TEXT", "ALTER TABLE orders ADD COLUMN address TEXT",
            "ALTER TABLE orders ADD COLUMN phone TEXT", "ALTER TABLE orders ADD COLUMN license TEXT",
            "ALTER TABLE orders ADD COLUMN delivery_method TEXT", "ALTER TABLE orders ADD COLUMN delivery_fee REAL",
            "ALTER TABLE orders ADD COLUMN payment TEXT", "ALTER TABLE orders ADD COLUMN created_at TEXT",
            "ALTER TABLE order_items ADD COLUMN vehicle_name TEXT", "ALTER TABLE order_items ADD COLUMN quantity INTEGER",
            "ALTER TABLE order_items ADD COLUMN days INTEGER", "ALTER TABLE order_items ADD COLUMN unit_price REAL",
            "ALTER TABLE order_items ADD COLUMN pickup_date TEXT", "ALTER TABLE order_items ADD COLUMN dropoff_date TEXT",
            "ALTER TABLE cart ADD COLUMN quantity INTEGER", "ALTER TABLE cart ADD COLUMN days INTEGER"
        };
        for (const auto &sql : addColumns) q.exec(sql);
        // Migration: luu ngay nhan/tra trong gio hang (loi bo qua neu cot da ton tai)
        q.exec("ALTER TABLE cart ADD COLUMN pickup_date TEXT");
        q.exec("ALTER TABLE cart ADD COLUMN dropoff_date TEXT");
        // Bien ban ban giao / tiep nhan xe (FR-12): moi don co toi da 1 ban ghi 'handover' va 1 ban ghi 'return'
        q.exec("CREATE TABLE IF NOT EXISTS handover_records(order_id INTEGER,kind TEXT,battery INTEGER,battery_ok INTEGER,"
               "exterior_ok INTEGER,docs_ok INTEGER,odo INTEGER,note TEXT,license_ok INTEGER,staff_id INTEGER,"
               "confirmed INTEGER DEFAULT 0,updated_at TEXT,PRIMARY KEY(order_id,kind))");
        // Tinh kha dung gio tinh theo NGAY (order_items), khong dung co vehicles.available nua.
        // Dat lai co cu (phien ban cu tung doi sang 0 khi dat xe) de xe khong bi "khong kha dung" mai.
        // Chi dat lai co "available" mot lan duy nhat (du lieu cu); sau do admin tu quan ly trang thai xe
        q.exec("PRAGMA user_version");
        if(q.next() && q.value(0).toInt()<1){
            q.finish();
            q.exec("UPDATE vehicles SET available=1");
            q.exec("PRAGMA user_version=1");
        }
        q.finish();
        // Don cu (tao truoc khi luu ngay) chua co pickup/dropoff -> suy ra tu ngay tao don + so ngay thue
        q.exec("UPDATE order_items SET "
               "pickup_date=(SELECT date(created_at) FROM orders WHERE orders.id=order_items.order_id), "
               "dropoff_date=(SELECT date(created_at,'+'||order_items.days||' days') FROM orders WHERE orders.id=order_items.order_id) "
               "WHERE pickup_date IS NULL OR pickup_date='' OR dropoff_date IS NULL OR dropoff_date=''");
        seed();
        // Chuyen mat khau cu (van ban thuong) sang bam SHA-256
        QSqlQuery m(db);
        m.exec("SELECT id,password FROM users WHERE length(password)<>64");
        QList<QPair<int,QString>> legacy;
        while(m.next()) legacy.append(qMakePair(m.value(0).toInt(),m.value(1).toString()));
        m.finish();
        for(const auto &lp : legacy){
            QSqlQuery u(db);
            u.prepare("UPDATE users SET password=? WHERE id=?");
            u.addBindValue(hashPassword(lp.second)); u.addBindValue(lp.first); u.exec();
        }
        return true;
    }
    QSqlDatabase connection() { return db; }
    void seed() {
        QSqlQuery q(db);
        q.exec("INSERT OR IGNORE INTO users(name,email,password,phone,address,role) VALUES"
               "('Administrator','admin@rental.com','admin123','0900000000','Ho Chi Minh City','Admin'),"
               "('Staff Demo','staff@rental.com','staff123','0911111111','Ho Chi Minh City','Staff'),"
               "('Customer Demo','customer@rental.com','123456','0922222222','Ho Chi Minh City','Customer')");
        q.exec("INSERT OR IGNORE INTO categories(name) VALUES('Electric Cars'),('Electric Scooters'),('Motorbike'),('4-seat car'),('7-seat car'),('Electric vehicle')");
        q.exec("INSERT OR IGNORE INTO vehicles(code,name,category,plate,make,seats,fuel,transmission,summary,price,discount,available,rental_count) VALUES"
               "('EV001','VinFast VF 8','Electric Cars','50H-00888','VinFast',5,'Electric','Automatic','SUV điện 5 chỗ, quãng đường di chuyển ấn tượng',1200000,0,1,18),"
               "('EV002','VinFast VF 9','Electric Cars','50H-00999','VinFast',7,'Electric','Automatic','SUV điện 7 chỗ rộng rãi cho gia đình',1800000,0,1,12),"
               "('ES001','VinFast Feliz S','Electric Scooters','59S1-12345','VinFast',2,'Electric','Automatic','Xe máy điện linh hoạt cho di chuyển đô thị',120000,0,1,35),"
               "('EV003','VinFast VF 8 Eco','Electric Cars','50H-00889','VinFast',5,'Electric','Automatic','Phiên bản VF 8 Eco tiện nghi',1100000,0,1,10),"
               "('EV004','VinFast VF 9 Plus','Electric Cars','50H-00998','VinFast',7,'Electric','Automatic','Phiên bản VF 9 Plus cao cấp',2000000,0,1,8),"
               "('ES002','VinFast Feliz S Plus','Electric Scooters','59S1-12346','VinFast',2,'Electric','Automatic','Xe máy điện Feliz S phiên bản nâng cấp',130000,0,1,21)");
    }
private:
    QSqlDatabase db;
};

class VehicleModel {
public:
    explicit VehicleModel(Database *d): db(d) {}
    QList<Vehicle> all(QString category="", QString keyword="", QString sort="name ASC") {
        QList<Vehicle> list;
        QSqlQuery q(db->connection());
        QString sql="SELECT id,code,name,category,plate,make,seats,fuel,transmission,summary,price,discount,available,rental_count,views FROM vehicles WHERE 1=1";
        if (!category.isEmpty() && category!="All") sql += " AND category=:cat";
        if (!keyword.isEmpty()) sql += " AND (name LIKE :kw OR make LIKE :kw OR code LIKE :kw)";
        const QStringList allowed={"name ASC","price ASC","price DESC","discount ASC","discount DESC","rental_count ASC","rental_count DESC"};
        if (!allowed.contains(sort)) sort="name ASC";
        sql += " ORDER BY "+sort;
        q.prepare(sql);
        if (!category.isEmpty() && category!="All") q.bindValue(":cat",category);
        if (!keyword.isEmpty()) q.bindValue(":kw","%"+keyword+"%");
        q.exec();
        while(q.next()){
            Vehicle v; v.id=q.value(0).toInt(); v.code=q.value(1).toString(); v.name=q.value(2).toString();
            v.category=q.value(3).toString(); v.plate=q.value(4).toString(); v.make=q.value(5).toString();
            v.seats=q.value(6).toInt(); v.fuel=q.value(7).toString(); v.transmission=q.value(8).toString();
            v.summary=q.value(9).toString(); v.price=q.value(10).toDouble(); v.discount=q.value(11).toDouble();
            v.available=q.value(12).toInt(); v.rentalCount=q.value(13).toInt(); v.views=q.value(14).toInt();
            list.append(v);
        }
        return list;
    }
    bool save(const Vehicle &v) {
        QSqlQuery q(db->connection());
        if(v.id) {
            q.prepare("UPDATE vehicles SET code=?,name=?,category=?,plate=?,make=?,seats=?,fuel=?,transmission=?,summary=?,price=?,discount=?,available=? WHERE id=?");
            q.addBindValue(v.code); q.addBindValue(v.name); q.addBindValue(v.category); q.addBindValue(v.plate);
            q.addBindValue(v.make); q.addBindValue(v.seats); q.addBindValue(v.fuel); q.addBindValue(v.transmission);
            q.addBindValue(v.summary); q.addBindValue(v.price); q.addBindValue(v.discount); q.addBindValue(v.available); q.addBindValue(v.id);
        } else {
            q.prepare("INSERT INTO vehicles(code,name,category,plate,make,seats,fuel,transmission,summary,price,discount,available) VALUES(?,?,?,?,?,?,?,?,?,?,?,?)");
            q.addBindValue(v.code); q.addBindValue(v.name); q.addBindValue(v.category); q.addBindValue(v.plate);
            q.addBindValue(v.make); q.addBindValue(v.seats); q.addBindValue(v.fuel); q.addBindValue(v.transmission);
            q.addBindValue(v.summary); q.addBindValue(v.price); q.addBindValue(v.discount); q.addBindValue(v.available);
        }
        return q.exec();
    }
    void remove(int id){ QSqlQuery q(db->connection()); q.prepare("DELETE FROM vehicles WHERE id=?"); q.addBindValue(id); q.exec(); }
private: Database *db;
};

class AccountModel {
public:
    explicit AccountModel(Database *d):db(d){}
    User login(const QString &email,const QString &password){
        QSqlQuery q(db->connection()); q.prepare("SELECT id,name,email,phone,address,role FROM users WHERE (email=? OR phone=?) AND password=?");
        q.addBindValue(email); q.addBindValue(email); q.addBindValue(hashPassword(password));
        if(!q.exec() || !q.next()) return {};
        User u; u.id=q.value(0).toInt(); u.name=q.value(1).toString(); u.email=q.value(2).toString();
        u.phone=q.value(3).toString(); u.address=q.value(4).toString(); u.role=q.value(5).toString(); return u;
    }
    bool registerUser(QString name,QString email,QString password,QString phone,QString address){
        QSqlQuery q(db->connection()); q.prepare("INSERT INTO users(name,email,password,phone,address,role) VALUES(?,?,?,?,?,'Customer')");
        q.addBindValue(name); q.addBindValue(email); q.addBindValue(hashPassword(password)); q.addBindValue(phone); q.addBindValue(address); return q.exec();
    }
    bool update(const User &u){
        QSqlQuery q(db->connection());
        q.prepare("UPDATE users SET name=?,email=?,phone=?,address=? WHERE id=?");

        q.addBindValue(u.name);
        q.addBindValue(u.email);
        q.addBindValue(u.phone);
        q.addBindValue(u.address);
        q.addBindValue(u.id);

        return q.exec();
    }

    bool reset(QString email,QString newPass){
        QSqlQuery q(db->connection()); q.prepare("UPDATE users SET password=? WHERE email=?");
        q.addBindValue(hashPassword(newPass)); q.addBindValue(email); return q.exec() && q.numRowsAffected()>0;
    }
    // Dat lai mat khau co xac minh so dien thoai
    bool resetVerified(const QString &email,const QString &phone,const QString &newPass){
        QSqlQuery q(db->connection()); q.prepare("UPDATE users SET password=? WHERE email=? AND phone=?");
        q.addBindValue(hashPassword(newPass)); q.addBindValue(email); q.addBindValue(phone);
        return q.exec() && q.numRowsAffected()>0;
    }
private: Database *db;
};

// ------------------------------------------------------------
// Kiem tra lich xe THEO NGAY (khoang ngay dong, 2 dau deu tinh).
// Xe da dat 22->25 thi 22,23,24,25 bi chan; dat tu 26 tro di van duoc.
// Chi cac don dang giu xe (New/Confirmed/Delivering/In-Rental) moi chan lich.
// Don Cancelled / Completed khong chan => huy don hoac tra xe la lich duoc giai phong.
// ------------------------------------------------------------
static const char *kActiveStatuses = "('New','Confirmed','Delivering','In-Rental')";

static QList<QPair<QDate,QDate>> bookedRanges(QSqlDatabase conn, int vehicleId){
    QList<QPair<QDate,QDate>> list;
    QSqlQuery q(conn);
    q.prepare(QString(
        "SELECT oi.pickup_date, oi.dropoff_date FROM order_items oi "
        "JOIN orders o ON o.id=oi.order_id "
        "WHERE oi.vehicle_id=? AND o.status IN %1 "
        "AND oi.pickup_date IS NOT NULL AND oi.dropoff_date IS NOT NULL "
        "ORDER BY oi.pickup_date").arg(kActiveStatuses));
    q.addBindValue(vehicleId);
    if(q.exec()){
        while(q.next()){
            QDate a = QDate::fromString(q.value(0).toString(), "yyyy-MM-dd");
            QDate b = QDate::fromString(q.value(1).toString(), "yyyy-MM-dd");
            if(a.isValid() && b.isValid()) list.append(qMakePair(a,b));
        }
    }
    return list;
}

// Tra ve khoang da dat bi trung (neu co) trong conflict
static bool vehicleFreeForDates(QSqlDatabase conn, int vehicleId,
                                const QDate &pickup, const QDate &dropoff,
                                QPair<QDate,QDate> *conflict = nullptr){
    if(!pickup.isValid() || !dropoff.isValid() || dropoff < pickup) return false;
    // Admin dat xe o trang thai "Dang thue" (available=0) => tam ngung cho thue
    QSqlQuery av(conn);
    av.prepare("SELECT available FROM vehicles WHERE id=?");
    av.addBindValue(vehicleId);
    if(av.exec() && av.next() && av.value(0).toInt()==0) return false;
    av.finish();
    const auto ranges = bookedRanges(conn, vehicleId);
    for(const auto &r : ranges){
        if(pickup <= r.second && dropoff >= r.first){
            if(conflict) *conflict = r;
            return false;
        }
    }
    return true;
}

static QString maskText(const QString &text,int keepStart,int keepEnd){
    if(text.size()<=keepStart+keepEnd) return text;
    return text.left(keepStart)+QString(text.size()-keepStart-keepEnd,QChar('*'))+text.right(keepEnd);
}
static QString categoryLabel(const QString &c){
    if(c=="Electric Cars") return "Ô tô điện";
    if(c=="Electric Scooters") return "Xe máy điện";
    if(c=="Motorbike") return "Xe máy";
    if(c=="4-seat car") return "Ô tô 4 chỗ";
    if(c=="7-seat car") return "Ô tô 7 chỗ";
    if(c=="Electric vehicle") return "Xe điện";
    return c;
}
static QString roleLabel(const QString &r){
    if(r=="Admin") return "Quản trị viên";
    if(r=="Staff") return "Nhân viên";
    return "Khách hàng";
}
static QString statusVi(const QString &st){
    if(st=="New") return "Chờ duyệt";
    if(st=="Confirmed") return "Đã xác nhận";
    if(st=="Delivering") return "Đang giao xe";
    if(st=="In-Rental") return "Đang thuê";
    if(st=="Completed") return "Hoàn thành";
    if(st=="Cancelled") return "Đã hủy";
    return st;
}
static QString vnd(double v){ return QLocale(QLocale::Vietnamese).toString((qlonglong)v)+"đ"; }
static QString handoverCode(int id){ return QString("HD%1").arg(id,3,10,QChar('0')); }

class OrderModel {
public:
    explicit OrderModel(Database *d):db(d){}
    bool create(int userId,QString recipient,QString address,QString phone,QString license,
                QString delivery,double fee,double total,QString payment,const QList<CartItem>& items,
                VehicleModel &vehicles) {
        QSqlDatabase conn=db->connection();
        if(!conn.transaction()) return false;
        QSqlQuery q(conn); q.prepare("INSERT INTO orders(user_id,recipient,address,phone,license,delivery_method,delivery_fee,total,status,payment,created_at) VALUES(?,?,?,?,?,?,?,?,'New',?,?)");
        q.addBindValue(userId); q.addBindValue(recipient); q.addBindValue(address); q.addBindValue(phone);
        q.addBindValue(license); q.addBindValue(delivery); q.addBindValue(fee); q.addBindValue(total);
        q.addBindValue(payment); q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
        if(!q.exec()){conn.rollback();return false;}
        int orderId=q.lastInsertId().toInt();
        for(const auto &item:items){
            // Kiem tra lai trong transaction de tranh dat trung ngay
            if(!vehicleFreeForDates(conn, item.vehicleId, item.pickupDate, item.dropoffDate)){
                conn.rollback(); return false;
            }
            const auto vs=vehicles.all();
            QString name; double price=0;
            for(const auto &v:vs) if(v.id==item.vehicleId){name=v.name;price=v.price*(1-v.discount/100.0);}
            QSqlQuery it(conn); it.prepare("INSERT INTO order_items(order_id,vehicle_id,vehicle_name,quantity,days,unit_price,pickup_date,dropoff_date) VALUES(?,?,?,?,?,?,?,?)");
            it.addBindValue(orderId);it.addBindValue(item.vehicleId);it.addBindValue(name);it.addBindValue(item.quantity);it.addBindValue(item.days);it.addBindValue(price);
            it.addBindValue(item.pickupDate.toString("yyyy-MM-dd"));
            it.addBindValue(item.dropoffDate.toString("yyyy-MM-dd"));
            if(!it.exec()){conn.rollback();return false;}
        }
        QSqlQuery del(conn); del.prepare("DELETE FROM cart WHERE user_id=?"); del.addBindValue(userId); del.exec();
        return conn.commit();
    }
    QSqlQueryModel *unused(){return nullptr;}
private: Database *db;
};

// ============================================================
// VIEW + CONTROLLER: Qt Widgets application
// ============================================================
class MainWindow : public QMainWindow {
public:
    MainWindow():db(),vehicleModel(&db),accountModel(&db),orderModel(&db) {
        if(!db.open()) QMessageBox::critical(this,"Database","Cannot open SQLite database.");

        setWindowTitle("VINFAST RENTAL | Vehicle Rental System");
        resize(1360,860);
        setMinimumSize(1080,700);

        // Tao giao dien chinh
        QWidget *central = new QWidget;
        QVBoxLayout *mainLayout = new QVBoxLayout(central);
        mainLayout->setContentsMargins(0,0,0,0);
        mainLayout->setSpacing(0);

        // Khu vuc cac trang
        tabs = new QTabWidget;

        buildVehiclesTab();
        buildAccountTab();
        buildCartTab();
        buildOrdersTab();
        buildStaffTab();
        buildAdminTab();
        buildPricingTab();
        buildGuideTab();


        // Tao thanh dieu huong giong giao dien VinFast
        QWidget *navBar = new QWidget;
        navBar->setFixedHeight(85);
        navBar->setStyleSheet(
            "QWidget { background-color: white; }"
            "QPushButton {"
            "   border: none;"
            "   padding: 12px 16px;"
            "   color: #263445;"
            "   font-size: 14px;"
            "}"
            "QPushButton:hover {"
            "   color: #1769E0;"
            "}"
            );

        QHBoxLayout *navLayout = new QHBoxLayout(navBar);
        navLayout->setContentsMargins(35, 0, 35, 0);
        navLayout->setSpacing(8);

        // Logo VINFAST RENTAL
        QLabel *logo = new QLabel(
            "<span style='font-size:25px; font-weight:bold; color:#172B45;'>VINFAST</span>"
            "<span style='font-size:11px; font-weight:bold; color:#1769E0;'>　RENTAL</span>"
            );
        navLayout->addWidget(logo);

        navLayout->addStretch();

        // Nut dieu huong
        auto addNavButton = [&](QString text, int pageIndex) -> QPushButton* {
            QPushButton *btn = new QPushButton(text);

            connect(btn, &QPushButton::clicked, this, [this, pageIndex]() {
                if((pageIndex==2||pageIndex==3) && !currentUser.id){
                    if(!showLoginDialog()) return;   // gio xe / don thue can dang nhap
                }
                tabs->setCurrentIndex(pageIndex);
            });

            navLayout->addWidget(btn);
            return btn;
        };


        addNavButton("Khám phá xe", 0);

        QPushButton *priceButton = new QPushButton("Bảng giá");
        navLayout->addWidget(priceButton);
        connect(priceButton, &QPushButton::clicked, this, [this]() {
            tabs->setCurrentIndex(6);
        });

        QPushButton *guideButton = new QPushButton("Hướng dẫn");
        navLayout->addWidget(guideButton);
        connect(guideButton, &QPushButton::clicked, this, [this]() {
            tabs->setCurrentIndex(7);
        });

        addNavButton("Đơn thuê của tôi", 3);
        staffNavButton = addNavButton("Quản lý thuê xe", 4);
        adminNavButton = addNavButton("Quản trị", 5);
        navLayout->addStretch();

        // Nut gio xe
        QPushButton *cartButton = new QPushButton("Giỏ xe");
        cartButton->setStyleSheet(
            "QPushButton {"
            "   background-color: #F0F5FF;"
            "   color: #1769D2;"
            "   font-weight: bold;"
            "   border-radius: 9px;"
            "   padding: 13px 22px;"
            "}"
            );

        connect(cartButton, &QPushButton::clicked, this, [this]() {
            if(!currentUser.id && !showLoginDialog()) return;
            tabs->setCurrentIndex(2);
        });

        navLayout->addWidget(cartButton);

        // Nut dang nhap
        loginButton = new QPushButton("Đăng nhập");
        loginButton->setStyleSheet(
            "QPushButton {"
            "   background-color: #1769E0;"
            "   color: white;"
            "   font-weight: bold;"
            "   border-radius: 9px;"
            "   padding: 13px 25px;"
            "}"
            "QPushButton:hover {"
            "   background-color: #0F54C5;"
            "}"
            );
        connect(loginButton, &QPushButton::clicked, this, [this]() {
            // Neu chua dang nhap thi mo popup dang nhap
            if(!currentUser.id){
                showLoginDialog();
                return;
            }

            // Neu da dang nhap thi hien menu tai khoan
            QMenu accountMenu(this);

            QAction *profileAction = accountMenu.addAction("Cập nhật thông tin cá nhân");
            QAction *logoutAction = accountMenu.addAction("Đăng xuất");

            QAction *selectedAction = accountMenu.exec(
                loginButton->mapToGlobal(
                    QPoint(0, loginButton->height())
                    )
                );

            if(selectedAction == profileAction){
                showProfileDialog();
            }
            else if(selectedAction == logoutAction){
                // Dua nguoi dung ve trang kham pha xe
                tabs->setCurrentIndex(0);

                // Xoa thong tin tai khoan hien tai
                currentUser = User{};

                // Xoa gio hang trong bo nho
                cart.clear();

                // Doi nut tai khoan thanh nut dang nhap
                loginButton->setText("Đăng nhập");

                // Cap nhat quyen hien thi
                updateRoleNavigation();

                // Lam moi giao dien
                refreshCart();
                refreshOrders();

                info("Bạn đã đăng xuất thành công.");
            }
        });


        navLayout->addWidget(loginButton);


        // An thanh tab mac dinh
        tabs->tabBar()->hide();
        // An trang Tai khoan cu
        tabs->setTabVisible(1, false);
        // Sap xep giao dien
        mainLayout->addWidget(navBar);
        mainLayout->addWidget(tabs, 1);

        setCentralWidget(central);
        updateRoleNavigation();
        refreshVehicles();
        refreshCart();
        refreshOrders();
        refreshAdmin();
        connect(tabs,&QTabWidget::currentChanged,this,[=](int i){
            if(i==0) refreshVehicles(); else if(i==2) refreshCart();
            else if(i==3||i==4) refreshOrders(); else if(i==5) refreshAdmin(); else if(i==6) refreshPricing();
        });
        refreshPricing();

    }

private:
    bool isRefreshingVehicles = false;
    Database db; VehicleModel vehicleModel; AccountModel accountModel; OrderModel orderModel;
    User currentUser;
    QList<CartItem> cart;
    QTabWidget *tabs{};
    QWidget *vehicleCardsContainer{};
    QGridLayout *vehicleCardsLayout{};
    int selectedVehicleId = 0;
    QTableWidget *vehicleTable{},*cartTable{},*ordersTable{},*staffTable{},*adminVehicleTable{},*usersTable{},*adminOrdersTable{};
    QLineEdit *searchEdit{},*loginEmail{},*loginPassword{},*recipientEdit{},*addressEdit{},*phoneEdit{},*licenseEdit{};
    QComboBox *categoryBox{},*sortBox{},*paymentBox{},*deliveryBox{},*statusBox{};
    // Bộ lọc giá thuê
    QRadioButton *under300{}, *from300to1000{}, *over1000{};
    // Bộ lọc số chỗ
    QPushButton *seat5{}, *seat7{};
    QLabel *loginLabel{},*totalLabel{};
    // Giao diện trang đơn thuê của tôi
    QLabel *ordersTotalCount{};
    QLabel *ordersPendingCount{};
    QLabel *ordersCompletedCount{};
    QString ordersFilter = "all";
    QPushButton *loginButton{};
    QPushButton *staffNavButton{};
    QPushButton *adminNavButton{};
    QSpinBox *daysSpin{},*qtySpin{};
    QDoubleSpinBox *priceSpin{},*discountSpin{};
    QLineEdit *codeEdit{},*nameEdit{},*plateEdit{},*makeEdit{},*summaryEdit{};
    QComboBox *adminCategory{},*fuelBox{},*transmissionBox{};
    int editingVehicleId=0;

    // ===== Ban giao & tiep nhan xe (FR-12) =====
    struct HandoverUi {
        QWidget *body{}; QLabel *empty{};
        QLineEdit *search{};
        QPushButton *modeHandover{}, *modeReturn{};
        QLabel *code{}, *badge{}, *customer{}, *phone{}, *vehicle{}, *plate{}, *period{}, *deposit{};
        QLabel *checkTitle{}, *checkDate{}, *progress{}, *hint{};
        QCheckBox *chk[3]{}; QLabel *desc[3]{};
        QCheckBox *confirmChk{};
        QSpinBox *battery{};   // mac dinh 96% (khong hien tren giao dien)
        QLineEdit *odoEdit{}; QFrame *odoFrame{}; QTextEdit *note{};
        QPushButton *save{}, *confirm{};
    } ho;
    int hoOrderId=0; bool hoReturnMode=false; QString hoStatus; int hoHandoverOdo=0;
    QTabWidget *staffInner{};
    QTableWidget *pricingTable{};
    int pricingFilter=0;

    // ===== Quan tri: FR-10 (doi xe & gia) va FR-11 (nguoi dung & thong ke) =====
    QStackedWidget *adminStack{};
    QButtonGroup *adminNavGroup{};
    struct StatSet { QLabel *revenue{}, *revenueSub{}, *rented{}, *rentedSub{}, *pending{}, *pendingSub{}; };
    StatSet adminStats[2];
    QLabel *fleetNote{};
    QLineEdit *fleetSearch{}, *usersSearch{};
    QComboBox *fleetStatus{}, *userRoleBox{}, *statusEditBox{};
    QLabel *fleetInfo{}, *panelTitle{}, *panelSub{};
    QHBoxLayout *fleetPager{};
    QWidget *panelBody{}, *extraFields{};
    QPushButton *panelDelete{};
    QSpinBox *seatsSpin{};
    QTableWidget *reportStatusTable{}, *reportTopTable{}, *reportMonthTable{}, *recentOrdersTable{};
    int fleetPage=1;
    QList<Vehicle> fleetShown;
    QSet<int> rentedIds;
    Vehicle editVehicle; bool addingVehicle=false; int editInitialStatus=0;
    static constexpr int kFleetPageSize=5;

    void info(QString s){QMessageBox::information(this,"Vehicle Rental System",s);}
    void error(QString s){QMessageBox::warning(this,"Validation",s);}
    bool showLoginDialog(){
        // Tạo lớp nền tối phía sau popup
        QWidget *overlay = new QWidget(centralWidget());
        overlay->setGeometry(centralWidget()->rect());
        overlay->setStyleSheet("background-color: rgba(15, 35, 60, 155);");
        overlay->show();
        overlay->raise();

        // Tạo cửa sổ đăng nhập
        QDialog d(this);
        d.setWindowTitle("Đăng nhập VinFast Rental");
        d.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        d.setModal(true);
        d.setFixedSize(520, 700);

        d.setStyleSheet(R"(
        QDialog {
            background: white;
            border-radius: 15px;
        }
        QLabel {
            color: #172b4d;
        }
        QLabel#loginEyebrow {
            color: #1767e8;
            font-size: 9pt;
            font-weight: bold;
        }
        QLabel#loginTitle {
            font-size: 23pt;
            font-weight: bold;
        }
        QLabel#loginHint {
            color: #718096;
        }
        QLineEdit {
            background: #f8fafc;
            border: 1px solid #dce3ef;
            border-radius: 8px;
            padding: 12px;
            min-height: 25px;
        }
        QLineEdit:focus {
            border: 1px solid #1767e8;
        }
        QPushButton#loginButton {
            background: #1767e8;
            color: white;
            border-radius: 8px;
            padding: 13px;
            font-weight: bold;
        }
        QPushButton#loginButton:hover {
            background: #0f54c7;
        }
        QPushButton#closeButton {
            background: #f1f5f9;
            color: #475569;
            font-size: 20pt;
            font-weight: bold;
            border: none;
            border-radius: 15px;
        }
        QPushButton#closeButton:hover {
            background: #fee2e2;
            color: #dc2626;
        }
        QPushButton#registerButton {
            background: transparent;
            color: #1767e8;
        }
        QLabel#loginError {
            color: #dc2626;
        }
        QLabel#loginNote {
            background: #eef3ff;
            border-radius: 8px;
            padding: 12px;
            color: #40536e;
        }
    )");

        auto *outer = new QVBoxLayout(&d);
        outer->setContentsMargins(35, 25, 35, 30);
        outer->setSpacing(12);

        // Tiêu đề và nút đóng
        auto *top = new QHBoxLayout;

        auto *eyebrow = new QLabel("ĐĂNG NHẬP VINFAST RENTAL");
        eyebrow->setObjectName("loginEyebrow");

        auto *close = new QPushButton("×");
        close->setObjectName("closeButton");
        close->setFixedSize(34, 34);
        close->setCursor(Qt::PointingHandCursor);
        close->setToolTip("Đóng cửa sổ đăng nhập");

        top->addWidget(eyebrow);
        top->addStretch();
        top->addWidget(close);

        outer->addLayout(top);

        auto *title = new QLabel("Chào mừng trở lại");
        title->setObjectName("loginTitle");

        auto *hint = new QLabel("Đăng nhập để tiếp tục hành trình của bạn.");
        hint->setObjectName("loginHint");

        outer->addWidget(title);
        outer->addWidget(hint);
        outer->addSpacing(15);

        // Ô nhập tài khoản
        auto *emailLabel = new QLabel("Số điện thoại / Email");

        auto *email = new QLineEdit;
        email->setPlaceholderText("Nhập thông tin đăng nhập");

        // Ô nhập mật khẩu
        auto *passwordLabel = new QLabel("Mật khẩu");

        auto *password = new QLineEdit;
        password->setPlaceholderText("Nhập mật khẩu");
        password->setEchoMode(QLineEdit::Password);

        auto *loginError = new QLabel("");
        loginError->setObjectName("loginError");

        outer->addWidget(emailLabel);
        outer->addWidget(email);
        outer->addSpacing(5);
        outer->addWidget(passwordLabel);
        outer->addWidget(password);
        outer->addWidget(loginError);

        auto *forgotButton = new QPushButton("Quên mật khẩu?");
        forgotButton->setStyleSheet("QPushButton{background:transparent;color:#1767e8;border:none;text-align:left;padding:2px;}");
        forgotButton->setCursor(Qt::PointingHandCursor);
        outer->addWidget(forgotButton, 0, Qt::AlignLeft);
        connect(forgotButton, &QPushButton::clicked, &d, [&](){ showResetPasswordDialog(); });

        outer->addSpacing(10);

        // Nút đăng nhập
        auto *login = new QPushButton("Đăng nhập  →");
        login->setObjectName("loginButton");
        login->setMinimumHeight(45);

        outer->addWidget(login);

        // Đăng ký
        auto *registerRow = new QHBoxLayout;
        auto *registerText = new QLabel("Chưa có tài khoản?");
        auto *registerButton = new QPushButton("Đăng ký ngay");
        registerButton->setObjectName("registerButton");

        registerRow->addStretch();
        registerRow->addWidget(registerText);
        registerRow->addWidget(registerButton);
        registerRow->addStretch();

        outer->addLayout(registerRow);
        outer->addStretch();

        auto *note = new QLabel(
            "Một tài khoản, đúng không gian làm việc.\n"
            "Tự động điều hướng theo vai trò sau đăng nhập."
            );
        note->setObjectName("loginNote");
        note->setWordWrap(true);
        note->setMinimumHeight(78);
        outer->addWidget(note);

        // Đóng popup
        connect(close, &QPushButton::clicked, &d, &QDialog::reject);

        // Đăng nhập
        connect(login, &QPushButton::clicked, &d, [&]{
            User user = accountModel.login(
                email->text().trimmed(),
                password->text()
                );

            if(!user.id){
                loginError->setText("Thông tin đăng nhập không chính xác.");
                return;
            }

            currentUser = user;

            // Hien thi ten tai khoan tren thanh dieu huong
            loginButton->setText(currentUser.name);

            loginLabel->setText(
                currentUser.name + " (" + currentUser.role + ")"
                );
            updateRoleNavigation();
            if(currentUser.role == "Admin")
                tabs->setCurrentIndex(5);
            else if(currentUser.role == "Staff")
                tabs->setCurrentIndex(4);
            else
                tabs->setCurrentIndex(0);

            refreshOrders();
            refreshCart();

            d.accept();
        });

        // Đăng ký: chuyển sang chức năng đăng ký hiện có
        connect(registerButton, &QPushButton::clicked, &d, [&](){
            showRegisterDialog();
        });

        // Căn giữa popup
        QPoint center = mapToGlobal(rect().center());
        d.move(center.x() - d.width()/2,
               center.y() - d.height()/2);

        d.exec();

        overlay->deleteLater();

        return currentUser.id != 0;
    }
    void showProfileDialog(){
        if(!currentUser.id){
            error("Vui lòng đăng nhập trước.");
            return;
        }

        QDialog d(this);
        d.setWindowTitle("Cập nhật thông tin cá nhân");
        d.setFixedSize(500, 560);
        d.setStyleSheet(R"(
        QDialog {
            background: white;
        }
        QLabel {
            color: #172b4d;
            font-size: 10pt;
        }
        QLineEdit {
            background: #f8fafc;
            border: 1px solid #dce3ef;
            border-radius: 8px;
            padding: 11px;
            min-height: 22px;
        }
        QLineEdit:focus {
            border: 1px solid #1767e8;
        }
        QPushButton {
            border-radius: 8px;
            padding: 11px 18px;
        }
        QPushButton#saveProfile {
            background: #1767e8;
            color: white;
            font-weight: bold;
        }
        QPushButton#cancelProfile {
            background: #f1f5f9;
            color: #475569;
        }
    )");


        auto *layout = new QVBoxLayout(&d);
        layout->setContentsMargins(28, 25, 28, 25);
        layout->setSpacing(12);

        auto *title = new QLabel("Thông tin cá nhân");
        title->setStyleSheet(
            "font-size: 20pt; font-weight: bold; color: #172b4d;"
            );

        auto *hint = new QLabel("Cập nhật thông tin tài khoản của bạn.");

        auto *form = new QFormLayout;

        auto *nameInput = new QLineEdit(currentUser.name);
        auto *emailInput = new QLineEdit(currentUser.email);
        auto *phoneInput = new QLineEdit(currentUser.phone);
        auto *addressInput = new QLineEdit(currentUser.address);

        form->addRow("Họ và tên:", nameInput);
        form->addRow("Email:", emailInput);
        form->addRow("Số điện thoại:", phoneInput);
        form->addRow("Địa chỉ:", addressInput);

        auto *oldPassInput = new QLineEdit; oldPassInput->setEchoMode(QLineEdit::Password);
        oldPassInput->setPlaceholderText("Chỉ nhập khi muốn đổi mật khẩu");
        auto *newPassInput = new QLineEdit; newPassInput->setEchoMode(QLineEdit::Password);
        newPassInput->setPlaceholderText("Tối thiểu 6 ký tự");
        form->addRow("Mật khẩu hiện tại:", oldPassInput);
        form->addRow("Mật khẩu mới:", newPassInput);

        auto *errorLabel = new QLabel("");
        errorLabel->setStyleSheet("color: #dc2626;");

        auto *buttons = new QHBoxLayout;
        auto *cancel = new QPushButton("Hủy");
        auto *save = new QPushButton("Lưu thay đổi");

        cancel->setObjectName("cancelProfile");
        save->setObjectName("saveProfile");

        buttons->addStretch();
        buttons->addWidget(cancel);
        buttons->addWidget(save);

        layout->addWidget(title);
        layout->addWidget(hint);
        layout->addSpacing(8);
        layout->addLayout(form);
        layout->addWidget(errorLabel);
        layout->addStretch();
        layout->addLayout(buttons);

        connect(cancel, &QPushButton::clicked, &d, &QDialog::reject);

        connect(save, &QPushButton::clicked, &d, [&](){
            User updatedUser = currentUser;

            updatedUser.name = nameInput->text().trimmed();
            updatedUser.email = emailInput->text().trimmed();
            updatedUser.phone = phoneInput->text().trimmed();
            updatedUser.address = addressInput->text().trimmed();

            if(updatedUser.name.isEmpty() ||
                updatedUser.email.isEmpty()){
                errorLabel->setText("Họ tên và email không được để trống.");
                return;
            }
            if(!validEmail(updatedUser.email)){
                errorLabel->setText("Email không hợp lệ.");
                return;
            }
            if(!updatedUser.phone.isEmpty() && !validPhone(updatedUser.phone)){
                errorLabel->setText("Số điện thoại phải gồm 9–12 chữ số.");
                return;
            }

            // Doi mat khau (tuy chon)
            const QString newPass = newPassInput->text();
            if(!newPass.isEmpty() || !oldPassInput->text().isEmpty()){
                if(accountModel.login(currentUser.email, oldPassInput->text()).id != currentUser.id){
                    errorLabel->setText("Mật khẩu hiện tại không đúng.");
                    return;
                }
                if(newPass.length() < 6){
                    errorLabel->setText("Mật khẩu mới phải có ít nhất 6 ký tự.");
                    return;
                }
            }

            if(accountModel.update(updatedUser)){
                currentUser = updatedUser;
                if(!newPass.isEmpty()) accountModel.reset(currentUser.email, newPass);

                loginButton->setText(currentUser.name);
                loginLabel->setText(
                    currentUser.name + " (" + currentUser.role + ")"
                    );

                info("Cập nhật thông tin thành công.");
                d.accept();
            }
            else {
                errorLabel->setText(
                    "Không thể cập nhật. Email có thể đã được sử dụng."
                    );
            }
        });

        d.exec();
    }
    void updateRoleNavigation(){
        bool isStaff =
            currentUser.role == "Staff" ||
            currentUser.role == "Admin";

        bool isAdmin =
            currentUser.role == "Admin";

        // An/hien nut tren thanh dieu huong
        if(staffNavButton){
            staffNavButton->setVisible(isStaff);
        }

        if(adminNavButton){
            adminNavButton->setVisible(isAdmin);
        }

        // An/hien cac trang tuong ung
        if(tabs){
            tabs->setTabVisible(4, isStaff);
            tabs->setTabVisible(5, isAdmin);
        }
    }
    void showRegisterDialog(){
        QDialog d(this);
        d.setWindowTitle("Đăng ký VINFAST RENTAL");
        d.setFixedSize(460, 520);

        d.setStyleSheet(R"(
        QDialog {
            background: white;
        }
        QLabel {
            color: #172b4d;
            font-size: 10pt;
        }
        QLineEdit {
            background: #f8fafc;
            border: 1px solid #dce3ef;
            border-radius: 8px;
            padding: 11px;
            min-height: 22px;
        }
        QLineEdit:focus {
            border: 1px solid #1767e8;
        }
        QPushButton {
            border-radius: 8px;
            padding: 12px;
        }
        QPushButton#registerSubmit {
            background: #1767e8;
            color: white;
            font-weight: bold;
        }
        QPushButton#registerCancel {
            background: #f1f5f9;
            color: #475569;
        }
    )");

        auto *layout = new QVBoxLayout(&d);
        layout->setContentsMargins(30, 25, 30, 25);
        layout->setSpacing(12);

        auto *title = new QLabel("Tạo tài khoản mới");
        title->setStyleSheet(
            "font-size: 21pt; font-weight: bold; color: #172b4d;"
            );

        auto *hint = new QLabel(
            "Đăng ký để bắt đầu hành trình cùng VINFAST RENTAL."
            );

        auto *form = new QFormLayout;

        auto *name = new QLineEdit;
        auto *email = new QLineEdit;
        auto *password = new QLineEdit;
        auto *phone = new QLineEdit;
        auto *address = new QLineEdit;

        password->setEchoMode(QLineEdit::Password);

        name->setPlaceholderText("Nhập họ và tên");
        email->setPlaceholderText("example@gmail.com");
        password->setPlaceholderText("Nhập mật khẩu");
        phone->setPlaceholderText("Nhập số điện thoại");
        address->setPlaceholderText("Nhập địa chỉ");

        form->addRow("Họ và tên:", name);
        form->addRow("Email:", email);
        form->addRow("Mật khẩu:", password);
        form->addRow("Số điện thoại:", phone);
        form->addRow("Địa chỉ:", address);

        auto *errorLabel = new QLabel("");
        errorLabel->setStyleSheet("color: #dc2626;");

        auto *registerBtn = new QPushButton("Tạo tài khoản");
        registerBtn->setObjectName("registerSubmit");

        auto *cancelBtn = new QPushButton("Hủy");
        cancelBtn->setObjectName("registerCancel");

        auto *buttons = new QHBoxLayout;
        buttons->addWidget(cancelBtn);
        buttons->addWidget(registerBtn);

        layout->addWidget(title);
        layout->addWidget(hint);
        layout->addSpacing(8);
        layout->addLayout(form);
        layout->addWidget(errorLabel);
        layout->addStretch();
        layout->addLayout(buttons);

        connect(cancelBtn, &QPushButton::clicked,
                &d, &QDialog::reject);

        connect(registerBtn, &QPushButton::clicked, &d, [&](){

            QString n = name->text().trimmed();
            QString e = email->text().trimmed();
            QString p = password->text();
            QString ph = phone->text().trimmed();
            QString ad = address->text().trimmed();

            if(n.isEmpty() || e.isEmpty() ||
                p.isEmpty() || ph.isEmpty()){
                errorLabel->setText(
                    "Vui lòng nhập đầy đủ họ tên, email, mật khẩu và số điện thoại."
                    );
                return;
            }

            if(!validEmail(e)){
                errorLabel->setText("Email không hợp lệ.");
                return;
            }

            if(!validPhone(ph)){
                errorLabel->setText("Số điện thoại phải gồm 9–12 chữ số.");
                return;
            }

            if(p.length() < 6){
                errorLabel->setText(
                    "Mật khẩu phải có ít nhất 6 ký tự."
                    );
                return;
            }

            if(accountModel.registerUser(n, e, p, ph, ad)){
                info("Đăng ký tài khoản thành công. Bạn có thể đăng nhập ngay.");
                d.accept();
            }
            else{
                errorLabel->setText(
                    "Đăng ký thất bại. Email có thể đã được sử dụng."
                    );
            }
        });

        QPoint center = mapToGlobal(rect().center());
        d.move(center.x() - d.width()/2,
               center.y() - d.height()/2);

        d.exec();
    }
    void showResetPasswordDialog(){
        QDialog d(this);
        d.setWindowTitle("Quên mật khẩu");
        d.setFixedSize(460, 470);
        d.setStyleSheet(R"(
            QDialog { background:white; }
            QLabel { color:#172b4d; }
            QLineEdit { background:#f8fafc; border:1px solid #dce3ef; border-radius:8px; padding:11px; min-height:22px; }
            QLineEdit:focus { border:1px solid #1767e8; }
            QPushButton { border-radius:8px; padding:12px; }
            QPushButton#ok { background:#1767e8; color:white; font-weight:bold; }
            QPushButton#cancel { background:#f1f5f9; color:#475569; }
        )");
        auto *layout = new QVBoxLayout(&d);
        layout->setContentsMargins(30,25,30,25); layout->setSpacing(10);
        auto *title = new QLabel("Đặt lại mật khẩu");
        title->setStyleSheet("font-size:20pt;font-weight:bold;");
        auto *hint = new QLabel("Nhập email và số điện thoại đã đăng ký để xác minh tài khoản.");
        hint->setWordWrap(true); hint->setStyleSheet("color:#718096;");
        auto *email = new QLineEdit; email->setPlaceholderText("Email đã đăng ký");
        auto *phone = new QLineEdit; phone->setPlaceholderText("Số điện thoại đã đăng ký");
        auto *pass1 = new QLineEdit; pass1->setPlaceholderText("Mật khẩu mới (tối thiểu 6 ký tự)"); pass1->setEchoMode(QLineEdit::Password);
        auto *pass2 = new QLineEdit; pass2->setPlaceholderText("Nhập lại mật khẩu mới"); pass2->setEchoMode(QLineEdit::Password);
        auto *err = new QLabel; err->setStyleSheet("color:#dc2626;"); err->setWordWrap(true);
        auto *row = new QHBoxLayout;
        auto *cancel = new QPushButton("Hủy"); cancel->setObjectName("cancel");
        auto *ok = new QPushButton("Đặt lại mật khẩu"); ok->setObjectName("ok");
        row->addWidget(cancel); row->addWidget(ok);
        layout->addWidget(title); layout->addWidget(hint); layout->addSpacing(6);
        layout->addWidget(email); layout->addWidget(phone); layout->addWidget(pass1); layout->addWidget(pass2);
        layout->addWidget(err); layout->addStretch(); layout->addLayout(row);
        connect(cancel,&QPushButton::clicked,&d,&QDialog::reject);
        connect(ok,&QPushButton::clicked,&d,[&](){
            if(!validEmail(email->text().trimmed())){ err->setText("Email không hợp lệ."); return; }
            if(pass1->text().length()<6){ err->setText("Mật khẩu mới phải có ít nhất 6 ký tự."); return; }
            if(pass1->text()!=pass2->text()){ err->setText("Hai mật khẩu nhập không khớp."); return; }
            if(accountModel.resetVerified(email->text().trimmed(),phone->text().trimmed(),pass1->text())){
                info("Đặt lại mật khẩu thành công. Bạn có thể đăng nhập bằng mật khẩu mới.");
                d.accept();
            } else err->setText("Email và số điện thoại không khớp với tài khoản nào.");
        });
        d.exec();
    }

    // Chi tiet don thue cua khach hang
    void showOrderDetails(int id){
        QSqlQuery q(db.connection());
        q.prepare("SELECT status,total,payment,created_at,recipient,phone FROM orders WHERE id=? AND user_id=?");
        q.addBindValue(id); q.addBindValue(currentUser.id);
        if(!q.exec()||!q.next()){ error("Không tìm thấy đơn thuê."); return; }
        const QString status=q.value(0).toString();
        const double total=q.value(1).toDouble();
        QString text=QString("Mã đơn: %1\nTrạng thái: %2\nNgười thuê: %3 (%4)\nNgày tạo: %5\nThanh toán: %6\n\nXe thuê:\n")
            .arg(handoverCode(id),statusVi(status),q.value(4).toString(),q.value(5).toString(),
                 QDateTime::fromString(q.value(3).toString(),Qt::ISODate).toString("dd/MM/yyyy HH:mm"),q.value(2).toString());
        q.finish();
        QSqlQuery it(db.connection());
        it.prepare("SELECT vehicle_name,pickup_date,dropoff_date,days,unit_price FROM order_items WHERE order_id=?");
        it.addBindValue(id); it.exec();
        while(it.next()){
            QDate a=QDate::fromString(it.value(1).toString(),"yyyy-MM-dd"), b=QDate::fromString(it.value(2).toString(),"yyyy-MM-dd");
            text+="  • "+it.value(0).toString()+"\n    "+(a.isValid()&&b.isValid()
                   ? a.toString("dd/MM/yyyy")+" → "+b.toString("dd/MM/yyyy")+" ("+QString::number(it.value(3).toInt())+" ngày)"
                   : QString::number(it.value(3).toInt())+" ngày")
                 +" · "+vnd(it.value(4).toDouble())+"/ngày\n";
        }
        text+="\nTổng tiền: "+vnd(total)+"\nĐã đặt cọc 50%: "+vnd(total*0.5)+"\nThanh toán khi nhận xe: "+vnd(total*0.5);
        QMessageBox::information(this,"Chi tiết đơn thuê "+handoverCode(id),text);
    }

    void cancelMyOrder(int id){
        if(QMessageBox::question(this,"Xác nhận hủy đơn","Bạn có chắc chắn muốn hủy đơn "+handoverCode(id)+"?\nXe sẽ được trả lại lịch cho người khác đặt.")!=QMessageBox::Yes) return;
        QSqlQuery c(db.connection());
        c.prepare("UPDATE orders SET status='Cancelled' WHERE id=? AND user_id=? AND status IN ('New','Confirmed')");
        c.addBindValue(id); c.addBindValue(currentUser.id);
        if(c.exec() && c.numRowsAffected()>0) info("Đã hủy đơn thuê. Lịch xe đã được giải phóng.");
        else error("Không thể hủy đơn này (đơn đã được bàn giao hoặc đã kết thúc).");
        refreshOrders(); refreshVehicles();
    }

    // ------------------------------------------------------------
    // BANG GIA & HUONG DAN
    // ------------------------------------------------------------
    static QString publicPageStyle(){
        return R"(
            QWidget#pgPage { background:#F4F7FC; }
            QFrame#pgCard { background:white; border-radius:12px; }
            QFrame#pgHero { background:#15365E; border-radius:13px; }
            QLabel#pgCrumb { color:#1769E0; font-size:9pt; font-weight:bold; }
            QLabel#pgTitle { color:#172B45; font-size:25pt; font-weight:bold; }
            QLabel#pgMuted { color:#718096; }
            QLabel#pgCardTitle { color:#172B45; font-size:13pt; font-weight:bold; }
            QLabel#pgStepNo { color:#1769E0; font-size:24pt; font-weight:bold; }
            QLabel#pgHeroTitle { color:white; font-size:22pt; font-weight:bold; }
            QLabel#pgHeroText { color:#D6E4FA; }
            QLabel#pgQuestion { color:#172B45; font-size:11pt; font-weight:bold; }
            QPushButton#pgChip { background:#EDF3FF; color:#1769E0; border:none; border-radius:8px; padding:11px 22px; font-weight:bold; }
            QPushButton#pgChip:checked { background:#1769E0; color:white; }
            QPushButton#pgPrimary { background:#1769E0; color:white; border:none; border-radius:8px; padding:13px 26px; font-weight:bold; }
            QPushButton#pgPrimary:hover { background:#0F54C5; }
            QPushButton#pgLink { background:transparent; color:#1769E0; border:none; padding:6px 10px; font-weight:bold; }
            QPushButton#pgLink:hover { text-decoration:underline; }
            QTableWidget#pgTable { background:white; border:none; selection-background-color:#EDF3FF; selection-color:#172B45; }
            QTableWidget#pgTable::item { padding:6px; border-bottom:1px solid #EEF2F7; }
            QHeaderView::section { background:white; color:#718096; border:none; border-bottom:1px solid #EEF2F7; padding:10px 6px; font-size:8pt; font-weight:bold; }
        )";
    }

    QFrame* pgInfoCard(const QString &title,const QString &text){
        auto *card=new QFrame; card->setObjectName("pgCard");
        auto *l=new QVBoxLayout(card); l->setContentsMargins(20,18,20,18); l->setSpacing(6);
        auto *t=new QLabel(title); t->setObjectName("pgCardTitle");
        auto *d=new QLabel(text); d->setObjectName("pgMuted"); d->setWordWrap(true);
        l->addWidget(t); l->addWidget(d); l->addStretch();
        return card;
    }

    void buildPricingTab(){
        auto *page=new QWidget; page->setObjectName("pgPage"); page->setStyleSheet(publicPageStyle());
        auto *v=new QVBoxLayout(page); v->setContentsMargins(35,26,35,26); v->setSpacing(16);
        auto *crumb=new QLabel("BẢNG GIÁ"); crumb->setObjectName("pgCrumb");
        auto *title=new QLabel("Giá thuê xe điện minh bạch, rõ ràng"); title->setObjectName("pgTitle");
        auto *sub=new QLabel("Giá được cập nhật trực tiếp từ đội xe. Nhận và trả xe tại trạm, không phụ phí giao nhận."); sub->setObjectName("pgMuted");
        v->addWidget(crumb); v->addWidget(title); v->addWidget(sub);

        auto *chips=new QHBoxLayout;
        auto *group=new QButtonGroup(page); group->setExclusive(true);
        const QStringList names={"Tất cả xe","Ô tô điện","Xe máy điện"};
        for(int i=0;i<names.size();++i){
            auto *b=new QPushButton(names[i]); b->setObjectName("pgChip"); b->setCheckable(true);
            group->addButton(b); chips->addWidget(b);
            if(i==0) b->setChecked(true);
            connect(b,&QPushButton::clicked,this,[=]{ pricingFilter=i; refreshPricing(); });
        }
        chips->addStretch();
        v->addLayout(chips);

        auto *card=new QFrame; card->setObjectName("pgCard");
        auto *cl=new QVBoxLayout(card); cl->setContentsMargins(20,20,20,20);
        pricingTable=new QTableWidget(0,8);
        pricingTable->setObjectName("pgTable");
        pricingTable->setHorizontalHeaderLabels({"PHƯƠNG TIỆN","LOẠI XE","SỐ CHỖ","GIÁ NIÊM YẾT / NGÀY","ƯU ĐÃI","GIÁ THUÊ / NGÀY","THUÊ 3 NGÀY",""});
        pricingTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        pricingTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft|Qt::AlignVCenter);
        pricingTable->verticalHeader()->hide();
        pricingTable->verticalHeader()->setDefaultSectionSize(58);
        pricingTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        pricingTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        pricingTable->setFrameShape(QFrame::NoFrame);
        pricingTable->setShowGrid(false);
        pricingTable->setMinimumHeight(430);
        cl->addWidget(pricingTable);
        v->addWidget(card,1);

        auto *info=new QHBoxLayout; info->setSpacing(16);
        info->addWidget(pgInfoCard("Cách tính giá","Số ngày thuê = ngày trả − ngày nhận. Tổng tiền = giá thuê / ngày × số ngày thuê."));
        info->addWidget(pgInfoCard("Đặt cọc 50%","Thanh toán 50% tổng tiền khi đặt xe qua VNPay hoặc quét mã QR. Phần còn lại thanh toán khi nhận xe."));
        info->addWidget(pgInfoCard("Nhận xe tại trạm","Không phát sinh phí giao nhận khi nhận và trả xe trực tiếp tại trạm."));
        v->addLayout(info);

        tabs->addTab(wrapScroll(page),"Bảng giá");
    }

    void refreshPricing(){
        if(!pricingTable) return;
        auto vs=vehicleModel.all("","","price ASC");
        pricingTable->setRowCount(0);
        for(const Vehicle &veh : vs){
            if(pricingFilter==1 && veh.category!="Electric Cars") continue;
            if(pricingFilter==2 && veh.category!="Electric Scooters") continue;
            const double price=veh.price*(1-veh.discount/100.0);
            const int r=pricingTable->rowCount(); pricingTable->insertRow(r);
            setCell(pricingTable,r,0,veh.name,"",true);
            setCell(pricingTable,r,1,categoryLabel(veh.category));
            setCell(pricingTable,r,2,QString::number(veh.seats)+" chỗ");
            setCell(pricingTable,r,3,vnd(veh.price));
            setCell(pricingTable,r,4,veh.discount>0 ? "-"+QString::number(veh.discount,'f',0)+"%" : QString("—"), veh.discount>0 ? "#16865A" : "");
            setCell(pricingTable,r,5,vnd(price),"#1769E0",true);
            setCell(pricingTable,r,6,vnd(price*3));
            if(veh.available){
                auto *btn=new QPushButton("Thuê ngay  →"); btn->setObjectName("pgLink"); btn->setCursor(Qt::PointingHandCursor);
                const int vid=veh.id;
                connect(btn,&QPushButton::clicked,this,[=]{ selectedVehicleId=vid; showVehicleDetails(); });
                pricingTable->setCellWidget(r,7,btn);
            } else {
                setCell(pricingTable,r,7,"Tạm ngừng","#B45309");
            }
        }
    }

    void buildGuideTab(){
        auto *page=new QWidget; page->setObjectName("pgPage"); page->setStyleSheet(publicPageStyle());
        auto *v=new QVBoxLayout(page); v->setContentsMargins(35,26,35,26); v->setSpacing(18);

        // Banner
        auto *hero=new QFrame; hero->setObjectName("pgHero");
        auto *hl=new QHBoxLayout(hero); hl->setContentsMargins(30,26,30,26);
        auto *ht=new QVBoxLayout;
        auto *hc=new QLabel("HƯỚNG DẪN THUÊ XE"); hc->setStyleSheet("color:#B8D2FF;font-size:9pt;font-weight:bold;");
        auto *htitle=new QLabel("Thuê xe điện chỉ trong 4 bước"); htitle->setObjectName("pgHeroTitle");
        auto *htext=new QLabel("Chọn xe, đặt cọc online và nhận xe tại trạm. Mọi thao tác đều rõ ràng, minh bạch."); htext->setObjectName("pgHeroText");
        ht->addWidget(hc); ht->addWidget(htitle); ht->addWidget(htext);
        auto *go=new QPushButton("Khám phá xe ngay  →"); go->setObjectName("pgPrimary");
        go->setStyleSheet("QPushButton{background:white;color:#15365E;border:none;border-radius:8px;padding:13px 26px;font-weight:bold;}QPushButton:hover{background:#EAF2FF;}");
        connect(go,&QPushButton::clicked,this,[=]{ tabs->setCurrentIndex(0); });
        hl->addLayout(ht,1); hl->addWidget(go,0,Qt::AlignVCenter);
        v->addWidget(hero);

        // 4 buoc
        auto *steps=new QHBoxLayout; steps->setSpacing(16);
        const QList<QPair<QString,QString>> stepTexts={
            {"Chọn xe & lịch thuê","Tìm xe theo loại, giá, số chỗ. Mở trang chi tiết, chọn ngày nhận và ngày trả. Ngày đã có người đặt được tô đỏ trên lịch."},
            {"Đăng nhập & xác minh","Đăng nhập hoặc tạo tài khoản. Khi thanh toán, nhập số GPLX và tải ảnh mặt trước, mặt sau GPLX."},
            {"Đặt cọc 50%","Thanh toán 50% tổng tiền bằng VNPay hoặc quét mã QR. Theo dõi trạng thái trong mục Đơn thuê của tôi."},
            {"Nhận & trả xe tại trạm","Mang CCCD và GPLX bản gốc. Nhân viên kiểm tra pin, ngoại thất, giấy tờ, ghi số ODO rồi bàn giao. Trả xe đúng ngày để hoàn tất đơn."}
        };
        for(int i=0;i<stepTexts.size();++i){
            auto *card=new QFrame; card->setObjectName("pgCard");
            auto *l=new QVBoxLayout(card); l->setContentsMargins(20,20,20,20); l->setSpacing(8);
            auto *no=new QLabel(QString("%1").arg(i+1,2,10,QChar('0'))); no->setObjectName("pgStepNo");
            auto *t=new QLabel(stepTexts[i].first); t->setObjectName("pgCardTitle"); t->setWordWrap(true);
            auto *d=new QLabel(stepTexts[i].second); d->setObjectName("pgMuted"); d->setWordWrap(true);
            l->addWidget(no); l->addWidget(t); l->addWidget(d); l->addStretch();
            steps->addWidget(card,1);
        }
        v->addLayout(steps);

        // Cau hoi thuong gap
        auto *faq=new QFrame; faq->setObjectName("pgCard");
        auto *fl=new QVBoxLayout(faq); fl->setContentsMargins(24,22,24,22); fl->setSpacing(10);
        auto *ft=new QLabel("Câu hỏi thường gặp"); ft->setObjectName("pgCardTitle"); fl->addWidget(ft);
        const QList<QPair<QString,QString>> qa={
            {"Tôi cần chuẩn bị giấy tờ gì?","Số giấy phép lái xe cùng ảnh hai mặt GPLX khi đặt xe. Tại trạm, nhân viên sẽ đối chiếu CCCD và GPLX bản gốc trước khi bàn giao."},
            {"Tôi có thể hủy đơn không?","Có. Bạn hủy được trong mục Đơn thuê của tôi khi đơn còn ở trạng thái Chờ duyệt hoặc Đã xác nhận. Khoản đặt cọc được xử lý theo điều khoản thuê xe."},
            {"Điều gì xảy ra nếu xe đã có người đặt?","Hệ thống kiểm tra lịch theo từng ngày. Nếu khoảng ngày bạn chọn trùng với đơn khác, nút thêm vào giỏ sẽ bị khóa và hiển thị khoảng ngày đã đặt. Xe vẫn đặt được vào các ngày còn trống."},
            {"Xe được kiểm tra thế nào khi bàn giao?","Nhân viên kiểm tra mức pin trên 90%, tình trạng ngoại thất, giấy tờ xe và bảo hiểm, ghi số ODO và lập biên bản cùng bạn."},
            {"Tôi trả xe như thế nào?","Mang xe về trạm đúng ngày trả. Nhân viên kiểm tra lại xe, ghi nhận số ODO và hoàn tất đơn thuê."}
        };
        for(int i=0;i<qa.size();++i){
            if(i>0){ auto *line=new QFrame; line->setFixedHeight(1); line->setStyleSheet("background:#EEF2F7;"); fl->addWidget(line); }
            auto *q=new QLabel(qa[i].first); q->setObjectName("pgQuestion");
            auto *a=new QLabel(qa[i].second); a->setObjectName("pgMuted"); a->setWordWrap(true);
            fl->addWidget(q); fl->addWidget(a);
        }
        v->addWidget(faq);
        v->addStretch();

        tabs->addTab(wrapScroll(page),"Hướng dẫn");
    }

    QTableWidget* makeTable(QStringList headers){
        auto *t=new QTableWidget; t->setColumnCount(headers.size());t->setHorizontalHeaderLabels(headers);
        t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);t->setSelectionBehavior(QAbstractItemView::SelectRows);
        t->setEditTriggers(QAbstractItemView::NoEditTriggers);t->setAlternatingRowColors(true);return t;
    }
    void buildVehiclesTab(){
        auto *w=new QWidget; auto *page=new QVBoxLayout(w);
        page->setContentsMargins(22,18,22,18); page->setSpacing(14);

        auto *hero=new QFrame; hero->setObjectName("heroBanner");
        auto *heroLayout=new QHBoxLayout(hero); heroLayout->setContentsMargins(26,22,26,22);
        auto *heroText=new QVBoxLayout;
        auto *eyebrow=new QLabel("THUÊ XE ĐIỆN VINFAST"); eyebrow->setObjectName("heroEyebrow");
        auto *headline=new QLabel("Chọn xe bạn thích.\nĐi nơi bạn muốn."); headline->setObjectName("heroTitle");
        auto *sub=new QLabel("Linh hoạt mỗi ngày. An tâm trên mọi hành trình."); sub->setObjectName("heroSubtitle");
        heroText->addWidget(eyebrow); heroText->addWidget(headline); heroText->addWidget(sub);
        auto *steps=new QLabel("01   Chọn xe & lịch thuê\n\n02   Xác minh GPLX · Đặt cọc 50%\n\n03   Nhận xe tại trạm, bắt đầu hành trình");
        steps->setObjectName("heroSteps"); steps->setMinimumWidth(300);
        heroLayout->addLayout(heroText,2); heroLayout->addWidget(steps,1); page->addWidget(hero);

        auto *searchRow=new QHBoxLayout;
        searchEdit=new QLineEdit; searchEdit->setPlaceholderText("Tìm theo tên xe hoặc mã xe..."); searchEdit->setMinimumHeight(40);
        auto *location=new QLabel("⌖  Trạm nhận: TP. Hồ Chí Minh"); location->setObjectName("locationLabel");
        auto *find=new QPushButton("Tìm xe"); find->setObjectName("primaryButton");
        searchRow->addWidget(searchEdit,3); searchRow->addWidget(location,2); searchRow->addWidget(find); page->addLayout(searchRow);

        auto *content=new QHBoxLayout; content->setSpacing(18);

        auto *filterPanel = new QFrame;
        filterPanel->setObjectName("filterPanel");
        filterPanel->setMinimumWidth(230);
        filterPanel->setMaximumWidth(250);

        auto *filterLayout = new QVBoxLayout(filterPanel);
        filterLayout->setContentsMargins(18, 18, 18, 18);
        filterLayout->setSpacing(12);

        // Tieu de bo loc
        auto *filterHeader = new QHBoxLayout;

        auto *filterTitle = new QLabel("Bộ lọc tìm kiếm");
        filterTitle->setStyleSheet(
            "font-size: 17px; font-weight: 700; color: #1E293B;"
            );

        auto *reset = new QPushButton("Đặt lại");
        reset->setCursor(Qt::PointingHandCursor);
        reset->setStyleSheet(
            "QPushButton { border: none; color: #2874D0; "
            "background: transparent; font-size: 12px; }"
            );

        filterHeader->addWidget(filterTitle);
        filterHeader->addStretch();
        filterHeader->addWidget(reset);
        filterLayout->addLayout(filterHeader);

        // An categoryBox cu de giu logic loc hien tai
        categoryBox = new QComboBox;
        categoryBox->addItems({
            "All",
            "Electric Cars",
            "Electric Scooters",
            "Motorbike",
            "4-seat car",
            "7-seat car",
            "Electric vehicle"
        });
        categoryBox->hide();
        // Khởi tạo bộ sắp xếp
        sortBox = new QComboBox(filterPanel);

        sortBox->addItem("Đề xuất", "name ASC");
        sortBox->addItem("Giá thấp đến cao", "price ASC");
        sortBox->addItem("Giá cao đến thấp", "price DESC");
        sortBox->addItem("Giảm giá", "discount DESC");

        sortBox->hide();
        // Ham tao tieu de tung nhom
        auto addCaption = [=](const QString &text){
            auto *label = new QLabel(text);
            label->setStyleSheet(
                "font-size: 11px; font-weight: 700; "
                "color: #64748B; margin-top: 12px;"
                );
            filterLayout->addWidget(label);
        };

        // Nhom loai phuong tien
        addCaption("LOẠI PHƯƠNG TIỆN");

        auto *allCars = new QRadioButton("Tất cả xe");
        auto *electricCars = new QRadioButton("Ô tô điện");
        auto *electricScooters = new QRadioButton("Xe máy điện");

        auto *categoryGroup = new QButtonGroup(filterPanel);
        categoryGroup->setExclusive(true);

        categoryGroup->addButton(allCars);
        categoryGroup->addButton(electricCars);
        categoryGroup->addButton(electricScooters);

        allCars->setChecked(true);

        filterLayout->addWidget(allCars);
        filterLayout->addWidget(electricCars);
        filterLayout->addWidget(electricScooters);

        // Ket noi loai xe voi bo loc cu
        connect(allCars, &QRadioButton::toggled, this, [=](bool checked){
            if(checked) categoryBox->setCurrentText("All");
        });

        connect(electricCars, &QRadioButton::toggled, this, [=](bool checked){
            if(checked) categoryBox->setCurrentText("Electric Cars");
        });

        connect(electricScooters, &QRadioButton::toggled, this, [=](bool checked){
            if(checked) categoryBox->setCurrentText("Electric Scooters");
        });

        // Nhom gia thue
        addCaption("GIÁ THUÊ / NGÀY");

        under300 = new QRadioButton("Dưới 300.000đ");
        from300to1000 = new QRadioButton("300.000đ – 1.000.000đ");
        over1000 = new QRadioButton("Trên 1.000.000đ");

        auto *priceGroup = new QButtonGroup(filterPanel);
        priceGroup->setExclusive(true);

        priceGroup->addButton(under300);
        priceGroup->addButton(from300to1000);
        priceGroup->addButton(over1000);

        filterLayout->addWidget(under300);
        filterLayout->addWidget(from300to1000);
        filterLayout->addWidget(over1000);

        // Nhom so cho
        addCaption("SỐ CHỖ NGỒI");

        auto *seatLayout = new QHBoxLayout;

        seat5 = new QPushButton("5 chỗ");
        seat7 = new QPushButton("7 chỗ");

        seat5->setCheckable(true);
        seat7->setCheckable(true);

        auto *seatGroup = new QButtonGroup(filterPanel);
        seatGroup->setExclusive(true);
        seatGroup->addButton(seat5);
        seatGroup->addButton(seat7);

        QString seatStyle =
            "QPushButton { background: #F1F5FF; color: #2874D0; "
            "border: none; border-radius: 9px; padding: 12px; "
            "font-weight: 600; }"
            "QPushButton:checked { background: #2874D0; color: white; }";

        seat5->setStyleSheet(seatStyle);
        seat7->setStyleSheet(seatStyle);

        seatLayout->addWidget(seat5);
        seatLayout->addWidget(seat7);
        filterLayout->addLayout(seatLayout);

        filterLayout->addStretch();

        content->addWidget(filterPanel);


        auto *catalog=new QVBoxLayout; auto *catalogHeader=new QHBoxLayout;
        auto *catalogTitle=new QLabel("Xe dành cho hành trình của bạn"); catalogTitle->setObjectName("catalogTitle");
        auto *catalogHint=new QLabel("Chọn xe · Chọn ngày thuê · Đặt xe"); catalogHint->setObjectName("mutedText");
        auto *titleCol=new QVBoxLayout; titleCol->addWidget(catalogTitle); titleCol->addWidget(catalogHint); catalogHeader->addLayout(titleCol); catalogHeader->addStretch(); catalog->addLayout(catalogHeader);
        auto *scroll=new QScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
        vehicleCardsContainer=new QWidget; vehicleCardsLayout=new QGridLayout(vehicleCardsContainer); vehicleCardsLayout->setContentsMargins(2,2,2,2); vehicleCardsLayout->setHorizontalSpacing(14); vehicleCardsLayout->setVerticalSpacing(14);
        scroll->setWidget(vehicleCardsContainer); catalog->addWidget(scroll); content->addLayout(catalog,1); page->addLayout(content,1);
        connect(find,&QPushButton::clicked,this,[=]{refreshVehicles();});
        connect(searchEdit,&QLineEdit::textChanged,this,[=]{refreshVehicles();});
        connect(categoryBox,&QComboBox::currentTextChanged,this,[=]{refreshVehicles();});
        // Lọc giá thuê
        connect(under300, &QRadioButton::toggled, this,
                [=](bool checked){
                    if(checked) refreshVehicles();
                });

        connect(from300to1000, &QRadioButton::toggled, this,
                [=](bool checked){
                    if(checked) refreshVehicles();
                });

        connect(over1000, &QRadioButton::toggled, this,
                [=](bool checked){
                    if(checked) refreshVehicles();
                });

        // Lọc số chỗ
        connect(seat5, &QPushButton::toggled, this,
                [=](bool){
                    refreshVehicles();
                });

        connect(seat7, &QPushButton::toggled, this,
                [=](bool){
                    refreshVehicles();
                });

        connect(reset, &QPushButton::clicked, this, [=]() {

            // 1. Chặn tín hiệu để tránh refresh nhiều lần
            QSignalBlocker b1(searchEdit);
            QSignalBlocker b2(categoryBox);
            QSignalBlocker b3(sortBox);

            QSignalBlocker b4(allCars);
            QSignalBlocker b5(electricCars);
            QSignalBlocker b6(electricScooters);

            QSignalBlocker b7(under300);
            QSignalBlocker b8(from300to1000);
            QSignalBlocker b9(over1000);

            QSignalBlocker b10(seat5);
            QSignalBlocker b11(seat7);

            // 2. Xóa nội dung tìm kiếm
            searchEdit->clear();

            // 3. Đưa loại xe về mặc định
            categoryBox->setCurrentIndex(0);
            allCars->setChecked(true);

            // 4. Đưa sắp xếp về mặc định
            sortBox->setCurrentIndex(0);

            // 5. Bỏ chọn toàn bộ bộ lọc giá
            priceGroup->setExclusive(false);

            under300->setChecked(false);
            from300to1000->setChecked(false);
            over1000->setChecked(false);

            priceGroup->setExclusive(true);

            // 6. Bỏ chọn toàn bộ bộ lọc số chỗ
            seatGroup->setExclusive(false);

            seat5->setChecked(false);
            seat7->setChecked(false);

            seatGroup->setExclusive(true);

            // 7. Chỉ cập nhật danh sách một lần
            refreshVehicles();
        });

        tabs->addTab(w,"Khám phá xe");
    }
    void buildAccountTab(){
        auto *w=new QWidget;auto *outer=new QVBoxLayout(w);
        auto *box=new QGroupBox("Account / Authentication");auto *f=new QFormLayout(box);
        loginEmail=new QLineEdit;loginPassword=new QLineEdit;loginPassword->setEchoMode(QLineEdit::Password);
        auto *login=new QPushButton("Login");auto *logout=new QPushButton("Logout");
        auto *reg=new QPushButton("Register customer");auto *reset=new QPushButton("Reset password");
        loginLabel=new QLabel("Not logged in");
        f->addRow("Email",loginEmail);f->addRow("Password",loginPassword);
        auto *row=new QHBoxLayout;row->addWidget(login);row->addWidget(logout);row->addWidget(reg);row->addWidget(reset);
        f->addRow(row);f->addRow("Current session",loginLabel);outer->addWidget(box);
        auto *profile=new QGroupBox("Update profile");auto *pf=new QFormLayout(profile);
        auto *pname=new QLineEdit;auto *pphone=new QLineEdit;auto *paddress=new QLineEdit;auto *save=new QPushButton("Save profile");
        pf->addRow("Full name",pname);pf->addRow("Phone",pphone);pf->addRow("Address",paddress);pf->addRow(save);outer->addWidget(profile);outer->addStretch();
        connect(login,&QPushButton::clicked,this,[=]{
            currentUser=accountModel.login(loginEmail->text().trimmed(),loginPassword->text());
            if(!currentUser.id){error("Invalid email or password.");return;}
            loginLabel->setText(currentUser.name+" ("+currentUser.role+")");
            if(currentUser.role=="Admin") tabs->setCurrentIndex(5);
            else if(currentUser.role=="Staff") tabs->setCurrentIndex(4);
            else tabs->setCurrentIndex(0);
            refreshOrders();refreshCart();info("Login successful.");
        });
        connect(logout,&QPushButton::clicked,this,[=]{currentUser={};loginLabel->setText("Not logged in");info("Logged out.");});
        connect(reg,&QPushButton::clicked,this,[=]{
            bool ok1,ok2,ok3,ok4;QString n=QInputDialog::getText(this,"Register","Full name:",QLineEdit::Normal,"",&ok1);
            if(!ok1||n.trimmed().isEmpty())return;
            QString e=QInputDialog::getText(this,"Register","Email:",QLineEdit::Normal,"",&ok2);if(!ok2)return;
            QString p=QInputDialog::getText(this,"Register","Password:",QLineEdit::Password,"",&ok3);if(!ok3)return;
            QString ph=QInputDialog::getText(this,"Register","Phone:",QLineEdit::Normal,"",&ok4);if(!ok4)return;
            QString ad=QInputDialog::getText(this,"Register","Address:");
            if(accountModel.registerUser(n,e,p,ph,ad))info("Account created. You can now log in.");else error("Registration failed. Email may already exist.");
        });
        connect(reset,&QPushButton::clicked,this,[=]{
            bool ok;QString e=QInputDialog::getText(this,"Reset password","Account email:",QLineEdit::Normal,"",&ok);if(!ok)return;
            QString p=QInputDialog::getText(this,"Reset password","Enter a new password (demo recovery):",QLineEdit::Password,"",&ok);
            if(ok && accountModel.reset(e,p))info("Password updated. In production, a recovery code should be sent by email.");else error("Email not found.");
        });
        connect(save,&QPushButton::clicked,this,[=]{
            if(!currentUser.id){error("Please log in first.");return;}
            if(!pname->text().trimmed().isEmpty())currentUser.name=pname->text();
            currentUser.phone=pphone->text();currentUser.address=paddress->text();
            if(accountModel.update(currentUser)){loginLabel->setText(currentUser.name+" ("+currentUser.role+")");info("Profile updated. Email username cannot be changed.");}
            else error("Could not update profile.");
        });
        tabs->addTab(w,"Tài khoản");
    }
    void buildCartTab(){
        auto *w=new QWidget; w->setStyleSheet("background:#f4f6fb;");
        auto *l=new QVBoxLayout(w); l->setContentsMargins(35,25,35,25); l->setSpacing(16);
        auto *crumb=new QLabel("Khám phá xe  /  Giỏ xe"); crumb->setStyleSheet("color:#8490a0;");
        auto *title=new QLabel("Giỏ xe của bạn"); title->setStyleSheet("font-size:25px;font-weight:bold;color:#172b4d;");
        auto *sub=new QLabel("Kiểm tra lịch thuê từng xe trước khi đặt cọc."); sub->setStyleSheet("color:#8490a0;");
        l->addWidget(crumb); l->addWidget(title); l->addWidget(sub);

        auto *card=new QFrame; card->setStyleSheet("QFrame{background:white;border-radius:12px;}");
        auto *cl=new QVBoxLayout(card); cl->setContentsMargins(20,20,20,20); cl->setSpacing(12);
        cartTable=makeTable({"MÃ XE","TÊN XE","GIÁ / NGÀY","SỐ NGÀY","SL","THÀNH TIỀN","NHẬN XE","TRẢ XE"});
        cartTable->setShowGrid(false); cartTable->setAlternatingRowColors(false);
        cartTable->verticalHeader()->hide(); cartTable->verticalHeader()->setDefaultSectionSize(54);
        cartTable->setMinimumHeight(260);
        cartTable->setStyleSheet(R"(
            QTableWidget { background:white; border:0; }
            QHeaderView::section { background:white; color:#8490a0; border:0; border-bottom:1px solid #e5e7eb; padding:12px 5px; font-weight:bold; }
            QTableWidget::item { border-bottom:1px solid #e5e7eb; padding:8px; }
            QTableWidget::item:selected { background:#edf3ff; color:#172b4d; }
        )");
        cl->addWidget(cartTable);

        auto *controls=new QHBoxLayout;
        daysSpin=new QSpinBox; daysSpin->setRange(1,90); daysSpin->setValue(1); daysSpin->setSuffix(" ngày");
        qtySpin=new QSpinBox; qtySpin->setRange(1,1); qtySpin->setValue(1); qtySpin->hide(); // moi xe chi co 1 chiec
        auto *update=new QPushButton("Cập nhật số ngày"); auto *remove=new QPushButton("Xóa khỏi giỏ");
        auto *daysLabel=new QLabel("Số ngày thuê của xe đã chọn:"); daysLabel->setStyleSheet("font-weight:bold;");
        controls->addWidget(daysLabel); controls->addWidget(daysSpin); controls->addWidget(update); controls->addStretch(); controls->addWidget(remove);
        cl->addLayout(controls);
        l->addWidget(card,1);

        auto *bottom=new QHBoxLayout;
        totalLabel=new QLabel("Tạm tính: 0đ"); totalLabel->setStyleSheet("font-size:16pt;font-weight:bold;color:#172b4d;");
        auto *checkout=new QPushButton("Đặt xe & thanh toán  →");
        checkout->setStyleSheet("QPushButton{background:#1769e8;color:white;border-radius:8px;padding:14px 30px;font-weight:bold;}QPushButton:hover{background:#0f54c5;}");
        bottom->addWidget(totalLabel); bottom->addStretch(); bottom->addWidget(checkout);
        l->addLayout(bottom);

        connect(cartTable,&QTableWidget::cellClicked,this,[=](int r,int){ if(r>=0&&r<cart.size()) daysSpin->setValue(cart[r].days); });
        connect(update,&QPushButton::clicked,this,[=]{
            int r=cartTable->currentRow();
            if(r<0||r>=cart.size()){ error("Vui lòng chọn một xe trong giỏ."); return; }
            QDate newDrop=cart[r].pickupDate.addDays(daysSpin->value());
            if(!vehicleFreeForDates(db.connection(),cart[r].vehicleId,cart[r].pickupDate,newDrop)){
                error("Xe đã có người đặt trong khoảng ngày này. Vui lòng chọn số ngày khác.");return;}
            cart[r].days=daysSpin->value();cart[r].dropoffDate=newDrop;cart[r].quantity=1;persistCart();refreshCart();});
        connect(remove,&QPushButton::clicked,this,[=]{
            int r=cartTable->currentRow();
            if(r<0||r>=cart.size()){ error("Vui lòng chọn một xe trong giỏ."); return; }
            cart.removeAt(r);persistCart();refreshCart();});
        connect(checkout,&QPushButton::clicked,this,[=]{checkoutBooking();});
        tabs->addTab(w,"Giỏ xe");
    }
    void buildOrdersTab(){
        auto *w = new QWidget;
        w->setStyleSheet("background:#f4f6fb;");

        auto *l = new QVBoxLayout(w);
        l->setContentsMargins(35,25,35,25);
        l->setSpacing(18);

        // Tiêu đề
        auto *breadcrumb = new QLabel("Tài khoản  /  Đơn thuê của tôi");
        breadcrumb->setStyleSheet("color:#8490a0;");
        l->addWidget(breadcrumb);

        auto *header = new QHBoxLayout;

        auto *titleBox = new QVBoxLayout;
        auto *title = new QLabel("Mọi hành trình, trong tầm tay.");
        title->setStyleSheet(
            "font-size:25px;font-weight:bold;color:#172b4d;"
            );

        auto *subtitle = new QLabel(
            "Theo dõi lịch nhận xe, trạng thái và lịch sử thuê của bạn."
            );
        subtitle->setStyleSheet("color:#8490a0;");

        titleBox->addWidget(title);
        titleBox->addWidget(subtitle);
        header->addLayout(titleBox);
        header->addStretch();

        auto *newBooking = new QPushButton("+ Đặt chuyến đi mới");
        newBooking->setStyleSheet(
            "QPushButton{background:#1769e8;color:white;"
            "border-radius:8px;padding:13px 22px;font-weight:bold;}"
            "QPushButton:hover{background:#0f54c5;}"
            );

        connect(newBooking,&QPushButton::clicked,this,[=](){
            tabs->setCurrentIndex(0);
        });

        header->addWidget(newBooking);
        l->addLayout(header);

        // Ba ô thống kê
        auto *stats = new QHBoxLayout;
        stats->setSpacing(18);

        auto makeStat = [&](QString text,QLabel *&number){
            auto *card = new QFrame;
            card->setStyleSheet(
                "QFrame{background:white;border-radius:10px;}"
                );

            auto *box = new QVBoxLayout(card);
            box->setContentsMargins(20,15,20,15);

            auto *label = new QLabel(text);
            label->setStyleSheet("color:#8490a0;");

            number = new QLabel("00");
            number->setStyleSheet(
                "font-size:25px;font-weight:bold;color:#172b4d;"
                );

            box->addWidget(label);
            box->addWidget(number);
            stats->addWidget(card);
        };

        makeStat("Tổng đơn thuê",ordersTotalCount);
        makeStat("Chờ duyệt",ordersPendingCount);
        makeStat("Đã hoàn thành",ordersCompletedCount);

        l->addLayout(stats);

        // Khu vực danh sách đơn
        auto *content = new QFrame;
        content->setStyleSheet(
            "QFrame{background:white;border-radius:12px;}"
            );

        auto *contentLayout = new QVBoxLayout(content);
        contentLayout->setContentsMargins(20,20,20,20);
        contentLayout->setSpacing(15);

        auto *filters = new QHBoxLayout;

        auto makeFilter = [&](QString text,QString key){
            auto *btn = new QPushButton(text);
            btn->setCheckable(true);

            btn->setStyleSheet(
                "QPushButton{background:#edf3ff;color:#1769e8;"
                "border:0;border-radius:7px;padding:12px 20px;}"
                "QPushButton:checked{background:#1769e8;color:white;}"
                );

            connect(btn,&QPushButton::clicked,this,[=](){
                ordersFilter = key;
                refreshOrders();
            });

            filters->addWidget(btn);
            return btn;
        };

        auto *filterGroup = new QButtonGroup(w);
        filterGroup->setExclusive(true);
        auto *allFilter = makeFilter("Tất cả","all");
        allFilter->setChecked(true);
        filterGroup->addButton(allFilter);
        filterGroup->addButton(makeFilter("Chờ duyệt","new"));
        filterGroup->addButton(makeFilter("Đã xác nhận","confirmed"));
        filterGroup->addButton(makeFilter("Đang thuê","rental"));
        filterGroup->addButton(makeFilter("Hoàn thành","completed"));
        filterGroup->addButton(makeFilter("Đã hủy","cancelled"));

        filters->addStretch();
        contentLayout->addLayout(filters);

        // Bảng đơn thuê
        ordersTable = new QTableWidget(0,6);
        ordersTable->setHorizontalHeaderLabels({
            "MÃ ĐƠN",
            "XE THUÊ",
            "THỜI GIAN THUÊ",
            "TỔNG TIỀN",
            "TRẠNG THÁI",
            "THAO TÁC"
        });

        ordersTable->horizontalHeader()->setSectionResizeMode(
            QHeaderView::Stretch
            );

        ordersTable->verticalHeader()->hide();
        ordersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        ordersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        ordersTable->setSelectionMode(QAbstractItemView::SingleSelection);
        ordersTable->setShowGrid(false);
        ordersTable->setAlternatingRowColors(false);

        ordersTable->setStyleSheet(R"(
        QTableWidget {
            background:white;
            border:0;
            color:#172b4d;
            font-size:10pt;
        }
        QHeaderView::section {
            background:white;
            color:#8490a0;
            border:0;
            border-bottom:1px solid #e5e7eb;
            padding:14px 5px;
            font-weight:bold;
        }
        QTableWidget::item {
            border-bottom:1px solid #e5e7eb;
            padding:10px;
        }
        QTableWidget::item:selected {
            background:#edf3ff;
            color:#172b4d;
        }
    )");

        contentLayout->addWidget(ordersTable);
        l->addWidget(content);

        // Thông báo chính sách
        auto *notice = new QFrame;
        notice->setStyleSheet(
            "QFrame{background:#eaf1ff;border-radius:10px;}"
            );

        auto *noticeLayout = new QVBoxLayout(notice);

        auto *noticeTitle = new QLabel(
            "Bạn có thể hủy đơn trước khi bàn giao xe."
            );
        noticeTitle->setStyleSheet("font-weight:bold;font-size:11pt;");

        auto *noticeText = new QLabel(
            "Khoản đặt cọc và thời gian hoàn tiền được xử lý "
            "theo điều khoản thuê xe."
            );
        noticeText->setStyleSheet("color:#8490a0;");

        noticeLayout->addWidget(noticeTitle);
        noticeLayout->addWidget(noticeText);

        l->addWidget(notice);

        tabs->addTab(w,"Đơn thuê của tôi");
    }

    void buildStaffTab(){
        auto *w=new QWidget; auto *outer=new QVBoxLayout(w);
        outer->setContentsMargins(0,0,0,0);
        staffInner=new QTabWidget; outer->addWidget(staffInner);

        // --- Tab 1: danh sach don (duyet / huy / cap nhat trang thai)
        auto *listPage=new QWidget; auto *l=new QVBoxLayout(listPage);
        staffTable=makeTable({"Order ID","Customer ID","Recipient","Total","Status","Created"});l->addWidget(staffTable);
        auto *row=new QHBoxLayout;statusBox=new QComboBox;statusBox->addItems({"Confirmed","Delivering","Cancelled"});
        auto *update=new QPushButton("Update status");
        auto *openHandover=new QPushButton("Mở bàn giao / nhận xe →");
        row->addWidget(statusBox);row->addWidget(update);row->addStretch();row->addWidget(openHandover);l->addLayout(row);
        connect(update,&QPushButton::clicked,this,[=]{updateStaffStatus(statusBox->currentText());});
        connect(openHandover,&QPushButton::clicked,this,[=]{
            int r=staffTable->currentRow();
            if(r<0){error("Vui lòng chọn một đơn thuê.");return;}
            loadHandoverOrder(staffTable->item(r,0)->text().toInt());
            staffInner->setCurrentIndex(1);
        });
        staffInner->addTab(listPage,"Danh sách đơn thuê");

        // --- Tab 2: ban giao & tiep nhan xe
        staffInner->addTab(wrapScroll(buildHandoverPage()),"Bàn giao && tiếp nhận xe");
        tabs->addTab(w,"Quản lý thuê xe");
    }
    // ------------------------------------------------------------
    // TRUNG TAM QUAN TRI (Admin)
    // ------------------------------------------------------------
    QTableWidget* makeAdminTable(const QStringList &headers){
        auto *t=makeTable(headers);
        t->setObjectName("admTable");
        t->setFrameShape(QFrame::NoFrame);
        t->setShowGrid(false);
        t->setAlternatingRowColors(false);
        t->setSelectionMode(QAbstractItemView::SingleSelection);
        t->verticalHeader()->hide();
        t->verticalHeader()->setDefaultSectionSize(56);
        t->horizontalHeader()->setHighlightSections(false);
        t->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft|Qt::AlignVCenter);
        return t;
    }
    void setCell(QTableWidget *t,int r,int c,const QString &text,const QString &color=QString(),bool bold=false){
        auto *it=new QTableWidgetItem(text);
        if(!color.isEmpty()) it->setForeground(QColor(color));
        if(bold){ QFont f=it->font(); f.setBold(true); it->setFont(f); }
        t->setItem(r,c,it);
    }
    QScrollArea* wrapScroll(QWidget *page){
        auto *sc=new QScrollArea; sc->setWidgetResizable(true); sc->setFrameShape(QFrame::NoFrame); sc->setWidget(page);
        return sc;
    }
    QHBoxLayout* adminHeader(const QString &crumb,const QString &title,const QString &sub,QPushButton *action=nullptr){
        auto *h=new QHBoxLayout;
        auto *box=new QVBoxLayout; box->setSpacing(4);
        auto *c=new QLabel(crumb); c->setObjectName("admCrumb");
        auto *t=new QLabel(title); t->setObjectName("admTitle");
        auto *m=new QLabel(sub); m->setObjectName("admMuted");
        box->addWidget(c); box->addWidget(t); box->addWidget(m);
        h->addLayout(box,1);
        if(action) h->addWidget(action,0,Qt::AlignTop);
        return h;
    }
    QFrame* adminCard(QVBoxLayout *&inner,int margin=22){
        auto *card=new QFrame; card->setObjectName("admCard");
        inner=new QVBoxLayout(card); inner->setContentsMargins(margin,margin,margin,margin); inner->setSpacing(12);
        return card;
    }
    QWidget* buildStatRow(StatSet &st){
        auto *row=new QWidget; auto *h=new QHBoxLayout(row); h->setContentsMargins(0,0,0,0); h->setSpacing(20);
        auto card=[&](const QString &cap,QLabel *&val,QLabel *&sub,const QString &valColor){
            QVBoxLayout *inner=nullptr; auto *f=adminCard(inner,20); inner->setSpacing(4);
            auto *c=new QLabel(cap); c->setObjectName("admMuted");
            val=new QLabel("0"); val->setObjectName("admStatVal");
            if(!valColor.isEmpty()) val->setStyleSheet("color:"+valColor+";");
            sub=new QLabel; sub->setObjectName("admMuted"); sub->setStyleSheet("font-size:8pt;");
            inner->addWidget(c); inner->addWidget(val); inner->addWidget(sub);
            h->addWidget(f,1);
        };
        card("Tổng doanh thu",st.revenue,st.revenueSub,"");
        card("Xe đang thuê",st.rented,st.rentedSub,"#1769E0");
        card("Đơn chờ duyệt",st.pending,st.pendingSub,"");
        return row;
    }

    void buildAdminTab(){
        auto *w=new QWidget; w->setObjectName("admRoot");
        w->setStyleSheet(R"(
            QWidget#admRoot { background:#F4F7FC; }
            QWidget#admPage { background:#F4F7FC; }
            QFrame#admSide { background:#12294A; }
            QLabel#admSideCap { color:#9FB3D1; font-size:8pt; font-weight:bold; }
            QLabel#admSideText { color:#D6E0F0; }
            QPushButton#admNav { background:transparent; color:#D6E0F0; border:none; border-radius:8px; padding:14px 16px; text-align:left; font-size:11pt; }
            QPushButton#admNav:hover { background:#1B3A66; }
            QPushButton#admNav:checked { background:#1F4E8C; color:white; font-weight:bold; }
            QFrame#admCard { background:white; border-radius:12px; }
            QLabel#admCrumb { color:#1769E0; font-size:8pt; font-weight:bold; }
            QLabel#admTitle { color:#172B45; font-size:24pt; font-weight:bold; }
            QLabel#admMuted { color:#718096; }
            QLabel#admCardTitle { color:#172B45; font-size:16pt; font-weight:bold; }
            QLabel#admStatVal { color:#172B45; font-size:24pt; font-weight:bold; }
            QLabel#admCap { color:#4A5568; font-weight:bold; font-size:9pt; }
            QPushButton#admPrimary { background:#1769E0; color:white; border:none; border-radius:8px; padding:13px 22px; font-weight:bold; }
            QPushButton#admPrimary:hover { background:#0F54C5; }
            QPushButton#admSecondary { background:#EAF2FF; color:#1769E0; border:none; border-radius:8px; padding:13px 22px; font-weight:bold; }
            QPushButton#admDanger { background:transparent; color:#DC2626; border:none; padding:8px; }
            QPushButton#admPager { background:transparent; color:#1769E0; border:none; padding:4px 8px; }
            QPushButton#admPager:checked { font-weight:bold; text-decoration:underline; }
            QLineEdit, QComboBox, QDoubleSpinBox, QSpinBox { background:#F8FAFC; border:1px solid #DCE3EF; border-radius:8px; padding:10px; min-height:22px; color:#172B45; }
            QTableWidget#admTable { background:white; border:none; selection-background-color:#EAF2FF; selection-color:#172B45; }
            QTableWidget#admTable::item { padding:6px; border-bottom:1px solid #EEF2F7; }
            QHeaderView::section { background:white; color:#718096; border:none; border-bottom:1px solid #EEF2F7; padding:8px; font-size:8pt; font-weight:bold; }
        )");
        auto *root=new QHBoxLayout(w); root->setContentsMargins(0,0,0,0); root->setSpacing(0);

        // ---- thanh ben trai
        auto *side=new QFrame; side->setObjectName("admSide"); side->setFixedWidth(240);
        auto *sl=new QVBoxLayout(side); sl->setContentsMargins(12,26,12,26); sl->setSpacing(6);
        auto *cap=new QLabel("KHÔNG GIAN QUẢN TRỊ"); cap->setObjectName("admSideCap");
        sl->addWidget(cap); sl->addSpacing(18);

        adminStack=new QStackedWidget;
        adminNavGroup=new QButtonGroup(w); adminNavGroup->setExclusive(true);
        const QStringList names={"Tổng quan","Đội xe && giá thuê","Người dùng","Đơn thuê","Báo cáo"};
        for(int i=0;i<names.size();++i){
            auto *b=new QPushButton(names[i]); b->setObjectName("admNav"); b->setCheckable(true);
            b->setCursor(Qt::PointingHandCursor);
            adminNavGroup->addButton(b,i); sl->addWidget(b);
            connect(b,&QPushButton::clicked,this,[=]{ adminStack->setCurrentIndex(i); refreshAdmin(); });
        }
        sl->addStretch();
        auto *supCap=new QLabel("HỖ TRỢ VẬN HÀNH"); supCap->setObjectName("admSideCap");
        auto *sup1=new QLabel("Trạm Mỹ Đình, Hà Nội"); sup1->setObjectName("admSideText"); sup1->setStyleSheet("font-size:11pt;color:white;");
        auto *sup2=new QLabel("Quyền quản trị viên"); sup2->setObjectName("admSideText");
        sl->addWidget(supCap); sl->addWidget(sup1); sl->addWidget(sup2);
        root->addWidget(side);

        // ---- cac trang
        adminStack->addWidget(wrapScroll(buildAdminOverview()));
        adminStack->addWidget(wrapScroll(buildAdminFleet()));
        adminStack->addWidget(wrapScroll(buildAdminUsers()));
        adminStack->addWidget(wrapScroll(buildAdminOrders()));
        adminStack->addWidget(wrapScroll(buildAdminReports()));
        root->addWidget(adminStack,1);

        adminStack->setCurrentIndex(1);
        adminNavGroup->button(1)->setChecked(true);
        tabs->addTab(w,"Quản trị");
    }

    QWidget* newAdminPage(QVBoxLayout *&v){
        auto *page=new QWidget; page->setObjectName("admPage");
        v=new QVBoxLayout(page); v->setContentsMargins(28,24,28,24); v->setSpacing(16);
        return page;
    }

    // --- Tong quan
    QWidget* buildAdminOverview(){
        QVBoxLayout *v=nullptr; auto *page=newAdminPage(v);
        v->addLayout(adminHeader("QUẢN TRỊ  /  TỔNG QUAN","Tổng quan","Chỉ số vận hành nhanh của hệ thống thuê xe."));
        v->addWidget(buildStatRow(adminStats[0]));
        QVBoxLayout *inner=nullptr; auto *card=adminCard(inner);
        auto *t=new QLabel("Đơn thuê gần đây"); t->setObjectName("admCardTitle"); inner->addWidget(t);
        recentOrdersTable=makeAdminTable({"MÃ ĐƠN","KHÁCH HÀNG","TỔNG TIỀN","TRẠNG THÁI","NGÀY TẠO"});
        recentOrdersTable->setMinimumHeight(340);
        inner->addWidget(recentOrdersTable);
        v->addWidget(card);
        v->addStretch();
        return page;
    }

    // --- Doi xe & gia thue (FR-10)
    QWidget* buildAdminFleet(){
        QVBoxLayout *v=nullptr; auto *page=newAdminPage(v);
        auto *addBtn=new QPushButton("+  Thêm xe mới"); addBtn->setObjectName("admPrimary");
        v->addLayout(adminHeader("QUẢN TRỊ  /  ĐỘI XE","Quản lý đội xe","Theo dõi trạng thái và tối ưu giá thuê từng phương tiện.",addBtn));
        v->addWidget(buildStatRow(adminStats[1]));
        fleetNote=new QLabel; fleetNote->setObjectName("admMuted"); fleetNote->setStyleSheet("font-size:8pt;");
        v->addWidget(fleetNote);

        auto *cols=new QHBoxLayout; cols->setSpacing(20);

        // bang doi xe
        QVBoxLayout *cl=nullptr; auto *tableCard=adminCard(cl);
        auto *title=new QLabel("Đội xe tại trạm"); title->setObjectName("admCardTitle"); cl->addWidget(title);
        auto *filters=new QHBoxLayout;
        fleetSearch=new QLineEdit; fleetSearch->setPlaceholderText("Tìm mã xe hoặc tên xe...");
        fleetStatus=new QComboBox; fleetStatus->addItems({"Tất cả trạng thái","Sẵn sàng","Đang thuê"});
        filters->addWidget(fleetSearch,3); filters->addWidget(fleetStatus,1);
        cl->addLayout(filters);
        adminVehicleTable=makeAdminTable({"MÃ XE","TÊN XE","LOẠI XE","GIÁ / NGÀY","TRẠNG THÁI","SỬA"});
        adminVehicleTable->setMinimumHeight(340);
        cl->addWidget(adminVehicleTable,1);
        auto *footer=new QHBoxLayout;
        fleetInfo=new QLabel; fleetInfo->setObjectName("admMuted");
        auto *pagerHost=new QWidget; fleetPager=new QHBoxLayout(pagerHost); fleetPager->setContentsMargins(0,0,0,0); fleetPager->setSpacing(2);
        footer->addWidget(fleetInfo); footer->addStretch(); footer->addWidget(pagerHost);
        cl->addLayout(footer);
        cols->addWidget(tableCard,2);

        // panel cap nhat phuong tien
        QVBoxLayout *pl=nullptr; auto *panelCard=adminCard(pl,24);
        panelCard->setMinimumWidth(340); panelCard->setMaximumWidth(420);
        panelTitle=new QLabel("Cập nhật phương tiện"); panelTitle->setObjectName("admCardTitle");
        panelSub=new QLabel; panelSub->setObjectName("admMuted"); panelSub->setWordWrap(true);
        pl->addWidget(panelTitle); pl->addWidget(panelSub);
        panelBody=new QWidget; auto *pb=new QVBoxLayout(panelBody); pb->setContentsMargins(0,6,0,0); pb->setSpacing(6);
        auto field=[&](const QString &caption,QWidget *editor,QVBoxLayout *into){
            auto *c=new QLabel(caption); c->setObjectName("admCap"); into->addWidget(c); into->addWidget(editor);
        };
        nameEdit=new QLineEdit;
        adminCategory=new QComboBox;
        for(const QString &c : {"Electric Cars","Electric Scooters","Motorbike","4-seat car","7-seat car","Electric vehicle"})
            adminCategory->addItem(categoryLabel(c),c);
        priceSpin=new QDoubleSpinBox; priceSpin->setRange(0,100000000); priceSpin->setDecimals(0); priceSpin->setGroupSeparatorShown(true);
        statusEditBox=new QComboBox; statusEditBox->addItems({"Sẵn sàng","Đang thuê"});
        field("Tên phương tiện",nameEdit,pb);
        field("Loại xe",adminCategory,pb);
        field("Giá thuê / ngày (VNĐ)",priceSpin,pb);
        field("Trạng thái",statusEditBox,pb);

        // truong bo sung - chi hien khi them xe moi
        extraFields=new QWidget; auto *ex=new QVBoxLayout(extraFields); ex->setContentsMargins(0,0,0,0); ex->setSpacing(6);
        codeEdit=new QLineEdit; plateEdit=new QLineEdit; makeEdit=new QLineEdit; summaryEdit=new QLineEdit;
        seatsSpin=new QSpinBox; seatsSpin->setRange(1,60); seatsSpin->setValue(5);
        fuelBox=new QComboBox; fuelBox->addItems({"Electric","Petrol","Diesel","Hybrid"});
        transmissionBox=new QComboBox; transmissionBox->addItems({"Automatic","Manual"});
        discountSpin=new QDoubleSpinBox; discountSpin->setRange(0,100); discountSpin->setSuffix("%");
        field("Mã xe",codeEdit,ex); field("Biển số",plateEdit,ex); field("Hãng xe",makeEdit,ex);
        field("Số chỗ ngồi",seatsSpin,ex); field("Nhiên liệu",fuelBox,ex); field("Hộp số",transmissionBox,ex);
        field("Giảm giá",discountSpin,ex); field("Mô tả ngắn",summaryEdit,ex);
        pb->addWidget(extraFields);

        panelDelete=new QPushButton("Xóa phương tiện này"); panelDelete->setObjectName("admDanger");
        pb->addWidget(panelDelete,0,Qt::AlignLeft);
        pb->addStretch();
        auto *btns=new QHBoxLayout;
        auto *cancel=new QPushButton("Hủy"); cancel->setObjectName("admSecondary");
        auto *save=new QPushButton("Lưu thay đổi"); save->setObjectName("admPrimary");
        btns->addWidget(cancel,1); btns->addWidget(save,2);
        pb->addLayout(btns);
        pl->addWidget(panelBody,1);
        cols->addWidget(panelCard,1);
        v->addLayout(cols,1);

        connect(fleetSearch,&QLineEdit::textChanged,this,[=](const QString &){ fleetPage=1; refreshFleet(); });
        connect(fleetStatus,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[=](int){ fleetPage=1; refreshFleet(); });
        connect(adminVehicleTable,&QTableWidget::cellClicked,this,[=](int r,int){ selectFleetRow(r); });
        connect(addBtn,&QPushButton::clicked,this,[=]{ startAddVehicle(); });
        connect(cancel,&QPushButton::clicked,this,[=]{ adminVehicleTable->clearSelection(); showPanelPlaceholder(); });
        connect(save,&QPushButton::clicked,this,[=]{ saveFleetVehicle(); });
        connect(panelDelete,&QPushButton::clicked,this,[=]{ deleteFleetVehicle(); });
        showPanelPlaceholder();
        return page;
    }

    // --- Nguoi dung (FR-11)
    QWidget* buildAdminUsers(){
        QVBoxLayout *v=nullptr; auto *page=newAdminPage(v);
        v->addLayout(adminHeader("QUẢN TRỊ  /  NGƯỜI DÙNG","Người dùng & phân quyền","Quản lý tài khoản khách hàng, nhân viên và quản trị viên."));
        QVBoxLayout *inner=nullptr; auto *card=adminCard(inner);
        usersSearch=new QLineEdit; usersSearch->setPlaceholderText("Tìm theo tên, email hoặc số điện thoại...");
        inner->addWidget(usersSearch);
        usersTable=makeAdminTable({"ID","HỌ TÊN","EMAIL","SỐ ĐIỆN THOẠI","VAI TRÒ"});
        usersTable->setMinimumHeight(340);
        inner->addWidget(usersTable,1);
        auto *row=new QHBoxLayout;
        auto *lab=new QLabel("Đổi quyền của tài khoản đã chọn:"); lab->setObjectName("admCap");
        userRoleBox=new QComboBox;
        for(const QString &r : {"Customer","Staff","Admin"}) userRoleBox->addItem(roleLabel(r),r);
        auto *apply=new QPushButton("Cập nhật quyền"); apply->setObjectName("admPrimary");
        row->addWidget(lab); row->addWidget(userRoleBox); row->addStretch(); row->addWidget(apply);
        inner->addLayout(row);
        v->addWidget(card,1);
        connect(usersSearch,&QLineEdit::textChanged,this,[=](const QString &){ refreshUsers(); });
        connect(usersTable,&QTableWidget::cellClicked,this,[=](int r,int){
            const QString role=usersTable->item(r,4)->data(Qt::UserRole).toString();
            int idx=userRoleBox->findData(role); if(idx>=0) userRoleBox->setCurrentIndex(idx);
        });
        connect(apply,&QPushButton::clicked,this,[=]{ changeUserRole(); });
        return page;
    }

    // --- Don thue
    QWidget* buildAdminOrders(){
        QVBoxLayout *v=nullptr; auto *page=newAdminPage(v);
        v->addLayout(adminHeader("QUẢN TRỊ  /  ĐƠN THUÊ","Đơn thuê","Toàn bộ đơn thuê trong hệ thống. Duyệt đơn và bàn giao xe ở mục \"Quản lý thuê xe\"."));
        QVBoxLayout *inner=nullptr; auto *card=adminCard(inner);
        adminOrdersTable=makeAdminTable({"MÃ ĐƠN","KHÁCH HÀNG","TỔNG TIỀN","TRẠNG THÁI","THANH TOÁN","NGÀY TẠO"});
        adminOrdersTable->setMinimumHeight(420);
        inner->addWidget(adminOrdersTable);
        v->addWidget(card,1);
        return page;
    }

    // --- Bao cao
    QWidget* buildAdminReports(){
        QVBoxLayout *v=nullptr; auto *page=newAdminPage(v);
        v->addLayout(adminHeader("QUẢN TRỊ  /  BÁO CÁO","Báo cáo","Thống kê đơn thuê, doanh thu và xe được thuê nhiều nhất."));
        auto *grid=new QGridLayout; grid->setHorizontalSpacing(20); grid->setVerticalSpacing(20);
        auto addCard=[&](const QString &title,QTableWidget *&table,const QStringList &headers,int row,int col){
            QVBoxLayout *inner=nullptr; auto *card=adminCard(inner);
            auto *t=new QLabel(title); t->setObjectName("admCardTitle"); inner->addWidget(t);
            table=makeAdminTable(headers); table->setMinimumHeight(300);
            inner->addWidget(table);
            grid->addWidget(card,row,col);
        };
        addCard("Đơn thuê theo trạng thái",reportStatusTable,{"TRẠNG THÁI","SỐ ĐƠN","TỔNG TIỀN"},0,0);
        addCard("Xe được thuê nhiều nhất",reportTopTable,{"MÃ XE","TÊN XE","LƯỢT THUÊ"},0,1);
        addCard("Doanh thu 6 tháng gần nhất",reportMonthTable,{"THÁNG","SỐ ĐƠN","DOANH THU"},1,0);
        v->addLayout(grid);
        v->addStretch();
        return page;
    }

    // ------------------------------------------------------------
    // Nap du lieu
    // ------------------------------------------------------------
    QString fleetStatusOf(const Vehicle &x) const {
        return (!x.available || rentedIds.contains(x.id)) ? "Đang thuê" : "Sẵn sàng";
    }

    void refreshAdminStats(){
        QSqlQuery q(db.connection());
        q.prepare("SELECT COALESCE(SUM(total),0) FROM orders WHERE status IN ('Confirmed','Delivering','In-Rental','Completed') "
                  "AND strftime('%Y-%m',created_at)=?");
        q.addBindValue(QDate::currentDate().toString("yyyy-MM"));
        double revenue=(q.exec()&&q.next()) ? q.value(0).toDouble() : 0;
        q.finish();

        rentedIds.clear();
        QSqlQuery r(db.connection());
        r.exec("SELECT DISTINCT oi.vehicle_id FROM order_items oi JOIN orders o ON o.id=oi.order_id WHERE o.status='In-Rental'");
        while(r.next()) rentedIds.insert(r.value(0).toInt());
        r.finish();
        int total=0, rented=0;
        QSqlQuery vq(db.connection());
        vq.exec("SELECT id,available FROM vehicles");
        while(vq.next()){
            ++total;
            if(vq.value(1).toInt()==0 || rentedIds.contains(vq.value(0).toInt())) ++rented;
        }
        vq.finish();

        int pending=0;
        QSqlQuery pq(db.connection());
        if(pq.exec("SELECT COUNT(*) FROM orders WHERE status='New'") && pq.next()) pending=pq.value(0).toInt();
        pq.finish();

        for(auto &st : adminStats){
            if(!st.revenue) continue;
            st.revenue->setText(vnd(revenue));
            st.revenueSub->setText("Trong tháng "+QDate::currentDate().toString("MM/yyyy"));
            st.rented->setText(QString("%1 / %2").arg(rented).arg(total));
            st.rentedSub->setText(QString("%1 xe sẵn sàng cho thuê").arg(total-rented));
            st.pending->setText(QString("%1").arg(pending,2,10,QChar('0')));
            st.pendingSub->setText("Cần xử lý hôm nay");
        }
        if(fleetNote) fleetNote->setText("Cập nhật "+QDate::currentDate().toString("dd/MM/yyyy"));
    }

    void refreshFleet(){
        if(!adminVehicleTable) return;
        QList<Vehicle> all=vehicleModel.all();
        std::sort(all.begin(),all.end(),[](const Vehicle &a,const Vehicle &b){ return a.id<b.id; });
        const QString kw=fleetSearch ? fleetSearch->text().trimmed() : QString();
        const int stIdx=fleetStatus ? fleetStatus->currentIndex() : 0;
        fleetShown.clear();
        for(const Vehicle &v : all){
            if(!kw.isEmpty() && !v.code.contains(kw,Qt::CaseInsensitive) && !v.name.contains(kw,Qt::CaseInsensitive)) continue;
            const QString st=fleetStatusOf(v);
            if(stIdx==1 && st!="Sẵn sàng") continue;
            if(stIdx==2 && st!="Đang thuê") continue;
            fleetShown.append(v);
        }
        const int total=fleetShown.size();
        const int pages=qMax(1,(total+kFleetPageSize-1)/kFleetPageSize);
        fleetPage=qBound(1,fleetPage,pages);
        const int from=(fleetPage-1)*kFleetPageSize;
        const int count=qMin(kFleetPageSize,total-from);

        adminVehicleTable->clearSelection();
        adminVehicleTable->setRowCount(qMax(0,count));
        for(int i=0;i<count;++i){
            const Vehicle &v=fleetShown[from+i];
            const QString st=fleetStatusOf(v);
            setCell(adminVehicleTable,i,0,v.code);
            adminVehicleTable->item(i,0)->setData(Qt::UserRole,v.id);
            setCell(adminVehicleTable,i,1,v.name,"",true);
            setCell(adminVehicleTable,i,2,categoryLabel(v.category));
            setCell(adminVehicleTable,i,3,vnd(v.price));
            setCell(adminVehicleTable,i,4,st,st=="Sẵn sàng" ? "#16865A" : "#B45309");
            setCell(adminVehicleTable,i,5,"Sửa","#1769E0");
        }
        fleetInfo->setText(QString("Hiển thị %1 / %2 xe").arg(count>0?count:0).arg(total));

        // phan trang
        while(QLayoutItem *it=fleetPager->takeAt(0)){
            if(QWidget *wd=it->widget()){ wd->hide(); wd->deleteLater(); }
            delete it;
        }
        auto pagerButton=[&](const QString &text,int target,bool current){
            auto *b=new QPushButton(text); b->setObjectName("admPager"); b->setCheckable(true); b->setChecked(current);
            b->setCursor(Qt::PointingHandCursor);
            connect(b,&QPushButton::clicked,this,[=]{ fleetPage=target; refreshFleet(); });
            fleetPager->addWidget(b);
        };
        pagerButton("‹",qMax(1,fleetPage-1),false);
        for(int pg=1;pg<=pages;++pg) pagerButton(QString::number(pg),pg,pg==fleetPage);
        pagerButton("›",qMin(pages,fleetPage+1),false);
    }

    void refreshUsers(){
        if(!usersTable) return;
        const QString kw=usersSearch ? usersSearch->text().trimmed() : QString();
        usersTable->setRowCount(0);
        QSqlQuery u(db.connection());
        u.prepare("SELECT id,name,email,phone,role FROM users WHERE (?='' OR name LIKE ? OR email LIKE ? OR phone LIKE ?) ORDER BY id");
        u.addBindValue(kw); u.addBindValue("%"+kw+"%"); u.addBindValue("%"+kw+"%"); u.addBindValue("%"+kw+"%");
        u.exec();
        while(u.next()){
            int r=usersTable->rowCount(); usersTable->insertRow(r);
            setCell(usersTable,r,0,u.value(0).toString());
            setCell(usersTable,r,1,u.value(1).toString(),"",true);
            setCell(usersTable,r,2,u.value(2).toString());
            setCell(usersTable,r,3,u.value(3).toString());
            setCell(usersTable,r,4,roleLabel(u.value(4).toString()),"#1769E0");
            usersTable->item(r,4)->setData(Qt::UserRole,u.value(4).toString());
        }
    }

    void refreshAdminOrders(){
        if(!adminOrdersTable) return;
        adminOrdersTable->setRowCount(0);
        QSqlQuery o(db.connection());
        o.exec("SELECT id,recipient,total,status,payment,created_at FROM orders ORDER BY id DESC");
        while(o.next()){
            int r=adminOrdersTable->rowCount(); adminOrdersTable->insertRow(r);
            setCell(adminOrdersTable,r,0,handoverCode(o.value(0).toInt()),"",true);
            setCell(adminOrdersTable,r,1,o.value(1).toString());
            setCell(adminOrdersTable,r,2,vnd(o.value(2).toDouble()));
            setCell(adminOrdersTable,r,3,statusVi(o.value(3).toString()));
            setCell(adminOrdersTable,r,4,o.value(4).toString());
            setCell(adminOrdersTable,r,5,QDateTime::fromString(o.value(5).toString(),Qt::ISODate).toString("dd/MM/yyyy HH:mm"));
        }
        // 5 don gan nhat cho trang Tong quan
        if(recentOrdersTable){
            recentOrdersTable->setRowCount(0);
            QSqlQuery q(db.connection());
            q.exec("SELECT id,recipient,total,status,created_at FROM orders ORDER BY id DESC LIMIT 5");
            while(q.next()){
                int r=recentOrdersTable->rowCount(); recentOrdersTable->insertRow(r);
                setCell(recentOrdersTable,r,0,handoverCode(q.value(0).toInt()),"",true);
                setCell(recentOrdersTable,r,1,q.value(1).toString());
                setCell(recentOrdersTable,r,2,vnd(q.value(2).toDouble()));
                setCell(recentOrdersTable,r,3,statusVi(q.value(3).toString()));
                setCell(recentOrdersTable,r,4,QDateTime::fromString(q.value(4).toString(),Qt::ISODate).toString("dd/MM/yyyy"));
            }
        }
    }

    void refreshReports(){
        if(!reportStatusTable) return;
        reportStatusTable->setRowCount(0);
        QSqlQuery q(db.connection());
        q.exec("SELECT status,COUNT(*),COALESCE(SUM(total),0) FROM orders GROUP BY status ORDER BY COUNT(*) DESC");
        while(q.next()){
            int r=reportStatusTable->rowCount(); reportStatusTable->insertRow(r);
            setCell(reportStatusTable,r,0,statusVi(q.value(0).toString()),"",true);
            setCell(reportStatusTable,r,1,q.value(1).toString());
            setCell(reportStatusTable,r,2,vnd(q.value(2).toDouble()));
        }
        q.finish();
        reportTopTable->setRowCount(0);
        QSqlQuery t(db.connection());
        t.exec("SELECT code,name,rental_count FROM vehicles ORDER BY rental_count DESC,id LIMIT 5");
        while(t.next()){
            int r=reportTopTable->rowCount(); reportTopTable->insertRow(r);
            setCell(reportTopTable,r,0,t.value(0).toString());
            setCell(reportTopTable,r,1,t.value(1).toString(),"",true);
            setCell(reportTopTable,r,2,t.value(2).toString());
        }
        t.finish();
        reportMonthTable->setRowCount(0);
        QSqlQuery m(db.connection());
        m.exec("SELECT strftime('%m/%Y',created_at),COUNT(*),COALESCE(SUM(total),0) FROM orders "
               "WHERE status IN ('Confirmed','Delivering','In-Rental','Completed') "
               "GROUP BY strftime('%Y-%m',created_at) ORDER BY strftime('%Y-%m',created_at) DESC LIMIT 6");
        while(m.next()){
            int r=reportMonthTable->rowCount(); reportMonthTable->insertRow(r);
            setCell(reportMonthTable,r,0,m.value(0).toString(),"",true);
            setCell(reportMonthTable,r,1,m.value(1).toString());
            setCell(reportMonthTable,r,2,vnd(m.value(2).toDouble()));
        }
    }

    void refreshAdmin(){
        if(!adminStack) return;
        refreshAdminStats();   // phai chay truoc refreshFleet (tinh danh sach xe dang thue)
        refreshFleet();
        refreshUsers();
        refreshAdminOrders();
        refreshReports();
    }

    // ------------------------------------------------------------
    // Hanh dong tren panel "Cap nhat phuong tien"
    // ------------------------------------------------------------
    void showPanelPlaceholder(){
        editVehicle=Vehicle(); addingVehicle=false; editingVehicleId=0;
        panelTitle->setText("Cập nhật phương tiện");
        panelSub->setText("Chọn một xe trong danh sách (hoặc bấm “Thêm xe mới”) để chỉnh sửa.");
        extraFields->hide(); panelDelete->hide();
        nameEdit->clear(); priceSpin->setValue(0); statusEditBox->setCurrentIndex(0);
        panelBody->setEnabled(false);
    }

    void selectFleetRow(int row){
        if(row<0 || !adminVehicleTable->item(row,0)) return;
        const int id=adminVehicleTable->item(row,0)->data(Qt::UserRole).toInt();
        for(const Vehicle &v : fleetShown){
            if(v.id!=id) continue;
            editVehicle=v; addingVehicle=false; editingVehicleId=v.id;
            panelTitle->setText("Cập nhật phương tiện");
            panelSub->setText(v.code+" · "+v.name);
            nameEdit->setText(v.name);
            int ci=adminCategory->findData(v.category); adminCategory->setCurrentIndex(ci>=0?ci:0);
            priceSpin->setValue(v.price);
            editInitialStatus=(fleetStatusOf(v)=="Sẵn sàng") ? 0 : 1;
            statusEditBox->setCurrentIndex(editInitialStatus);
            extraFields->hide(); panelDelete->show();
            panelBody->setEnabled(true);
            return;
        }
    }

    void startAddVehicle(){
        if(currentUser.role!="Admin"){ error("Chỉ quản trị viên mới thêm được phương tiện."); return; }
        adminVehicleTable->clearSelection();
        editVehicle=Vehicle(); addingVehicle=true; editingVehicleId=0; editInitialStatus=0;
        panelTitle->setText("Thêm phương tiện mới");
        panelSub->setText("Điền thông tin xe mới để đưa vào đội xe.");
        nameEdit->clear(); adminCategory->setCurrentIndex(0); priceSpin->setValue(0); statusEditBox->setCurrentIndex(0);
        codeEdit->clear(); plateEdit->clear(); makeEdit->clear(); summaryEdit->clear();
        seatsSpin->setValue(5); fuelBox->setCurrentIndex(0); transmissionBox->setCurrentIndex(0); discountSpin->setValue(0);
        extraFields->show(); panelDelete->hide();
        panelBody->setEnabled(true);
        nameEdit->setFocus();
    }

    void saveFleetVehicle(){
        if(currentUser.role!="Admin"){ error("Chỉ quản trị viên mới thay đổi được đội xe."); return; }
        if(!addingVehicle && !editVehicle.id){ error("Hãy chọn một xe trong danh sách hoặc bấm “Thêm xe mới”."); return; }
        const QString name=nameEdit->text().trimmed();
        if(name.isEmpty()){ error("Vui lòng nhập tên phương tiện."); return; }
        if(priceSpin->value()<=0){ error("Giá thuê / ngày phải lớn hơn 0."); return; }

        Vehicle v=editVehicle;
        v.name=name;
        v.category=adminCategory->currentData().toString();
        v.price=priceSpin->value();
        if(addingVehicle){
            v.code=codeEdit->text().trimmed();
            if(v.code.isEmpty()){ error("Vui lòng nhập mã xe."); return; }
            v.plate=plateEdit->text().trimmed(); v.make=makeEdit->text().trimmed();
            v.seats=seatsSpin->value(); v.fuel=fuelBox->currentText(); v.transmission=transmissionBox->currentText();
            v.discount=discountSpin->value(); v.summary=summaryEdit->text().trimmed();
            v.available=(statusEditBox->currentIndex()==0);
        } else if(statusEditBox->currentIndex()!=editInitialStatus){
            v.available=(statusEditBox->currentIndex()==0);   // admin chu dong doi trang thai
        }
        if(vehicleModel.save(v)){
            info(addingVehicle ? "Đã thêm phương tiện mới vào đội xe." : "Đã lưu thay đổi phương tiện.");
            showPanelPlaceholder();
            refreshAdmin(); refreshVehicles();
        } else {
            error("Không thể lưu phương tiện. Mã xe có thể đã tồn tại.");
        }
    }

    void deleteFleetVehicle(){
        if(currentUser.role!="Admin"){ error("Chỉ quản trị viên mới xóa được phương tiện."); return; }
        if(addingVehicle || !editVehicle.id) return;
        QSqlQuery q(db.connection());
        q.prepare(QString("SELECT COUNT(*) FROM order_items oi JOIN orders o ON o.id=oi.order_id WHERE oi.vehicle_id=? AND o.status IN %1").arg(kActiveStatuses));
        q.addBindValue(editVehicle.id);
        if(q.exec() && q.next() && q.value(0).toInt()>0){
            error("Xe này đang có đơn thuê chưa hoàn tất nên không thể xóa.");
            return;
        }
        q.finish();
        if(QMessageBox::question(this,"Xóa phương tiện","Bạn có chắc chắn muốn xóa "+editVehicle.name+" khỏi đội xe?")!=QMessageBox::Yes) return;
        vehicleModel.remove(editVehicle.id);
        QSqlQuery c(db.connection());
        c.prepare("DELETE FROM cart WHERE vehicle_id=?"); c.addBindValue(editVehicle.id); c.exec();
        showPanelPlaceholder();
        refreshAdmin(); refreshVehicles(); refreshCart();
    }

    void changeUserRole(){
        if(currentUser.role!="Admin"){ error("Chỉ quản trị viên mới đổi được quyền."); return; }
        int r=usersTable->currentRow();
        if(r<0||!usersTable->item(r,0)){ error("Vui lòng chọn một tài khoản trong bảng."); return; }
        const int id=usersTable->item(r,0)->text().toInt();
        if(id==currentUser.id){ error("Bạn không thể tự đổi quyền của chính mình."); return; }
        const QString newRole=userRoleBox->currentData().toString();
        QSqlQuery q(db.connection());
        q.prepare("UPDATE users SET role=? WHERE id=?");
        q.addBindValue(newRole); q.addBindValue(id);
        if(q.exec()&&q.numRowsAffected()>0){
            info("Đã cập nhật quyền thành: "+roleLabel(newRole)+".");
            refreshUsers();
        } else error("Không thể cập nhật quyền.");
    }

    void refreshVehicles(){
        if(!vehicleCardsLayout || !vehicleCardsContainer)return;
        if (isRefreshingVehicles) return;
        isRefreshingVehicles = true;
        // Xóa toàn bộ card cũ khỏi layout
        while (QLayoutItem *item = vehicleCardsLayout->takeAt(0)){
            QWidget *widget = item->widget();
            if (widget) {
                widget->hide();
                delete widget;
            }
            delete item;
        }
        QString cat=categoryBox?categoryBox->currentText():"All";
        auto vs = vehicleModel.all(
            cat,
            searchEdit ? searchEdit->text() : "",
            sortBox ? sortBox->currentData().toString() : "name ASC"
            );

        // Lọc giá thuê
        for(int i = vs.size() - 1; i >= 0; --i){

            double price = vs[i].price * (1 - vs[i].discount / 100.0);

            if(under300 && under300->isChecked() && price >= 300000){
                vs.removeAt(i);
                continue;
            }

            if(from300to1000 && from300to1000->isChecked()
                && (price < 300000 || price > 1000000)){
                vs.removeAt(i);
                continue;
            }

            if(over1000 && over1000->isChecked() && price <= 1000000){
                vs.removeAt(i);
                continue;
            }

            // Lọc số chỗ
            if(seat5 && seat5->isChecked() && vs[i].seats != 5){
                vs.removeAt(i);
                continue;
            }

            if(seat7 && seat7->isChecked() && vs[i].seats != 7){
                vs.removeAt(i);
                continue;
            }
        }
        int col=0,row=0;
        for(const auto &v:std::as_const(vs)){
            auto *card=new QFrame; card->setObjectName("vehicleCard"); card->setMinimumWidth(205); card->setMaximumWidth(390);
            auto *box=new QVBoxLayout(card); box->setContentsMargins(12,12,12,12); box->setSpacing(7);
            auto *visual = new QLabel;
            visual->setObjectName("vehicleVisual");
            visual->setAlignment(Qt::AlignCenter);
            visual->setMinimumHeight(112);
            visual->setFixedHeight(112);
            // Lấy đường dẫn ảnh xe
            QString imagePath = QCoreApplication::applicationDirPath()
                                + "/images/" + getVehicleImage(v);

            // Đọc ảnh
            QPixmap pixmap(imagePath);

            // Hiển thị ảnh
            if (!pixmap.isNull()) {
                visual->setPixmap(
                    pixmap.scaled(
                        300, 112,
                        Qt::KeepAspectRatio,
                        Qt::SmoothTransformation
                        )
                    );
            } else {
                visual->setText("Chưa có ảnh xe");
            }
            auto *name=new QLabel(v.name); name->setObjectName("vehicleName");
            auto *category=new QLabel(categoryLabel(v.category)+" · "+QString::number(v.seats)+" chỗ"); category->setObjectName("mutedText");
            auto *priceRow=new QHBoxLayout; auto *price=new QLabel(QLocale(QLocale::Vietnamese).toString((qlonglong)(v.price*(1-v.discount/100.0)))+"đ"); price->setObjectName("vehiclePrice");
            auto *perDay=new QLabel("/ngày"); perDay->setObjectName("mutedText"); priceRow->addWidget(price); priceRow->addWidget(perDay); priceRow->addStretch();
            auto *actions=new QHBoxLayout; auto *detail=new QPushButton("Xem xe"); auto *add=new QPushButton("＋ Thêm giỏ"); detail->setObjectName("secondaryButton"); add->setObjectName("primaryButton");
            connect(detail,&QPushButton::clicked,this,[=]{selectedVehicleId=v.id;showVehicleDetails();});
            connect(add,&QPushButton::clicked,this,[=]{selectedVehicleId=v.id;addSelectedVehicle();});
            actions->addWidget(detail); actions->addWidget(add);
            box->addWidget(visual); box->addWidget(name); box->addWidget(category);
            if(!v.available){ auto *off=new QLabel("Tạm ngừng cho thuê"); off->setObjectName("statusUnavailable"); box->addWidget(off,0,Qt::AlignLeft); }
            box->addLayout(priceRow); box->addLayout(actions);
            vehicleCardsLayout->addWidget(card,row,col); if(++col==3){col=0;++row;}
        }
        if(vs.isEmpty()){auto *empty=new QLabel("Không tìm thấy xe phù hợp. Hãy thử thay đổi bộ lọc.");empty->setObjectName("mutedText");vehicleCardsLayout->addWidget(empty,0,0,1,3);}
        vehicleCardsContainer->adjustSize();
        isRefreshingVehicles = false;
    }
    void showVehicleDetails(){
        if(!selectedVehicleId){
            error("Vui lòng chọn một xe.");
            return;
        }

        for(const auto &v : vehicleModel.all()){
            if(v.id != selectedVehicleId) continue;

            // Cap nhat luot xem xe
            QSqlQuery q(db.connection());
            q.prepare("UPDATE vehicles SET views=views+1 WHERE id=?");
            q.addBindValue(v.id);
            q.exec();

            double dailyPrice = v.price * (1 - v.discount / 100.0);

            // Cua so chi tiet xe
            QDialog d(this);
            d.setWindowTitle("Chi tiết xe - " + v.name);
            d.resize(1320, 850);
            d.setMinimumSize(1050, 700);

            d.setStyleSheet(R"(
            QDialog {
                background: #F5F7FB;
            }
            QLabel {
                color: #172B45;
                font-size: 10pt;
            }
            QLabel#detailTitle {
                font-size: 27pt;
                font-weight: bold;
                color: #172B45;
            }
            QLabel#detailPrice {
                font-size: 24pt;
                font-weight: bold;
                color: #172B45;
            }
            QLabel#sectionTitle {
                font-size: 16pt;
                font-weight: bold;
                color: #172B45;
            }
            QLabel#specValue {
                font-size: 12pt;
                font-weight: bold;
                color: #172B45;
            }
            QLabel#specName {
                color: #718096;
                font-size: 9pt;
            }
            QFrame#whiteCard {
                background: white;
                border: none;
                border-radius: 12px;
            }
            QDateEdit, QComboBox, QLineEdit {
                background: #F8FAFC;
                border: 1px solid #DCE3EF;
                border-radius: 7px;
                padding: 10px;
                min-height: 25px;
                color: #172B45;
            }
            QPushButton#primaryButton {
                background: #1769E0;
                color: white;
                border: none;
                border-radius: 8px;
                padding: 14px;
                font-size: 11pt;
                font-weight: bold;
            }
            QPushButton#primaryButton:hover {
                background: #0F54C5;
            }
            QPushButton#closeButton {
                background: #E9EEF5;
                color: #475569;
                border: none;
                border-radius: 8px;
                padding: 12px 20px;
            }
        )");

            auto *mainLayout = new QVBoxLayout(&d);
            mainLayout->setContentsMargins(28, 22, 28, 22);
            mainLayout->setSpacing(15);

            // Chia giao dien thanh hai cot
            auto *contentLayout = new QHBoxLayout;
            contentLayout->setSpacing(25);

            // =================================
            // COT TRAI: THONG TIN XE
            // =================================

            auto *leftWidget = new QWidget;
            auto *leftLayout = new QVBoxLayout(leftWidget);
            leftLayout->setContentsMargins(0, 0, 0, 0);
            leftLayout->setSpacing(15);

            auto *breadcrumb = new QLabel(
                "Khám phá xe  /  " + categoryLabel(v.category) + "  /  " + v.name
                );
            breadcrumb->setStyleSheet("color:#718096;");

            auto *headingLayout = new QHBoxLayout;

            auto *title = new QLabel(v.name);
            title->setObjectName("detailTitle");

            auto *status = new QLabel(
                v.available ? "● Sẵn sàng cho thuê" : "● Không khả dụng"
                );
            status->setStyleSheet(
                v.available
                    ? "background:#E2F7ED;color:#16865A;"
                      "padding:8px 14px;border-radius:15px;"
                    : "background:#FDE8E8;color:#DC2626;"
                      "padding:8px 14px;border-radius:15px;"
                );

            headingLayout->addWidget(title);
            headingLayout->addStretch();
            headingLayout->addWidget(status);

            auto *codeLabel = new QLabel(
                v.code + "  ·  " + v.make + " " + categoryLabel(v.category)
                );
            codeLabel->setStyleSheet("color:#718096;");

            // Anh xe
            auto *photo = new QLabel;
            photo->setAlignment(Qt::AlignCenter);
            photo->setMinimumHeight(260);
            photo->setStyleSheet(
                "background:white;border-radius:12px;"
                );

            QString imagePath =
                QCoreApplication::applicationDirPath()
                + "/images/" + getVehicleImage(v);

            QPixmap pixmap(imagePath);

            if(!pixmap.isNull()){
                photo->setPixmap(
                    pixmap.scaled(
                        750, 340,
                        Qt::KeepAspectRatio,
                        Qt::SmoothTransformation
                        )
                    );
            }
            else{
                photo->setText("Chưa có ảnh xe");
            }

            // Tieu de thong so
            auto *specTitle = new QLabel("Thông số kỹ thuật");
            specTitle->setObjectName("sectionTitle");

            // Cac the thong so
            auto *specGrid = new QGridLayout;
            specGrid->setSpacing(12);

            auto addSpec = [&](int row, int col,
                               QString name, QString value){

                auto *card = new QFrame;
                card->setObjectName("whiteCard");

                auto *layout = new QVBoxLayout(card);
                layout->setContentsMargins(15, 12, 15, 12);

                auto *nameLabel = new QLabel(name);
                nameLabel->setObjectName("specName");

                auto *valueLabel = new QLabel(value);
                valueLabel->setObjectName("specValue");

                layout->addWidget(nameLabel);
                layout->addWidget(valueLabel);

                specGrid->addWidget(card, row, col);
            };

            addSpec(0, 0, "Dòng xe", v.make);
            addSpec(0, 1, "Số chỗ", QString::number(v.seats) + " chỗ");
            addSpec(1, 0, "Nhiên liệu", v.fuel);
            addSpec(1, 1, "Hộp số", v.transmission);

            auto *summary = new QLabel(v.summary);
            summary->setWordWrap(true);
            summary->setStyleSheet(
                "background:#EAF2FF;padding:15px;border-radius:10px;"
                );

            leftLayout->addWidget(breadcrumb);
            leftLayout->addLayout(headingLayout);
            leftLayout->addWidget(codeLabel);
            leftLayout->addWidget(photo, 1);
            leftLayout->addWidget(specTitle);
            leftLayout->addLayout(specGrid);
            leftLayout->addWidget(summary);


            // =================================
            // COT PHAI: GIAO DIEN DAT XE
            // =================================

            auto *rightCard = new QFrame;
            rightCard->setObjectName("whiteCard");
            rightCard->setMinimumWidth(390);
            rightCard->setMaximumWidth(510);

            auto *rightLayout = new QVBoxLayout(rightCard);
            rightLayout->setContentsMargins(25, 25, 25, 25);
            rightLayout->setSpacing(13);

            // Gia thue
            auto *price = new QLabel(
                QLocale(QLocale::Vietnamese).toString(
                    (qlonglong)dailyPrice
                    ) + "đ"
                );

            price->setStyleSheet(
                "font-size:25pt;font-weight:bold;color:#172B45;"
                );

            auto *perDay = new QLabel("/ ngày");
            perDay->setStyleSheet("color:#718096;font-size:10pt;");

            auto *priceLayout = new QHBoxLayout;
            priceLayout->addWidget(price);
            priceLayout->addWidget(perDay);
            priceLayout->addStretch();

            // Tieu de
            auto *bookingTitle = new QLabel(
                "Chọn lịch cho hành trình của bạn"
                );
            bookingTitle->setStyleSheet(
                "font-size:12pt;font-weight:bold;"
                );

            // Ngay nhan va tra xe
            auto *dateLayout = new QGridLayout;
            dateLayout->setHorizontalSpacing(12);
            dateLayout->setVerticalSpacing(7);

            auto *pickupLabel = new QLabel("Ngày nhận xe");
            auto *dropoffLabel = new QLabel("Ngày trả xe");

            auto *pickup = new QDateEdit(QDate::currentDate());
            pickup->setCalendarPopup(true);
            pickup->setMinimumDate(QDate::currentDate());
            pickup->setDisplayFormat("dd/MM/yyyy");

            auto *dropoff = new QDateEdit(
                QDate::currentDate().addDays(1)
                );
            dropoff->setCalendarPopup(true);
            dropoff->setMinimumDate(QDate::currentDate().addDays(1));
            dropoff->setDisplayFormat("dd/MM/yyyy");

            dateLayout->addWidget(pickupLabel, 0, 0);
            dateLayout->addWidget(dropoffLabel, 0, 1);
            dateLayout->addWidget(pickup, 1, 0);
            dateLayout->addWidget(dropoff, 1, 1);

            // Lich hien thi truc tiep
            auto *calendar = new QCalendarWidget;
            calendar->setMinimumDate(QDate::currentDate());
            calendar->setGridVisible(false);
            calendar->setVerticalHeaderFormat(
                QCalendarWidget::NoVerticalHeader
                );
            calendar->setFirstDayOfWeek(Qt::Monday);
            calendar->setFixedHeight(270);

            calendar->setStyleSheet(R"(
    QCalendarWidget QWidget {
        background: #F8FAFC;
        color: #172B45;
    }
    QCalendarWidget QToolButton {
        color: #172B45;
        background: transparent;
        font-weight: bold;
        padding: 7px;
    }
    QCalendarWidget QMenu {
        background: white;
    }
    QCalendarWidget QAbstractItemView {
        selection-background-color: #1769E0;
        selection-color: white;
        background: #F8FAFC;
        outline: 0;
    }
)");

            // Chon ngay tren lich
            bool choosingDropoff = false;

            auto updateCalendarSelection = [=](){
                calendar->setSelectedDate(pickup->date());
            };

            // Tram nhan va tra xe
            auto *stationLabel = new QLabel("Trạm nhận và trả xe");

            auto *station = new QComboBox;
            station->addItems({
                "Mỹ Đình, Hà Nội",
                "Cầu Giấy, Hà Nội",
                "Hoàn Kiếm, Hà Nội",
                "Quận 1, TP. Hồ Chí Minh",
                "Thủ Đức, TP. Hồ Chí Minh"
            });

            // Thoi gian va tong tien
            auto *summaryLayout = new QGridLayout;

            auto *daysLabel = new QLabel;
            auto *unitPriceLabel = new QLabel;
            auto *totalLabel = new QLabel;
            auto *daysValue = new QLabel;
            daysValue->setStyleSheet(
                "font-weight:bold;color:#172B45;"
                );

            totalLabel->setStyleSheet(
                "font-size:15pt;font-weight:bold;color:#172B45;"
                );

            summaryLayout->addWidget(daysLabel, 0, 0);
            summaryLayout->addWidget(daysValue, 0, 1, Qt::AlignRight);
            summaryLayout->addWidget(unitPriceLabel, 1, 0);
            summaryLayout->addWidget(totalLabel, 1, 1, Qt::AlignRight);

            // Cap nhat ngay va tong tien
            auto refreshBooking = [=](){
                int days = qMax(
                    1,
                    static_cast<int>(
                        pickup->date().daysTo(dropoff->date())
                        )
                    );

                qlonglong total =
                    static_cast<qlonglong>(days * dailyPrice);

                daysValue->setText(QString("%1 ngày").arg(days));

                unitPriceLabel->setText(
                    QLocale(QLocale::Vietnamese).toString(
                        (qlonglong)dailyPrice
                        ) + "đ × " + QString::number(days) + " ngày"
                    );

                totalLabel->setText(
                    QLocale(QLocale::Vietnamese).toString(total) + "đ"
                    );
            };

            // Trang thai chon ngay tren lich

            // Khi nguoi dung bam vao mot ngay tren lich
            connect(calendar, &QCalendarWidget::clicked,
                    &d, [&](const QDate &date){

                        // Khong cho chon ngay trong qua khu
                        if(date < QDate::currentDate()){
                            return;
                        }

                        if(!choosingDropoff){

                            // Lan bam thu nhat: chon ngay nhan xe
                            pickup->setDate(date);
                            dropoff->setDate(date.addDays(1));

                            choosingDropoff = true;
                        }
                        else{

                            // Lan bam thu hai: chon ngay tra xe
                            if(date <= pickup->date()){
                                pickup->setDate(date);
                                dropoff->setDate(date.addDays(1));
                            }
                            else{
                                dropoff->setDate(date);
                            }

                            choosingDropoff = false;
                        }

                        calendar->setSelectedDate(date);
                        refreshBooking();
                    });

            connect(pickup, &QDateEdit::dateChanged,
                    &d, [=](const QDate &date){

                        if(dropoff->date() <= date){
                            dropoff->setDate(date.addDays(1));
                        }

                        updateCalendarSelection();
                        refreshBooking();
                    });

            connect(dropoff, &QDateEdit::dateChanged,
                    &d, refreshBooking);

            refreshBooking();

            // Nut them gio hang
            auto *addButton = new QPushButton(
                "Thêm vào giỏ hàng  →"
                );
            addButton->setObjectName("primaryButton");

            // Ghi chu dat coc
            auto *depositNote = new QLabel(
                "Đặt cọc 50% ở bước thanh toán tiếp theo."
                );
            depositNote->setAlignment(Qt::AlignCenter);
            depositNote->setStyleSheet(
                "color:#718096;font-size:9pt;"
                );

            // Trang thai kha dung theo NGAY dang chon
            auto *availLabel = new QLabel;
            availLabel->setWordWrap(true);

            auto updateAvailability = [=](){
                QPair<QDate,QDate> conflict;
                bool free = vehicleFreeForDates(db.connection(), v.id,
                                                pickup->date(), dropoff->date(), &conflict);

                // To mau cac ngay da co nguoi dat tren lich
                calendar->setDateTextFormat(QDate(), QTextCharFormat());
                QTextCharFormat booked;
                booked.setBackground(QColor("#FDE8E8"));
                booked.setForeground(QColor("#DC2626"));
                for(const auto &r : bookedRanges(db.connection(), v.id))
                    for(QDate day = r.first; day <= r.second; day = day.addDays(1))
                        calendar->setDateTextFormat(day, booked);

                if(free){
                    availLabel->setText("● Xe còn trống trong khoảng ngày đã chọn");
                    availLabel->setStyleSheet("background:#E2F7ED;color:#16865A;padding:8px 12px;border-radius:8px;");
                    status->setText("● Sẵn sàng cho thuê");
                    status->setStyleSheet("background:#E2F7ED;color:#16865A;padding:8px 14px;border-radius:15px;");
                } else {
                    QString msg = "● Không khả dụng trong khoảng ngày đã chọn";
                    if(conflict.first.isValid())
                        msg += "\nĐã có người đặt từ " + conflict.first.toString("dd/MM/yyyy") +
                               " đến " + conflict.second.toString("dd/MM/yyyy") + ".";
                    if(!conflict.first.isValid()) msg += "\nXe đang tạm ngừng cho thuê.";
                    availLabel->setText(msg);
                    availLabel->setStyleSheet("background:#FDE8E8;color:#DC2626;padding:8px 12px;border-radius:8px;");
                    status->setText("● Không khả dụng (theo ngày chọn)");
                    status->setStyleSheet("background:#FDE8E8;color:#DC2626;padding:8px 14px;border-radius:15px;");
                }
                addButton->setEnabled(free);
            };
            connect(pickup, &QDateEdit::dateChanged, &d, [=](const QDate &){ updateAvailability(); });
            connect(dropoff, &QDateEdit::dateChanged, &d, [=](const QDate &){ updateAvailability(); });
            updateAvailability();

            // Sap xep giao dien
            rightLayout->addLayout(priceLayout);
            rightLayout->addWidget(bookingTitle);
            rightLayout->addLayout(dateLayout);
            rightLayout->addWidget(calendar);
            rightLayout->addWidget(stationLabel);
            rightLayout->addWidget(station);
            rightLayout->addSpacing(5);
            rightLayout->addWidget(availLabel);
            rightLayout->addLayout(summaryLayout);
            rightLayout->addStretch();
            rightLayout->addWidget(addButton);
            rightLayout->addWidget(depositNote);


            // Ghép hai cot
            contentLayout->addWidget(leftWidget, 6);
            contentLayout->addWidget(rightCard, 4);

            mainLayout->addLayout(contentLayout, 1);

            // =================================
            // SU KIEN
            // =================================


            connect(addButton, &QPushButton::clicked, &d, [&](){

                // Kiem tra dang nhap
                if(!currentUser.id){
                    if(!showLoginDialog()){
                        return;
                    }
                }

                // Kiem tra ngay thue
                if(dropoff->date() <= pickup->date()){
                    error("Ngày trả xe phải sau ngày nhận xe.");
                    return;
                }

                // Kiem tra xe kha dung DUNG THEO KHOANG NGAY DA CHON
                QPair<QDate,QDate> conflict;
                if(!vehicleFreeForDates(db.connection(), v.id, pickup->date(), dropoff->date(), &conflict)){
                    error("Xe đã được đặt từ " + conflict.first.toString("dd/MM/yyyy") +
                          " đến " + conflict.second.toString("dd/MM/yyyy") +
                          ". Vui lòng chọn khoảng ngày khác.");
                    return;
                }

                int days = static_cast<int>(
                    pickup->date().daysTo(dropoff->date())
                    );

                bool found = false;

                for(auto &it : cart){
                    if(it.vehicleId == v.id){
                        it.days = days;
                        it.pickupDate = pickup->date();
                        it.dropoffDate = dropoff->date();
                        found = true;
                    }
                }

                if(!found){
                    cart.append({v.id,
                                 1,
                                 days,
                                 pickup->date(),
                                 dropoff->date()
                    });
                }

                persistCart();
                refreshCart();

                info("Đã thêm xe vào giỏ hàng.");
                d.accept();
            });

            d.exec();
            return;
        }
    }

    void addSelectedVehicle(){
        // Bước 1: Kiểm tra đăng nhập
        if(!currentUser.id){
            if(!showLoginDialog()) return;
        }
        // Can chon ngay thue nen mo trang chi tiet de chon lich
        if(selectedVehicleId){ showVehicleDetails(); return; }
        // Bước 2: Kiểm tra xe được chọn
        if(!selectedVehicleId){
            error("Vui lòng chọn một xe.");
            return;
        }
        // Bước 3: Kiểm tra tình trạng xe
        for(const auto &v : vehicleModel.all()){
            if(v.id == selectedVehicleId){
                if(!v.available){
                    error("Xe hiện không khả dụng.");
                    return;
                }
                // Bước 4: Thêm xe vào giỏ hàng
                bool found = false;

                for(auto &it : cart){
                    if(it.vehicleId == selectedVehicleId){
                        it.quantity++;
                        found = true;
                    }
                }

                if(!found){
                    cart.append({selectedVehicleId, 1, 1});
                }

                persistCart();
                refreshCart();
                info("Đã thêm xe vào giỏ hàng.");
                return;
            }
        }
    }

    void persistCart(){
        if(!currentUser.id)return;
        QSqlQuery del(db.connection());
        del.prepare("DELETE FROM cart WHERE user_id=?");
        del.addBindValue(currentUser.id);
        del.exec();
        for(const auto &it: std::as_const(cart)){
            QSqlQuery q(db.connection());
            q.prepare("INSERT INTO cart(user_id,vehicle_id,quantity,days,pickup_date,dropoff_date) VALUES(?,?,?,?,?,?)");
            q.addBindValue(currentUser.id);
            q.addBindValue(it.vehicleId);
            q.addBindValue(it.quantity);
            q.addBindValue(it.days);
            q.addBindValue(it.pickupDate.toString("yyyy-MM-dd"));
            q.addBindValue(it.dropoffDate.toString("yyyy-MM-dd"));
            q.exec();}
    }
    void refreshCart(){
        if(currentUser.id){
            cart.clear();
            QSqlQuery q(db.connection());
            q.prepare("SELECT vehicle_id,quantity,days,pickup_date,dropoff_date FROM cart WHERE user_id=?");
            q.addBindValue(currentUser.id);
            q.exec();
            while(q.next()){
                CartItem it;
                it.vehicleId=q.value(0).toInt();
                it.quantity=1;
                it.days=qMax(1,q.value(2).toInt());
                it.pickupDate=QDate::fromString(q.value(3).toString(),"yyyy-MM-dd");
                it.dropoffDate=QDate::fromString(q.value(4).toString(),"yyyy-MM-dd");
                if(!it.pickupDate.isValid()||!it.dropoffDate.isValid()){ // du lieu cu chua co ngay
                    it.pickupDate=QDate::currentDate();
                    it.dropoffDate=it.pickupDate.addDays(it.days);
                }
                cart.append(it);
            }
        }
        if(!cartTable)return;cartTable->setRowCount(cart.size());double total=0;
        const auto vs=vehicleModel.all();
        for(int i=0;i<cart.size();++i){Vehicle v;
            for(const auto &x:vs)if(x.id==cart[i].vehicleId)v=x;
            double p=v.price*(1-v.discount/100.0);double sub=p*cart[i].days*cart[i].quantity;total+=sub;
            QStringList vals={v.code,v.name,vnd(p),QString::number(cart[i].days),QString::number(cart[i].quantity),vnd(sub),
                              cart[i].pickupDate.toString("dd/MM/yyyy"),cart[i].dropoffDate.toString("dd/MM/yyyy")};
            for(int c=0;c<vals.size();++c)cartTable->setItem(i,c,new QTableWidgetItem(vals[c]));}
        totalLabel->setText("Tạm tính: "+vnd(total));
    }

    void checkoutBooking()
    {
        if(!currentUser.id || currentUser.role != "Customer"){
            error("Vui lòng đăng nhập bằng tài khoản khách hàng trước khi đặt xe.");
            return;
        }

        if(cart.isEmpty()){
            error("Giỏ xe của bạn hiện đang trống.");
            return;
        }

        // Kiem tra lai lich tung xe truoc khi thanh toan
        for(const auto &it : std::as_const(cart)){
            QPair<QDate,QDate> conflict;
            if(!vehicleFreeForDates(db.connection(), it.vehicleId, it.pickupDate, it.dropoffDate, &conflict)){
                error("Một xe trong giỏ đã có người đặt từ " + conflict.first.toString("dd/MM/yyyy") +
                      " đến " + conflict.second.toString("dd/MM/yyyy") +
                      ". Vui lòng xóa xe đó hoặc chọn ngày khác.");
                return;
            }
        }

        // Tinh tong tien
        double subtotal = 0;
        const auto vs = vehicleModel.all();

        for(const auto &it : std::as_const(cart)){
            for(const auto &v : vs){
                if(v.id == it.vehicleId){
                    subtotal += v.price * (1-v.discount/100.0)
                    * it.days * it.quantity;
                }
            }
        }

        double deposit = subtotal * 0.5;

        // Tao cua so thanh toan
        QDialog d(this);
        d.setWindowTitle("VINFAST RENTAL - Hoàn tất đặt xe");
        d.resize(1250, 850);

        d.setStyleSheet(R"(
        QDialog {
            background: #F4F7FC;
        }
        QFrame#card {
            background: white;
            border: none;
            border-radius: 12px;
        }
        QLabel {
            color: #243247;
        }
        QLabel#title {
            font-size: 27px;
            font-weight: 800;
        }
        QLabel#heading {
            font-size: 18px;
            font-weight: 700;
        }
        QLabel#muted {
            color: #7B8798;
            font-size: 12px;
        }
        QLineEdit {
            background: #F8FAFC;
            border: 1px solid #DCE3EC;
            border-radius: 8px;
            padding: 12px;
            min-height: 20px;
        }
        QPushButton {
            border: none;
            border-radius: 8px;
            padding: 12px;
            background: #EAF1FF;
            color: #2874D0;
            font-weight: 600;
        }
        QPushButton#primary {
            background: #1769E8;
            color: white;
        }
        QPushButton#upload {
            background: #F8FAFC;
            border: 1px dashed #B9C9DF;
            color: #2874D0;
            min-height: 75px;
        }
        QCheckBox {
            color: #263445;
        }
    )");

        auto *mainLayout = new QVBoxLayout(&d);
        mainLayout->setContentsMargins(25,20,25,20);
        mainLayout->setSpacing(14);

        // Tieu de
        auto *breadcrumb = new QLabel(
            "Khám phá xe   /   Giỏ xe   /   Thanh toán"
            );
        breadcrumb->setObjectName("muted");
        mainLayout->addWidget(breadcrumb);

        auto *title = new QLabel("Hoàn tất đặt xe");
        title->setObjectName("title");
        mainLayout->addWidget(title);

        auto *steps = new QLabel(
            "01  Chọn xe       ─────       "
            "02  Xác minh & thanh toán       ─────       "
            "03  Nhận xe"
            );
        steps->setStyleSheet("color:#2874D0;font-weight:600;");
        mainLayout->addWidget(steps);

        // Chia hai cot
        auto *columns = new QHBoxLayout;
        columns->setSpacing(22);

        auto *left = new QVBoxLayout;
        left->setSpacing(14);

        auto *right = new QVBoxLayout;
        right->setSpacing(14);

        // =========================
        // 1. XE TRONG GIO HANG
        // =========================

        auto *cartCard = new QFrame;
        cartCard->setObjectName("card");

        auto *cartLayout = new QVBoxLayout(cartCard);
        cartLayout->setContentsMargins(20,18,20,18);

        auto *cartTitle = new QLabel("Xe trong giỏ hàng");
        cartTitle->setObjectName("heading");
        cartLayout->addWidget(cartTitle);

        auto *table = new QTableWidget;
        table->setColumnCount(4);
        table->setHorizontalHeaderLabels({
            "XE / MÃ XE",
            "THỜI GIAN",
            "ĐƠN GIÁ / NGÀY",
            "THÀNH TIỀN"
        });

        table->setRowCount(cart.size());
        table->horizontalHeader()->setSectionResizeMode(
            QHeaderView::Stretch
            );
        table->verticalHeader()->hide();
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionMode(QAbstractItemView::NoSelection);
        table->setFixedHeight(120);

        for(int i=0;i<cart.size();++i){
            Vehicle v;

            for(const auto &x:vs){
                if(x.id==cart[i].vehicleId){
                    v=x;
                    break;
                }
            }

            double price=v.price*(1-v.discount/100.0);
            double total=price*cart[i].days*cart[i].quantity;

            table->setItem(i,0,new QTableWidgetItem(
                                     v.name+" · "+v.code
                                     ));

            table->setItem(i,1,new QTableWidgetItem(
                                     cart[i].pickupDate.toString("dd/MM")+" – "+cart[i].dropoffDate.toString("dd/MM/yyyy")
                                     +" ("+QString::number(cart[i].days)+" ngày)"
                                     ));

            table->setItem(i,2,new QTableWidgetItem(
                                     vnd(price)
                                     ));

            table->setItem(i,3,new QTableWidgetItem(
                                     vnd(total)
                                     ));
        }

        cartLayout->addWidget(table);
        left->addWidget(cartCard);

        // =========================
        // 2. THONG TIN NGUOI THUE
        // =========================

        auto *infoCard = new QFrame;
        infoCard->setObjectName("card");

        auto *infoLayout = new QVBoxLayout(infoCard);
        infoLayout->setContentsMargins(20,18,20,18);
        infoLayout->setSpacing(12);

        auto *infoTitle = new QLabel("Thông tin người thuê & GPLX");
        infoTitle->setObjectName("heading");
        infoLayout->addWidget(infoTitle);

        auto *hint = new QLabel(
            "Thông tin cần khớp với giấy phép lái xe của bạn."
            );
        hint->setObjectName("muted");
        infoLayout->addWidget(hint);

        auto *form = new QGridLayout;

        auto *name = new QLineEdit(currentUser.name);
        auto *license = new QLineEdit;
        auto *address = new QLineEdit(currentUser.address);
        auto *phone = new QLineEdit(currentUser.phone);

        name->setPlaceholderText("Nhập họ và tên");
        license->setPlaceholderText("Nhập số giấy phép lái xe");
        address->setPlaceholderText("Nhập địa chỉ nhận xe");
        phone->setPlaceholderText("Nhập số điện thoại");

        form->addWidget(new QLabel("Họ và tên"),0,0);
        form->addWidget(new QLabel("Số giấy phép lái xe"),0,1);
        form->addWidget(name,1,0);
        form->addWidget(license,1,1);

        form->addWidget(new QLabel("Địa chỉ nhận xe"),2,0);
        form->addWidget(new QLabel("Số điện thoại"),2,1);
        form->addWidget(address,3,0);
        form->addWidget(phone,3,1);

        infoLayout->addLayout(form);

        // Upload GPLX
        QString frontPath;
        QString backPath;

        auto *uploadLayout = new QGridLayout;

        auto *frontBtn = new QPushButton(
            "↑  Tải mặt trước GPLX"
            );
        frontBtn->setObjectName("upload");

        auto *backBtn = new QPushButton(
            "↑  Tải mặt sau GPLX"
            );
        backBtn->setObjectName("upload");

        auto *frontLabel = new QLabel("JPG, PNG · Tối đa 5 MB");
        frontLabel->setObjectName("muted");

        auto *backLabel = new QLabel("JPG, PNG · Tối đa 5 MB");
        backLabel->setObjectName("muted");

        uploadLayout->addWidget(frontBtn,0,0);
        uploadLayout->addWidget(backBtn,0,1);
        uploadLayout->addWidget(frontLabel,1,0);
        uploadLayout->addWidget(backLabel,1,1);

        infoLayout->addLayout(uploadLayout);

        connect(frontBtn,&QPushButton::clicked,&d,[&](){
            QString path=QFileDialog::getOpenFileName(
                &d,"Chọn ảnh mặt trước GPLX","","Ảnh (*.jpg *.jpeg *.png)"
                );

            if(!path.isEmpty()){
                frontPath=path;
                frontLabel->setText(QFileInfo(path).fileName());
            }
        });

        connect(backBtn,&QPushButton::clicked,&d,[&](){
            QString path=QFileDialog::getOpenFileName(
                &d,"Chọn ảnh mặt sau GPLX","","Ảnh (*.jpg *.jpeg *.png)"
                );

            if(!path.isEmpty()){
                backPath=path;
                backLabel->setText(QFileInfo(path).fileName());
            }
        });

        auto *note = new QLabel(
            "Ảnh rõ nét, đủ 4 góc. Hồ sơ được kiểm tra trước khi bàn giao xe."
            );
        note->setObjectName("muted");
        infoLayout->addWidget(note);

        left->addWidget(infoCard);

        // =========================
        // 3. PHUONG THUC THANH TOAN
        // =========================

        auto *paymentCard = new QFrame;
        paymentCard->setObjectName("card");

        auto *paymentLayout = new QVBoxLayout(paymentCard);
        paymentLayout->setContentsMargins(20,18,20,18);

        auto *paymentTitle = new QLabel("Phương thức thanh toán");
        paymentTitle->setObjectName("heading");
        paymentLayout->addWidget(paymentTitle);

        auto *vnpay = new QRadioButton("VNPay");
        auto *qr = new QRadioButton("Quét mã QR");
        vnpay->setChecked(true);

        auto *paymentGroup = new QButtonGroup(&d);
        paymentGroup->addButton(vnpay);
        paymentGroup->addButton(qr);

        auto *paymentRow = new QHBoxLayout;
        paymentRow->addWidget(vnpay);
        paymentRow->addWidget(qr);

        paymentLayout->addLayout(paymentRow);

        auto *paymentHint = new QLabel(
            "Bạn sẽ được chuyển đến cổng thanh toán để hoàn tất giao dịch."
            );
        paymentHint->setObjectName("muted");
        paymentLayout->addWidget(paymentHint);

        left->addWidget(paymentCard);

        // =========================
        // 4. TOM TAT DAT XE
        // =========================

        auto *summaryCard = new QFrame;
        summaryCard->setObjectName("card");

        auto *summary = new QVBoxLayout(summaryCard);
        summary->setContentsMargins(22,22,22,22);
        summary->setSpacing(17);

        auto *summaryTitle = new QLabel("Tóm tắt đặt xe");
        summaryTitle->setObjectName("heading");
        summary->addWidget(summaryTitle);

        auto *summaryName = new QLabel(
            QString::number(cart.size())+" xe trong giỏ hàng"
            );
        summaryName->setStyleSheet("font-weight:700;font-size:15px;");
        summary->addWidget(summaryName);

        auto *totalRow = new QHBoxLayout;
        totalRow->addWidget(new QLabel("Tổng tiền thuê"));
        auto *totalValue = new QLabel(vnd(subtotal));
        totalValue->setStyleSheet("font-weight:700;");
        totalRow->addWidget(totalValue);
        summary->addLayout(totalRow);

        auto *depositRow = new QHBoxLayout;
        depositRow->addWidget(new QLabel("Thanh toán khi nhận xe"));
        auto *depositValue = new QLabel(vnd(subtotal-deposit));
        depositValue->setStyleSheet("font-weight:700;");
        depositRow->addWidget(depositValue);
        summary->addLayout(depositRow);

        auto *depositBox = new QFrame;
        depositBox->setStyleSheet(
            "background:#EDF4FF;border-radius:10px;"
            );

        auto *depositLayout = new QVBoxLayout(depositBox);

        auto *depositHint = new QLabel("Đặt cọc hôm nay · 50%");
        depositHint->setStyleSheet("color:#2874D0;");

        auto *depositAmount = new QLabel(vnd(deposit));
        depositAmount->setStyleSheet(
            "font-size:26px;font-weight:800;color:#1769E8;"
            );

        depositLayout->addWidget(depositHint);
        depositLayout->addWidget(depositAmount);
        summary->addWidget(depositBox);

        auto *agree = new QCheckBox(
            "Tôi đồng ý với điều khoản thuê xe"
            );
        summary->addWidget(agree);

        auto *warning = new QLabel(
            "Vui lòng tải đủ 2 ảnh GPLX trước khi tiếp tục."
            );
        warning->setObjectName("muted");
        summary->addWidget(warning);

        auto *confirm = new QPushButton(
            "Đặt cọc "+vnd(deposit)+"  →"
            );
        confirm->setObjectName("primary");
        summary->addWidget(confirm);

        right->addWidget(summaryCard);

        auto *security = new QFrame;
        security->setObjectName("card");

        auto *securityLayout = new QVBoxLayout(security);
        securityLayout->setContentsMargins(18,18,18,18);

        auto *securityTitle = new QLabel(
            "Thông tin của bạn được bảo mật"
            );
        securityTitle->setStyleSheet("font-weight:700;");

        auto *securityText = new QLabel(
            "GPLX chỉ được dùng để xác minh hồ sơ thuê xe.\n"
            "Kiểm tra kỹ thông tin trước khi thanh toán."
            );
        securityText->setObjectName("muted");

        securityLayout->addWidget(securityTitle);
        securityLayout->addWidget(securityText);

        right->addWidget(security);
        right->addStretch();

        columns->addLayout(left, 2);
        columns->addLayout(right, 1);

        mainLayout->addLayout(columns,1);

        // Xac nhan dat xe
        connect(confirm,&QPushButton::clicked,&d,[&](){

            if(name->text().trimmed().isEmpty() ||
                license->text().trimmed().isEmpty() ||
                address->text().trimmed().isEmpty() ||
                phone->text().trimmed().isEmpty()){
                error("Vui lòng điền đầy đủ thông tin người thuê.");
                return;
            }

            if(!validPhone(phone->text().trimmed())){
                error("Số điện thoại phải gồm 9–12 chữ số.");
                return;
            }

            if(frontPath.isEmpty() || backPath.isEmpty()){
                error("Vui lòng tải đủ ảnh mặt trước và mặt sau GPLX.");
                return;
            }

            if(!agree->isChecked()){
                error("Vui lòng đồng ý với điều khoản thuê xe.");
                return;
            }

            QMessageBox::information(
                &d,
                "Xác nhận",
                "Thông tin đặt xe đã được kiểm tra."
                );

            d.accept();
        });

        if(d.exec()!=QDialog::Accepted)
            return;

        // Giu lai logic tao don hang hien tai
        QString paymentMethod = vnpay->isChecked()
                                    ? "VNPay" : "Quét mã QR";

        if(orderModel.create(
                currentUser.id,
                name->text(),
                address->text(),
                phone->text(),
                license->text(),
                "Nhận xe tại trạm",
                0,
                subtotal,
                paymentMethod,
                cart,
                vehicleModel
                )){
            cart.clear();
            refreshCart();
            refreshOrders();
            refreshVehicles();

            info("Đặt xe thành công!");
        }
        else{
            error("Không thể tạo đơn thuê xe.");
        }
    }


    void refreshOrders(){
        if(!ordersTable) return;

        ordersTable->setRowCount(0);

        int totalCount = 0;
        int pendingCount = 0;
        int completedCount = 0;

        if(currentUser.id){

            // Thống kê đơn hàng
            QSqlQuery countQuery(db.connection());

            countQuery.prepare(
                "SELECT COUNT(*), "
                "SUM(CASE WHEN status='New' THEN 1 ELSE 0 END), "
                "SUM(CASE WHEN status='Completed' THEN 1 ELSE 0 END) "
                "FROM orders WHERE user_id=?"
                );

            countQuery.addBindValue(currentUser.id);

            if(countQuery.exec() && countQuery.next()){
                totalCount = countQuery.value(0).toInt();
                pendingCount = countQuery.value(1).toInt();
                completedCount = countQuery.value(2).toInt();
            }

            // Điều kiện lọc
            QString condition;

            if(ordersFilter == "new")
                condition = " AND o.status='New'";

            else if(ordersFilter == "confirmed")
                condition = " AND o.status IN ('Confirmed','Delivering')";

            else if(ordersFilter == "rental")
                condition = " AND o.status='In-Rental'";

            else if(ordersFilter == "completed")
                condition = " AND o.status='Completed'";

            else if(ordersFilter == "cancelled")
                condition = " AND o.status='Cancelled'";

            // Lấy dữ liệu đơn hàng và xe thuê
            QString sql =
                "SELECT o.id,o.created_at,o.total,o.status,"
                "COALESCE((SELECT GROUP_CONCAT(oi.vehicle_name, ', ') "
                "FROM order_items oi WHERE oi.order_id=o.id),''),"
                "COALESCE((SELECT MAX(oi.days) "
                "FROM order_items oi WHERE oi.order_id=o.id),0),"
                "(SELECT MIN(oi.pickup_date) FROM order_items oi WHERE oi.order_id=o.id),"
                "(SELECT MAX(oi.dropoff_date) FROM order_items oi WHERE oi.order_id=o.id) "
                "FROM orders o WHERE o.user_id=?"
                + condition +
                " ORDER BY o.id DESC";

            QSqlQuery q(db.connection());
            q.prepare(sql);
            q.addBindValue(currentUser.id);

            if(q.exec()){

                while(q.next()){

                    int id = q.value(0).toInt();
                    QString created = q.value(1).toString();
                    double total = q.value(2).toDouble();
                    QString status = q.value(3).toString();
                    QString vehicle = q.value(4).toString();
                    int days = q.value(5).toInt();
                    QDate pickupD = QDate::fromString(q.value(6).toString(),"yyyy-MM-dd");
                    QDate dropoffD = QDate::fromString(q.value(7).toString(),"yyyy-MM-dd");

                    QDateTime date =
                        QDateTime::fromString(created,Qt::ISODate);

                    QString dateText = date.isValid()
                                           ? date.toString("dd/MM/yyyy")
                                           : created;

                    QString statusText;
                    QString statusColor;

                    if(status == "New"){
                        statusText = "Chờ duyệt";
                        statusColor = "#fff2d9";
                    }
                    else if(status == "Confirmed"){
                        statusText = "Đã xác nhận";
                        statusColor = "#eaf1ff";
                    }
                    else if(status == "Completed"){
                        statusText = "Hoàn thành";
                        statusColor = "#e5f4ed";
                    }
                    else if(status == "Cancelled"){
                        statusText = "Đã hủy";
                        statusColor = "#fde8e8";
                    }
                    else if(status == "Delivering"){
                        statusText = "Đang giao xe";
                        statusColor = "#eaf1ff";
                    }
                    else if(status == "In-Rental"){
                        statusText = "Đang thuê";
                        statusColor = "#eaf1ff";
                    }

                    int r = ordersTable->rowCount();
                    ordersTable->insertRow(r);
                    ordersTable->setRowHeight(r,65);

                    auto *idItem = new QTableWidgetItem(
                        QString("HD%1").arg(id,3,10,QChar('0'))
                        );

                    idItem->setData(Qt::UserRole,id);
                    ordersTable->setItem(r,0,idItem);

                    ordersTable->setItem(
                        r,1,new QTableWidgetItem(vehicle)
                        );

                    ordersTable->setItem(
                        r,2,new QTableWidgetItem(
                            (pickupD.isValid() && dropoffD.isValid()
                                 ? pickupD.toString("dd/MM") + " – " + dropoffD.toString("dd/MM/yyyy")
                                 : dateText)
                            + " · " + QString::number(days) + " ngày"
                            )
                        );

                    ordersTable->setItem(
                        r,3,new QTableWidgetItem(
                            QLocale(QLocale::Vietnamese)
                                .toString((qlonglong)total) + "đ"
                            )
                        );

                    auto *statusLabel = new QLabel(statusText);
                    statusLabel->setAlignment(Qt::AlignCenter);

                    statusLabel->setStyleSheet(
                        QString(
                            "background:%1;color:#34445a;"
                            "border-radius:14px;padding:7px;"
                            ).arg(statusColor)
                        );

                    ordersTable->setCellWidget(r,4,statusLabel);

                    auto *actionBox = new QWidget;
                    auto *actionLayout = new QHBoxLayout(actionBox);
                    actionLayout->setContentsMargins(0,0,0,0);
                    actionLayout->setSpacing(4);
                    const QString linkStyle =
                        "QPushButton{background:transparent;color:%1;border:0;padding:8px;font-weight:bold;}"
                        "QPushButton:hover{text-decoration:underline;}";

                    auto *detailBtn = new QPushButton("Chi tiết");
                    detailBtn->setStyleSheet(linkStyle.arg("#1769e8"));
                    connect(detailBtn,&QPushButton::clicked,this,[=](){ showOrderDetails(id); });
                    actionLayout->addWidget(detailBtn);

                    if(status == "New" || status == "Confirmed"){
                        auto *cancelBtn = new QPushButton("Hủy đơn");
                        cancelBtn->setStyleSheet(linkStyle.arg("#dc2626"));
                        connect(cancelBtn,&QPushButton::clicked,this,[=](){ cancelMyOrder(id); });
                        actionLayout->addWidget(cancelBtn);
                    }
                    actionLayout->addStretch();
                    ordersTable->setCellWidget(r,5,actionBox);
                }
            }
        }

        // Cập nhật thống kê
        if(ordersTotalCount)
            ordersTotalCount->setText(
                QString("%1").arg(totalCount,2,10,QChar('0'))
                );

        if(ordersPendingCount)
            ordersPendingCount->setText(
                QString("%1").arg(pendingCount,2,10,QChar('0'))
                );

        if(ordersCompletedCount)
            ordersCompletedCount->setText(
                QString("%1").arg(completedCount,2,10,QChar('0'))
                );

        // Giữ nguyên bảng quản lý của nhân viên
        if(staffTable){
            staffTable->setRowCount(0);

            QSqlQuery s(db.connection());

            s.exec(
                "SELECT id,user_id,recipient,total,status,created_at "
                "FROM orders ORDER BY id DESC"
                );

            while(s.next()){
                int r = staffTable->rowCount();
                staffTable->insertRow(r);

                for(int c=0;c<6;++c){
                    staffTable->setItem(
                        r,c,
                        new QTableWidgetItem(s.value(c).toString())
                        );
                }
            }
        }
    }

    // May trang thai don: New -> Confirmed/Cancelled; Confirmed -> Delivering/In-Rental/Cancelled;
    // Delivering -> In-Rental/Cancelled; In-Rental -> Completed. Completed/Cancelled la trang thai cuoi.
    bool changeOrderStatus(int id,const QString &status,QString *err,bool manageTx=true){
        auto fail=[&](const QString &m){ if(err)*err=m; return false; };
        QSqlQuery cur(db.connection());
        cur.prepare("SELECT status FROM orders WHERE id=?");
        cur.addBindValue(id);
        if(!cur.exec()||!cur.next()) return fail("Không tìm thấy đơn thuê.");
        const QString old=cur.value(0).toString();
        cur.finish();
        static const QMap<QString,QStringList> allowed={
            {"New",{"Confirmed","Cancelled"}},
            {"Confirmed",{"Delivering","In-Rental","Cancelled"}},
            {"Delivering",{"In-Rental","Cancelled"}},
            {"In-Rental",{"Completed"}}
        };
        if(!allowed.value(old).contains(status))
            return fail("Không thể chuyển đơn từ trạng thái '"+old+"' sang '"+status+"'.");

        QSqlDatabase conn=db.connection();
        if(manageTx && !conn.transaction()) return fail("Không thể bắt đầu giao dịch: "+conn.lastError().text());
        QSqlQuery q(conn);
        q.prepare("UPDATE orders SET status=? WHERE id=? AND status=?");
        q.addBindValue(status);q.addBindValue(id);q.addBindValue(old);
        bool ok=q.exec();
        QString dbErr=q.lastError().text();
        if(ok&&q.numRowsAffected()==0){ ok=false; dbErr="đơn không còn ở trạng thái '"+old+"' (có thể vừa được người khác cập nhật)."; }
        q.finish();
        if(ok&&status=="Completed"){
            // Hoan thanh/tra xe: tang so luot thue. Lich xe tu duoc giai phong vi don khong con o trang thai giu xe.
            QSqlQuery rc(conn);
            rc.prepare("UPDATE vehicles SET rental_count=rental_count+1 WHERE id IN (SELECT vehicle_id FROM order_items WHERE order_id=?)");
            rc.addBindValue(id);
            ok=rc.exec();
        }
        if(!ok){ if(manageTx) conn.rollback(); return fail("Cập nhật trạng thái thất bại: "+(dbErr.isEmpty()?conn.lastError().text():dbErr)); }
        if(manageTx) conn.commit();
        return true;
    }

    void updateStaffStatus(QString status){
        if(currentUser.role!="Staff"&&currentUser.role!="Admin"){error("Staff or Admin login required.");return;}
        int r=staffTable->currentRow();if(r<0||!staffTable->item(r,0)){error("Vui lòng bấm chọn một dòng đơn thuê trong bảng trước, rồi mới bấm Update status.");return;}
        int id=staffTable->item(r,0)->text().toInt();
        QString err;
        if(!changeOrderStatus(id,status,&err)){error(err);return;}
        info("Order status updated to "+status+".");
        refreshOrders();refreshAdmin();refreshVehicles();
        if(hoOrderId==id) renderHandover();
    }

    // ------------------------------------------------------------
    // FR-12: Ban giao & tiep nhan xe
    // ------------------------------------------------------------
    QWidget* buildHandoverPage(){
        auto *page=new QWidget; page->setObjectName("hoPage");
        page->setStyleSheet(R"(
            QWidget#hoPage { background:#F4F7FC; }
            QFrame#hoCard { background:white; border-radius:12px; }
            QFrame#hoRow { background:#F8FAFC; border-radius:10px; }
            QFrame#hoInput { background:#F8FAFC; border:1px solid #DCE3EF; border-radius:8px; }
            QFrame#hoInput QLineEdit { background:transparent; border:none; padding:8px 0px; font-size:12pt; color:#172B45; }
            QFrame#hoInput QLabel { background:transparent; }
            QLabel#hoEyebrow { color:#1769E0; font-size:9pt; font-weight:bold; }
            QLabel#hoTitle { color:#172B45; font-size:24pt; font-weight:bold; }
            QLabel#hoMuted { color:#718096; }
            QLabel#hoValue { color:#172B45; font-size:12pt; font-weight:bold; }
            QLabel#hoCap { color:#172B45; font-weight:bold; }
            QPushButton#hoPrimary { background:#1769E0; color:white; border:none; border-radius:8px; padding:13px 20px; font-weight:bold; }
            QPushButton#hoPrimary:hover { background:#0F54C5; }
            QPushButton#hoPrimary:disabled { background:#9DB8EE; }
            QPushButton#hoSecondary { background:#EAF2FF; color:#1769E0; border:none; border-radius:8px; padding:13px 20px; font-weight:bold; }
            QPushButton#hoSecondary:disabled { color:#9DB8EE; }
            QPushButton#hoMode { background:#EAF2FF; color:#1769E0; border:none; border-radius:8px; padding:13px 26px; font-weight:bold; }
            QPushButton#hoMode:checked { background:#1769E0; color:white; }
            QTextEdit, QLineEdit { background:#F8FAFC; border:1px solid #DCE3EF; border-radius:8px; padding:9px; color:#172B45; }
            QSpinBox { background:#F8FAFC; border:1px solid #DCE3EF; border-radius:8px; padding:6px 12px; min-height:28px; color:#172B45; }
        )");
        auto *root=new QVBoxLayout(page);
        root->setContentsMargins(28,22,28,22); root->setSpacing(14);

        // Tieu de + o tra cuu ma don
        auto *header=new QHBoxLayout;
        auto *titleBox=new QVBoxLayout;
        auto *eyebrow=new QLabel("VẬN HÀNH TẠI TRẠM"); eyebrow->setObjectName("hoEyebrow");
        auto *title=new QLabel("Bàn giao & tiếp nhận xe"); title->setObjectName("hoTitle");
        auto *sub=new QLabel("Thực hiện kiểm tra an toàn trước mỗi hành trình."); sub->setObjectName("hoMuted");
        titleBox->addWidget(eyebrow); titleBox->addWidget(title); titleBox->addWidget(sub);
        header->addLayout(titleBox,1);

        auto *searchCard=new QFrame; searchCard->setObjectName("hoCard"); searchCard->setMinimumWidth(460);
        auto *searchLayout=new QHBoxLayout(searchCard); searchLayout->setContentsMargins(12,10,12,10);
        ho.search=new QLineEdit; ho.search->setPlaceholderText("Nhập mã đơn thuê (ví dụ HD001)...");
        auto *find=new QPushButton("Tra cứu"); find->setObjectName("hoPrimary");
        searchLayout->addWidget(ho.search,1); searchLayout->addWidget(find);
        header->addWidget(searchCard,0,Qt::AlignTop);
        root->addLayout(header);

        // Chuyen che do
        auto *modeRow=new QHBoxLayout;
        ho.modeHandover=new QPushButton("Bàn giao xe");
        ho.modeReturn=new QPushButton("Tiếp nhận trả xe");
        auto *modeGroup=new QButtonGroup(page); modeGroup->setExclusive(true);
        for(QPushButton *b : {ho.modeHandover, ho.modeReturn}){
            b->setObjectName("hoMode"); b->setCheckable(true); modeGroup->addButton(b); modeRow->addWidget(b);
        }
        ho.modeHandover->setChecked(true);
        modeRow->addStretch();
        root->addLayout(modeRow);

        ho.empty=new QLabel("Nhập mã đơn thuê (ví dụ HD001) hoặc chọn một đơn ở tab \"Danh sách đơn thuê\" rồi bấm \"Mở bàn giao / nhận xe\".");
        ho.empty->setObjectName("hoMuted"); ho.empty->setAlignment(Qt::AlignCenter); ho.empty->setWordWrap(true);
        ho.empty->setMinimumHeight(220);
        root->addWidget(ho.empty);

        ho.body=new QWidget;
        auto *bodyLayout=new QHBoxLayout(ho.body); bodyLayout->setContentsMargins(0,0,0,0); bodyLayout->setSpacing(18);

        // ---- Cot trai: thong tin don thue
        auto *left=new QFrame; left->setObjectName("hoCard"); left->setMinimumWidth(320); left->setMaximumWidth(400); left->setMinimumHeight(700);
        auto *ll=new QVBoxLayout(left); ll->setContentsMargins(22,22,22,22); ll->setSpacing(4);
        ho.code=new QLabel; ho.code->setStyleSheet("font-size:17pt;font-weight:bold;color:#172B45;");
        ho.badge=new QLabel; ho.badge->setAlignment(Qt::AlignCenter);
        auto *badgeRow=new QHBoxLayout; badgeRow->addWidget(ho.badge); badgeRow->addStretch();
        ll->addWidget(ho.code); ll->addLayout(badgeRow);
        auto addInfo=[&](const QString &caption,QLabel *&value){
            auto *c=new QLabel(caption); c->setObjectName("hoMuted");
            value=new QLabel; value->setObjectName("hoValue"); value->setWordWrap(true);
            ll->addSpacing(8); ll->addWidget(c); ll->addWidget(value);
        };
        addInfo("Khách hàng",ho.customer);
        addInfo("Số điện thoại",ho.phone);
        addInfo("Xe bàn giao",ho.vehicle);
        addInfo("Biển số",ho.plate);
        addInfo("Lịch thuê",ho.period);
        addInfo("Tiền đặt cọc",ho.deposit);
        ll->addStretch();
        auto *originalNote=new QLabel("<b style='color:#1769E0'>Đối chiếu bản gốc tại trạm</b><br>"
                                      "Kiểm tra CCCD và GPLX của người thuê trước khi xác nhận bàn giao.");
        originalNote->setWordWrap(true);
        originalNote->setStyleSheet("background:#EAF2FF;border-radius:10px;padding:14px;color:#40536E;");
        ll->addWidget(originalNote);

        // ---- Cot phai: danh muc kiem tra
        auto *right=new QFrame; right->setObjectName("hoCard");
        right->setMinimumHeight(700);
        auto *rl=new QVBoxLayout(right); rl->setContentsMargins(24,22,24,22); rl->setSpacing(12);
        auto *rh=new QHBoxLayout;
        auto *rt=new QVBoxLayout;
        ho.checkTitle=new QLabel; ho.checkTitle->setStyleSheet("font-size:17pt;font-weight:bold;color:#172B45;");
        ho.progress=new QLabel;
        rt->addWidget(ho.checkTitle); rt->addWidget(ho.progress);
        ho.checkDate=new QLabel; ho.checkDate->setObjectName("hoMuted");
        rh->addLayout(rt,1); rh->addWidget(ho.checkDate,0,Qt::AlignTop);
        rl->addLayout(rh);

        for(int i=0;i<3;++i){
            auto *rowFrame=new QFrame; rowFrame->setObjectName("hoRow");
            auto *rowLayout=new QVBoxLayout(rowFrame); rowLayout->setContentsMargins(16,12,16,12); rowLayout->setSpacing(2);
            ho.chk[i]=new QCheckBox; ho.chk[i]->setStyleSheet("font-size:11pt;font-weight:bold;color:#172B45;");
            ho.desc[i]=new QLabel; ho.desc[i]->setObjectName("hoMuted"); ho.desc[i]->setIndent(26);
            rowLayout->addWidget(ho.chk[i]); rowLayout->addWidget(ho.desc[i]);
            rl->addWidget(rowFrame);
            connect(ho.chk[i],&QCheckBox::toggled,this,[=](bool){ refreshHandoverChecks(); });
        }

        auto *form=new QHBoxLayout; form->setSpacing(18);
        auto *formLeft=new QVBoxLayout;
        auto *odoCap=new QLabel("Số ODO hiện tại"); odoCap->setObjectName("hoCap");
        // O nhap ODO: khung vien + o nhap khong vien + chu "km" ben phai (giong mau)
        ho.odoFrame=new QFrame; ho.odoFrame->setObjectName("hoInput"); ho.odoFrame->setMinimumHeight(48);
        auto *odoLayout=new QHBoxLayout(ho.odoFrame); odoLayout->setContentsMargins(14,0,14,0); odoLayout->setSpacing(6);
        ho.odoEdit=new QLineEdit; ho.odoEdit->setPlaceholderText("Nhập số km hiện tại");
        ho.odoEdit->setValidator(new QIntValidator(0,9999999,ho.odoEdit));
        auto *kmLabel=new QLabel("km"); kmLabel->setObjectName("hoMuted");
        odoLayout->addWidget(ho.odoEdit,1); odoLayout->addWidget(kmLabel);
        // Muc pin: dat san 96% (> 90%), khong hien o nhap
        ho.battery=new QSpinBox(page); ho.battery->setRange(0,100); ho.battery->setValue(96); ho.battery->hide();
        formLeft->addWidget(odoCap); formLeft->addWidget(ho.odoFrame);
        formLeft->addStretch();
        auto *formRight=new QVBoxLayout;
        auto *noteCap=new QLabel("Ghi chú hiện trạng"); noteCap->setObjectName("hoCap");
        ho.note=new QTextEdit; ho.note->setPlaceholderText("Ví dụ: 1 vết xước nhỏ ở cản sau. Khách hàng đã xác nhận hiện trạng.");
        ho.note->setFixedHeight(120);
        formRight->addWidget(noteCap); formRight->addWidget(ho.note);
        form->addLayout(formLeft,1); form->addLayout(formRight,1);
        rl->addLayout(form);
        
        ho.confirmChk=new QCheckBox;
        rl->addWidget(ho.confirmChk);
        ho.hint=new QLabel; ho.hint->setWordWrap(true);
        ho.hint->setStyleSheet("background:#FFF4DC;color:#B45309;border-radius:8px;padding:10px;");
        rl->addWidget(ho.hint);
        rl->addStretch();
        auto *buttons=new QHBoxLayout;
        ho.save=new QPushButton("Lưu biên bản"); ho.save->setObjectName("hoSecondary");
        ho.confirm=new QPushButton; ho.confirm->setObjectName("hoPrimary");
        buttons->addWidget(ho.save,1); buttons->addWidget(ho.confirm,3);
        rl->addLayout(buttons);

        bodyLayout->addWidget(left,0); bodyLayout->addWidget(right,1);
        root->addWidget(ho.body,1);
        ho.body->hide();

        connect(find,&QPushButton::clicked,this,[=]{ searchHandover(); });
        connect(ho.search,&QLineEdit::returnPressed,this,[=]{ searchHandover(); });
        connect(ho.modeHandover,&QPushButton::clicked,this,[=]{ hoReturnMode=false; renderHandover(); });
        connect(ho.modeReturn,&QPushButton::clicked,this,[=]{ hoReturnMode=true; renderHandover(); });
        connect(ho.save,&QPushButton::clicked,this,[=]{ saveHandoverClicked(); });
        connect(ho.confirm,&QPushButton::clicked,this,[=]{ confirmHandoverAction(); });
        return page;
    }

    int odoValue() const { return ho.odoEdit->text().trimmed().toInt(); }
    void setOdo(int v){ ho.odoEdit->setText(v>0 ? QString::number(v) : QString()); }

    void searchHandover(){
        QString digits;
        for(const QChar &ch : ho.search->text()) if(ch.isDigit()) digits += ch;
        if(digits.isEmpty()){ error("Vui lòng nhập mã đơn thuê, ví dụ HD001."); return; }
        loadHandoverOrder(digits.toInt());
    }

    void loadHandoverOrder(int id){
        QSqlQuery q(db.connection());
        q.prepare("SELECT status FROM orders WHERE id=?");
        q.addBindValue(id);
        if(!q.exec()||!q.next()){ error("Không tìm thấy đơn thuê "+handoverCode(id)+"."); return; }
        hoOrderId=id;
        hoReturnMode=(q.value(0).toString()=="In-Rental");   // xe dang thue -> chuyen sang tiep nhan tra xe
        ho.search->setText(handoverCode(id));
        renderHandover();
    }

    static QString hoStatusText(const QString &st){
        if(st=="New") return "Chờ duyệt";
        if(st=="Confirmed"||st=="Delivering") return "Chờ bàn giao";
        if(st=="In-Rental") return "Đang thuê";
        if(st=="Completed") return "Hoàn thành";
        if(st=="Cancelled") return "Đã hủy";
        return st;
    }

    void refreshHandoverChecks(){
        if(!ho.progress) return;
        int n=0;
        for(QCheckBox *c : ho.chk) if(c->isChecked()) ++n;
        ho.progress->setText(QString("%1 / 3 hạng mục đã kiểm tra").arg(n));
        ho.progress->setStyleSheet(n==3 ? "color:#16865A;" : "color:#B45309;");
        const bool ret=hoReturnMode;
        ho.desc[0]->setText(ho.chk[0]->isChecked()
            ? "Đã kiểm tra · Mức pin hiện tại "+QString::number(ho.battery->value())+"%"
            : (ret ? "Ghi nhận mức pin khi nhận lại xe" : "Yêu cầu mức pin trên 90% trước khi bàn giao"));
        ho.desc[1]->setText(ho.chk[1]->isChecked()
            ? "Đã kiểm tra ngoại thất · ghi chú hư hại (nếu có) ở ô ghi chú hiện trạng"
            : "Kiểm tra vết xước, va chạm quanh thân xe");
        ho.desc[2]->setText(ho.chk[2]->isChecked()
            ? (ret ? "Đầy đủ giấy tờ xe và phụ kiện" : "Đầy đủ giấy đăng ký và bảo hiểm còn hạn")
            : (ret ? "Kiểm tra giấy tờ xe và phụ kiện đi kèm" : "Kiểm tra giấy đăng ký và bảo hiểm còn hạn"));
    }

    void renderHandover(){
        ho.modeHandover->setChecked(!hoReturnMode);
        ho.modeReturn->setChecked(hoReturnMode);
        if(!hoOrderId){ ho.body->hide(); ho.empty->show(); return; }

        QSqlQuery q(db.connection());
        q.prepare("SELECT recipient,phone,total,status FROM orders WHERE id=?");
        q.addBindValue(hoOrderId);
        if(!q.exec()||!q.next()){ hoOrderId=0; ho.body->hide(); ho.empty->show(); return; }
        const QString recipient=q.value(0).toString(), phone=q.value(1).toString();
        const double total=q.value(2).toDouble();
        hoStatus=q.value(3).toString();

        QStringList vehicles, plates; QDate minPickup, maxDropoff;
        QSqlQuery it(db.connection());
        it.prepare("SELECT oi.vehicle_name,v.code,v.plate,oi.pickup_date,oi.dropoff_date "
                   "FROM order_items oi LEFT JOIN vehicles v ON v.id=oi.vehicle_id WHERE oi.order_id=?");
        it.addBindValue(hoOrderId);
        it.exec();
        while(it.next()){
            vehicles << it.value(0).toString()+" · "+it.value(1).toString();
            plates << maskText(it.value(2).toString(),4,2);
            QDate a=QDate::fromString(it.value(3).toString(),"yyyy-MM-dd");
            QDate b=QDate::fromString(it.value(4).toString(),"yyyy-MM-dd");
            if(a.isValid() && (!minPickup.isValid()||a<minPickup)) minPickup=a;
            if(b.isValid() && (!maxDropoff.isValid()||b>maxDropoff)) maxDropoff=b;
        }

        ho.code->setText("Đơn thuê "+handoverCode(hoOrderId));
        ho.badge->setText("●  "+hoStatusText(hoStatus));
        if(hoStatus=="Confirmed"||hoStatus=="Delivering"||hoStatus=="New")
            ho.badge->setStyleSheet("background:#FFF1D6;color:#8A5A00;border-radius:14px;padding:6px 14px;font-weight:bold;");
        else if(hoStatus=="Cancelled")
            ho.badge->setStyleSheet("background:#FDE8E8;color:#DC2626;border-radius:14px;padding:6px 14px;font-weight:bold;");
        else
            ho.badge->setStyleSheet("background:#E2F7ED;color:#16865A;border-radius:14px;padding:6px 14px;font-weight:bold;");
        ho.customer->setText(recipient);
        ho.phone->setText(maskText(phone,3,2));
        ho.vehicle->setText(vehicles.join("\n"));
        ho.plate->setText(plates.join("\n"));
        ho.period->setText(minPickup.isValid() && maxDropoff.isValid()
                               ? minPickup.toString("dd/MM/yyyy")+" – "+maxDropoff.toString("dd/MM/yyyy") : "—");
        ho.deposit->setText(QLocale(QLocale::Vietnamese).toString((qlonglong)(total*0.5))+"đ · Đã nhận");

        const bool ret=hoReturnMode;
        ho.checkTitle->setText(ret ? "Kiểm tra khi nhận lại xe" : "Kiểm tra trước bàn giao");
        ho.checkDate->setText("Ngày "+QDate::currentDate().toString("dd/MM/yyyy")+" · Trạm Mỹ Đình");
        ho.chk[0]->setText(ret ? "Mức pin khi nhận lại" : "Mức pin trên 90%");
        ho.chk[1]->setText(ret ? "Kiểm tra ngoại thất & hư hại" : "Kiểm tra ngoại thất");
        ho.chk[2]->setText(ret ? "Giấy tờ & phụ kiện đầy đủ" : "Giấy tờ xe & bảo hiểm");
        ho.confirmChk->setText(ret ? "Đã đối chiếu tình trạng xe và thống nhất biên bản với khách hàng."
                                   : "Đã đối chiếu GPLX và thống nhất biên bản với khách hàng.");
        ho.confirm->setText(ret ? "Xác nhận đã nhận lại xe  →" : "Xác nhận bàn giao hoàn tất  →");

        // ODO luc ban giao (de doi chieu khi nhan lai xe)
        hoHandoverOdo=0;
        QSqlQuery hq(db.connection());
        hq.prepare("SELECT odo FROM handover_records WHERE order_id=? AND kind='handover' AND confirmed=1");
        hq.addBindValue(hoOrderId);
        if(hq.exec() && hq.next()) hoHandoverOdo=hq.value(0).toInt();

        // Nap lai bien ban da luu (neu co)
        QSqlQuery r(db.connection());
        r.prepare("SELECT battery,battery_ok,exterior_ok,docs_ok,odo,note,license_ok FROM handover_records WHERE order_id=? AND kind=?");
        r.addBindValue(hoOrderId); r.addBindValue(QString(ret ? "return" : "handover"));
        if(r.exec() && r.next()){
            ho.battery->setValue(r.value(0).toInt());
            ho.chk[0]->setChecked(r.value(1).toInt()!=0);
            ho.chk[1]->setChecked(r.value(2).toInt()!=0);
            ho.chk[2]->setChecked(r.value(3).toInt()!=0);
            setOdo(r.value(4).toInt());
            ho.note->setPlainText(r.value(5).toString());
            ho.confirmChk->setChecked(r.value(6).toInt()!=0);
        } else {
            ho.battery->setValue(96);
            for(QCheckBox *c : ho.chk) c->setChecked(false);
            setOdo(ret ? hoHandoverOdo : 0);
            ho.note->clear();
            ho.confirmChk->setChecked(false);
        }

        // Cho phep thao tac hay chi xem
        const bool staff=(currentUser.role=="Staff"||currentUser.role=="Admin");
        const bool canAct= ret ? (hoStatus=="In-Rental")
                               : (hoStatus=="Confirmed"||hoStatus=="Delivering");
        QString hint;
        if(!staff) hint="Chỉ nhân viên hoặc quản trị viên mới thực hiện được bàn giao / nhận xe.";
        else if(!canAct){
            if(ret) hint="Chỉ nhận lại được xe của đơn đang thuê (In-Rental). Trạng thái hiện tại: "+hoStatusText(hoStatus)+".";
            else if(hoStatus=="New") hint="Đơn đang chờ duyệt. Hãy xác nhận đơn (Confirmed) ở tab \"Danh sách đơn thuê\" trước khi bàn giao.";
            else if(hoStatus=="In-Rental") hint="Xe đã được bàn giao. Chuyển sang \"Tiếp nhận trả xe\" khi khách trả xe.";
            else hint="Đơn đã "+QString(hoStatus=="Cancelled" ? "bị hủy" : "hoàn thành")+" nên không thể bàn giao.";
        }
        const bool enable = staff && canAct;
        for(QCheckBox *c : ho.chk) c->setEnabled(enable);
        ho.battery->setEnabled(enable); ho.odoFrame->setEnabled(enable); ho.note->setEnabled(enable);
        ho.confirmChk->setEnabled(enable); ho.save->setEnabled(enable); ho.confirm->setEnabled(enable);
        ho.hint->setText(hint); ho.hint->setVisible(!hint.isEmpty());

        ho.empty->hide(); ho.body->show();
        refreshHandoverChecks();
    }

    bool saveHandoverRecord(bool confirmed){
        QSqlQuery q(db.connection());
        q.prepare("INSERT OR REPLACE INTO handover_records(order_id,kind,battery,battery_ok,exterior_ok,docs_ok,odo,note,license_ok,staff_id,confirmed,updated_at) "
                  "VALUES(?,?,?,?,?,?,?,?,?,?,?,?)");
        q.addBindValue(hoOrderId);
        q.addBindValue(QString(hoReturnMode ? "return" : "handover"));
        q.addBindValue(ho.battery->value());
        q.addBindValue(ho.chk[0]->isChecked() ? 1 : 0);
        q.addBindValue(ho.chk[1]->isChecked() ? 1 : 0);
        q.addBindValue(ho.chk[2]->isChecked() ? 1 : 0);
        q.addBindValue(odoValue());
        q.addBindValue(ho.note->toPlainText().trimmed());
        q.addBindValue(ho.confirmChk->isChecked() ? 1 : 0);
        q.addBindValue(currentUser.id);
        q.addBindValue(confirmed ? 1 : 0);
        q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
        return q.exec();
    }

    void saveHandoverClicked(){
        if(!hoOrderId) return;
        if(currentUser.role!="Staff"&&currentUser.role!="Admin"){ error("Staff or Admin login required."); return; }
        if(saveHandoverRecord(false)) info("Đã lưu biên bản. Bạn có thể quay lại hoàn tất sau.");
        else error("Không thể lưu biên bản.");
    }

    void confirmHandoverAction(){
        if(!hoOrderId) return;
        if(currentUser.role!="Staff"&&currentUser.role!="Admin"){ error("Staff or Admin login required."); return; }
        const bool ret=hoReturnMode;

        if(ret ? hoStatus!="In-Rental" : !(hoStatus=="Confirmed"||hoStatus=="Delivering")){
            error("Trạng thái đơn hiện tại không cho phép thao tác này.");
            return;
        }
        if(!ho.chk[0]->isChecked()||!ho.chk[1]->isChecked()||!ho.chk[2]->isChecked()){
            error("Vui lòng hoàn tất cả 3 hạng mục kiểm tra.");
            return;
        }
        if(!ret && ho.battery->value()<=90){
            error("Mức pin phải trên 90% mới được bàn giao xe.");
            return;
        }
        if(ret && odoValue()<hoHandoverOdo){
            error("Số ODO khi nhận lại không được nhỏ hơn số ODO lúc bàn giao ("+QString::number(hoHandoverOdo)+" km).");
            return;
        }
        if(!ho.confirmChk->isChecked()){
            error("Vui lòng xác nhận đã đối chiếu và thống nhất biên bản với khách hàng.");
            return;
        }

        QSqlDatabase conn=db.connection();
        if(!conn.transaction()){ error("Không thể bắt đầu giao dịch."); return; }
        QString err;
        // Luu bien ban va doi trang thai trong CUNG mot giao dich
        if(!saveHandoverRecord(true) || !changeOrderStatus(hoOrderId, ret ? "Completed" : "In-Rental", &err, false)){
            conn.rollback();
            error(err.isEmpty() ? "Không thể lưu biên bản." : err);
            return;
        }
        conn.commit();

        info(ret ? "Đã nhận lại xe. Đơn thuê hoàn thành và lịch xe đã được giải phóng."
                 : "Bàn giao hoàn tất. Đơn thuê chuyển sang trạng thái In-Rental.");
        refreshOrders(); refreshAdmin(); refreshVehicles();
        renderHandover();
    }

};

int main(int argc,char *argv[]){
    QApplication app(argc,argv);
    QApplication::setStyle("Fusion");
    app.setStyleSheet(R"(
        QWidget { font-family: "Segoe UI"; font-size: 10pt; color: #172b4d; }
        QMainWindow { background: #f4f6fb; }
        QTabWidget::pane { border: 0; background: #f4f6fb; }
        QTabBar::tab { background: #ffffff; color: #52627a; padding: 13px 22px; margin-right: 4px; border: 0; border-bottom: 3px solid transparent; }
        QTabBar::tab:selected { color: #1264e8; font-weight: 700; border-bottom: 3px solid #1264e8; }
        QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QDateEdit { background: #ffffff; border: 1px solid #dce3ef; border-radius: 7px; padding: 9px 10px; min-height: 18px; }
        QPushButton { background: #eaf1ff; color: #145bd7; border: 0; border-radius: 7px; padding: 9px 14px; font-weight: 600; }
        QPushButton:hover { background: #dce8ff; }
        QPushButton#primaryButton { background: #1767e8; color: white; }
        QPushButton#primaryButton:hover { background: #0f54c7; }
        QPushButton#secondaryButton { background: #eef3fb; color: #28517f; }
        QFrame#heroBanner { background: #15365e; border-radius: 13px; }
        QLabel#heroEyebrow { color: #b8d2ff; font-size: 9pt; font-weight: 700; }
        QLabel#heroTitle { color: white; font-size: 23pt; font-weight: 800; }
        QLabel#heroSubtitle, QLabel#heroSteps { color: #e4edff; }
        QLabel#heroSteps { background: #24486f; border-radius: 9px; padding: 12px; }
        QFrame#filterPanel, QFrame#vehicleCard { background: white; border: 1px solid #e7ebf3; border-radius: 11px; }
        QFrame#vehicleCard:hover { border: 1px solid #9bbcf4; }
        QLabel#vehicleVisual { background: #eef3fb; color: #2863ad; border-radius: 8px; font-size: 48pt; }
        QLabel#vehicleName { font-size: 12pt; font-weight: 700; }
        QLabel#vehiclePrice { color: #173c70; font-size: 14pt; font-weight: 800; }
        QLabel#catalogTitle { font-size: 17pt; font-weight: 800; }
        QLabel#sectionTitle { font-weight: 800; color: #203a60; }
        QLabel#filterCaption { color: #73829a; font-size: 8pt; font-weight: 800; }
        QLabel#mutedText, QLabel#locationLabel { color: #718096; }
        QLabel#statusUnavailable { color: #b45309; background: #fff4dc; border-radius: 5px; padding: 5px; }
        QLabel#detailVisual { background: #edf3fb; color: #2863ad; border-radius: 12px; font-size: 65pt; }
        QLabel#detailTitle { font-size: 21pt; font-weight: 800; }
        QLabel#detailSpecs { line-height: 1.5; color: #4b5c73; }
        QTableWidget { background: white; alternate-background-color: #f7f9fc; border: 1px solid #e4eaf2; gridline-color: #edf0f5; border-radius: 8px; }
        QHeaderView::section { background: #edf2f9; color: #40536e; padding: 9px; border: 0; font-weight: 700; }
        QGroupBox { background: white; border: 1px solid #e3e9f2; border-radius: 9px; margin-top: 12px; padding: 12px; font-weight: 700; }
        QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 5px; }
        QStatusBar { background: #ffffff; color: #607086; }
    )");
    MainWindow window;window.show();
    return app.exec();
}
