# VINFAST RENTAL – Vehicle Rental System (C++ / Qt 6 / SQLite)

## Architecture
The application follows MVC:
- **Model:** `User`, `Vehicle`, `CartItem`, `Database`, `VehicleModel`, `AccountModel`, `OrderModel`.
- **View:** Qt Widgets pages, product cards, tables and dialogs.
- **Controller:** event handling and Qt signal/slot logic in `MainWindow`.

## UI update based on the supplied specification
The desktop GUI has been restyled to follow the visual direction and screen structure shown on pages 36–40 of `Vehicle Rental System Specification - Section I(1).pdf`:
- Home/catalog page: dark blue hero banner, search bar, category/sort filters and vehicle product cards.
- Vehicle details: large visual area, vehicle specifications, rental date pickers and add-to-cart action.
- Account, cart, booking, order history, staff and admin pages remain part of the desktop app.
- Main colors, buttons, tabs, cards, form fields and tables use a consistent blue/white visual theme.

**Image note:** the reference PDF contains vehicle photos, but the project does not include the original image assets. The current Qt version uses vehicle-symbol placeholders in the product cards and detail view. Replace these placeholders with licensed/local vehicle images to match the PDF more closely.

## Requirements
- C++17 compiler
- Qt 6 with Widgets and Sql modules
- CMake 3.16+

## Build
```bash
cmake -S . -B build
cmake --build build
```
Run `build/VehicleRentalSystem` (Linux/macOS) or the generated `.exe` in the Windows build directory.

The SQLite file `vehicle_rental.db` is created automatically in the application's working directory. To start with the new VinFast demo catalog, close the application and delete an older `vehicle_rental.db` once before running the updated version.

## Demo accounts
| Role | Email | Password |
|---|---|---|
| Admin | admin@rental.com | admin123 |
| Staff | staff@rental.com | staff123 |
| Customer | customer@rental.com | 123456 |

## Prototype functionality
- Browse, search, filter and sort vehicle catalog
- Vehicle details and rental date selection
- Add/update/remove cart items
- Customer registration, login, profile update and demo password reset
- Booking with driver/license details
- COD and simulated online payment
- Customer order history and cancellation for eligible orders
- Staff order status operations
- Admin vehicle management and user/order overview
- SQLite persistence

## Known limitations
1. This is a **desktop Qt application**, not a browser website. The interface is adapted to resemble the PDF while retaining Qt Widgets.
2. Vehicle pictures in the PDF are not bundled; placeholders are used until image assets are supplied.
3. Online payment and email confirmation are simulations.
4. Password handling is simplified for a classroom prototype and must be replaced with secure password hashing before production.
5. Delivery charges are demonstration values, not a complete official ward-level delivery fee table.
6. The current implementation should still be reviewed against every detailed use-case flow and the class diagram before final submission.

## Build verification
The source package was updated, but it was **not compiled in this environment** because Qt 6 development packages are not installed here. Build it on a computer with Qt 6 and CMake to verify the local toolchain.
