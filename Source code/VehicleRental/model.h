#pragma once
// ============================================================
// model.h - Lop nghiep vu + truy cap SQLite (KHONG phu thuoc Qt Widgets)
// Dung chung cho ung dung (main.cpp) va unit test (tests/test_model.cpp)
// ============================================================
#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>
#include <QPair>
#include <QDate>
#include <QDateTime>
#include <QLocale>
#include <QVariant>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QSqlError>
#include <QCryptographicHash>
#include <utility>

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
inline QString getVehicleImage(const Vehicle &v){
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

inline QString hashPassword(const QString &plain){
    return QString::fromLatin1(QCryptographicHash::hash(("vrs$"+plain).toUtf8(),QCryptographicHash::Sha256).toHex());
}
inline bool validPhone(const QString &p){
    if(p.size()<9||p.size()>12) return false;
    for(const QChar &c : p) if(!c.isDigit()) return false;
    return true;
}
inline bool validEmail(const QString &e){
    const int at=e.indexOf('@');
    return at>0 && e.indexOf('.',at)>at+1 && !e.endsWith('.') && !e.contains(' ');
}

class Database {
public:
    explicit Database(QString path="vehicle_rental.db", QString connName="vehicle_connection")
        : path_(std::move(path)), connName_(std::move(connName)) {}
    bool open() {
        if (QSqlDatabase::contains(connName_))
            db = QSqlDatabase::database(connName_);
        else {
            db = QSqlDatabase::addDatabase("QSQLITE", connName_);
            db.setDatabaseName(path_);
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
    QString path_, connName_;
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
inline const char *kActiveStatuses = "('New','Confirmed','Delivering','In-Rental')";

inline QList<QPair<QDate,QDate>> bookedRanges(QSqlDatabase conn, int vehicleId){
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
inline bool vehicleFreeForDates(QSqlDatabase conn, int vehicleId,
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

inline QString maskText(const QString &text,int keepStart,int keepEnd){
    if(text.size()<=keepStart+keepEnd) return text;
    return text.left(keepStart)+QString(text.size()-keepStart-keepEnd,QChar('*'))+text.right(keepEnd);
}
inline QString categoryLabel(const QString &c){
    if(c=="Electric Cars") return "Ô tô điện";
    if(c=="Electric Scooters") return "Xe máy điện";
    if(c=="Motorbike") return "Xe máy";
    if(c=="4-seat car") return "Ô tô 4 chỗ";
    if(c=="7-seat car") return "Ô tô 7 chỗ";
    if(c=="Electric vehicle") return "Xe điện";
    return c;
}
inline QString roleLabel(const QString &r){
    if(r=="Admin") return "Quản trị viên";
    if(r=="Staff") return "Nhân viên";
    return "Khách hàng";
}
inline QString statusVi(const QString &st){
    if(st=="New") return "Chờ duyệt";
    if(st=="Confirmed") return "Đã xác nhận";
    if(st=="Delivering") return "Đang giao xe";
    if(st=="In-Rental") return "Đang thuê";
    if(st=="Completed") return "Hoàn thành";
    if(st=="Cancelled") return "Đã hủy";
    return st;
}
inline QString vnd(double v){ return QLocale(QLocale::Vietnamese).toString((qlonglong)v)+"đ"; }
inline QString handoverCode(int id){ return QString("HD%1").arg(id,3,10,QChar('0')); }

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

    // State machine: New->Confirmed/Cancelled; Confirmed->Delivering/In-Rental/Cancelled;
    // Delivering->In-Rental/Cancelled; In-Rental->Completed. rental_count tang khi Completed.
    bool changeStatus(int id,const QString &status,QString *err,bool manageTx=true){
        auto fail=[&](const QString &m){ if(err)*err=m; return false; };
        QSqlQuery cur(db->connection());
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
        QSqlDatabase conn=db->connection();
        if(manageTx && !conn.transaction()) return fail("Không thể bắt đầu giao dịch: "+conn.lastError().text());
        QSqlQuery q(conn);
        q.prepare("UPDATE orders SET status=? WHERE id=? AND status=?");
        q.addBindValue(status);q.addBindValue(id);q.addBindValue(old);
        bool ok=q.exec();
        QString dbErr=q.lastError().text();
        if(ok&&q.numRowsAffected()==0){ ok=false; dbErr="đơn không còn ở trạng thái '"+old+"' (có thể vừa được người khác cập nhật)."; }
        q.finish();
        if(ok&&status=="Completed"){
            QSqlQuery rc(conn);
            rc.prepare("UPDATE vehicles SET rental_count=rental_count+1 WHERE id IN (SELECT vehicle_id FROM order_items WHERE order_id=?)");
            rc.addBindValue(id);
            ok=rc.exec();
        }
        if(!ok){ if(manageTx) conn.rollback(); return fail("Cập nhật trạng thái thất bại: "+(dbErr.isEmpty()?conn.lastError().text():dbErr)); }
        if(manageTx) conn.commit();
        return true;
    }
    // Khach chi huy duoc don cua minh khi con New/Confirmed
    bool cancelByCustomer(int orderId,int userId){
        QSqlQuery c(db->connection());
        c.prepare("UPDATE orders SET status='Cancelled' WHERE id=? AND user_id=? AND status IN ('New','Confirmed')");
        c.addBindValue(orderId); c.addBindValue(userId);
        return c.exec() && c.numRowsAffected()>0;
    }
    QSqlQueryModel *unused(){return nullptr;}
private: Database *db;
};

